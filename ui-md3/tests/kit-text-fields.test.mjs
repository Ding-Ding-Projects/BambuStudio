import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Editable fields that drew the system's white box and sunken border: the colour
// picker's HEX and "Enter any format" fields, inside a Material card, and the
// object list's rename editor, whose light border stood out in dark mode. All of
// them are kit TextInput fields now. lane-text-fields.test.mjs covers the other
// single-line editors and keeps the list of native edit boxes closed.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const strip = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

test('the colour picker\'s editable fields are kit fields', async () => {
  const picker = strip(await readFile(path.join(gui, 'Widgets', 'MD3ColorPicker.cpp'), 'utf8'));
  assert.match(picker, /auto \*hex_field = new ::TextInput\(this, [^;]*\);\s*hex_field->SetName\(_L\("HEX color"\)\);\s*m_hex = hex_field->GetTextCtrl\(\);/);
  assert.match(picker, /hex_row->Add\(hex_field, /);
  assert.match(picker, /auto \*any_field = new ::TextInput\(this, [^;]*\);\s*any_field->SetName\(_L\("Enter any color format"\)\);\s*m_any_format = any_field->GetTextCtrl\(\);/);
  assert.match(picker, /left->Add\(any_field, /);
  assert.doesNotMatch(picker, /m_hex = new wxTextCtrl\(|m_any_format = new wxTextCtrl\(/);
});

test('the object list\'s rename editor is a kit text field', async () => {
  // It was a flat wxTextCtrl tinted by hand (SurfaceContainerHighest behind OnSurface);
  // the kit field draws that fill, its outline and its focus ring itself, so the
  // editor is the kit TextInput and the list reads the name from its inner entry.
  const renderers = strip(await readFile(path.join(gui, 'ExtraRenderers.cpp'), 'utf8'));
  assert.match(renderers, /::TextInput\* editor = new ::TextInput\(parent, data\.GetText\(\), wxEmptyString, wxEmptyString,\s*position, labelRect\.GetSize\(\), 0\);/);
  assert.match(renderers, /wxTextCtrl\* text_editor = editor->GetTextCtrl\(\);/);
  assert.match(renderers, /auto\* editor = dynamic_cast<::TextInput\*>\(ctrl\);\s*wxTextCtrl\* text_editor = editor \? editor->GetTextCtrl\(\) : nullptr;/);
  assert.doesNotMatch(renderers, /new wxTextCtrl\(|wxDynamicCast\(ctrl, wxTextCtrl\)/);
  assert.doesNotMatch(renderers, /text_editor->SetBackgroundColour|text_editor->SetForegroundColour/, 'the kit field paints its own fill and text colour');
});
