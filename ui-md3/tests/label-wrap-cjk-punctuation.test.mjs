import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Label wraps CJK text between any two characters. It used to break right
// before "。" (U+3002 sits below the U+4E00 it treated as the start of CJK), so
// the Preferences description of "Update automatically" began its second
// Cantonese line with a full stop on md3-v151. Closing punctuation must never
// start a line, opening punctuation must never end one, and an emoji (a UTF-16
// surrogate pair) must never be split.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets', 'Label.cpp'), 'utf8');

// The characters of a wxString::FromUTF8("...") set, from its byte escapes.
function decodeSet(name) {
  const match = source.match(new RegExp('static const wxString ' + name + ' = wxString::FromUTF8\\(((?:\\s*"[^"]*")+)\\)'));
  assert.ok(match, name + ' set missing');
  const literal = [...match[1].matchAll(/"([^"]*)"/g)].map((m) => m[1]).join('');
  const bytes = [...literal.matchAll(/\\x([0-9A-Fa-f]{2})/g)].map((m) => parseInt(m[1], 16));
  return new TextDecoder().decode(new Uint8Array(bytes));
}

test('the wrapper breaks next to CJK only where the punctuation rules allow', () => {
  assert.doesNotMatch(source, /c > 0x4E00/, 'the old rule broke before "。" and inside surrogate pairs');
  assert.match(source, /if \(\(wrap_is_cjk\(c\) \|\| wrap_is_cjk\(prev\)\) && !wrap_no_line_start\(c\) && !wrap_no_line_end\(prev\)\)\s*break;/);
  assert.match(source, /return code >= 0x3000 && \(code < 0xD800 \|\| code >= 0xF900\);/, 'CJK punctuation counts, surrogates do not');
});

test('closing punctuation never starts a line and opening punctuation never ends one', () => {
  const closing = decodeSet('closing');
  const opening = decodeSet('opening');
  for (const mark of '。，、：；？！）」』】》') assert.ok(closing.includes(mark), mark + ' must not start a line');
  for (const mark of '（「『【《') assert.ok(opening.includes(mark), mark + ' must not end a line');
  for (const mark of closing) assert.ok(!opening.includes(mark), mark + ' is in both sets');
});

test('a model of the break search keeps "。" off the start of a line', () => {
  // Same search as the C++ loop, with every character one unit wide.
  const closing = decodeSet('closing') + '.,:;?!)]}%';
  const opening = decodeSet('opening') + '([{';
  const isCjk = (ch) => {
    const code = ch.charCodeAt(0);
    return code >= 0x3000 && (code < 0xd800 || code >= 0xf900);
  };
  const wrap = (text, width) => {
    const lines = [];
    let line = text;
    while (line.length > width) {
      let at = width; // first character that does not fit
      while (at > 0) {
        const c = line[at];
        const prev = line[at - 1];
        if (c === ' ') break;
        if ((isCjk(c) || isCjk(prev)) && !closing.includes(c) && !opening.includes(prev)) break;
        at -= 1;
      }
      if (at === 0) at = width;
      lines.push(line.slice(0, at));
      line = line.slice(line[at] === ' ' ? at + 1 : at);
    }
    lines.push(line);
    return lines;
  };
  const sentence = '喺背景下載新版本，當有版本準備好時會提供重新啟動。只有安裝咗嘅版本先會自己更新。';
  for (let width = 4; width <= sentence.length; width += 1) {
    for (const line of wrap(sentence, width).slice(1)) {
      assert.ok(!closing.includes(line[0]), `width ${width}: a line starts with "${line[0]}"`);
    }
  }
  const quoted = '按「開始」就會開始列印';
  for (let width = 3; width <= quoted.length; width += 1) {
    for (const line of wrap(quoted, width).slice(0, -1)) {
      assert.ok(!opening.includes(line[line.length - 1]), `width ${width}: a line ends with "${line[line.length - 1]}"`);
    }
  }
});
