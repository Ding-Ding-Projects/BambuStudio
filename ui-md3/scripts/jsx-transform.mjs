#!/usr/bin/env node
/*
 * A JSX compiler, just large enough for this repository's UI kit.
 *
 * The kit used to ship @babel/standalone to the visitor's browser and compile
 * its own source on every page load. That cost three third-party requests, a
 * 2.7 MB download and a blank page whenever unpkg was unreachable — on a site
 * whose whole premise is that it is served from this repository and nowhere
 * else. Compiling at build time removes all of it.
 *
 * Reaching for Babel here would only move the dependency: nothing else under
 * ui-md3/scripts/ has an npm tree, CI runs these files with a bare `node`, and
 * a lockfile plus an install step is a lot of machinery for twelve files of
 * ordinary JSX. So this is a compiler for the subset those files actually use:
 *
 *   - elements with intrinsic, component, and dotted member names
 *   - string, expression, boolean-shorthand and {...spread} attributes
 *   - element, text, and expression children, including comment-only containers
 *   - HTML entities in text and in quoted attribute values
 *
 * Everything outside that subset THROWS. That is the whole safety argument: a
 * construct this file does not understand stops the build with a file:line:col,
 * rather than compiling to something that merely looks close. Fragments,
 * namespaced attributes and unknown entities are all deliberate errors — if the
 * kit ever needs one, teach it here and cover it in the tests.
 *
 * Output matches @babel/plugin-transform-react-jsx's classic runtime, including
 * its whitespace rules, so the compiled page renders the DOM Babel used to. It
 * does not downlevel: the result is the same modern JavaScript the .jsx sources
 * are written in.
 *
 * Line numbers are preserved. Every element pads itself back to the number of
 * newlines it consumed, so a stack trace against the assembled index.html lands
 * on the same line as the .jsx it came from, and a one-line source edit stays a
 * one-line diff in the assembled file.
 */

// Only what the kit's text actually contains, plus the handful anyone would
// reach for next. An unlisted entity is an error rather than a pass-through,
// because passing it through would render the literal characters and nobody
// would notice until a screenshot looked wrong.
const NAMED_ENTITIES = new Map(Object.entries({
  amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ',
  copy: '©', reg: '®', trade: '™', deg: '°',
  times: '×', middot: '·', bull: '•', hellip: '…',
  ndash: '–', mdash: '—', laquo: '«', raquo: '»',
  lsquo: '‘', rsquo: '’', ldquo: '“', rdquo: '”',
  larr: '←', uarr: '↑', rarr: '→', darr: '↓',
}));

// Words after which a `<` or `/` begins a JSX element or a regular expression
// rather than continuing an expression. `this`, `true`, `false`, `null` and
// `super` are deliberately absent: they end an expression, so `x < y` after one
// of them is a comparison.
const NON_VALUE_KEYWORDS = new Set([
  'return', 'typeof', 'instanceof', 'in', 'of', 'new', 'delete', 'void',
  'throw', 'case', 'do', 'else', 'yield', 'await', 'let', 'const', 'var',
  'if', 'while', 'for', 'switch', 'function', 'class', 'extends', 'default',
]);

const OPERATORS = [
  '>>>=', '...', '===', '!==', '**=', '<<=', '>>=', '>>>', '&&=', '||=', '??=',
  '=>', '==', '!=', '<=', '>=', '&&', '||', '??', '?.', '++', '--', '+=', '-=',
  '*=', '/=', '%=', '&=', '|=', '^=', '**', '<<', '>>',
];

const IDENTIFIER = /^[A-Za-z_$][A-Za-z0-9_$]*/;
const NUMBER = /^[0-9][0-9a-zA-Z_.]*/;
const JSX_NAME = /^[A-Za-z_$][A-Za-z0-9_$-]*/;
const ATTRIBUTE_NAME = /^[A-Za-z_$][A-Za-z0-9_$-]*/;
const PLAIN_KEY = /^[A-Za-z_$][A-Za-z0-9_$]*$/;

/**
 * Compiles JSX to React.createElement calls.
 *
 * @param {string} source     the .jsx source
 * @param {string} [filename] used only in error messages
 * @returns {string} JavaScript with every JSX element replaced
 */
export function transform(source, filename = '<jsx>') {
  const state = { src: source, file: filename };
  const result = scanJs(state, 0, false);
  if (result.next !== source.length) {
    throw fail(state, result.next, 'Unbalanced "}"');
  }
  return result.code;
}

/**
 * Reads JavaScript, replacing any JSX element it meets.
 *
 * @param stopAtBrace when true, returns at the `}` that closes the enclosing
 *   `{`, leaving the brace unconsumed — that is how an attribute value, an
 *   expression child and a `${}` substitution find their end.
 * @returns {{code: string, next: number, meaningful: boolean}} `meaningful` is
 *   false when the region held nothing but whitespace and comments, which is
 *   how a comment-only container is recognised as a child to drop.
 */
