import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import { maskNonCode } from './cpp-source.mjs';

// The Windows registry calls in GUI_App.cpp leave something usable behind only
// when they succeed. RegGetValueW fills its buffer and its type only when it
// returns ERROR_SUCCESS: after ERROR_MORE_DATA (a value longer than the
// buffer), ERROR_ACCESS_DENIED or any other failure the buffer holds whatever
// the stack held, possibly with no terminator, and is_associate_files ran
// wcscmp over it after every result except ERROR_FILE_NOT_FOUND. RegCreateKeyExW
// and RegOpenKeyExW hand back a key only when they succeed, and
// set_into_win_registry passed its HKEY to RegCloseKey even when
// RegCreateKeyExW had failed and the handle had never been set.
//
// These contracts read GUI_App.cpp as text, so they run without a build. Every
// later use of the buffer or the type a registry read fills, and every
// RegCloseKey, must sit where the call that filled the buffer or opened the key
// is known to have returned ERROR_SUCCESS. Known means implied by the
// conditions on the way there: the enclosing if and else branches, earlier
// exits such as `if (...) return false;`, and the left-hand side of && and ||.
// A condition only counts when it is decided after the call and before the
// result is overwritten, and the implication is checked by truth table, so a
// guard written as `if (rc != ERROR_SUCCESS && !missing) return false;` and
// then `if (!missing)` is understood without trusting any variable's name.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiAppPath = path.join(repoDir, 'src', 'slic3r', 'GUI', 'GUI_App.cpp');

