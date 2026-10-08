import assert from 'node:assert/strict';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v229 never started: LoadLibrary returned ERROR_DLL_INIT_FAILED (1114)
// for BambuStudio.dll. MainFrame.cpp translated "Ctrl+" in a namespace-scope
// initializer, the translation now ends in PersonalVocabulary::remember(), and
// remember() inserted into a namespace-scope std::set whose own initializer had
// not run yet. The empty tree's null head was dereferenced inside the CRT
// initializers, so the DLL faulted while loading.
//
// The order in which translation units run their initializers is not under
// anyone's control, so two source contracts keep this from coming back:
//  1. Code reached by every translation keeps no namespace-scope object that
//     needs a dynamic initializer; mutable state is created on first use.
//  2. Namespace-scope initializers in src/slic3r and src/libslic3r do not
//     translate text, except the reviewed ones listed below. They run before
//     any catalog exists, so they are never localized anyway.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (relative) => readFileSync(path.join(repoDir, relative), 'utf8');

// ---------------------------------------------------------------------------
// A small C++ reader: enough to find the declarations at namespace scope.

// The shipped build is Windows-only and compiled by MSVC. Each #if group keeps
// the branch that build compiles; a condition this cannot decide keeps its
// first branch. Alternative branches often hold half of a bracket pair.
const DEFINED = new Set(['_WIN32', 'WIN32', '_WIN64', '_MSC_VER', '__WXMSW__', '_WINDOWS']);
const UNDEFINED = new Set(['__APPLE__', '__MACH__', '__linux__', '__linux', '__unix__', '__unix', '__FreeBSD__',
    '__OpenBSD__', '__GNUC__', '__clang__', '__MINGW32__', '__WXOSX__', '__WXMAC__', '__WXGTK__', '__WXGTK20__',
    '__WXGTK3__']);
const macroValue = (name) => (DEFINED.has(name) ? true : UNDEFINED.has(name) ? false : null);

// true, false, or null when the condition cannot be decided from the sets above.
export function windowsCondition(expression) {
    const tokens = expression.match(/[A-Za-z_]\w*|\d\w*|&&|\|\||[!()]|\S/g) ?? [];
    let at = 0;
    const fail = () => { at = tokens.length + 1; return null; };
    const not = (v) => (v === null ? null : !v);
    const and = (a, b) => (a === false || b === false ? false : a === null || b === null ? null : true);
    const or = (a, b) => (a === true || b === true ? true : a === null || b === null ? null : false);
    function primary() {
        const token = tokens[at++];
        if (token === '!') return not(primary());
        if (token === '(') { const v = disjunction(); return tokens[at++] === ')' ? v : fail(); }
        if (token === 'defined') {
            const paren = tokens[at] === '(';
            if (paren) at++;
            const name = tokens[at++];
            if (paren && tokens[at++] !== ')') return fail();
            return macroValue(name);
        }
        if (token !== undefined && /^\d+$/.test(token)) return Number(token) !== 0;
        if (token !== undefined && /^[A-Za-z_]/.test(token) && tokens[at] !== '(') return macroValue(token);
        return fail();
    }
    function conjunction() { let v = primary(); while (tokens[at] === '&&') { at++; v = and(v, primary()); } return v; }
    function disjunction() { let v = conjunction(); while (tokens[at] === '||') { at++; v = or(v, conjunction()); } return v; }
    const value = disjunction();
    return at === tokens.length ? value : null;
}

