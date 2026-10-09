import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Single-line text editors that were still bare native edit boxes, and the Add and
// Delete buttons of the Height range rows, which were flat native buttons. Each one
// is now a kit TextInput or a kit icon Button:
//   - the inline percentage editor of the Add / Edit Mixed Filament dialog,
//   - the x and y fields of every point option (Printer settings > Best object position),
//   - the three fields of each Height range row and their Add / Delete buttons,
//   - the Objects list's rename editor (its click-away commit goes through a bridge),
//   - the read-only rows of the colour picker's Translations column,
//   - the regex builder's pattern and literal-text fields.
// Every inner entry stays a wxTextCtrl reached through TextInput::GetTextCtrl(), so
// the reads, writes and binds are unchanged. The last two tests keep the list of
// native edit boxes closed: a new bare wxTextCtrl has to be argued for here.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const read = async (...parts) => code(await readFile(path.join(guiDir, ...parts), 'utf8'));
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `${signature} must exist`);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};
const count = (text, pattern) => (text.match(pattern) || []).length;

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}

const rel = (file) => path.relative(guiDir, file).replaceAll('\\', '/');
// String and character literals are data, not constructions: the generated
// Documentation/DocumentationBundle.hpp quotes the parity register, whose prose
// names "new wxTextCtrl(". One left-to-right pass, so a quote inside the other
// kind of literal is consumed with it.
const withoutLiterals = (text) => text.replace(/"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*'/g, '""');

test('the ratio editor of the Mixed Filament dialog is a kit field with the percent unit', async () => {
  const dialog = await read('MixedFilamentDialog.cpp');
  const editor = fn(dialog, 'void MixedFilamentDialog::start_ratio_editor(');
  assert.match(editor, /m_ratio_field = new ::TextInput\(m_ratio_editor_panel, wxEmptyString, wxEmptyString, wxEmptyString,\s*wxDefaultPosition, wxDefaultSize, 0, wxT\("%"\)\);/);
  assert.match(editor, /m_ratio_editor = m_ratio_field->GetTextCtrl\(\);/, 'the editor stays the inner entry');
  assert.match(editor, /m_ratio_editor->SetWindowStyleFlag\(m_ratio_editor->GetWindowStyleFlag\(\) \| wxTE_RIGHT\);/, 'the digits stay right aligned');
  assert.match(editor, /hsizer->Add\(m_ratio_field, 1, wxALIGN_CENTER_VERTICAL \| wxEXPAND\);/);
  // The kit draws the outline: a framed panel around it would stack two frames.
  assert.match(editor, /m_ratio_editor_panel = new wxPanel\(this, wxID_ANY, wxDefaultPosition,\s*wxDefaultSize, wxBORDER_NONE\);/);
  assert.doesNotMatch(dialog, /new wxTextCtrl\(/);
  assert.doesNotMatch(dialog, /wxBORDER_SIMPLE/);
  assert.doesNotMatch(editor, /pct_label/, 'the percent sign is the field\'s unit, not a second label');
  // The three binds still reach the entry the person types into.
  for (const event of ['wxEVT_TEXT_ENTER', 'wxEVT_KILL_FOCUS', 'wxEVT_CHAR_HOOK'])
    assert.ok(editor.includes(`m_ratio_editor->Bind(${event},`), `${event} must stay bound to the inner entry`);
  assert.match(dialog, /#include "Widgets\/TextInput\.hpp"/);
  const header = code(await readFile(path.join(guiDir, 'MixedFilamentDialog.hpp'), 'utf8'));
  assert.match(header, /^class TextInput;$/m, 'a global forward declaration');
  assert.match(header, /::TextInput\*\s+m_ratio_field\{nullptr\};/);
  assert.match(header, /wxTextCtrl\*\s+m_ratio_editor\{nullptr\};/);
});

test('every point option builds two kit fields', async () => {
  const field = await read('Field.cpp');
  const build = fn(field, 'void PointCtrl::BUILD()');
  for (const axis of ['x', 'y']) {
    const value = axis.toUpperCase();
    assert.match(build, new RegExp(`${axis}_input = new ::TextInput\\(m_parent, ${value}, wxEmptyString, wxEmptyString, wxDefaultPosition, field_size, wxTE_PROCESS_ENTER\\);`));
    assert.match(build, new RegExp(`${axis}_textctrl = ${axis}_input->GetTextCtrl\\(\\);`), 'the entries stay reachable for the reads and the binds');
    assert.ok(build.includes(`temp->Add(${axis}_input);`), 'the field, not its entry, goes into the sizer');
    assert.ok(build.includes(`${axis}_textctrl->Bind(wxEVT_TEXT_ENTER,`));
    assert.ok(build.includes(`${axis}_textctrl->Bind(wxEVT_KILL_FOCUS,`));
    assert.ok(build.includes(`${axis}_input->SetToolTip(`));
  }
  assert.doesNotMatch(build, /new ::TextCtrl\(/);
  assert.doesNotMatch(build, /wxBORDER_SIMPLE/);
  assert.doesNotMatch(build, /_WIN32/, 'the Windows-only frame style is gone');
  assert.match(build, /const wxSize field_size\(6 \* m_em_unit, -1\);/, 'six em: the kit takes 15 px of the width for its own padding');
  const enable = fn(field, 'void PointCtrl::enable()');
  assert.ok(enable.includes('x_input->Enable();') && enable.includes('y_input->Enable();'));
  const disable = fn(field, 'void PointCtrl::disable()');
  assert.ok(disable.includes('x_input->Disable();') && disable.includes('y_input->Disable();'));
  assert.ok(fn(field, 'wxWindow* PointCtrl::getWindow()').includes('return x_input;'));
  const rescale = fn(field, 'void PointCtrl::msw_rescale()');
  assert.match(rescale, /x_input->SetMinSize\(field_size\);\s*y_input->SetMinSize\(field_size\);/);
  assert.match(rescale, /x_input->Rescale\(\);\s*y_input->Rescale\(\);/);
  const header = code(await readFile(path.join(guiDir, 'Field.hpp'), 'utf8'));
  assert.match(header, /^class TextInput;$/m, 'a global forward declaration');
  // Field.hpp declares enable() and disable() for several classes; look at this one only.
  const point = header.slice(header.indexOf('class PointCtrl : public Field'), header.indexOf('class StaticText : public Field'));
  assert.match(point, /::TextInput\*\s+x_input\{ nullptr \};\s*::TextInput\*\s+y_input\{ nullptr \};/);
  assert.match(point, /void\s+enable\(\) override;\s*void\s+disable\(\) override;/, 'they need the complete kit type, so they live in Field.cpp');
  assert.match(point, /wxWindow\*\s+getWindow\(\) override;/);
});

test('the Height range rows are kit fields and kit icon buttons', async () => {
  const header = await read('GUI_ObjectLayers.hpp');
  assert.match(header, /class LayerRangeEditor : public ::TextInput\b/);
  assert.match(header, /class PlusMinusButton : public ::Button\b/);
  assert.doesNotMatch(header, /ScalableButton|ScalableBitmap/);
  assert.match(header, /SetIconButton\(::Button::IconShape::Circle, 28\);\s*SetGlyph\(glyph\);/);
  assert.match(header, /#include "Widgets\/Button\.hpp"/);
  assert.match(header, /#include "Widgets\/TextInput\.hpp"/);

  const layers = await read('GUI_ObjectLayers.cpp');
  const ctor = fn(layers, 'LayerRangeEditor::LayerRangeEditor(');
  assert.match(ctor, /::TextInput\(parent->m_parent, value, wxEmptyString, wxEmptyString, wxDefaultPosition,\s*wxSize\(7 \* em_unit\(parent->m_parent\), wxDefaultCoord\), 0\),/);
  assert.doesNotMatch(layers, /wxBORDER_SIMPLE/);
  // The kit field sends Enter and the focus loss on to itself under its own id, so
  // those two stay on the field; everything else is bound to the entry, which has
  // an id of its own and must not be filtered by the field's id.
  assert.match(ctor, /entry->Bind\(wxEVT_TEXT, \[this\]\(wxEvent&\) \{ m_enter_pressed = false; \}\);/);
  assert.match(ctor, /this->Bind\(wxEVT_TEXT_ENTER, \[this, edit_fn\]/);
  assert.match(ctor, /this->Bind\(wxEVT_KILL_FOCUS, \[this, edit_fn\]/);
  assert.equal(count(ctor, /\}, this->GetId\(\)\);/g), 2, 'exactly the Enter and the focus-loss binds filter by the field\'s id');
  assert.match(ctor, /entry->Bind\(wxEVT_SET_FOCUS, /);
  assert.match(ctor, /entry->Bind\(wxEVT_CHAR, /);
  // The window that takes the focus is the next field's entry: its parent is the field.
  assert.match(ctor, /dynamic_cast<LayerRangeEditor\*>\(e\.GetWindow\(\) \? e\.GetWindow\(\)->GetParent\(\) : nullptr\)/);
  // A click on an Add or Delete button is detected by the window that takes the focus.
  assert.equal(count(ctor, /dynamic_cast<ObjectLayers::PlusMinusButton\*>\(e\.GetWindow\(\)\)/g), 2);
  assert.doesNotMatch(layers, /(?<!GetTextCtrl\(\)->)SetValue\(/, 'the field has no SetValue of its own; every write goes to its entry');
  assert.doesNotMatch(layers, /[^.>]GetValue\(\)/, 'nor a GetValue');

  const list = fn(layers, 'void ObjectLayers::create_layers_list()');
  assert.match(list, /new PlusMinusButton\(m_parent, "delete_filament", MaterialIcon::Delete, range\)/);
  assert.match(list, /new PlusMinusButton\(m_parent, "add_filament", MaterialIcon::Add, range\)/);
  // The edit fields need the button to take the mouse focus (Button does, while it can focus).
  assert.doesNotMatch(layers + header, /SetCanFocus\(false\)/);
  assert.match(list, /add_btn->Enable\(tooltip\.IsEmpty\(\)\);/);
  assert.doesNotMatch(list, /SetBackgroundColour/, 'the kit button reads its parent surface itself');
  assert.doesNotMatch(layers, /m_bmp_delete|m_bmp_add/);
  assert.equal(count(layers, /button->Rescale\(\);/g), 2, 'rescale and theme change re-derive the kit button');
  assert.doesNotMatch(layers, /button->msw_rescale\(\);/);
});

test('the Objects list rename editor commits when the focus leaves it', async () => {
  const renderers = await read('ExtraRenderers.cpp');
  assert.match(renderers, /#include "Widgets\/TextInput\.hpp"/);
  const create = fn(renderers, 'wxWindow* BitmapTextRenderer::CreateEditorCtrl(');
  assert.match(create, /::TextInput\* editor = new ::TextInput\(parent, data\.GetText\(\), wxEmptyString, wxEmptyString,\s*position, labelRect\.GetSize\(\), 0\);/);
  assert.match(create, /editor->SetSize\(wxRect\(position, labelRect\.GetSize\(\)\)\);/, 'the box is put on exactly the cell');
  assert.match(create, /text_editor->SetFont\(parent->GetFont\(\)\);/);
  assert.match(create, /return editor;/);
  assert.doesNotMatch(create, /new wxTextCtrl\(/);
  // A stale rename editor is a kit field now; the Filament column's ComboBox editor is one too and is not ours.
  assert.match(create, /dynamic_cast<::TextInput\*>\(child\) && !dynamic_cast<::ComboBox\*>\(child\)/);
  assert.doesNotMatch(create, /dynamic_cast<wxTextCtrl\*>\(child\)/);
  // The focus sits on the inner entry, so the handler wx pushes onto the field never sees
  // it leave. The bridge finishes the edit after the event, and only for the same editor.
  assert.match(create, /text_editor->Bind\(wxEVT_KILL_FOCUS, \[this, editor_ref = wxWeakRef<wxWindow>\(editor\)\]\(wxFocusEvent& e\) \{\s*e\.Skip\(\);/);
  assert.match(create, /for \(wxWindow\* win = e\.GetWindow\(\); win; win = win->GetParent\(\)\)\s*if \(win == editor_ref\.get\(\)\)\s*return;/, 'focus moving inside the editor is not leaving it');
  assert.match(create, /wxTheApp->CallAfter\(\[this, editor_ref\] \{\s*if \(editor_ref && GetEditorCtrl\(\) == editor_ref\.get\(\)\)\s*FinishEditing\(\);/);
  const value = fn(renderers, 'bool BitmapTextRenderer::GetValueFromEditorCtrl(');
  assert.match(value, /auto\* editor = dynamic_cast<::TextInput\*>\(ctrl\);\s*wxTextCtrl\* text_editor = editor \? editor->GetTextCtrl\(\) : nullptr;/);
  assert.doesNotMatch(renderers, /wxDynamicCast\(ctrl, wxTextCtrl\)/, 'a kit field has no wx run-time class information');
});

test('the colour picker translations and the regex builder fields are kit fields', async () => {
  const picker = await read('Widgets', 'MD3ColorPicker.cpp');
  assert.match(picker, /auto \*value_field = new ::TextInput\(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition,\s*wxSize\(FromDIP\(kValueW\) \+ kFieldPadPx, FromDIP\(kRowH\)\), wxTE_READONLY\);/);
  assert.match(picker, /row\.value = value_field->GetTextCtrl\(\);/);
  assert.match(picker, /line->Add\(value_field, 1, /);
  assert.match(picker, /constexpr int kFieldPadPx = 15;/, 'the kit\'s padding is added to the widest value so its entry keeps the old width');
  assert.doesNotMatch(picker, /new wxTextCtrl\(/);
  assert.doesNotMatch(picker, /row\.value->SetBackgroundColour|row\.value->SetForegroundColour/);
  assert.match(picker, /row\.value->GetParent\(\)->SetToolTip\(text\);/, 'the tip covers the whole field');
  // The HEX and any-format fields of the same dialog stay pinned by kit-text-fields.test.mjs.

  const regex = await read('Widgets', 'RegexBuilderPopup.cpp');
  assert.match(regex, /#include "TextInput\.hpp"/);
  assert.match(regex, /auto \*pattern_field = new ::TextInput\(m_scroll, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition,\s*wxSize\(contentW - FromDIP\(50\), FromDIP\(kTargetH\)\), wxTE_PROCESS_ENTER\);/);
  assert.match(regex, /m_pattern = pattern_field->GetTextCtrl\(\);/);
  assert.match(regex, /pat_row->Add\(pattern_field, 1, wxALIGN_CENTER_VERTICAL\);/);
  assert.match(regex, /auto \*literal_field = new ::TextInput\(m_scroll, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition,\s*wxSize\(contentW - FromDIP\(92\), FromDIP\(kTargetH\)\), wxTE_PROCESS_ENTER\);/);
  assert.match(regex, /m_literal = literal_field->GetTextCtrl\(\);/);
  assert.match(regex, /lit_row->Add\(literal_field, 1, wxALIGN_CENTER_VERTICAL\);/);
  assert.doesNotMatch(regex, /new wxTextCtrl\(/);
  assert.doesNotMatch(regex, /m_pattern->SetBackgroundColour|m_literal->SetBackgroundColour/);
  // The popup still focuses the inner entry, the control the person types into.
  assert.match(regex, /m_pattern->SetInsertionPointEnd\(\);\s*Popup\(m_pattern\);/);
  // The multi-line views of the same popup stay on the kit scrollbar editor.
  assert.match(regex, /m_sample = new TextAreaEditor\(/);
  assert.match(regex, /m_results = new TextAreaEditor\(/);
});

test('native edit boxes are constructed only by the kit\'s own fields', async () => {
  // new wxTextCtrl( and the colour-safe kit subclass, new ::TextCtrl( / new TextCtrl(, are
  // the two ways to get a bare system edit box. The list is the kit fields' own inner
  // entries plus one developer-only URL bar; it is written out, so a file that joins it
  // and a file that leaves it both turn this red.
  const expected = [
    'Widgets/SearchField.cpp',   // the pill's inner entry
    'Widgets/SpinInput.cpp',     // the spin field's inner entry
    'Widgets/TempInput.cpp',     // the temperature field's inner entry
    'Widgets/TextInput.cpp',     // the text field's inner entry
    'WebViewDialog.cpp',         // developer-only URL bar, compiled out of public releases
  ].sort();
  const found = [];
  for (const file of await sources(guiDir)) {
    const text = withoutLiterals(code(await readFile(file, 'utf8')));
    if (/new\s+(?:::)?(?:wx)?TextCtrl\s*\(/.test(text)) found.push(rel(file));
  }
  assert.deepEqual(found.sort(), expected);
});

test('only the kit\'s own classes derive from wxTextCtrl', async () => {
  // A class that derives from wxTextCtrl is a bare edit box however it is constructed.
  // TextAreaEditor is the multi-line entry of the kit TextArea, TextCtrl is the kit's
  // colour-safe subclass, and DiamTextCtrl belongs to the configuration wizard, whose
  // pages are not reachable. LayerRangeEditor used to be on this list and is gone.
  const found = [];
  for (const file of await sources(guiDir)) {
    const text = code(await readFile(file, 'utf8'));
    for (const match of text.matchAll(/\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?:\s*(?:public\s+)?(?:::)?wxTextCtrl\b/g))
      found.push(match[1]);
    assert.doesNotMatch(text, /\b(?:class|struct)\s+\w+\s*(?:final\s*)?:\s*(?:public\s+)?(?:::)?TextCtrl\b/, `${rel(file)} derives from the kit's TextCtrl`);
  }
  assert.deepEqual(found.sort(), ['DiamTextCtrl', 'TextAreaEditor', 'TextCtrl']);
});