// The build defines UNICODE, so the plain names are the W functions.
const CALL = /(?<![A-Za-z0-9_])(Reg(?:GetValue|QueryValueEx|OpenKeyEx|OpenKey|CreateKeyEx|CreateKey|CloseKey)[AW]?)(?=\s*\()/g;
// The argument positions of the type and the data a read fills.
const OUTPUTS = { RegGetValue: [4, 5], RegQueryValueEx: [3, 4] };
const OPENS = new Set(['RegOpenKeyEx', 'RegOpenKey', 'RegCreateKeyEx', 'RegCreateKey']);
const SIZE_OF = /(?:sizeof|std::size|_countof|ARRAYSIZE)\s*\(\s*$|sizeof\s+$/;

const squeeze = (text) => text.replace(/\s+/g, ' ').trim();
const isWordChar = (c) => c !== undefined && /[A-Za-z0-9_]/.test(c);
const isWordAt = (code, i, word) => code.startsWith(word, i) && !isWordChar(code[i - 1]) && !isWordChar(code[i + word.length]);
const lineOf = (code, index) => code.slice(0, index).split('\n').length;

function skipSpace(code, i) {
  while (i < code.length && /\s/.test(code[i])) i++;
  return i;
}

// Blanks preprocessor lines, continuations included, and keeps their newlines.
function maskDirectives(code) {
  let continued = false;
  return code
    .split('\n')
    .map((line) => {
      const directive = continued || /^\s*#/.test(line);
      continued = directive && line.endsWith('\\');
      return directive ? ' '.repeat(line.length) : line;
    })
    .join('\n');
}

// The offset of the bracket that closes the one at `open`.
function closing(code, open) {
  const pair = { '(': ')', '[': ']', '{': '}' }[code[open]];
  let depth = 0;
  for (let i = open; i < code.length; i++) {
    if (code[i] === code[open]) depth++;
    else if (code[i] === pair && --depth === 0) return i;
  }
  assert.fail(`the ${code[open]} at offset ${open} never closes`);
}

// The function body around `index`. GUI_App.cpp opens and closes every
// function body at column 0, so it runs from the last "\n{" before the index to
// the first "\n}" after it.
function functionAround(code, index) {
  const open = code.lastIndexOf('\n{', index) + 1;
  const close = code.indexOf('\n}', index) + 1;
  assert.ok(open > 0 && close > index && code.lastIndexOf('\n}', index) + 1 < open, `offset ${index} must lie in a function body whose braces sit at column 0`);
  return { open, close, signature: squeeze(code.slice(code.lastIndexOf('\n', open - 2) + 1, open)) };
}

// Every registry call: its name without A or W, its arguments, and where its
// argument list opens and ends.
function callsIn(code) {
  const calls = [];
  for (const match of code.matchAll(CALL)) {
    const open = code.indexOf('(', match.index + match[1].length);
    const close = closing(code, open);
    const args = [];
    let from = open + 1;
    let depth = 0;
    for (let i = open + 1; i < close; i++) {
      const c = code[i];
      if (c === '(' || c === '[' || c === '{') depth++;
      else if (c === ')' || c === ']' || c === '}') depth--;
      else if (c === ',' && depth === 0) {
        args.push(squeeze(code.slice(from, i)));
        from = i + 1;
      }
    }
    args.push(squeeze(code.slice(from, close)));
    calls.push({ name: match[1].replace(/[AW]$/, ''), index: match.index, open, end: close + 1, args });
  }
  return calls;
}

// How the result of `call` is kept: the variable it is assigned to, or the
// call itself when it is compared with ERROR_SUCCESS where it stands.
function resultOf(code, call) {
  const statement = Math.max(code.lastIndexOf(';', call.index), code.lastIndexOf('{', call.index), code.lastIndexOf('}', call.index)) + 1;
  const head = code.slice(statement, call.index);
  const assigned = /([A-Za-z_]\w*)\s*=\s*(?:::)?\s*$/.exec(head);
  if (assigned) return { variable: assigned[1] };
  if (/^\s*[!=]=\s*ERROR_SUCCESS(?![A-Za-z0-9_])/.test(code.slice(call.end)) || /ERROR_SUCCESS\s*[!=]=\s*(?:::)?\s*$/.test(head)) {
    return { call: squeeze(code.slice(call.index, call.end)) };
  }
  return null;
}

// Where a condition about the result of `call` is about this call: from the
// call to the next assignment of its variable.
function windowOf(code, call, result, end) {
  if (!result.variable) return { from: call.index, to: end };
  for (const at of wordsIn(code, result.variable, call.end, end)) {
    const next = skipSpace(code, at + result.variable.length);
    if (code[next] === '=' && code[next + 1] !== '=') return { from: call.end, to: at };
  }
  return { from: call.end, to: end };
}

// Offsets of `word` as a whole name, and not as a member, between `from` and `to`.
function* wordsIn(code, word, from, to) {
  for (let i = code.indexOf(word, from); i >= 0 && i < to; i = code.indexOf(word, i + word.length)) {
    const before = code[i - 1];
    if (isWordChar(before) || before === '.' || (before === '>' && code[i - 2] === '-') || isWordChar(code[i + word.length])) continue;
    yield i;
  }
}

// The variable an output argument names: `value`, `&type` or `(LPBYTE)buffer`.
function outputName(arg) {
  const match = /^(?:(?:reinterpret|static)_cast<[^<>]*>\(\s*)?(?:\([^()]*\)\s*)?&?\s*([A-Za-z_]\w*)/.exec(arg ?? '');
  return match && match[1] !== 'nullptr' && match[1] !== 'NULL' ? match[1] : null;
}

// Reads the statement that starts at or after `i` and before `end`.
function statementAt(code, i, end) {
  i = skipSpace(code, i);
  if (i >= end) return null;
  if (code[i] === '{') {
    const close = closing(code, i);
    return { kind: 'block', start: i, end: close + 1, body: statementsIn(code, i + 1, close) };
  }
  if (isWordAt(code, i, 'if')) {
    let open = skipSpace(code, i + 2);
    if (isWordAt(code, open, 'constexpr')) open = skipSpace(code, open + 'constexpr'.length);
    const close = closing(code, open);
    const then = statementAt(code, close + 1, end);
    const next = skipSpace(code, then.end);
    const otherwise = isWordAt(code, next, 'else') ? statementAt(code, next + 'else'.length, end) : null;
    return { kind: 'if', start: i, end: (otherwise ?? then).end, cond: { start: open + 1, end: close }, then, otherwise };
  }
  for (const loop of ['for', 'while', 'switch']) {
    if (isWordAt(code, i, loop)) {
      const body = statementAt(code, closing(code, skipSpace(code, i + loop.length)) + 1, end);
      return { kind: 'loop', start: i, end: body.end, body };
    }
  }
  if (isWordAt(code, i, 'do')) {
    const body = statementAt(code, i + 'do'.length, end);
    return { kind: 'loop', start: i, end: code.indexOf(';', body.end) + 1, body };
  }
  // A declaration, an expression or a jump, up to the ; that ends it.
  let depth = 0;
  for (let j = i; j < end; j++) {
    const c = code[j];
    if (c === '(' || c === '[' || c === '{') depth++;
    else if (c === ')' || c === ']' || c === '}') depth--;
    else if (c === ';' && depth === 0) return { kind: 'simple', start: i, end: j + 1 };
  }
  assert.fail(`the statement at offset ${i} never ends with a ;`);
}

function statementsIn(code, start, end) {
  const list = [];
  for (let s = statementAt(code, start, end); s; s = statementAt(code, s.end, end)) list.push(s);
  return list;
}

// True when control never runs past the end of the statement.
function exits(code, s) {
  if (!s) return false;
  if (s.kind === 'simple') return /^(?:return|continue|break|throw|goto)(?![A-Za-z0-9_])/.test(code.slice(s.start, s.end));
  if (s.kind === 'block') return s.body.some((child) => exits(code, child));
  if (s.kind === 'if') return exits(code, s.then) && exits(code, s.otherwise);
  return false;
}

// One comparison or other operand of && and ||. `rc != ERROR_SUCCESS` becomes
// the negation of `rc == ERROR_SUCCESS`, and both sides of == are sorted, so a
// test written either way meets the same atom in the truth table.
function atomOf(text) {
  const t = squeeze(text)
    .replace(/ ?([()[\],]) ?/g, '$1')
    .replace(/(^|[^A-Za-z0-9_>])::/g, '$1');
  let depth = 0;
  for (let i = 0; i < t.length - 1; i++) {
    const c = t[i];
    if (c === '(' || c === '[') depth++;
    else if (c === ')' || c === ']') depth--;
    else if (depth === 0 && (c === '=' || c === '!') && t[i + 1] === '=' && !'=!<>'.includes(t[i - 1] ?? ' ') && t[i + 2] !== '=') {
      const key = [t.slice(0, i).trim(), t.slice(i + 2).trim()].sort().join(' == ');
      return c === '!' ? { op: '!', item: { key } } : { key };
    }
  }
  return { key: t };
}

// Parses the condition between `start` and `end` into &&, || and ! over atoms.
// Every node keeps the offsets it covers, so a position can be found in it.
function parseCondition(code, start, end) {
  let i = start;
  const at = (op) => {
    i = skipSpace(code, i);
    return i < end && code.startsWith(op, i);
  };
  const list = (op, next) => {
    const items = [next()];
    while (at(op)) {
      i += op.length;
      items.push(next());
    }
    return items.length === 1 ? items[0] : { op, items, start: items[0].start, end: items.at(-1).end };
  };
  const or = () => list('||', and);
  const and = () => list('&&', unary);
  const unary = () => {
    i = skipSpace(code, i);
    const from = i;
    if (code[i] === '!' && code[i + 1] !== '=') {
      i++;
      const item = unary();
      return { op: '!', item, start: from, end: item.end };
    }
    if (code[i] === '(') {
      // (a && b) is a group; the (a + b) of (a + b) == c belongs to an atom.
      const close = closing(code, i);
      const after = skipSpace(code, close + 1);
      if (after >= end || code.startsWith('&&', after) || code.startsWith('||', after)) {
        i = close + 1;
        return { ...parseCondition(code, from + 1, close), start: from, end: close + 1 };
      }
    }
    let depth = 0;
    while (i < end && !(depth === 0 && (code.startsWith('&&', i) || code.startsWith('||', i)))) {
      if (code[i] === '(' || code[i] === '[') depth++;
      else if (code[i] === ')' || code[i] === ']') depth--;
      i++;
    }
    return { ...atomOf(code.slice(from, i)), start: from, end: i };
  };
  return or();
}

// What holds at `p` inside a condition: the operands to its left were true
// under && and false under ||.
function shortCircuit(node, p) {
  if (!node.op) return [];
  if (node.op === '!') return shortCircuit(node.item, p);
  const facts = [];
  for (const item of node.items) {
    if (p < item.end) return [...facts, ...shortCircuit(item, p)];
    facts.push({ node: node.op === '&&' ? item : { op: '!', item }, start: item.start, end: item.end });
  }
  return facts;
}

// The part of a simple statement that is one expression: what follows
// `return`, or the right-hand side of its assignment.
function expressionOf(code, s) {
  let start = s.start;
  if (/^return(?![A-Za-z0-9_])/.test(code.slice(s.start, s.end))) start += 'return'.length;
  let depth = 0;
  for (let i = start; i < s.end; i++) {
    const c = code[i];
    if (c === '(' || c === '[' || c === '{') depth++;
    else if (c === ')' || c === ']' || c === '}') depth--;
    else if (c === '=' && depth === 0 && !'=!<>+-*/%&|^'.includes(code[i - 1]) && code[i + 1] !== '=') start = i + 1;
  }
  return { start, end: code[s.end - 1] === ';' ? s.end - 1 : s.end };
}

// The conditions known to hold at `p`, each with the offsets of the code that
// decides it.
function factsAt(code, statements, p, facts = []) {
  for (const s of statements) {
    if (s.end <= p) {
      if (s.kind === 'if' && exits(code, s.then) !== exits(code, s.otherwise)) {
        const cond = parseCondition(code, s.cond.start, s.cond.end);
        facts.push({ node: exits(code, s.then) ? { op: '!', item: cond } : cond, ...s.cond });
      }
      continue;
    }
    if (p < s.start) break;
    if (s.kind === 'block') return factsAt(code, s.body, p, facts);
    if (s.kind === 'loop') return p >= s.body.start ? factsAt(code, [s.body], p, facts) : facts;
    if (s.kind === 'if') {
      const cond = parseCondition(code, s.cond.start, s.cond.end);
      if (p < s.cond.end) return [...facts, ...shortCircuit(cond, p)];
      if (p < s.then.end) return factsAt(code, [s.then], p, [...facts, { node: cond, ...s.cond }]);
      return factsAt(code, [s.otherwise], p, [...facts, { node: { op: '!', item: cond }, ...s.cond }]);
    }
    const expression = expressionOf(code, s);
    return [...facts, ...shortCircuit(parseCondition(code, expression.start, expression.end), p)];
  }
  return facts;
}

function holds(node, value) {
  if (node.op === '&&') return node.items.every((item) => holds(item, value));
  if (node.op === '||') return node.items.some((item) => holds(item, value));
  if (node.op === '!') return !holds(node.item, value);
  return value.get(node.key);
}

function atomsOf(node, keys) {
  if (node.op === '!') atomsOf(node.item, keys);
  else if (node.op) node.items.forEach((item) => atomsOf(item, keys));
  else keys.add(node.key);
  return keys;
}

// True when the facts can all hold, and every assignment of the atoms that
// makes them all true makes `goal` true as well.
function implies(facts, goal) {
  const keys = [...atomsOf({ op: '&&', items: [...facts.map((fact) => fact.node), { key: goal }] }, new Set())];
  assert.ok(keys.length <= 16, `too many conditions for a truth table: ${keys.join(' | ')}`);
  let reachable = false;
  for (let bits = 0; bits < 2 ** keys.length; bits++) {
    const value = new Map(keys.map((key, n) => [key, ((bits >> n) & 1) === 1]));
    if (!facts.every((fact) => holds(fact.node, value))) continue;
    if (!value.get(goal)) return false;
    reachable = true;
  }
  return reachable;
}

// Checks every use of what a registry read fills, and every RegCloseKey, in
// `source`. `checked` lists each use that was checked; `problems` names, by
// line, each one whose call is not known to have succeeded there.
function analyse(source) {
  const { code: masked, complete } = maskNonCode(source);
  const code = maskDirectives(masked);
  const calls = callsIn(code);
  const parsed = new Map();
  const reads = [];
  const checked = [];
  const problems = [];

  const unkept = (call, fn) =>
    problems.push(`${lineOf(code, call.index)}: the result of ${call.name}, in ${fn.signature}, is neither kept in a variable nor compared with ERROR_SUCCESS`);

  const verify = (fn, call, result, position, what) => {
    if (!parsed.has(fn.open)) parsed.set(fn.open, statementsIn(code, fn.open + 1, fn.close));
    const { from, to } = windowOf(code, call, result, fn.close);
    const facts = factsAt(code, parsed.get(fn.open), position).filter(
      (fact) => (fact.start >= from || (fact.start <= call.index && call.end <= fact.end)) && fact.end <= to
    );
    const goal = `${result.variable ?? result.call} == ERROR_SUCCESS`;
    const line = lineOf(code, position);
    checked.push({ line, signature: fn.signature, what });
    if (!implies(facts, atomOf(goal).key)) problems.push(`${line}: ${what}, in ${fn.signature}, is used where ${goal} is not known`);
  };

  for (const call of calls) {
    const fn = functionAround(code, call.index);
    if (OUTPUTS[call.name]) {
      reads.push({ call, signature: fn.signature });
      const outputs = OUTPUTS[call.name].map((index) => outputName(call.args[index])).filter(Boolean);
      if (outputs.length === 0) continue;
      const result = resultOf(code, call);
      if (!result) {
        unkept(call, fn);
        continue;
      }
      for (const name of outputs) {
        for (const position of wordsIn(code, name, call.end, fn.close)) {
          if (SIZE_OF.test(code.slice(Math.max(fn.open, position - 40), position))) continue;
          // A later read that fills the same variable owns the uses after it.
          const filler = calls
            .filter((other) => OUTPUTS[other.name] && other.index > fn.open && other.index < position)
            .filter((other) => OUTPUTS[other.name].some((index) => outputName(other.args[index]) === name))
            .at(-1);
          if (filler !== call) continue;
          verify(fn, call, result, position, `${name} (filled by ${call.name} on line ${lineOf(code, call.index)})`);
        }
      }
    } else if (call.name === 'RegCloseKey') {
      const key = call.args[0];
      const opener = calls
        .filter((other) => OPENS.has(other.name) && other.index > fn.open && other.index < call.index)
        .filter((other) => other.args.some((arg) => arg.replace(/\s+/g, '') === `&${key}`))
        .at(-1);
      if (!opener) {
        problems.push(`${lineOf(code, call.index)}: RegCloseKey(${key}), in ${fn.signature}, closes a key no call before it in the same function opened`);
        continue;
      }
      const result = resultOf(code, opener);
      if (!result) {
        unkept(opener, fn);
        continue;
      }
      verify(fn, opener, result, call.index, `RegCloseKey(${key}) (the key ${opener.name} opened on line ${lineOf(code, opener.index)})`);
    }
  }
  return { complete, reads, checked, problems };
}

const inFunction = (body) => `void example()\n{\n${body}\n}\n`;

test('the scan flags a buffer, a type or a key used where its call may have failed', () => {
  const flagged = {
    'compared unless the value was missing, as is_associate_files did': `
wchar_t buf[1000];
DWORD size = sizeof(buf);
int rc = ::RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_ANY, nullptr, buf, &size);
bool missing = rc == ERROR_FILE_NOT_FOUND;
if (!missing && ::wcscmp(buf, id) == 0)
    return true;
return false;`,
    'compared without looking at the result': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
return ::wcscmp(buf, id) == 0;`,
    'used after ERROR_MORE_DATA was let through': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
if (rc != ERROR_SUCCESS && rc != ERROR_MORE_DATA)
    return false;
use(buf);`,
    'the type read after a failure': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_ANY, &type, buf, &size);
if (rc != ERROR_FILE_NOT_FOUND && type == REG_SZ)
    return true;
return false;`,
    'checked before the call, against an older result': `
if (rc == ERROR_SUCCESS) {
    rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
    use(buf);
}`,
    'a read whose result is never kept': `
RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
use(buf);`,
    'closed after RegCreateKeyExW failed, as set_into_win_registry did': `
HKEY key;
LSTATUS rc = RegCreateKeyExW(hive, path, 0, 0, 0, KEY_ALL_ACCESS, nullptr, &key, &disposition);
if (rc == ERROR_SUCCESS) {
    rc = RegSetValueExW(key, L"", 0, REG_SZ, data, bytes);
}
RegCloseKey(key);`,
    'checked only after the result was overwritten': `
LSTATUS rc = RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &key);
rc = RegDeleteValueW(key, name);
if (rc == ERROR_SUCCESS)
    RegCloseKey(key);`,
    'closed when either of two opens into one handle succeeded': `
LSTATUS first = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key);
LSTATUS second = RegOpenKeyExW(HKEY_LOCAL_MACHINE, other, 0, KEY_READ, &key);
if (first == ERROR_SUCCESS || second == ERROR_SUCCESS)
    RegCloseKey(key);`,
    'guarded only by a comment': `
LSTATUS rc = RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &key);
/* if (rc == ERROR_SUCCESS) */ RegCloseKey(key);`,
    'closed without being opened in the function': `
RegCloseKey(key);`,
  };
  for (const [name, body] of Object.entries(flagged)) {
    assert.equal(analyse(inFunction(body)).problems.length, 1, `must be flagged once: ${name}`);
  }
});

test('the scan accepts each way this code checks a result before it uses it', () => {
  const accepted = {
    'the comparison behind && on ERROR_SUCCESS': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
return rc == ERROR_SUCCESS && ::wcscmp(buf, id) == 0;`,
    'a branch taken on success, written either way round': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
if (ERROR_SUCCESS == rc)
    use(buf);`,
    'the else branch of a failure test': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
if (rc != ERROR_SUCCESS)
    missing();
else
    use(buf);`,
    'an early return on failure': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
if (rc != ERROR_SUCCESS)
    return false;
use(buf);`,
    'an early return at the end of a block': `
LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, key, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size);
if (rc != ERROR_SUCCESS) {
    log(rc);
    return false;
}
use(buf);`,
    'a continue in a loop, as read_installer_language_mode does': `
for (const wchar_t *key : keys) {
    wchar_t value[64] = {};
    DWORD size = sizeof(value);
    const LSTATUS status = ::RegGetValueW(
        HKEY_CURRENT_USER, key, L"Mode", RRF_RT_REG_SZ,
        nullptr, value, &size);
    if (status != ERROR_SUCCESS || value[0] == 0)
        continue;
    use(value);
}`,
    'two tests that together leave only success, as set_into_win_registry reads': `
int rc = ::RegGetValueW(hive, path, nullptr, RRF_RT_ANY, &type, buf, &size);
bool missing = rc == ERROR_FILE_NOT_FOUND;
if ((rc != ERROR_SUCCESS) && !missing)
    return false;
if (!missing) {
    if (type != REG_SZ)
        return false;
    if (::wcscmp(buf, value) == 0)
        return false;
}
return true;`,
    'a close in the branch that saw the key open, before the result is reused': `
HKEY key = nullptr;
LSTATUS rc = RegCreateKeyExW(hive, path, 0, 0, 0, KEY_ALL_ACCESS, nullptr, &key, &disposition);
if (rc == ERROR_SUCCESS) {
    rc = RegSetValueExW(key, L"", 0, REG_SZ, data, bytes);
    RegCloseKey(key);
}`,
    'an open compared with ERROR_SUCCESS where it stands': `
if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    read(key);
    RegCloseKey(key);
}`,
    'an early return after the open, with a directive before the close': `