// Blank comments, preprocessor lines and branches the build does not compile,
// and empty every string and character literal (keeping its quotes). Line
// breaks are preserved so line numbers still match the file.
export function stripCpp(source) {
    let out = '';
    let i = 0;
    const n = source.length;
    let lineStart = true;
    const blank = (text) => text.replace(/[^\n]/g, ' ');
    const lines = (text) => text.replace(/[^\n]/g, '');
    // One frame per open #if: is the enclosing code compiled, was a branch
    // already chosen, and is the current branch compiled.
    const frames = [];
    const active = () => frames.every((frame) => frame.current);
    const directive = (text) => {
        const clean = text.replace(/\\\r?\n/g, ' ').replace(/\/\*.*?\*\/|\/\/.*$/g, ' ');
        const match = /^\s*#\s*(\w+)\s*(.*)$/.exec(clean);
        if (!match) return;
        const [, name, rest] = match;
        if (name === 'if' || name === 'ifdef' || name === 'ifndef') {
            const word = rest.trim().split(/\s+/)[0];
            const value = name === 'if' ? windowsCondition(rest) : name === 'ifdef' ? macroValue(word) : not(macroValue(word));
            const enclosing = active();
            frames.push({ enclosing, taken: value !== false, current: enclosing && value !== false });
        } else if ((name === 'elif' || name === 'else') && frames.length) {
            const frame = frames[frames.length - 1];
            const keep = !frame.taken && (name === 'else' || windowsCondition(rest) !== false);
            frame.current = frame.enclosing && keep;
            frame.taken = frame.taken || keep;
        } else if (name === 'endif' && frames.length) {
            frames.pop();
        }
    };
    const not = (v) => (v === null ? null : !v);
    while (i < n) {
        const c = source[i];
        if (lineStart && /[ \t]/.test(c)) { out += c; i++; continue; }
        if (lineStart && c === '#') {
            let j = i;
            while (j < n && source[j] !== '\n') {
                if (source[j] === '\\' && source[j + 1] === '\n') j += 2;
                else if (source[j] === '\\' && source[j + 1] === '\r' && source[j + 2] === '\n') j += 3;
                else j++;
            }
            directive(source.slice(i, j));
            out += blank(source.slice(i, j));
            i = j;
            continue;
        }
        if (lineStart && c !== '\n' && !active()) {
            let j = source.indexOf('\n', i);
            if (j < 0) j = n;
            out += blank(source.slice(i, j));
            i = j;
            continue;
        }
        lineStart = false;
        if (c === '\n') { out += c; i++; lineStart = true; continue; }
        if (c === '/' && source[i + 1] === '/') {
            let j = i;
            while (j < n && source[j] !== '\n') j++;
            out += blank(source.slice(i, j));
            i = j;
            continue;
        }
        if (c === '/' && source[i + 1] === '*') {
            const end = source.indexOf('*/', i + 2);
            const j = end < 0 ? n : end + 2;
            out += blank(source.slice(i, j));
            i = j;
            continue;
        }
        const raw = /^(?:u8|u|U|L)?R"([^()\\\s]{0,16})\(/.exec(source.slice(i, i + 24));
        if (raw && !/[\w]/.test(source[i - 1] ?? '')) {
            const close = `)${raw[1]}"`;
            const end = source.indexOf(close, i + raw[0].length);
            const j = end < 0 ? n : end + close.length;
            out += '""' + lines(source.slice(i, j));
            i = j;
            continue;
        }
        if (c === '"' || (c === "'" && !/[0-9A-Fa-f]/.test(source[i - 1] ?? ''))) {
            let j = i + 1;
            while (j < n && source[j] !== c && source[j] !== '\n') j += source[j] === '\\' ? 2 : 1;
            out += c + lines(source.slice(i + 1, j)) + c;
            i = j < n && source[j] === c ? j + 1 : j;
            continue;
        }
        out += c;
        i++;
    }
    return out;
}

function matchClose(text, open) {
    const pairs = { '{': '}', '(': ')', '[': ']' };
    const stack = [pairs[text[open]]];
    for (let i = open + 1; i < text.length; i++) {
        const c = text[i];
        if (pairs[c]) stack.push(pairs[c]);
        else if (c === '}' || c === ')' || c === ']') {
            if (stack.pop() !== c) return -1;
            if (stack.length === 0) return i;
        }
    }
    return -1;
}

// Top-level characters of a declaration, with every bracketed group blanked.
function topLevel(text) {
    let out = '';
    let depth = 0;
    for (const c of text) {
        if (c === '(' || c === '[' || c === '{') { out += depth === 0 ? c : ' '; depth++; continue; }
        if (c === ')' || c === ']' || c === '}') { depth--; out += depth === 0 ? c : ' '; continue; }
        out += depth === 0 ? c : ' ';
    }
    return out;
}

