import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

const root = fileURLToPath(new URL('../../', import.meta.url));
const genericPath = 'src/slic3r/GUI/Widgets/ComboBox.cpp';
// A sibling source commit can be inspected without changing either checkout.
const generic = process.env.BAMBU_PRESET_PAIRING_GENERIC_REF
  ? execFileSync('git', ['show', `${process.env.BAMBU_PRESET_PAIRING_GENERIC_REF}:${genericPath}`], { cwd: root, encoding: 'utf8' })
  : readFileSync(new URL('../../' + genericPath, import.meta.url), 'utf8');
const preset = readFileSync(new URL('../../src/slic3r/GUI/PresetComboBoxes.cpp', import.meta.url), 'utf8')
  .split('void PresetComboBox::apply_inspector_style()')[1].split('void PresetComboBox::msw_rescale()')[0];

function balanced(source, start) {
  let depth = 0;
  for (let end = start; end < source.length; end++) {
    if (source[end] === '(') depth++;
    if (source[end] === ')' && --depth === 0) return source.slice(start + 1, end);
  }
  throw new Error('Unbalanced state table');
}
function table(source, setter) {
  const start = source.indexOf(setter + '(StateColor(');
  if (start < 0) return null;
  const value = balanced(source, source.indexOf('(', start));
  return [...value.matchAll(/std::make_pair\(/g)].map(match => {
    const pair = balanced(value, match.index + 'std::make_pair'.length);
    const state = pair.match(/StateColor::(Disabled|Pressed|Focused|Hovered|Normal)/)?.[1];
    const role = pair.match(/MD3::(?:Light::(\w+)|resolve\(MD3::Role::(\w+))|ThemeColor::(\w+)/);
    assert.ok(state && role, 'Every tested state entry must be recognized');
    const name = role[1] ?? role[2] ?? role[3];
    const aliases = {scHigh:'SurfaceContainerHigh',scHighest:'SurfaceContainerHighest',scLow:'SurfaceContainerLow',
      scLowest:'SurfaceContainerLowest',secondaryContainer:'SecondaryContainer',onSecondaryContainer:'OnSecondaryContainer',
      onSurface:'OnSurface',TextPrimary:'OnSurface',TextDisabled:'TextDisabled'};
    return { state, role: aliases[name] ?? name };
  });
}
function resolve(entries, states) {
  return entries.find(entry => entry.state === 'Normal' || states.includes(entry.state)
    || (entry.state === 'Hovered' && states.includes('Focused')))?.role;
}
const inheritedFill = table(generic, 'SetBackgroundColor');
const inheritedText = table(generic, 'SetLabelColor');
const fill = table(preset, 'SetBackgroundColor') ?? inheritedFill;
const text = table(preset, 'SetLabelColor') ?? inheritedText;

test('preset neutral states override the inherited focused label role as a pair', () => {
  for (const states of [['Focused'], ['Hovered'], ['Pressed', 'Focused'], []]) {
    assert.match(resolve(fill, states), /^SurfaceContainer/);
    assert.equal(resolve(text, states), 'OnSurface', `${states.join('+') || 'Normal'} neutral fill must use OnSurface`);
  }
});
test('preset disabled foreground retains precedence over simultaneous focus', () => {
  assert.equal(text[0].state, 'Disabled');
  assert.equal(resolve(text, ['Disabled', 'Focused']), 'TextDisabled');
  assert.equal(resolve(fill, ['Disabled', 'Focused']), 'SurfaceContainerHigh');
});