function scanJs(state, start, stopAtBrace) {
  const src = state.src;
  let code = '';
  let i = start;
  let depth = 0;
  let meaningful = false;
  // The previous significant token, which decides whether `<` opens an element
  // and whether `/` opens a regular expression. 'value' stands in for any
  // token that ends an expression without being a word: a string, a template,
  // a regular expression, or a JSX element.
  let previous = '';

  while (i < src.length) {
    const c = src[i];

    if (c === ' ' || c === '\t' || c === '\n' || c === '\r') {
      code += c;
      i += 1;
      continue;
    }

    if (c === '/' && src[i + 1] === '/') {
      const end = lineEnd(src, i);
      code += src.slice(i, end);
      i = end;
      continue;
    }

    if (c === '/' && src[i + 1] === '*') {
      const end = src.indexOf('*/', i + 2);
      if (end === -1) throw fail(state, i, 'Unterminated block comment');
      code += src.slice(i, end + 2);
      i = end + 2;
      continue;
    }

    // Before the flag is raised, so a container holding only a comment stays
    // unmeaningful and is dropped as a child.
    if (c === '}') {
      if (depth === 0) return { code, next: i, meaningful };
      depth -= 1;
      code += c;
      i += 1;
      previous = '}';
      meaningful = true;
      continue;
    }

    meaningful = true;

    if (c === '"' || c === "'") {
      const end = stringEnd(state, i);
      code += src.slice(i, end);
      i = end;
      previous = 'value';
      continue;
    }

    if (c === '`') {
      const template = readTemplate(state, i);
      code += template.code;
      i = template.next;
      previous = 'value';
      continue;
    }

    if (c === '/' && !endsExpression(previous)) {
      const end = regexEnd(state, i);
      code += src.slice(i, end);
      i = end;
      previous = 'value';
      continue;
    }

    if (c === '<' && !endsExpression(previous)) {
      const element = parseElement(state, i);
      code += element.code;
      i = element.next;
      previous = 'value';
      continue;
    }

    if (c === '{') {
      depth += 1;
      code += c;
      i += 1;
      previous = '{';
      continue;
    }

    const word = IDENTIFIER.exec(src.slice(i)) || NUMBER.exec(src.slice(i));
    if (word) {
      code += word[0];
      i += word[0].length;
      previous = word[0];
      continue;
    }

    const operator = OPERATORS.find((candidate) => src.startsWith(candidate, i));
    const token = operator || c;
    code += token;
    i += token.length;
    previous = token;
  }

  if (stopAtBrace) throw fail(state, start, 'Unterminated "{"');
  return { code, next: i, meaningful };
}

/** True when `token` can end an expression, making a following `<` or `/` an operator. */
function endsExpression(token) {
  if (!token) return false;
  if (token === 'value' || token === ')' || token === ']' || token === '}') return true;
  if (token === '++' || token === '--') return true;
  if (/^[0-9]/.test(token)) return true;
  if (/^[A-Za-z_$]/.test(token)) return !NON_VALUE_KEYWORDS.has(token);
  return false;
}