// Drop `template <...>` headers, whose default arguments are not initializers.
function withoutTemplateHeader(shape) {
    let out = shape;
    for (let at = out.search(/\btemplate\s*</); at >= 0; at = out.search(/\btemplate\s*</)) {
        let i = out.indexOf('<', at);
        let depth = 0;
        for (; i < out.length; i++) {
            if (out[i] === '<') depth++;
            else if (out[i] === '>' && --depth === 0) break;
        }
        out = out.slice(0, at) + ' '.repeat(Math.min(i, out.length - 1) - at + 1) + out.slice(i + 1);
    }
    return out;
}

const NAMESPACE_HEAD = /(?:^|[\s;{}])(?:inline\s+)?namespace(?:\s+[\w:]+)?(?:\s*\[\[[^\]]*\]\])?\s*$|(?:^|[\s;{}])extern\s*""\s*$/;
const CLASS_HEAD = /(?:^|[\s>;{}])(?:class|struct|union|enum)\b/;

// Every statement at namespace scope: { kind, text, line } where kind is
// 'function', 'class' or 'declaration'. Class and function bodies are blanked.
export function namespaceStatements(stripped) {
    const statements = [];
    let start = 0;
    let i = 0;
    const lineStarts = [0];
    for (let at = stripped.indexOf('\n'); at >= 0; at = stripped.indexOf('\n', at + 1)) lineStarts.push(at + 1);
    const lineOf = (offset) => {
        let low = 0;
        let high = lineStarts.length - 1;
        while (low < high) {
            const mid = (low + high + 1) >> 1;
            if (lineStarts[mid] <= offset) low = mid; else high = mid - 1;
        }
        return low + 1;
    };
    // Bodies of classes and functions in the current statement, left out of its text.
    let hidden = [];
    const hide = (from, to) => hidden.push([from, to]);
    const emit = (end, kind) => {
        let text = '';
        let at = start;
        for (const [from, to] of hidden) { text += stripped.slice(at, from); at = to; }
        text += stripped.slice(at, end);
        if (text.trim()) {
            const first = start + stripped.slice(start, end).search(/\S/);
            statements.push({ kind, text: text.trim(), line: lineOf(first) });
        }
        start = end + 1;
        hidden = [];
    };
    while (i < stripped.length) {
        const c = stripped[i];
        if (c === '(' || c === '[') {
            const close = matchClose(stripped, i);
            if (close < 0) throw new Error(`unbalanced ${c} at line ${lineOf(i)}`);
            i = close + 1;
            continue;
        }
        if (c === ';') { emit(i, 'declaration'); i++; continue; }
        if (c === '}') { emit(i, 'declaration'); i++; continue; }
        if (c !== '{') { i++; continue; }
        const head = stripped.slice(start, i);
        if (hidden.length === 0 && NAMESPACE_HEAD.test(head)) { start = i + 1; i++; continue; }
        const close = matchClose(stripped, i);
        if (close < 0) throw new Error(`unbalanced { at line ${lineOf(i)}`);
        const shape = withoutTemplateHeader(topLevel(head));
        const before = head.trimEnd().slice(-1);
        if (shape.includes('=') && !/\boperator\s*[^\s(]*=/.test(shape)) { i = close + 1; continue; }
        if (shape.includes('(')) {
            const afterParams = shape.slice(shape.indexOf(')') + 1);
            const memberInit = /(?:^|[^:]):(?!:)/.test(afterParams) && /[\w>]/.test(before);
            if (memberInit) { i = close + 1; continue; }
            hide(i + 1, close);
            emit(close, 'function');
            i = close + 1;
            continue;
        }
        if (CLASS_HEAD.test(shape)) {
            hide(i + 1, close);
            const tail = /^\s*;/.exec(stripped.slice(close + 1, close + 1 + 4096));
            if (tail) { emit(close + tail[0].length, 'class'); i = close + 1 + tail[0].length; continue; }
            i = close + 1;
            continue;
        }
        i = close + 1; // brace initializer: Type name{...};
    }
    return statements;
}

// A namespace-scope declaration that defines a variable, or null.
const PARAMETER = /^(?:(?:const|volatile|unsigned|signed|struct|class|enum)\s+)*[\w:]+(?:\s*<.*>)?(?:\s*(?:const\s*)?[*&]+\s*|\s+)(?:const\s+)?(?:[A-Za-z_]\w*)?(?:\s*\[\w*\])?$|^\.\.\.$|^void$/;

export function variableDefinition(statement) {
    if (statement.kind === 'function') return null;
    const text = statement.text.replace(/\s+/g, ' ').trim();
    if (/^(?:using|typedef|template|static_assert|friend|namespace)\b/.test(text)) return null;
    if (/^(?:(?:enum\s+)?class|struct|union|enum)\b/.test(text) && !/\}\s*[\w*&]/.test(text)) return null;
    if (statement.kind === 'class' && !/\}\s*[\w*&]/.test(text)) return null;
    const shape = topLevel(text);
    if (/^extern\b/.test(text) && !shape.includes('=')) return null;
    const eq = shape.indexOf('=');
    const paren = shape.indexOf('(');
    if (/\boperator\b/.test(paren < 0 ? shape : shape.slice(0, paren))) return null;
    if (eq >= 0 && (paren < 0 || eq < paren)) return { text, initializer: text.slice(eq + 1) };
    if (paren >= 0) {
        const head = text.slice(0, paren).trim();
        if (!/^[\w:<>,\s*&]*[\w>*&]\s*[*&]?\s*[A-Za-z_][\w:]*$/.test(head) || !/\s|[*&]/.test(head)) return null;
        const close = matchClose(text, paren);
        if (close < 0 || text.slice(close + 1).trim()) return null; // const, noexcept, override, ...
        const inside = text.slice(paren + 1, close).trim();
        if (!inside || topLevel(inside).includes('=')) return null; // prototype or default arguments
        const items = topLevel(inside).split(',').map((item, index, all) => {
            const from = all.slice(0, index).join(',').length + (index ? 1 : 0);
            return inside.slice(from, from + item.length).trim();
        });
        if (items.every((item) => PARAMETER.test(item))) return null;
        return { text, initializer: inside };
    }
    const brace = shape.indexOf('{');
    if (brace >= 0) return { text, initializer: text.slice(brace) };
    if (/^[\w:<>,\s*&\[\]]+$/.test(text) && /[\w>*&\]]\s*[*&]*\s*[A-Za-z_][\w:]*(?:\s*\[[^\]]*\])?$/.test(text) && /\s/.test(text)) {
        return { text, initializer: '' };
    }
    return null;
}

