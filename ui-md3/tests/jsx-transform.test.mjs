/*
 * Contracts for the JSX compiler that replaced @babel/standalone.
 *
 * Two kinds of case matter here. The first is agreement: the compiler's job is
 * to emit what Babel's classic runtime emitted, down to the whitespace rules,
 * because the published page has to render exactly what it rendered before.
 * The second is refusal: every construct outside the supported subset must
 * throw with a location, since a compiler that silently mishandles something is
 * far worse than one that stops.
 *
 *   node --test ui-md3/tests/jsx-transform.test.mjs
 */
import assert from 'node:assert/strict';
import test from 'node:test';

import { transform } from '../scripts/jsx-transform.mjs';

/** Collapses whitespace so a case reads as one line without pinning formatting. */
const compiled = (source) => transform(source, 'case.jsx').replace(/\s+/g, ' ').trim();

test('an intrinsic element becomes a string type, a component becomes an identifier', () => {
  assert.equal(compiled('<div />'), 'React.createElement("div", null)');
  assert.equal(compiled('<Button />'), 'React.createElement(Button, null)');
  // A dotted name is a member expression whatever its first letter, which is
  // how the kit reaches window.Overlays.SendDialog.
  assert.equal(compiled('<MD.IconButton />'), 'React.createElement(MD.IconButton, null)');
  assert.equal(
    compiled('<window.Overlays.SendDialog />'),
    'React.createElement(window.Overlays.SendDialog, null)'
  );
  // A hyphen makes a name intrinsic even though it is not a known HTML tag.
  assert.equal(compiled('<my-widget />'), 'React.createElement("my-widget", null)');
});

test('attributes carry their string, expression, shorthand and spread forms', () => {
  assert.equal(compiled('<div id="a" />'), 'React.createElement("div", { id: "a" })');
  assert.equal(compiled('<div onClick={go} />'), 'React.createElement("div", { onClick: go })');
  // Bare attribute means true — <Button elevated> in the kit.
  assert.equal(compiled('<Button elevated />'), 'React.createElement(Button, { elevated: true })');
  // A name that is not a valid identifier has to be quoted as a key.
  assert.equal(compiled('<span data-icon="x" />'), 'React.createElement("span", { "data-icon": "x" })');
  // A lone spread is passed straight through, exactly as Babel does.
  assert.equal(compiled('<Screen {...props} />'), 'React.createElement(Screen, props)');
  assert.equal(
    compiled('<Screen {...props} id="a" />'),
    'React.createElement(Screen, { ...props, id: "a" })'
  );
});

test('a quoted attribute is JSX text, not a JavaScript string', () => {
  // Backslashes are literal inside JSX attribute quotes: this is a Windows path,
  // not an escape sequence.
  assert.equal(
    compiled('<div title="C:\\new" />'),
    'React.createElement("div", { title: "C:\\\\new" })'
  );
  // Entities are decoded before the value becomes a string literal.
  assert.equal(compiled('<div title="a &amp; b" />'), 'React.createElement("div", { title: "a & b" })');
});

test('JSX whitespace follows Babel: blank lines and indentation are layout, not text', () => {
  assert.equal(compiled('<div>hello</div>'), 'React.createElement("div", null, "hello")');
  // A line break plus indentation between two words collapses to one space.
  // The space before the closing paren is the line-count padding, normalised.
  assert.equal(
    compiled('<div>\n  one\n  two\n</div>'),
    'React.createElement("div", null, "one two" )'
  );
  // Whitespace that is only a line break between elements disappears entirely.
  assert.equal(
    compiled('<div>\n  <a />\n  <b />\n</div>'),
    'React.createElement("div", null, React.createElement("a", null), React.createElement("b", null) )'
  );
  // A space on one line beside an element is content and survives.
  assert.equal(compiled('<div><a /> text</div>'),
    'React.createElement("div", null, React.createElement("a", null), " text")');
});

test('an empty or comment-only expression container renders nothing', () => {
  assert.equal(compiled('<div>{/* a note */}</div>'), 'React.createElement("div", null)');
  assert.equal(compiled('<div>{}</div>'), 'React.createElement("div", null)');
  // An expression that merely carries a comment is still an expression.
  assert.equal(compiled('<div>{/* why */ value}</div>'), 'React.createElement("div", null, /* why */ value)');
});

test('entities are decoded in text, and an unknown one stops the build', () => {
  assert.equal(compiled('<div>a &amp; b</div>'), 'React.createElement("div", null, "a & b")');
  assert.equal(compiled('<div>&#65;&#x42;</div>'), 'React.createElement("div", null, "AB")');
  assert.throws(() => transform('<div>&fake;</div>', 'case.jsx'), /Unknown HTML entity "&fake;"/);
});

test('a "<" that is an operator stays an operator', () => {
  // The kit's hexToHsl contains exactly this shape, and reading it as JSX would
  // swallow the rest of the file.
  const source = 'const t = (g<b?6:0) + (a > b ? 1 : 2);';
  assert.equal(transform(source, 'case.jsx'), source);
  assert.equal(transform('while (i < n) i++;', 'case.jsx'), 'while (i < n) i++;');
  assert.equal(transform('const s = a <= b;', 'case.jsx'), 'const s = a <= b;');
});

test('JSX inside strings, comments, templates and regular expressions is left alone', () => {
  for (const source of [
    `const s = '<div>not jsx</div>';`,
    `// <div>not jsx</div>`,
    `/* <div>not jsx</div> */`,
    'const r = /<div>/g;',
    'const t = `<div>not jsx</div>`;',
  ]) {
    assert.equal(transform(source, 'case.jsx'), source, source);
  }
  // …but a substitution inside a template is real code and is compiled.
  assert.equal(
    compiled('const t = `${<div />}`;'),
    'const t = `${React.createElement("div", null)}`;'
  );
});

test('a division is not mistaken for a regular expression', () => {
  const source = 'const x = total / count / 2;';
  assert.equal(transform(source, 'case.jsx'), source);
});

test('every element keeps the line count it consumed', () => {
  const source = 'const a = (\n  <div>\n    <span>x</span>\n  </div>\n);\nconst after = 1;\n';
  const output = transform(source, 'case.jsx');
  assert.equal(
    output.split('\n').length,
    source.split('\n').length,
    'a stack trace against the assembled page must land on the source line'
  );
  assert.match(output.split('\n').at(-2), /const after = 1;/);
});

test('unsupported syntax throws with a file, line and column', () => {
  assert.throws(() => transform('const a = <>x</>;', 'case.jsx'), /case\.jsx:1:11: JSX fragments/);
  assert.throws(() => transform('<svg xlink:href="#a" />', 'case.jsx'), /Namespaced attributes/);
  assert.throws(() => transform('<div>text', 'case.jsx'), /case\.jsx:1:1: Unclosed <div>/);
  assert.throws(() => transform('<div><span></div>', 'case.jsx'), /<span> is closed by <\/div>/);
  assert.throws(() => transform('<div id={} />', 'case.jsx'), /Empty expression for attribute "id"/);
  assert.throws(() => transform('<div', 'case.jsx'), /Unterminated <div> tag/);
});