/** Parses one JSX element at `start`, which must be its `<`. */
function parseElement(state, start) {
  const src = state.src;
  let i = start + 1;

  if (src[i] === '>') {
    throw fail(state, start, 'JSX fragments (<>) are not supported; use an element or an array');
  }

  const tag = readElementName(state, i);
  i = tag.next;

  const attributes = [];
  let selfClosing = false;

  for (;;) {
    i = skipSpace(src, i);
    if (i >= src.length) throw fail(state, start, `Unterminated <${tag.name}> tag`);

    if (src[i] === '/' && src[i + 1] === '>') {
      selfClosing = true;
      i += 2;
      break;
    }
    if (src[i] === '>') {
      i += 1;
      break;
    }

    if (src[i] === '{') {
      const open = i;
      let j = skipSpace(src, i + 1);
      if (!src.startsWith('...', j)) {
        throw fail(state, open, 'Only {...spread} is allowed where an attribute is expected');
      }
      const inner = scanJs(state, j + 3, true);
      if (src[inner.next] !== '}') throw fail(state, open, 'Unterminated attribute spread');
      const expression = inner.code.trim();
      if (!expression) throw fail(state, open, 'Empty attribute spread');
      attributes.push({ kind: 'spread', code: expression });
      i = inner.next + 1;
      continue;
    }

    const nameMatch = ATTRIBUTE_NAME.exec(src.slice(i));
    if (!nameMatch) throw fail(state, i, `Expected an attribute name in <${tag.name}>`);
    const name = nameMatch[0];
    i += name.length;
    if (src[i] === ':') {
      throw fail(state, i, `Namespaced attributes (${name}:…) are not supported`);
    }

    const afterName = skipSpace(src, i);
    if (src[afterName] !== '=') {
      // Shorthand: `<Button elevated>` is `elevated={true}`.
      attributes.push({ kind: 'prop', name, value: 'true' });
      i = afterName;
      continue;
    }

    i = skipSpace(src, afterName + 1);
    const quote = src[i];
    if (quote === '"' || quote === "'") {
      const end = src.indexOf(quote, i + 1);
      if (end === -1) throw fail(state, i, 'Unterminated attribute value');
      // A quoted JSX attribute is not a JavaScript string: backslashes are
      // literal and entities are decoded. Re-quoting the decoded text is what
      // makes both true in the output.
      const text = decodeEntities(state, src.slice(i + 1, end), i + 1);
      attributes.push({ kind: 'prop', name, value: JSON.stringify(text) });
      i = end + 1;
      continue;
    }

    if (quote === '{') {
      const open = i;
      const inner = scanJs(state, i + 1, true);
      if (src[inner.next] !== '}') throw fail(state, open, 'Unterminated attribute expression');
      const expression = inner.code.trim();
      if (!expression) throw fail(state, open, `Empty expression for attribute "${name}"`);
      attributes.push({ kind: 'prop', name, value: expression });
      i = inner.next + 1;
      continue;
    }

    if (quote === '<') {
      const element = parseElement(state, i);
      attributes.push({ kind: 'prop', name, value: element.code });
      i = element.next;
      continue;
    }

    throw fail(state, i, `Expected a value for attribute "${name}"`);
  }

  const children = [];
  if (!selfClosing) {
    for (;;) {
      if (i >= src.length) throw fail(state, start, `Unclosed <${tag.name}>`);

      if (src[i] === '<') {
        if (src[i + 1] === '/') {
          const closing = readElementName(state, i + 2);
          if (closing.name !== tag.name) {
            throw fail(state, i, `<${tag.name}> is closed by </${closing.name}>`);
          }
          const end = skipSpace(src, closing.next);
          if (src[end] !== '>') throw fail(state, i, `Unterminated </${tag.name}>`);
          i = end + 1;
          break;
        }
        const element = parseElement(state, i);
        children.push(element.code);
        i = element.next;
        continue;
      }

      if (src[i] === '{') {
        const open = i;
        const inner = scanJs(state, i + 1, true);
        if (src[inner.next] !== '}') throw fail(state, open, 'Unterminated expression child');
        // A container holding only whitespace and comments is a JSXEmptyExpression,
        // which renders nothing — `{/* tab bar */}` must not become a child.
        if (inner.meaningful) children.push(inner.code.trim());
        i = inner.next + 1;
        continue;
      }

      let end = i;
      while (end < src.length && src[end] !== '<' && src[end] !== '{') end += 1;
      const text = cleanText(decodeEntities(state, src.slice(i, end), i));
      if (text) children.push(JSON.stringify(text));
      i = end;
    }
  }

  const parts = [elementType(tag.name), propsCode(attributes), ...children];
  let code = `React.createElement(${parts.join(', ')})`;

  // Pad back to the source's line count so everything after this element keeps
  // its line number. Text children lose their newlines to cleanText, so the
  // generated form is never longer in lines than the source it replaced.
  const consumed = countNewlines(state.src.slice(start, i));
  const produced = countNewlines(code);
  if (produced < consumed) {
    code = `${code.slice(0, -1)}${'\n'.repeat(consumed - produced)})`;
  }

  return { code, next: i };
}

/** Reads `div`, `Button`, `MD.IconButton` or `window.Overlays.SendDialog`. */
function readElementName(state, start) {
  const src = state.src;
  const head = JSX_NAME.exec(src.slice(start));
  if (!head) throw fail(state, start, 'Expected a JSX element name');
  let name = head[0];
  let i = start + head[0].length;
  if (src[i] === ':') throw fail(state, i, `Namespaced element names (${name}:…) are not supported`);
  while (src[i] === '.') {
    const part = IDENTIFIER.exec(src.slice(i + 1));
    if (!part) throw fail(state, i, `Expected a name after "." in <${name}>`);
    name += `.${part[0]}`;
    i += 1 + part[0].length;
  }
  return { name, next: i };
}

/**
 * A lowercase or hyphenated name is an intrinsic element and becomes a string;
 * anything else — `Button`, `MD.IconButton` — is code already in scope.
 */
function elementType(name) {
  if (name.includes('.')) return name;
  if (name.includes('-') || /^[a-z]/.test(name)) return JSON.stringify(name);
  return name;
}

