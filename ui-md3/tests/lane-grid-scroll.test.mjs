import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Parameter Table grid, the two search popups and the two cell editors of
// the table. Each check pins one of the conversions to the kit scrollbar
// (MD3ScrollBars) or to the kit combo box, and the two scrolled surfaces must
// not fall back to a Windows bar or to the legacy hand-drawn one.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const read = (...parts) => readFile(path.join(gui, ...parts), 'utf8');
// Code only: comments and the contents of string literals removed.
const code = (text) => text.replace(/\r\n/g, '\n')
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/\/\/.*$/gm, '')
  .replace(/"(?:[^"\\\n]|\\.)*"/g, '""');
const count = (text, pattern) => (text.match(pattern) ?? []).length;

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp)$/.test(entry.name)) out.push(full);
  }
  return out;
}

// The text of one class body: from its head to the closing brace at column 0.
function classBody(source, name, base) {
  const head = new RegExp(`class\\s+${name}\\s*:\\s*public\\s+${base}\\b`).exec(source);
  assert.ok(head, `class ${name} : public ${base}`);
  const end = source.indexOf('\n};', head.index);
  assert.notEqual(end, -1, `${name} has a closing brace`);
  return source.slice(head.index, end);
}

// The text of one function body: from its signature to the closing brace at column 0.
function fn(source, signature) {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, signature);
  const end = source.indexOf('\n}\n', start);
  assert.notEqual(end, -1, `${signature} has a closing brace`);
  return source.slice(start, end + 2);
}

