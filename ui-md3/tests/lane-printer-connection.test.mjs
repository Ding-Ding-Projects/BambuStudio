import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The printer connection surfaces and the network lookup list, on the kit:
//   - the Physical Printer dialog's four labelled buttons (Browse, Test,
//     Refresh Printers, and the CA file Browse) were bordered ScalableButtons that
//     drew the Windows push button face inside a dialog of kit pills;
//   - the Printer settings page's "Set ..." button next to the bed shape was the
//     same native face;
//   - the Network lookup dialog listed its results in a native report list
//     (SysListView32) with the Windows header strip, selection and scrollbar.
// Each assertion below was run against the old sources first and failed there.

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
// The text from a lambda's introducer to the line that closes it.
const lambda = (source, introducer) => {
  const start = source.indexOf(introducer);
  assert.notEqual(start, -1, introducer);
  return source.slice(start, source.indexOf('\n    };\n', start) + 7);
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

test('no native report list is constructed anywhere in the GUI', async () => {
  const found = [];
  for (const file of await sources(guiDir)) {
    const text = code(await readFile(file, 'utf8')).replace(/"(?:[^"\\\n]|\\.)*"/g, '""');
    const name = path.relative(guiDir, file).replaceAll('\\', '/');
    if (/\bwxListView\b/.test(text)) found.push(`${name}: wxListView`);
    if (/new\s+wxListCtrl\s*\(/.test(text)) found.push(`${name}: new wxListCtrl(`);
  }
  assert.deepEqual(found, []);
});

test('the Network lookup results are a kit table', async () => {
  const header = await read('BonjourDialog.hpp');
  const source = await read('BonjourDialog.cpp');
  // Only this dialog is pinned to the include: other files keep <wx/listctrl.h> for
  // their own reasons (the bilingual decorator walks stock lists, for one).
  assert.doesNotMatch(header, /wxListView|wx\/listctrl\.h/);
  assert.doesNotMatch(source, /wx\/listctrl\.h/);
  assert.match(header, /^class MD3DataViewListCtrl;/m, 'the kit table is forward declared at global scope');
  assert.match(header, /^\s*MD3DataViewListCtrl\s*\*list;/m);
  assert.match(source, /#include "Widgets\/MD3DataView\.hpp"/);
  assert.match(source, /#include "wxExtensions\.hpp"/);

  const ctor = source.slice(source.indexOf('BonjourDialog::BonjourDialog('), source.indexOf('BonjourDialog::~BonjourDialog()'));
  assert.match(ctor, /list\(new MD3DataViewListCtrl\(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_SINGLE \| wxBORDER_NONE\)\)/);
  assert.match(ctor, /GUI::wxGetApp\(\)\.UpdateDVCDarkUI\(list\);/, 'the header follows the theme');
  assert.match(ctor, /md3_style_data_view\(list\);/);
  assert.match(ctor, /list->SetMinSize\(wxSize\(80 \* em, 30 \* em\)\);/, 'a table asks for almost no height of its own');
  // The columns, hand-written: the OctoPrint version column only exists for FFF.
  for (const title of ['Address', 'Hostname', 'Service name'])
    assert.match(ctor, new RegExp(`add_column\\(_\\(L\\("${title}"\\)\\)`), `${title} column`);
  assert.match(ctor, /if \(tech == ptFFF\) \{\s*add_column\(_\(L\("OctoPrint version"\)\)/);
  // The table must be styled after its columns exist, as the other kit tables are.
  assert.match(ctor, /list->AppendTextColumn\(title, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE\)/, 'a column fits its content, as the native list did');
  assert.ok(ctor.indexOf('md3_style_data_view(list);') > ctor.lastIndexOf('add_column('));
  // Nothing of the native list's API may survive.
  for (const gone of ['SetSingleStyle', 'wxLC_', 'AppendColumn(', 'GetFirstSelected', 'FindItem', 'SetColumnWidth', 'GetItemText', 'SetItemState', 'SetItem('])
    assert.ok(!source.includes(gone), `${gone} is part of the native list's API`);
});

test('the Network lookup table still reports, selects and restores the selection', async () => {
  const source = await read('BonjourDialog.cpp');
  assert.match(fn(source, 'bool BonjourDialog::show_and_lookup()'), /ShowModal\(\) == wxID_OK && list->GetSelectedRow\(\) != wxNOT_FOUND;/);
  const selected = fn(source, 'wxString BonjourDialog::get_selected() const');
  assert.match(selected, /list->GetSelectedRow\(\)/);
  assert.match(selected, /list->GetTextValue\(row, 0\)/, 'the address is the first column');
  const reply = fn(source, 'void BonjourDialog::on_reply(BonjourReplyEvent &e)');
  assert.match(reply, /wxWindowUpdateLocker freeze_guard\(this\);/);
  assert.match(reply, /list->DeleteAllItems\(\);/);
  assert.match(reply, /wxVector<wxVariant> row;/);
  assert.match(reply, /list->InsertItem\(0, row\);/, 'rows are still inserted at the top, so the order stays descending');
  assert.match(reply, /if \(tech == ptFFF\) \{[^}]*row\.push_back\(/, 'the version cell is only built for FFF, matching its column');
  assert.match(reply, /list->GetTextValue\(r, 0\) == selected/);
  assert.match(reply, /list->SelectRow\(r\);/);
});

test('the Physical Printer dialog buttons are kit outlined buttons', async () => {
  const header = await read('PhysicalPrinterDialog.hpp');
  const source = await read('PhysicalPrinterDialog.cpp');
  assert.doesNotMatch(header, /ScalableButton/);
  assert.doesNotMatch(source, /ScalableButton/);
  for (const member of ['m_printhost_browse_btn', 'm_printhost_test_btn', 'm_printhost_cafile_browse_btn', 'm_printhost_client_cert_browse_btn', 'm_printhost_port_browse_btn'])
    assert.match(header, new RegExp(`^\\s*Button\\*\\s+${member}\\s+\\{nullptr\\};`, 'm'), member);

  const make = lambda(source, 'auto create_sizer_with_btn = ');
  assert.match(make, /Button\*\* btn/);
  assert.match(make, /\*btn = new Button\(parent, label, wxString::FromUTF8\(icon_name\.c_str\(\)\), 0, 16\);/);
  assert.match(make, /\(\*btn\)->SetVariant\(Button::Variant::Outlined\);/);
  assert.match(make, /\(\*btn\)->SetButtonSize\(Button::Size::Small\);/);
  assert.doesNotMatch(make, /SetFont|wxBU_/, 'the variant supplies the font, and a later SetFont would override it');

  // The four buttons are all built through it, each with its raster icon.
  assert.match(source, /create_sizer_with_btn\(parent, &m_printhost_browse_btn, "printer_host_browser", /);
  assert.match(source, /create_sizer_with_btn\(parent, &m_printhost_test_btn, "printer_host_test", /);
  assert.match(source, /create_sizer_with_btn\(parent, &m_printhost_port_browse_btn, "monitor_signal_strong", /);
  assert.match(source, /create_sizer_with_btn\(parent, &m_printhost_cafile_browse_btn, "monitor_signal_strong", /);
  // The Refresh Printers button takes no font of its own either.
  const refresh = lambda(source, 'auto print_host_printers = ');
  assert.match(refresh, /Button\* btn = m_printhost_port_browse_btn;/);
  assert.doesNotMatch(refresh, /SetFont/);
  // Every handler still reaches the control a person uses.
  assert.equal((source.match(/_btn->Bind\(wxEVT_BUTTON,/g) || []).length, 3, 'Browse, Test and the CA file Browse keep their handlers');
  assert.match(refresh, /btn->Bind\(wxEVT_BUTTON, /);
  assert.match(source, /m_printhost_test_btn->Enable\(/);
  assert.match(source, /m_printhost_browse_btn->Enable\(host->has_auto_discovery\(\)\);/);
  assert.match(source, /m_printhost_port_browse_btn->Show\(supports_multiple_printers\);/);

  // The kit button rescales itself on a DPI change.
  const dpi = fn(source, 'void PhysicalPrinterDialog::on_dpi_changed(');
  assert.match(dpi, /m_printhost_browse_btn->Rescale\(\);/);
  assert.match(dpi, /m_printhost_test_btn->Rescale\(\);/);
  assert.match(dpi, /m_printhost_cafile_browse_btn->Rescale\(\);/);
  assert.match(dpi, /m_printhost_port_browse_btn->Rescale\(\);/);
  assert.doesNotMatch(dpi, /_btn->msw_rescale\(\)/);
});

test('the bed shape Set button is a kit outlined button', async () => {
  const tab = await read('Tab.cpp');
  const widget = fn(tab, 'wxSizer* TabPrinter::create_bed_shape_widget(wxWindow* parent)');
  assert.match(widget, /Button\* btn = new Button\(parent, _\(L\("Set"\)\) \+ " " \+ dots, "printer", 0, 16\);/);
  assert.match(widget, /btn->SetVariant\(Button::Variant::Outlined\);/);
  assert.match(widget, /btn->SetButtonSize\(Button::Size::Small\);/);
  assert.doesNotMatch(widget, /ScalableButton|wxBU_LEFT|wxBU_EXACTFIT/);
  assert.doesNotMatch(widget, /btn->SetFont|btn->SetSize|GetBestSize/, 'the kit button measures itself');
  assert.match(widget, /sizer->Add\(btn, 0, wxALIGN_CENTER_VERTICAL\);/);
  assert.match(widget, /btn->Bind\(wxEVT_BUTTON, /, 'the click still opens the bed shape dialog');
  assert.match(widget, /BedShapeDialog dlg\(this\);/);
});
