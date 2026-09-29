import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// wx's stock prompts, choosers, busy notice and colour dialog are system
// dialogs: their frame, fields, buttons and colour grid ignore the theme and the
// language modes. The Material stand-ins take the same arguments, so nothing
// but a new call site can bring the stock ones back, and this refuses them.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const slic3rDir = path.join(repoDir, 'src', 'slic3r');
const guiDir = path.join(slic3rDir, 'GUI');

// Comments go; string literals stay, for the checks that quote a label.
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
// For the scan: also no includes, no string contents, and no forward
// declarations, which name a class without showing it.
const strip = (text) => code(text)
  .replace(/^\s*#\s*include\b.*$/gm, '')
  .replace(/"(?:[^"\\\n]|\\.)*"/g, '""')
  .replace(/\bclass\s+\w+\s*;/g, '');
const read = async (...parts) => code(await readFile(path.join(guiDir, ...parts), 'utf8'));
// A function body: from its signature to the closing brace in column 0.
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, signature);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

const STOCK = [
  'wxTextEntryDialog', 'wxPasswordEntryDialog', 'wxNumberEntryDialog', 'wxMultiChoiceDialog', 'wxSingleChoiceDialog',
  'wxColourDialog', 'wxFontDialog', 'wxFindReplaceDialog', 'wxProgressDialog', 'wxGenericProgressDialog', 'wxBusyInfo',
  'wxTipDialog', 'wxLogWindow', 'wxGetTextFromUser', 'wxGetPasswordFromUser', 'wxGetNumberFromUser', 'wxGetSingleChoice',
  'wxGetSingleChoiceIndex', 'wxGetSingleChoiceData', 'wxGetSelectedChoices', 'wxGetMultipleChoices', 'wxGetColourFromUser',
  'wxGetFontFromUser', 'wxShowTip', 'wxAboutBox', 'wxGenericAboutBox',
];
// Where a stock dialog may stay, and why.
const ALLOWED = new Map([
  // Preferences > Developer Tools > Internal developer mode opens it. That tab is
  // compiled out of every release (BBL_RELEASE_TO_PUBLIC=1), so no released
  // build can show the log window.
  ['GUI/MainFrame.cpp', ['wxLogWindow']],
  ['GUI/MainFrame.hpp', ['wxLogWindow']],
]);

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}

test('no stock prompt, chooser, busy notice or colour dialog is left in the GUI', async () => {
  const pattern = new RegExp(`\\b(?:${STOCK.join('|')})\\b`, 'g');
  const found = [];
  for (const file of await sources(slic3rDir)) {
    const rel = path.relative(slic3rDir, file).replaceAll('\\', '/');
    const allowed = ALLOWED.get(rel) ?? [];
    for (const name of new Set(strip(await readFile(file, 'utf8')).match(pattern) ?? []))
      if (!allowed.includes(name)) found.push(`${rel}: ${name}`);
  }
  assert.deepEqual(found, []);
});

