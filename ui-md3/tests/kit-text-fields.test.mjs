import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Two editable fields still drew the system's white box and sunken border: the
// colour picker's HEX and "Enter any format" fields, inside a Material card, and
// the object list's rename editor, whose light border stood out in dark mode.

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

test('the object list\'s rename editor is a Material filled field', async () => {
  const renderers = strip(await readFile(path.join(gui, 'ExtraRenderers.cpp'), 'utf8'));
  assert.match(renderers, /new wxTextCtrl\(parent, wxID_ANY, data\.GetText\(\),\s*position, labelRect\.GetSize\(\), wxTE_PROCESS_ENTER \| wxBORDER_NONE\);/);
  assert.match(renderers, /text_editor->SetBackgroundColour\(StateColor::semantic\(MD3::Role::SurfaceContainerHighest\)\);/);
  assert.match(renderers, /text_editor->SetForegroundColour\(StateColor::semantic\(MD3::Role::OnSurface\)\);/);
});
