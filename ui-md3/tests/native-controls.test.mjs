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

test('no native tab control, report list, check list, month calendar, tip window or info bar is constructed', async () => {
  const pattern = /new\s+(wxNotebook|wxListCtrl|wxCheckListBox|wxCalendarCtrl|wxTipWindow|wxInfoBar)\s*\(/g;
  const found = [];
  for (const file of await sources(guiDir)) {
    const text = code(await readFile(file, 'utf8')).replace(/"(?:[^"\\\n]|\\.)*"/g, '""');
    for (const match of text.matchAll(pattern))
      found.push(`${path.relative(guiDir, file).replaceAll('\\', '/')}: ${match[1]}`);
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
  assert.match(ui, /m_files = new wxDataViewListCtrl\(/);
  assert.match(ui, /m_agenda = new wxDataViewListCtrl\(/);
  assert.match(ui, /md3_style_data_view\(m_files\);/);
  assert.match(ui, /md3_style_data_view\(m_agenda\);/);
  assert.match(ui, /m_checklist = new ListBox\(list_page, wxID_ANY\);\s*m_checklist->EnableChecks\(\);/);
  assert.match(ui, /m_month = new wxGenericCalendarCtrl\([^;]*wxCAL_SEQUENTIAL_MONTH_SELECTION/);
  assert.match(ui, /m_month->SetHighlightColours\(StateColor::semantic\(MD3::Role::OnPrimary\), StateColor::semantic\(MD3::Role::Primary\)\);/);
  // The agenda keeps each row's slot index for the actions.
  assert.match(panel, /m_agenda->AppendItem\(row, static_cast<wxUIntPtr>\(position\)\);/);
  assert.equal((panel.match(/m_agenda->GetItemData\(m_agenda->RowToItem\(row\)\)/g) || []).length, 3);
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
