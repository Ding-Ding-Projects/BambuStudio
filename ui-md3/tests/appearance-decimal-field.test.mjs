import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The appearance editor's font size, letter spacing and line height were native
// wxSpinCtrlDouble controls: a system box with system arrows, and the system edit
// menu on a right-click. They are AppearanceDecimalField now, the kit TextInput
// with two kit chevron buttons, clamped to the same ranges and steps.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const dir = path.join(repoDir, 'src', 'slic3r', 'GUI', 'Appearance');
const strip = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const cpp = strip(await readFile(path.join(dir, 'AppearanceEditorPopover.cpp'), 'utf8'));
const hpp = strip(await readFile(path.join(dir, 'AppearanceEditorPopover.hpp'), 'utf8'));
const field = cpp.slice(cpp.indexOf('class AppearanceDecimalField : public wxPanel'), cpp.indexOf('AppearanceEditorPopover *AppearanceEditorPopover::s_current'));

test('the decimal fields are the kit field with kit steppers', () => {
  assert.match(field, /m_input = new ::TextInput\(this, /);
  assert.match(field, /b->SetGlyph\(direction > 0 \? MaterialIcon::ExpandLess : MaterialIcon::ExpandMore, 14\);/);
  assert.match(field, /b->SetName\(direction > 0 \? _L\("Increase"\) : _L\("Decrease"\)\);/, 'each stepper has a translated name');
  assert.doesNotMatch(cpp, /new wxSpinCtrlDouble\(/);
  assert.doesNotMatch(hpp, /wxSpinCtrlDouble/);
});

test('values stay in range, and only a change made in the field is reported', () => {
  assert.match(field, /m_value = std::clamp\(value, m_min, m_max\);/);
  assert.match(field, /ChangeValue\(wxString::FromCDouble\(m_value, m_digits\)\)/, 'ChangeValue, so loading a value sends no text event');
  assert.match(field, /if \(m_value != before && on_change\)\s*on_change\(m_value\);/);
  assert.match(field, /text\.Replace\(",", "\."\);/, 'a comma is taken as the decimal point');
  assert.match(field, /if \(e\.GetKeyCode\(\) == WXK_UP\)\s*step_by\(\+1\);/);
});

test('the three fields keep their ranges and steps', () => {
  assert.match(cpp, /m_size = new AppearanceDecimalField\(page, 4\.0, 96\.0, 13\.0, 0\.5, 1, _L\("Font size in points"\)\);/);
  assert.match(cpp, /m_letter_spacing = new AppearanceDecimalField\(page, -4\.0, 20\.0, 0\.0, 0\.1, 1, _L\("Letter spacing in pixels"\)\);/);
  assert.match(cpp, /m_line_height = new AppearanceDecimalField\(page, 0\.8, 3\.0, 1\.0, 0\.05, 2, _L\("Line height multiplier"\)\);/);
  for (const [member, prop] of [['m_size', 'font_size'], ['m_letter_spacing', 'letter_spacing'], ['m_line_height', 'line_height']]) {
    assert.ok(cpp.includes(`${member}->on_change = [this](double value) { if (!m_loading) write_number(StyleProp::${prop}, value); };`), `${member} writes ${prop}`);
  }
});
