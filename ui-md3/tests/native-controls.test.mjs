import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Native Windows controls that were still on user-facing surfaces, and their kit
// replacements: the tip window of a disabled button, the web view's info bar,
// and the Workspace panel's tab control, report lists, check list and month
// calendar. The first test refuses the native classes anywhere in the GUI.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const read = async (...parts) => code(await readFile(path.join(guiDir, ...parts), 'utf8'));
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, signature);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}

test('no native tab control, list, check list, calendar, tip window, info bar, group box, choice book or tree is constructed', async () => {
  // A wxStaticBoxSizer given an orientation first makes its own native box; given a
  // box, it takes whatever box it is handed (MD3GroupBox, or the sidebar's StaticGroup).
  // The standard button sizers make native OK and Cancel buttons.
  const pattern = /new\s+(wxNotebook|wxListCtrl|wxCheckListBox|wxCalendarCtrl|wxTipWindow|wxInfoBar|wxStaticBox|wxChoicebook|wxTreeCtrl)\s*\(|(wxStaticBoxSizer)\s*\(\s*wx(?:VERTICAL|HORIZONTAL)|\b(CreateButtonSizer|CreateStdDialogButtonSizer|CreateSeparatedButtonSizer|wxStdDialogButtonSizer)\b/g;
  const found = [];
  for (const file of await sources(guiDir)) {
    const text = code(await readFile(file, 'utf8')).replace(/"(?:[^"\\\n]|\\.)*"/g, '""');
    for (const match of text.matchAll(pattern))
      found.push(`${path.relative(guiDir, file).replaceAll('\\', '/')}: ${match[1] ?? match[2] ?? match[3]}`);
  }
  assert.deepEqual(found, []);
});

test('a disabled button shows the Material plain tooltip', async () => {
  const button = await read('Widgets', 'Button.cpp');
  const tip = button.slice(button.indexOf('class ButtonDisabledTip : public wxPopupWindow'), button.indexOf('void Button::EnableTooltipEvenDisabled()'));
  assert.match(tip, /StateColor::semantic\(MD3::Role::InverseSurface\)/);
  assert.match(tip, /StateColor::semantic\(MD3::Role::InverseOn\)/);
  assert.match(tip, /::Label::Body_12/);
  assert.match(tip, /Disable\(\);/, 'it never takes the pointer or the focus');
  assert.match(fn(button, 'void Button::OnParentMotion('), /tipWindow = new ButtonDisabledTip\(this\);/);
  assert.match(await read('Widgets', 'Button.hpp'), /ButtonDisabledTip\* tipWindow = nullptr;/);
});

test('the web view reports a failed page in a Material banner', async () => {
  const web = await read('WebViewDialog.cpp');
  const banner = web.slice(web.indexOf('class MD3InfoBanner : public wxPanel'), web.indexOf('wxDECLARE_EVENT(EVT_RESPONSE_MESSAGE'));
  assert.match(banner, /SetBackgroundColour\(StateColor::semantic\(MD3::Role::SurfaceContainerHigh\)\);/);
  assert.match(banner, /action->SetVariant\(Button::Variant::Text\);/, 'the action is a kit text button');
  assert.match(banner, /close->SetGlyph\(MaterialIcon::Close, 18\);/);
  assert.match(banner, /void ShowMessage\(const wxString &message, int flags\)/);
  assert.match(banner, /void Dismiss\(\)/);
  assert.match(web, /m_info = new MD3InfoBanner\(this, m_cloud_retry_button_id, _L\("Retry"\)\);/);
});

test('the Workspace panel is built from kit controls', async () => {
  const panel = await read('WorkspacePanel.cpp');
  const ui = fn(panel, 'void WorkspacePanel::create_ui()');
  assert.match(ui, /m_section_tabs = new TextTabbar\(this, TextTabbar::Align::Left\);/);
  assert.match(ui, /m_sections = new wxSimplebook\(this, wxID_ANY\);/);
  assert.match(ui, /m_files = new MD3DataViewListCtrl\(/);
  assert.match(ui, /m_agenda = new MD3DataViewListCtrl\(/);
  assert.match(ui, /md3_style_data_view\(m_files\);/);
  assert.match(ui, /md3_style_data_view\(m_agenda\);/);
  assert.match(ui, /m_checklist = new ListBox\(list_page, wxID_ANY\);\s*m_checklist->EnableChecks\(\);/);
  assert.match(ui, /m_month = new wxGenericCalendarCtrl\([^;]*wxCAL_SEQUENTIAL_MONTH_SELECTION/);
  assert.match(ui, /m_month->SetHighlightColours\(StateColor::semantic\(MD3::Role::OnPrimary\), StateColor::semantic\(MD3::Role::Primary\)\);/);
  // The agenda keeps each row's slot index for the actions.
  assert.match(panel, /m_agenda->AppendItem\(row, static_cast<wxUIntPtr>\(position\)\);/);
  assert.equal((panel.match(/m_agenda->GetItemData\(m_agenda->RowToItem\(row\)\)/g) || []).length, 3);
});

test('a source that uses the generic calendar includes its base header first', async () => {
  // <wx/generic/calctrlg.h> declares only the control: wxCalendarCtrlBase, the
  // wxCAL_* styles and the calendar events live in <wx/calctrl.h>. Without it
  // the Workspace panel failed to compile ("wxCalendarCtrlBase: base class
  // undefined"), which only a hosted build noticed.
  for (const file of await sources(guiDir)) {
    const text = code(await readFile(file, 'utf8'));
    const generic = text.search(/#\s*include\s*[<"]wx\/generic\/calctrlg\.h[>"]/);
    if (generic === -1) continue;
    const base = text.search(/#\s*include\s*[<"]wx\/calctrl\.h[>"]/);
    const rel = path.relative(guiDir, file).replaceAll('\\', '/');
    assert.ok(base !== -1 && base < generic, `${rel} includes <wx/generic/calctrlg.h> without <wx/calctrl.h> before it`);
  }
});

test('the kit list can carry check boxes, and the kit text tabs take the Material roles', async () => {
  const list = await read('Widgets', 'ListBox.cpp');
  assert.match(fn(list, 'void ListBox::toggle('), /wxCommandEvent event\(wxEVT_CHECKLISTBOX, GetId\(\)\);/);
  assert.match(fn(list, 'void ListBox::onKey('), /WXK_SPACE/);
  assert.match(fn(list, 'void ListBox::OnDrawItem('), /MaterialIcon::CheckBox : MaterialIcon::CheckBoxOutlineBlank/);
  const tabs = await read('Widgets', 'TextTabbar.cpp');
  assert.doesNotMatch(tabs, /ThemeColor::|\*wxWHITE/, 'no legacy palette');
  assert.match(fn(tabs, 'void TextTabbar::render()'), /active \? MD3::Role::Primary : MD3::Role::OnSurfaceVariant/);
});

test('every group box draws the Material outline and title', async () => {
  const group = await read('Widgets', 'StaticGroup.cpp');
  const paint = fn(group, 'void MD3GroupBox::PaintForeground(');
  assert.match(paint, /wxPen\(StateColor::semantic\(MD3::Role::OutlineVariant\), 1\)/);
  assert.match(paint, /DrawRoundedRectangle\(0, top, rc\.right, rc\.bottom - top, FromDIP\(4\)\)/);
  assert.match(paint, /IsEnabled\(\) \? MD3::Role::OnSurface : MD3::Role::OnSurfaceVariant/);
  assert.match(fn(group, 'MD3GroupBox::MD3GroupBox('), /SetFont\(::Label::Head_14\);/);
  assert.doesNotMatch(fn(group, 'StaticGroup::StaticGroup('), /ThemeColor::/, 'the sidebar extruder groups take the roles too');
  assert.match(await read('OptionsGroup.cpp'), /wxStaticBox \* stb = new MD3GroupBox\(m_parent, _\(title\)\);/);
  const bed = await read('BedShapeDialog.cpp');
  assert.match(bed, /new wxStaticBoxSizer\(new MD3GroupBox\(this, _L\("Shape"\)\), wxVERTICAL\)/);
  assert.match(bed, /m_shape_choice = new ComboBox\(/);
  assert.match(bed, /m_shape_options_book = new wxSimplebook\(/);
  assert.doesNotMatch(bed, /\*wxWHITE|\*wxRED/, 'no white panels or buttons, no raw red');
});

test('the Objects list has no native column header and no system frame', async () => {
  const list = await read('GUI_ObjectList.cpp');
  assert.match(list, /MD3DataViewCtrl\(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_MULTIPLE \| wxDV_NO_HEADER \| wxBORDER_NONE\)/,
    'the kit Objects card is rows under a search field: no header strip, no outline');
  assert.doesNotMatch(list, /GenericGetHeader\(\)/, 'there is no header to style (a call here would dereference null)');

  const dark = fn(await read('GUI_App.cpp'), 'void GUI_App::UpdateDVCDarkUI(');
  assert.match(dark, /if \(wxHeaderCtrl \*header = dvc->GenericGetHeader\(\)\) \{/, 'a table without a header is safe to pass in');
  assert.doesNotMatch(dark, /dvc->GenericGetHeader\(\)->/);
  assert.match(dark, /if \(\(dvc->GetWindowStyle\(\) & wxBORDER_MASK\) == wxBORDER_DEFAULT\)\s*dvc->SetWindowStyle\(dvc->GetWindowStyle\(\) \| wxBORDER_SIMPLE\);/,
    'only a table that left its border at the default gets the system frame');
  assert.doesNotMatch(dark, /GetBorder\(\) != wxBORDER_SIMPLE/);

  assert.match(await read('Plater.cpp'), /const int header_h = list->HasFlag\(wxDV_NO_HEADER\) \? 0 : list->GetCharHeight\(\) \+ FromDIP\(12\);/,
    'the list height reserves no header row');
});

test('the plate settings dropdowns take the width of their row', async () => {
  const build = fn(await read('Tab.cpp'), 'void TabPrintPlate::build()');
  // A narrower label column (labels wrap instead of being cut) leaves the dropdowns room at the default width.
  assert.match(build, /auto optgroup = page->new_optgroup\("", wxEmptyString, 14\);/);
  assert.match(build, /auto append_select = \[&optgroup\]\(const std::string &key, const std::string &path = std::string\(\)\) \{\s*Option option = optgroup->get_option\(key\);\s*option\.opt\.full_width = true;\s*optgroup->append_single_option_line\(option, path\);\s*\};/);
  for (const key of ['curr_bed_type', 'print_sequence', 'first_layer_sequence_choice', 'other_layers_sequence_choice']) {
    assert.match(build, new RegExp(`append_select\\("${key}"`), `${key} is row-wide, so a long value is not cut to a 12 em face`);
    assert.doesNotMatch(build, new RegExp(`append_single_option_line\\("${key}"`), key);
  }
  assert.match(build, /optgroup->append_single_option_line\("spiral_mode", "spiral-vase"\);/, 'the checkbox row is unchanged');
  // Row-wide fields are sized during paint, so the option panel must repaint when it is resized.
  const panel = await read('OG_CustomCtrl.cpp');
  assert.match(fn(panel, 'OG_CustomCtrl::OG_CustomCtrl('), /this->Bind\(wxEVT_SIZE, \[this\]\(wxSizeEvent &e\) \{ Refresh\(\); e\.Skip\(\); \}\);/);
  // At the default sidebar width the row leaves less than the old fixed face; the field keeps
  // at least that, so a row-wide dropdown never shows less of its value than before.
  assert.match(panel, /const int row_width = ctrl->GetSize\(\)\.x - h_pos2 \+ h_pos3 - h_pos - ctrl->m_em_unit \* 3;\s*field->getWindow\(\)->SetSize\(std::max\(row_width, Field::def_width_wider\(\) \* ctrl->m_em_unit\), -1\);/);
  // The popup list caches its width; a face that changes width must make it measure again.
  assert.match(await read('Widgets', 'ComboBox.cpp'), /drop\.Create\(this, style & DD_STYLE_MASK\);\s*applyDropChevron\(\);\s*Bind\(wxEVT_SIZE, \[this\]\(wxSizeEvent &e\) \{ drop\.Invalidate\(\); e\.Skip\(\); \}\);/);
});

test('every data-view table takes the Material table style', async () => {
  const ext = await read('wxExtensions.cpp');
  const style = fn(ext, 'void md3_style_data_view(');
  assert.match(style, /SetFont\(::Label::Body_13\)/);
  assert.match(style, /SetHeaderAttr\(header\)/);
  assert.match(style, /MD3::Role::SurfaceContainerLowest/);
  for (const [file, list] of [['ConfigProfilesDialog.cpp', 'm_profile_list'], [path.join('Export', 'ExportDialog.cpp'), 'm_format_list'],
    ['ProjectHistoryDialog.cpp', 'm_version_list'], ['NotificationCenterPanel.cpp', 'm_list'], ['WorkspacePanel.cpp', 'm_files']])
    assert.match(await read(file), new RegExp(`md3_style_data_view\\(${list}\\);`), file);
});