// Remove lambda bodies: they are only defined, not run, by an initializer.
function withoutLambdaBodies(text) {
    let out = text;
    for (let i = 0; i < out.length; i++) {
        if (out[i] !== '{') continue;
        const before = out.slice(0, i).trimEnd();
        if (!/(?:[)\]]|\bmutable|\bnoexcept|->\s*[\w:<>,\s*&]+)$/.test(before)) continue;
        const close = matchClose(out, i);
        if (close < 0) break;
        if (/^\s*\(/.test(out.slice(close + 1))) continue; // invoked at once: it runs
        out = out.slice(0, i + 1) + ' '.repeat(close - i - 1) + out.slice(close);
        i = close;
    }
    return out;
}

const TRANSLATE_CALL = /(?<![\w.>:])(?:_|_L|_u8L|_utf8|_CTX|_CTX_utf8|_CHB|_L_PLURAL)\s*\(|\bI18N::translate(?:_utf8|_mode)?\s*\(/;

export function translatingInitializers(source) {
    const found = [];
    for (const statement of namespaceStatements(stripCpp(source))) {
        const variable = variableDefinition(statement);
        if (!variable) continue;
        if (!TRANSLATE_CALL.test(withoutLambdaBodies(variable.initializer))) continue;
        found.push({ line: statement.line, name: variableName(variable.text) });
    }
    return found;
}

// The declared name: the last identifier before the initializer.
function variableName(text) {
    const shape = topLevel(text);
    let cut = -1;
    for (let i = 0, angle = 0; i < shape.length && cut < 0; i++) {
        if (shape[i] === '<') angle++;
        else if (shape[i] === '>') angle--;
        else if (angle <= 0 && '=({'.includes(shape[i])) cut = i;
    }
    const head = (cut < 0 ? text : text.slice(0, cut)).replace(/\s*\[[^\]]*\]\s*$/, '').trim();
    const match = /[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$/.exec(head);
    return match ? match[0] : head;
}

// ---------------------------------------------------------------------------
// Contract 1: state reached by every translation is created on first use.

// Every _L()/_u8L()/I18N::translate() runs this code, including calls from
// other files' static initializers while the DLL loads: the local include
// closure of I18N, with each header's source file.
function translationPath() {
    const seen = new Set();
    const queue = ['src/slic3r/GUI/I18N.hpp', 'src/slic3r/GUI/I18N.cpp'];
    while (queue.length) {
        const file = queue.shift();
        if (seen.has(file)) continue;
        seen.add(file);
        const dir = path.posix.dirname(file);
        for (const [, name] of read(file).matchAll(/^[ \t]*#[ \t]*include[ \t]*"([^"]+)"/gm)) {
            const found = [`${dir}/${name}`, `src/slic3r/GUI/${name}`, `src/slic3r/${name}`, `src/${name}`]
                .map((candidate) => path.posix.normalize(candidate))
                .find((candidate) => existsSync(path.join(repoDir, candidate)));
            if (!found) continue;
            queue.push(found);
            const source = found.replace(/\.(?:hpp|h)$/, '.cpp');
            if (source !== found && existsSync(path.join(repoDir, source))) queue.push(source);
        }
    }
    return [...seen].sort();
}
const TRANSLATION_PATH = translationPath();

// Reviewed globals on that path whose type the reader cannot see; each is
// constant-initialized. Do not add containers, strings or mutexes here.
const CONSTANT_TRANSLATION_PATH_GLOBALS = {
    // struct Suffix { const wchar_t *english; const wchar_t *cantonese; }, from literals.
    'src/slic3r/GUI/BilingualRegistry.cpp': ['SUFFIXES'],
};

// Declarations a compiler initializes before any code runs: constexpr,
// std::atomic and plain scalars or pointers set from literals, arrays of
// literal pointer pairs, and captureless lambdas. Anything else (a container,
// string or mutex) has a constructor that runs in an unspecified order
// relative to other files.
function constantInitialized(text) {
    if (/\b(?:constexpr|constinit)\b/.test(text)) return true;
    const string = String.raw`(?:u8|[uUL])?""`;
    const literal = String.raw`(?:${string}|(?:u8|[uUL])?''|-?[\d.]+[uUlLfF]*|true|false|nullptr|[A-Z_][A-Z0-9_]*)`;
    const scalar = String.raw`(?:bool|char|wchar_t|short|int|long|long long|unsigned(?: int| long| char)?|float|double|(?:std::)?size_t|(?:std::)?u?int(?:8|16|32|64)_t)`;
    const prefix = String.raw`(?:(?:static|inline|const|volatile)\s+)*`;
    if (new RegExp(String.raw`^${prefix}std::atomic<\s*${scalar}\s*>\s+\w+\s*(?:\{\s*${literal}?\s*\}|=\s*${literal})?$`).test(text)) return true;
    if (new RegExp(String.raw`^${prefix}${scalar}\s+\w+\s*(?:=\s*${literal}|\{\s*${literal}?\s*\})?$`).test(text)) return true;
    if (new RegExp(String.raw`^${prefix}(?:char|wchar_t)\s*\*\s*(?:const\s+)?\w+\s*=\s*${string}$`).test(text)) return true;
    if (new RegExp(String.raw`^${prefix}auto\s+\w+\s*=\s*\[\s*\][\s\S]*\}$`).test(text)) return true;
    const pairArray = String.raw`^${prefix}std::pair<\s*const (?:char|wchar_t) \*\s*,\s*const (?:char|wchar_t) \*\s*>\s+\w+\s*\[\s*\d*\s*\]\s*=\s*\{(.*)\}$`;
    const pairs = new RegExp(pairArray).exec(text);
    return Boolean(pairs) && new RegExp(String.raw`^(?:\s|[{},]|${string})*$`).test(pairs[1]);
}

function dynamicNamespaceState(source, reviewed = []) {
    return namespaceStatements(stripCpp(source))
        .map((statement) => ({ statement, variable: variableDefinition(statement) }))
        .filter(({ variable }) => variable && !constantInitialized(variable.text))
        .filter(({ variable }) => !reviewed.includes(variableName(variable.text)))
        .map(({ statement, variable }) => `${statement.line}: ${variable.text}`);
}

test('the reader finds namespace-scope variables and skips functions, classes and lambdas', () => {
    const sample = [
        '#include <set>',
        'namespace a { namespace {',
        'std::mutex m;',
        'std::set<std::wstring> sources;',
        'struct Observer { int x = _L("no"); };',
        'std::map<int, Observer> observers;',
        'bool flag = false;',
        'wxString f(const wxString &s = _L("default")) { static wxString t = _L("local"); return t; }',
        'auto lazy = [] { return _L("later"); };',
        'const auto typed = []() -> wxString { return _L("later"); };',
        'const wxString now = [] { return _L("now"); }();',
        'auto once = [] { return 1; }();',
        'Foo::Foo() : a{1}, b(_L("member")) { c = _L("body"); }',
        '} }',
        '#ifdef __APPLE__',
        'static const wxString ctrl = ("Ctrl+");',
        '#else',
        'static const wxString ctrl = _L("Ctrl+");',
        '#endif',
        '#if 0',
        'static const wxString unused = _L("never compiled"); {',
        '#endif',
        'const wxString Klass::label = _u8L("x");',
        'static const wxString names[] = { _L("a"), "b" };',
        'wxString direct(_L("paren"));',
        'BEGIN_EVENT_TABLE(A, B) EVT_MENU(1, A::f) END_EVENT_TABLE()',
        'const char *const NAME = "TPU-AMS";',
        'void g(); // _L("comment")',
        'const char *s = "_L(\\"string\\")";',
    ].join('\n');
    assert.deepEqual(translatingInitializers(sample).map((item) => item.name),
        ['now', 'ctrl', 'Klass::label', 'names', 'direct']);
    assert.deepEqual(dynamicNamespaceState(sample).map((line) => line.replace(/^\d+: /, '')), [
        'std::mutex m',
        'std::set<std::wstring> sources',
        'std::map<int, Observer> observers',
        'const wxString now = [] { return _L(""); }()',
        'auto once = [] { return 1; }()',
        'static const wxString ctrl = _L("")',
        'const wxString Klass::label = _u8L("")',
        'static const wxString names[] = { _L(""), "" }',
        'wxString direct(_L(""))',
    ]);
});

test('code reached by every translation has no namespace-scope state with a dynamic initializer', () => {
    for (const file of ['I18N.cpp', 'LanguageMode.cpp', 'BilingualRegistry.cpp', 'PersonalVocabulary.cpp', 'PersonalModes/SchoolMode.hpp'])
        assert.ok(TRANSLATION_PATH.includes(`src/slic3r/GUI/${file}`), file);
    const offenders = [];
    for (const file of TRANSLATION_PATH)
        for (const line of dynamicNamespaceState(read(file), CONSTANT_TRANSLATION_PATH_GLOBALS[file]))
            offenders.push(`${file}:${line}`);
    assert.deepEqual(offenders, []);
});

test('the md3-v229 PersonalVocabulary globals are rejected', () => {
    const before = [
        'namespace Slic3r::GUI::PersonalVocabulary {',
        'namespace {',
        'std::mutex source_mutex;',
        'std::set<std::wstring> sources;',
        'Entries entries;',
        'bool initialized = false;',
        'bool cache_loaded = false;',
        'std::map<wxWindow *, Observer> observers;',
        '}',
        'wxString remember(const wxString &source)',
        '{ std::lock_guard<std::mutex> lock(source_mutex); sources.insert(source.ToStdWstring()); return source; }',
        '}',
    ].join('\n');
    assert.deepEqual(dynamicNamespaceState(before).map((line) => line.replace(/^\d+: /, '')), [
        'std::mutex source_mutex',
        'std::set<std::wstring> sources',
        'Entries entries',
        'std::map<wxWindow *, Observer> observers',
    ]);
});

test('PersonalVocabulary keeps its state in one object created on first use', () => {
    const source = read('src/slic3r/GUI/PersonalVocabulary.cpp');
    assert.match(source, /State &state\(\)\s*\{\s*static State \*const instance = new State\(\);\s*return \*instance;\s*\}/);
    for (const name of ['source_mutex', 'sources', 'entries', 'observers', 'initialized', 'cache_loaded'])
        assert.match(source, new RegExp(String.raw`struct State \{[^}]*\b${name}\b`), name);
});

// ---------------------------------------------------------------------------
// Contract 2: no new translation in a namespace-scope initializer.

// Reviewed initializers that translate before main. They are safe because the
// translation path keeps no dynamic namespace-scope state (contract 1), but
// they are never localized; move them into functions when they are touched.
// Do not add entries: compute the text where it is used instead.
const REVIEWED_STATIC_TRANSLATIONS = {
    'src/slic3r/GUI/AuxiliaryDataViewModel.cpp': ['s_default_folders'],
    'src/slic3r/GUI/CalibrationWizard.cpp': ['NA_STR'],
    'src/slic3r/GUI/CalibrationWizardCaliPage.cpp': ['NA_STR'],
    'src/slic3r/GUI/Gizmos/GLGizmoMeshBoolean.cpp': [
        'COMMON', 'GROUPING', 'JOB_CANCELED', 'JOB_FAILED', 'MIN_OBJECTS_DIFFERENCE', 'MIN_OBJECTS_INTERSECTION',
        'MIN_OBJECTS_UNION', 'MIN_VOLUMES_DIFFERENCE', 'MIN_VOLUMES_INTERSECTION', 'MIN_VOLUMES_UNION', 'OVERLAPING',
        'PREPAREING',
    ].map((name) => `MeshBooleanWarnings::${name}`),
    'src/slic3r/GUI/Jobs/BindJob.cpp': ['login_failed_str', 'waiting_auth_str'],
    'src/slic3r/GUI/NetworkTestDialog.cpp': ['NA_STR'],
    'src/slic3r/GUI/PublishDialog.cpp': ['NOTE_STRING', 'PUBLISH_STEP_STRING'],
    'src/slic3r/GUI/SelectMachine.cpp': ['task_canceled_text'],
    'src/slic3r/GUI/StatusPanel.cpp': ['NA_STR'],
    'src/slic3r/Utils/CalibUtils.cpp': ['nozzle_not_set_text', 'nozzle_volume_type_not_match_text'],
    // Listed in CMakeLists.txt but included by no translation unit.
    'src/slic3r/Utils/ProfileDescription.hpp': Array.from({ length: 53 }, (_, i) => `PROFILE_DESCRIPTION_${i}`).sort(),
};

function sourceFiles(dir) {
    const out = [];
    for (const entry of readdirSync(path.join(repoDir, dir), { withFileTypes: true })) {
        const relative = `${dir}/${entry.name}`;
        if (entry.isDirectory()) out.push(...sourceFiles(relative));
        else if (/\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx|inl)$/.test(entry.name)) out.push(relative);
    }
    return out;
}

test('namespace-scope initializers in src/slic3r and src/libslic3r translate only where reviewed', () => {
    const found = {};
    const unreadable = [];
    for (const file of [...sourceFiles('src/slic3r'), ...sourceFiles('src/libslic3r')]) {
        let items;
        try { items = translatingInitializers(read(file)); } catch (error) { unreadable.push(`${file}: ${error.message}`); continue; }
        if (items.length) found[file] = [...new Set(items.map((item) => item.name))].sort();
    }
    assert.deepEqual(unreadable, []);
    assert.deepEqual(found, REVIEWED_STATIC_TRANSLATIONS);
});

test('MainFrame keeps its accelerator prefix untranslated and out of static translation', () => {
    const source = read('src/slic3r/GUI/MainFrame.cpp');
    assert.match(source, /static const wxString ctrl = wxS\("Ctrl\+"\);/);
    assert.deepEqual(translatingInitializers('static const wxString ctrl = _L("Ctrl+");').map((item) => item.name), ['ctrl']);
});