test('MD3Grid routes every native scrollbar call of a wxGrid to MD3ScrollBars', async () => {
  const header = code(await read('Widgets', 'MD3Grid.hpp'));
  const source = code(await read('Widgets', 'MD3Grid.cpp'));
  assert.match(header, /class MD3Grid : public wxGrid\b/, 'the kit grid is a wxGrid');
  // Include lines are read from the raw text: code() blanks string literals.
  const rawHeader = await read('Widgets', 'MD3Grid.hpp');
  assert.match(rawHeader, /#include <wx\/grid\.h>/);
  assert.match(rawHeader, /#include "MD3ScrollBars\.hpp"/);
  assert.match(header, /MD3ScrollBars m_bars \{ this \};/, 'it owns the strips');
  assert.match(header, /bool IsBarShown\(int orient\) const \{ return m_bars\.IsShown\(orient\); \}/, 'the layout probe can ask which bars show');
  for (const name of ['SetScrollbar', 'SetScrollPos', 'GetScrollPos', 'GetScrollThumb', 'GetScrollRange', 'MSWGetStyle', 'MSWWindowProc']) {
    assert.match(header, new RegExp(`\\b${name}\\([^;]*\\)[^;]*override;`), `MD3Grid.hpp overrides ${name}`);
    assert.match(source, new RegExp(`MD3Grid::${name}\\(`), `MD3Grid.cpp defines ${name}`);
  }
  assert.doesNotMatch(source, /wxGrid::(?:SetScrollbar|SetScrollPos)\(/, 'nothing reaches the native scrollbar');
  assert.match(source, /m_bars\.Before\(msg, wParam, lParam, result\)/, 'every message is offered to the strips first');
  assert.match(source, /m_bars\.After\(msg, wParam, lParam, result\)/, 'the strips finish every message');
  assert.match(source, /MD3ScrollBars::WithoutNativeBars\(wxGrid::MSWGetStyle\(flags, exstyle\)\)/, 'no WS_VSCROLL or WS_HSCROLL');
  assert.match(source, /result = wxGrid::MSWWindowProc\(msg, wParam, lParam\);/, 'the grid keeps its own window procedure');
  assert.match(source, /m_bars\.Abandon\(\)/, 'a drag cut short by destruction ends quietly');
  assert.match(source, /m_bars\.SetScrollbar\(orient, pos, thumbVisible, range, refresh\)/);
  assert.match(source, /m_bars\.SetScrollPos\(orient, pos, refresh\)/);
  // The window has to be created from the class's own constructor body: a base
  // constructor creates it before the overrides above exist.
  assert.doesNotMatch(source, /MD3Grid::MD3Grid\([^)]*\)\s*:\s*wxGrid\(/, 'the window is not created through a wxGrid constructor');
  assert.doesNotMatch(source, /:\s*wxGrid\(/, 'no wxGrid initialiser anywhere in MD3Grid.cpp');
  assert.match(source, /MD3Grid::MD3Grid\(wxWindow \*parent[^)]*\)\s*\{[\s\S]*?Create\(parent, id, pos, size, style, name\);/,
    'from its own constructor body');
  assert.match(source, /return wxGrid::Create\(parent, id, pos, size, style, name\);/);
});

test('the only wxGrid in the GUI is the Parameter Table grid, and it is an MD3Grid', async () => {
  const offenders = [];
  for (const file of await sources(gui)) {
    const rel = path.relative(gui, file).replaceAll('\\', '/');
    if (rel.startsWith('Widgets/MD3Grid.')) continue;
    const text = code(await readFile(file, 'utf8'));
    const hits = text.match(/\bnew\s+wxGrid\s*\(|\bpublic\s+wxGrid\b|:\s*wxGrid\s*\(/g) ?? [];
    if (hits.length) offenders.push(`${rel} (${hits.length})`);
  }
  assert.deepEqual(offenders, [], 'derive from MD3Grid, whose bars are the kit scrollbar');

  const header = code(await read('GUI_ObjectTable.hpp'));
  assert.match(await read('GUI_ObjectTable.hpp'), /#include "Widgets\/MD3Grid\.hpp"/);
  assert.match(header, /class ObjectGrid : public MD3Grid\b/);
  assert.match(header, /:\s*MD3Grid\(parent, id, pos, size, style, name\)/, 'ObjectGrid hands its arguments to MD3Grid');
  const table = code(await read('GUI_ObjectTable.cpp'));
  assert.match(table, /m_object_grid = new ObjectGrid\(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS\);/);
  // wx's event table names its base class and still resolves through MD3Grid.
  assert.match(table, /wxBEGIN_EVENT_TABLE\(\s*ObjectGrid, wxGrid\s*\)/);
});

test('selected cells and the current cell of the Parameter Table use Material colours', async () => {
  const table = code(await read('GUI_ObjectTable.cpp'));
  assert.match(table, /m_object_grid->SetSelectionBackground\(StateColor::semantic\(MD3::Role::SecondaryContainer\)\);/);
  assert.match(table, /m_object_grid->SetSelectionForeground\(StateColor::semantic\(MD3::Role::OnSecondaryContainer\)\);/);
  assert.match(table, /m_object_grid->SetCellHighlightColour\(StateColor::semantic\(MD3::Role::Primary\)\);/);
  assert.match(table, /m_object_grid->SetCellHighlightPenWidth\(FromDIP\(1\)\);/, 'a 1 DIP current-cell rectangle');
});

test('the Parameter Table grid is built and the layout probe reports its bars', async () => {
  const cmake = await readFile(path.join(repoDir, 'src', 'slic3r', 'CMakeLists.txt'), 'utf8');
  for (const file of ['MD3Grid.cpp', 'MD3Grid.hpp'])
    assert.ok(cmake.includes(`GUI/Widgets/${file}`), `${file} is part of libslic3r_gui`);
  const probe = await read('LayoutProbe.cpp');
  assert.ok(probe.includes('#include "Widgets/MD3Grid.hpp"'), 'the probe sees the class');
  assert.ok(probe.includes('dynamic_cast<const MD3Grid *>(w)'), 'the probe reports the kit bars of MD3Grid');
  assert.match(code(probe), /else if \(const auto \*grid = dynamic_cast<const MD3Grid \*>\(w\)\) \{\s*kit_v = grid->IsBarShown\(wxVERTICAL\);\s*kit_h = grid->IsBarShown\(wxHORIZONTAL\);\s*\}/);
});

test('both choice editors of the Parameter Table reset and read through the kit combo box', async () => {
  // The untouched wxGridCellChoiceEditor::Reset() and GetValue() cast the
  // control to a wxComboBox, which the kit ComboBox is not; Escape in a cell
  // calls Reset(). Both editors override them to go through their own Combo().
  const header = code(await read('GUI_ObjectTable.hpp'));
  const source = code(await read('GUI_ObjectTable.cpp'));
  for (const name of ['GridCellFilamentsEditor', 'GridCellChoiceEditor']) {
    const body = classBody(header, name, 'wxGridCellChoiceEditor');
    assert.match(body, /virtual\s+void\s+Reset\(\)\s+wxOVERRIDE;/, `${name} overrides Reset()`);
    assert.match(body, /virtual\s+wxString\s+GetValue\(\)\s+const\s+wxOVERRIDE;/, `${name} overrides GetValue()`);
    assert.match(body, /::ComboBox \*Combo\(\) const \{ return \(::ComboBox \*\) ?m_control; \}/, `${name} keeps the kit Combo()`);

    const reset = fn(source, `void ${name}::Reset()`);
    assert.match(reset, /Combo\(\)->FindString\(m_value\)/, `${name}::Reset() finds the stored value in the kit combo box`);
    assert.match(reset, /Combo\(\)->SetSelection\(/, `${name}::Reset() selects it in the kit combo box`);
    assert.doesNotMatch(reset, /wxComboBox/, `${name}::Reset() never treats the control as a wxComboBox`);
    const value = fn(source, `wxString ${name}::GetValue() const`);
    assert.match(value, /return Combo\(\)->GetValue\(\);/, `${name}::GetValue() reads the kit combo box`);
  }
  assert.doesNotMatch(source, /\(wxComboBox\s*\*\)/, 'nothing in the table casts a control to wxComboBox');
  // BeginEdit and EndEdit already went through the kit combo box and stay as they were.
  assert.match(fn(source, 'void GridCellFilamentsEditor::BeginEdit('), /Combo\(\)->SetFocus\(\);/);
  assert.match(fn(source, 'bool GridCellFilamentsEditor::EndEdit('), /const wxString value = Combo\(\)->GetValue\(\);/);
  assert.match(fn(source, 'void GridCellChoiceEditor::BeginEdit('), /Combo\(\)->SetFocus\(\);/);
  assert.match(fn(source, 'bool GridCellChoiceEditor::EndEdit('), /const wxString value = Combo\(\)->GetValue\(\);/);
});

test('the two search popups scroll in an MD3ScrolledWindow, not in the legacy hand-drawn one', async () => {
  const header = code(await read('Search.hpp'));
  const source = code(await read('Search.cpp'));
  for (const [name, text] of [['Search.hpp', header], ['Search.cpp', source]]) {
    assert.doesNotMatch(text, /\bnew\s+ScrolledWindow\s*\(/, `${name} builds no legacy ScrolledWindow`);
    assert.doesNotMatch(text, /(?<![\w])ScrolledWindow\s*\*/, `${name} holds no legacy ScrolledWindow pointer`);
  }
  assert.match(await read('Search.hpp'), /#include "Widgets\/MD3ScrolledWindow\.hpp"/);
  assert.equal(count(header, /MD3ScrolledWindow\s*\*\s*m_scrolledWindow\b/g), 2, 'both dialogs hold an MD3ScrolledWindow');
  assert.equal(count(source, /m_scrolledWindow = new MD3ScrolledWindow\(m_client_panel, wxID_ANY, wxDefaultPosition, wxSize\([^;]*\), wxVSCROLL\);/g), 4,
    'the constructor and update_list() of each dialog build one, with no margin or bar width arguments');
  // The legacy class's own API is gone from every call site.
  assert.doesNotMatch(source, /GetPanel\(\)|SetMarginColor\(|SetScrollbarColor\(/);
  assert.equal(count(source, /new wxWindow\(m_scrolledWindow, -1\)/g), 4, 'the list panel is a direct child of the scrolled window');
  // The strip is reserved when the list panel is sized, as RegexBuilderPopup does.
  assert.equal(count(source, /m_listPanel->SetSize\(wxSize\(m_scrolledWindow->GetSize\(\)\.GetWidth\(\) - MD3ScrolledWindow::BarThickness\(m_scrolledWindow\), -1\)\);/g), 4,
    'each list panel leaves room for the kit bar');
  assert.equal(count(source, /m_scrolledWindow->SetScrollbars\(1, 1, 0, m_listPanel->GetSize\(\)\.GetHeight\(\)\);/g), 4);
});