test('the Material prompts are MsgDialogs built from kit fields', async () => {
  const header = await read('MsgDialog.hpp');
  for (const name of ['TextEntryDialog', 'NumberEntryDialog', 'MultiChoiceDialog'])
    assert.match(header, new RegExp(`class ${name} : public MsgDialog`), name);
  const source = await read('MsgDialog.cpp');
  const body = (name) => fn(source, `${name}::${name}(`);
  assert.match(body('TextEntryDialog'), /new ::TextArea\(/, 'a multi-line prompt uses the kit text area');
  assert.match(body('TextEntryDialog'), /new ::TextInput\(/, 'a one-line prompt uses the kit text field');
  assert.match(body('TextEntryDialog'), /Bind\(wxEVT_TEXT_ENTER, \[this\]\(wxCommandEvent &\) \{ EndModal\(wxID_OK\); \}\)/, 'Enter accepts');
  assert.match(body('NumberEntryDialog'), /new ::SpinInput\(/, 'a number prompt uses the kit spin field');
  assert.match(source, /long NumberEntryDialog::GetValue\(\) const\s*\{[^}]*GetTextCtrl\(\)->GetValue\(\)\.ToLong\(&typed\)[^}]*std::clamp\(typed, m_min, m_max\)/,
    'the typed number counts before the field commits it, kept inside the range');
  assert.match(body('MultiChoiceDialog'), /new ::LabeledCheckBox\(list, choice\)/, 'each choice is a kit checkbox row');
  // The prompt text goes through the message body renderer, which stacks the
  // Cantonese under the English in bilingual mode.
  for (const name of ['TextEntryDialog', 'NumberEntryDialog', 'MultiChoiceDialog'])
    assert.match(body(name), /add_msg_content\(this, content_sizer, message\);/, name);
});

test('the Material busy notice paints before the blocking work and shows both languages', async () => {
  const source = await read('MsgDialog.cpp');
  const busy = fn(source, 'BusyInfo::BusyInfo(');
  assert.match(busy, /I18N::bilingual_secondary\(message\)/);
  assert.match(busy, /StateColor::semantic\(MD3::Role::SurfaceContainerHigh\)/);
  assert.match(busy, /MD3::Metrics::radius_dialog/);
  assert.match(busy, /m_frame->Show\(\);\s*m_frame->Refresh\(\);\s*m_frame->Update\(\);\s*\}\s*$/, 'painted before the constructor returns');
  const plater = await read('Plater.cpp');
  assert.match(plater, /BusyInfo info\(_L\("Replace from:"\), q->get_current_canvas3D\(\)->get_wxglcanvas\(\), from_u8\(path\)\);/);
  assert.match(plater, /BusyInfo info\(_L\("Reload from:"\), q->get_current_canvas3D\(\)->get_wxglcanvas\(\), from_u8\(path\)\);/);
});

test('every ink colour is picked in the Material picker, with the recently used colours', async () => {
  const picker = await read('Widgets', 'MD3ColorPicker.hpp');
  assert.match(picker, /struct Options \{\s*std::vector<wxColour> recent;\s*bool opacity \{ true \};\s*wxString title;\s*\};/);
  const pickerSource = await read('Widgets', 'MD3ColorPicker.cpp');
  assert.match(pickerSource, /if \(!m_options\.recent\.empty\(\)\) \{\s*left->Add\(caption_label\(_L\("Recently used"\)\)/, 'the recently used row is captioned');
  assert.match(pickerSource, /if \(m_options\.opacity\) \{\s*auto \*alpha_row/, 'the opacity row can be left out');

  const ext = await read('wxExtensions.cpp');
  const pick = fn(ext, 'wxColour pick_filament_color(');
  assert.match(pick, /options\.recent\s*=\s*recent_custom_colors\(\);/);
  assert.match(pick, /options\.opacity = false;/, 'ink colours are stored opaque');
  assert.match(pick, /remember_custom_color\(picked\);/, 'an accepted pick joins the recently used colours');
  assert.match(fn(ext, 'wxColourData show_sys_picker_dialog('), /pick_filament_color\(parent, clr_data\.GetColour\(\)/);

  assert.match(await read('AMSMaterialsSetting.cpp'), /const wxColour picked = pick_filament_color\(nullptr, m_clrData->GetColour\(\)\);/);
  assert.match(await read('PresetComboBoxes.cpp'), /const wxColour picked = pick_filament_color\(this, clr\);/);
  assert.match(await read('BulkFilamentDialog.cpp'), /const wxColour picked = pick_filament_color\(this, initial\);/);
  const field = await read('Field.cpp');
  assert.match(field, /options\.recent = recent_custom_colors\(\);/, 'the settings colour field offers them too');
  assert.match(field, /void ColourPicker::remember_custom_color\(const wxColour &color\)\s*\{\s*::remember_custom_color\(color\);\s*\}/,
    'one shared list, kept in one place');
});

test('the object list settings chooser is the Material multi-choice dialog', async () => {
  const factories = await read('GUI_Factories.cpp');
  assert.match(factories, /MultiChoiceDialog dialog\(nullptr, _L\("Select settings"\), category_name, names\);\s*dialog\.SetSelections\(selections\);/);
});

test('the single-choice helper is the kit dialog on every platform', async () => {
  const app = await read('GUI_App.cpp');
  assert.match(app, /int GUI_App::GetSingleChoiceIndex\([^)]*\)\s*\{\s*SingleChoiceDialog dialog\(message, caption, choices, initialSelection\);\s*return dialog\.GetSingleChoiceIndex\(\);\s*\}/);
});
