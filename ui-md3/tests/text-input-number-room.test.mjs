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
//
// The field anatomy was later rebuilt around one measurement, measureContent(),
// and one layout helper, atlasFieldLayout(), shared by DoSetSize(), render() and
// messureSize() (b88acafa9, "Refresh field and preset control anatomy"; see
// docs/features/design-system/studio-atlas-fields-and-presets.md). These checks
// pin the same three properties on that shape.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = (await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets', 'TextInput.cpp'), 'utf8')).replace(/\r\n/g, '\n');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

// The balanced body that starts at `signature`, comments removed.
function body(signature) {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `${signature} must exist`);
  const open = source.indexOf('{', start);
  const masked = source.slice(open).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\\n])*"/g, (m) => ' '.repeat(m.length));
  let depth = 0;
  for (let i = 0; i < masked.length; ++i) {
    if (masked[i] === '{') ++depth;
    if (masked[i] === '}' && --depth === 0) return stripComments(source.slice(start, open + i + 1));
  }
  assert.fail(`unbalanced ${signature}`);
}

// The argument list of the atlasFieldLayout(...) call in a body, whitespace folded.
function layoutCall(code) {
  const at = code.indexOf('atlasFieldLayout(');
  assert.notEqual(at, -1, 'the body must lay the field out through atlasFieldLayout()');
  let depth = 0;
  for (let i = at + 'atlasFieldLayout'.length; i < code.length; ++i) {
    if (code[i] === '(') ++depth;
    if (code[i] === ')' && --depth === 0)
      return code.slice(at + 'atlasFieldLayout('.length, i).replace(/\s+/g, ' ').trim();
  }
  assert.fail('unbalanced atlasFieldLayout call');
}

const layout = body('AtlasFieldLayout atlasFieldLayout(');
const measure = body('TextInput::ContentMetrics TextInput::measureContent(');
const doSetSize = body('void TextInput::DoSetSize(');
const render = body('void TextInput::render(');
const messure = body('void TextInput::messureSize(');

test('a TextInput with a label keeps room for a short number beside it', () => {
  // The room is a sample number measured in the entry's own font.
  assert.match(measure, /dc\.SetFont\(text_ctrl->GetFont\(\)\);[\s\S]*m\.entry = \{dc\.GetTextExtent\(wxS\("0\.000"\)\)\.x,/,
    'the room is measured with the entry font on a sample number');
  // The label (or its support line, whichever is wider) is part of the room needed.
  assert.match(layoutCall(messure), /std::max\(m\.label\.x, m\.support\.x\)/, 'the label width is part of the room needed');
  assert.match(layout, /const int label_slot = label > 0 \? label \+ \(editable \? gap : 0\) : 0;/, 'the label takes its own slot');
  assert.match(layout, /2 \* padding \+ icons \+ label_slot \+ prefix_slot \+ unit_slot \+ \(editable \? entry_min : 0\)/,
    'the minimum width adds the number room to every other part');
  assert.match(messure, /const wxSize minimum\(std::max\(GetMinWidth\(\), layout\.minimum_width\), minimum_height\);\s*SetMinSize\(minimum\);/,
    'the minimum width never drops below what the number and label need');
  assert.match(messure, /SetSize\(std::max\(current\.x, minimum\.x\), std::max\(current\.y, minimum\.y\)\);/,
    'a field created narrower than that grows to it');
});

test('the room counts everything DoSetSize takes out of the entry', () => {
  // One helper computes both the entry width DoSetSize() gives the editor and the
  // minimum messureSize() asks for, from the same measured parts, so the two
  // cannot disagree about what the entry loses.
  for (const part of [/icon\.bmp\(\)\.IsOk\(\)/, /icon_1\.bmp\(\)\.IsOk\(\)/, /m\.prefix = dc\.GetTextExtent\(m_prefix\)/, /m\.unit = dc\.GetTextExtent\(m_unit\)/]) {
    assert.match(measure, part);
  }
  assert.match(layout, /const int entry_width = editable \? std::max\(0, width - 2 \* padding - icons - label_slot - prefix_slot - unit_slot\) : 0;/,
    'the entry loses exactly the parts the minimum adds');
  const sized = layoutCall(doSetSize).replace(/^size\.x,/, 'WIDTH,');
  const measured = layoutCall(messure).replace(/^GetSize\(\)\.x,/, 'WIDTH,');
  assert.equal(measured, sized, 'messureSize() and DoSetSize() lay out the same parts');
  assert.match(doSetSize, /text_ctrl->SetSize\(layout\.entry_width, entry_height\);/, 'DoSetSize() gives the editor the computed width');
});

test('a centred field draws its unit after the number, not under it', () => {
  // The calibration fields are created with wxTE_CENTRE. The unit label was then
  // drawn at the field's left edge while the entry also sat there, so the entry
  // covered the start of the unit: "0.1/mm" for "0.1 mm/mm", "5 /秒" for
  // "5 mm³/秒". Only a right-aligned field moves the entry to make room for a
  // label on the left, so only a right-aligned field draws it there.
  for (const [name, code] of [['DoSetSize()', doSetSize], ['render()', render], ['messureSize()', messure]]) {
    const call = layoutCall(code);
    assert.match(call, /m\.editable, !m\.editable \|\| \(GetWindowStyle\(\) & wxALIGN_RIGHT\)$/,
      `${name}: an editable field puts its label on the left only when right-aligned`);
    assert.doesNotMatch(call, /wxALIGN_CENTER|wxTE_CENTRE/, `${name}: centring never moves the label before the entry`);
  }
  assert.equal(layoutCall(render), layoutCall(doSetSize), 'render() draws where DoSetSize() placed the entry');
  assert.match(layout, /const int unit_x = entry_x \+ entry_width \+ \(unit > 0 \? gap : 0\);/, 'the unit follows the number');
  assert.match(layout, /const int label_x = label_left \? left : entry_x \+ entry_width \+ unit_slot \+ \(label > 0 \? gap : 0\);/,
    'a label that is not on the left follows the number and its unit');
  assert.match(render, /dc\.DrawText\(m_unit, layout\.unit_x,/, 'render() draws the unit at the computed place');
  assert.match(doSetSize, /text_ctrl->SetPosition\(\{layout\.entry_x,/, 'the entry sits where the layout put it');
});