LSTATUS rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key);
if (rc != ERROR_SUCCESS) return {};
#if defined(_DEBUG)
trace(key);
#endif
RegCloseKey(key);`,
  };
  for (const [name, body] of Object.entries(accepted)) {
    const { checked, problems } = analyse(inFunction(body));
    assert.deepEqual(problems, [], `must be accepted: ${name}`);
    assert.ok(checked.length > 0, `the scan must have checked a use: ${name}`);
  }
});

test('comments and literals can neither hide a registry call nor fake one', () => {
  const open = 'LSTATUS rc = RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &key);';
  for (const body of [`${open}\n// RegCloseKey(key);`, `${open}\nconst char *text = "RegCloseKey(key);";`, `${open}\n/* RegCloseKey(key); */`]) {
    assert.deepEqual(analyse(inFunction(body)).checked, [], `not a call: ${JSON.stringify(body)}`);
  }
  const { checked, problems } = analyse(inFunction(`${open}\nconst char *text = "if (rc == ERROR_SUCCESS) {";\nRegCloseKey(key);`));
  assert.equal(checked.length, 1, 'the close after the literal is still seen');
  assert.equal(problems.length, 1, 'a condition inside a literal guards nothing');
});

test('every registry read and key close in GUI_App.cpp checks its result first', () => {
  const { complete, reads, checked, problems } = analyse(readFileSync(guiAppPath, 'utf8'));
  assert.equal(complete, true, 'GUI_App.cpp must scan to its end outside comments and literals, or a call after the misread part would be missed');
  for (const name of ['read_installer_language_mode', 'is_associate_files', 'set_into_win_registry', 'del_win_registry']) {
    assert.ok(reads.some((read) => read.signature.includes(`${name}(`)), `the scan must see the registry read in ${name}`);
  }
  assert.ok(
    checked.some((entry) => entry.signature.includes('is_associate_files(') && entry.what.startsWith('szValueCurrent ')),
    'the scan must check the comparison in is_associate_files'
  );
  assert.ok(
    checked.some((entry) => entry.signature.includes('set_into_win_registry(') && entry.what.startsWith('RegCloseKey(hkey) ')),
    'the scan must check the RegCloseKey in set_into_win_registry'
  );
  assert.deepEqual(
    problems.map((problem) => `src/slic3r/GUI/GUI_App.cpp:${problem}`),
    [],
    'use what a registry read filled only where the read returned ERROR_SUCCESS, and close a key only where the call that opened it returned ERROR_SUCCESS'
  );
});

test('is_associate_files reads the association as REG_SZ only', () => {
  const read = analyse(readFileSync(guiAppPath, 'utf8')).reads.find((entry) => entry.signature.includes('is_associate_files('));
  assert.ok(read, 'the scan must see the registry read in is_associate_files');
  assert.equal(
    read.call.args[3],
    'RRF_RT_REG_SZ',
    'only a REG_SZ read that succeeds is sure to leave a terminated string for wcscmp; RRF_RT_ANY also returns binary data with no terminator'
  );
});
