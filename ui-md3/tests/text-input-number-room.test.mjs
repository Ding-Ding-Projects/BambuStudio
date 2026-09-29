import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A TextInput draws its label (the unit in "0.1 mm/mm") after the entry and
// takes the label's width out of the field. The calibration dialogs create
// their fields 90 DIP wide, so the retraction step's "mm/mm" left the number
// 22 px in Cantonese and 26 px in English: "0.1" was cut, and any longer value
// would have been cut in both (clipping inventory CJ-019). The field now keeps
// room for a short number beside its label whatever width the caller asked for.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets', 'TextInput.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

test('a TextInput with a label keeps room for a short number beside it', () => {
  const measure = source.match(/void TextInput::messureSize\(\)[\s\S]*?\n\}/);
  assert.ok(measure, 'messureSize() must exist');
  const code = stripComments(measure[0]);
  assert.match(code, /GetTextExtent\(wxS\("0\.000"\)\)/, 'the room is measured with the entry font on a sample number');
  assert.match(code, /labelSize\.x/, 'the label width is part of the room needed');
  assert.match(code, /minSize\.x = std::max\(minSize\.x, needed\);/, 'the minimum width never drops below what the number and label need');
  assert.match(code, /size\.x = std::max\(size\.x, needed\);/, 'a field created narrower than that grows to it');
});

test('the room counts everything DoSetSize takes out of the entry', () => {
  // DoSetSize gives the entry size.x - textPos.x - labelSize.x - 10 - prefix - unit;
  // the room computed here has to subtract the same parts or the entry still ends short.
  const measure = stripComments(source.match(/void TextInput::messureSize\(\)[\s\S]*?\n\}/)[0]);
  for (const part of [/icon\.bmp\(\)\.IsOk\(\)/, /icon_1\.bmp\(\)\.IsOk\(\)/, /m_prefix/, /m_unit/]) {
    assert.match(measure, part);
  }
});

test('a centred field draws its unit after the number, not under it', () => {
  // The calibration fields are created with wxTE_CENTRE. The unit label was then
  // drawn at the field's left edge while the entry also sat there, so the entry
  // covered the start of the unit: "0.1/mm" for "0.1 mm/mm", "5 /秒" for
  // "5 mm³/秒". Only a right-aligned field moves the entry to make room for a
  // label on the left, so only a right-aligned field draws it there.
  const render = source.match(/void TextInput::render\(wxDC& dc\)[\s\S]*?\n\}/);
  assert.ok(render, 'render() must exist');
  const code = stripComments(render[0]);
  assert.doesNotMatch(code, /if \(align_right \|\| align_center\)\s*\{/, 'a centred field does not draw its label at the left edge');
  assert.equal((code.match(/if \(align_right\)\s*\{/g) || []).length, 3, 'every label branch keeps the left placement for right alignment only');
  const layout = stripComments(source.match(/void TextInput::DoSetSize\([\s\S]*?\n\}/)[0]);
  assert.match(layout, /if \(align_right\)\s*textPos\.x \+= labelSize\.x;/, 'the entry moves right only for a right-aligned field, matching render()');
});