function propsCode(attributes) {
  if (attributes.length === 0) return 'null';
  // `<Screen {...props} />` becomes createElement(Screen, props), not a copy of
  // it — Babel's own shortcut when a spread is the only attribute. It is safe
  // because createElement copies each own property out of what it is handed and
  // keeps no reference to the object itself.
  if (attributes.length === 1 && attributes[0].kind === 'spread') return attributes[0].code;
  const parts = attributes.map((attribute) => (
    attribute.kind === 'spread'
      ? `...${attribute.code}`
      : `${PLAIN_KEY.test(attribute.name) ? attribute.name : JSON.stringify(attribute.name)}: ${attribute.value}`
  ));
  return `{ ${parts.join(', ')} }`;
}

/*
 * JSX text is not taken literally: a line break plus indentation between two
 * elements is layout, not content. This is @babel/types' cleanJSXElementLiteralChild,
 * reproduced exactly — every blank line goes, indentation goes, and a line
 * break between words becomes one space. An approximation here would show up as
 * missing or doubled spaces in the rendered page.
 */
function cleanText(raw) {
  const lines = raw.split(/\r\n|\n|\r/);
  let lastNonEmptyLine = 0;
  for (let i = 0; i < lines.length; i += 1) {
    if (/[^ \t]/.test(lines[i])) lastNonEmptyLine = i;
  }
  let text = '';
  for (let i = 0; i < lines.length; i += 1) {
    let line = lines[i].replace(/\t/g, ' ');
    if (i !== 0) line = line.replace(/^ +/, '');
    if (i !== lines.length - 1) line = line.replace(/ +$/, '');
    if (line) {
      if (i !== lastNonEmptyLine) line += ' ';
      text += line;
    }
  }
  return text;
}

function decodeEntities(state, raw, offset) {
  if (!raw.includes('&')) return raw;
  return raw.replace(/&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z][a-zA-Z0-9]*);/g, (match, body, index) => {
    if (body[0] === '#') {
      const code = body[1] === 'x' || body[1] === 'X'
        ? Number.parseInt(body.slice(2), 16)
        : Number.parseInt(body.slice(1), 10);
      if (!Number.isFinite(code) || code < 0 || code > 0x10ffff) {
        throw fail(state, offset + index, `Out-of-range character reference "${match}"`);
      }
      return String.fromCodePoint(code);
    }
    const decoded = NAMED_ENTITIES.get(body);
    if (decoded === undefined) {
      throw fail(state, offset + index, `Unknown HTML entity "${match}"; add it to NAMED_ENTITIES`);
    }
    return decoded;
  });
}

function readTemplate(state, start) {
  const src = state.src;
  let code = '`';
  let i = start + 1;
  while (i < src.length) {
    const c = src[i];
    if (c === '\\') {
      code += src.slice(i, i + 2);
      i += 2;
      continue;
    }
    if (c === '`') return { code: `${code}\``, next: i + 1 };
    if (c === '$' && src[i + 1] === '{') {
      const inner = scanJs(state, i + 2, true);
      if (src[inner.next] !== '}') throw fail(state, i, 'Unterminated template substitution');
      code += `\${${inner.code}}`;
      i = inner.next + 1;
      continue;
    }
    code += c;
    i += 1;
  }
  throw fail(state, start, 'Unterminated template literal');
}

function stringEnd(state, start) {
  const src = state.src;
  const quote = src[start];
  let i = start + 1;
  while (i < src.length) {
    const c = src[i];
    if (c === '\\') { i += 2; continue; }
    if (c === quote) return i + 1;
    if (c === '\n') break;
    i += 1;
  }
  throw fail(state, start, 'Unterminated string literal');
}

function regexEnd(state, start) {
  const src = state.src;
  let i = start + 1;
  let inClass = false;
  while (i < src.length) {
    const c = src[i];
    if (c === '\\') { i += 2; continue; }
    if (c === '\n') break;
    if (c === '[') inClass = true;
    else if (c === ']') inClass = false;
    else if (c === '/' && !inClass) {
      i += 1;
      while (i < src.length && /[a-z]/i.test(src[i])) i += 1;
      return i;
    }
    i += 1;
  }
  throw fail(state, start, 'Unterminated regular expression');
}

function skipSpace(src, start) {
  let i = start;
  while (i < src.length && (src[i] === ' ' || src[i] === '\t' || src[i] === '\n' || src[i] === '\r')) i += 1;
  return i;
}

function lineEnd(src, start) {
  const index = src.indexOf('\n', start);
  return index === -1 ? src.length : index;
}

function countNewlines(text) {
  let count = 0;
  for (let i = 0; i < text.length; i += 1) if (text[i] === '\n') count += 1;
  return count;
}

function fail(state, offset, message) {
  const before = state.src.slice(0, offset);
  const line = countNewlines(before) + 1;
  const column = offset - (before.lastIndexOf('\n') + 1) + 1;
  return new Error(`${state.file}:${line}:${column}: ${message}`);
}
