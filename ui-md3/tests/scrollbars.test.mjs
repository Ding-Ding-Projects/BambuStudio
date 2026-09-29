import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Windows draws its own scrollbar in every window created with WS_VSCROLL or
// WS_HSCROLL: a 17px grey strip with a thin grey thumb, in every theme. The kit
// scrollbar (design-system/tokens/base.css) is a 10px strip with no track fill
// and a rounded OutlineVariant thumb, Outline while hovered. MD3ScrollBars draws
// the kit one for every MD3ScrolledWindow and for the kit ListBox; these checks
// keep a Windows scrollbar from coming back through a new wxScrolledWindow.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const read = (...parts) => readFile(path.join(gui, ...parts), 'utf8');
// Code only: comments and the contents of string literals removed.
const code = (text) => text.replace(/\r\n/g, '\n')
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/\/\/.*$/gm, '')
  .replace(/"(?:[^"\\\n]|\\.)*"/g, '""');

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp)$/.test(entry.name)) out.push(full);
  }
  return out;
}

test('no GUI source builds a wxScrolledWindow of its own', async () => {
  const offenders = [];
  for (const file of await sources(gui)) {
    const rel = path.relative(gui, file).replaceAll('\\', '/');
    if (rel.startsWith('Widgets/MD3ScrolledWindow.')) continue;
    const text = code(await readFile(file, 'utf8'));
    const hits = text.match(/\bnew\s+wxScrolledWindow\s*\(|\bpublic\s+wxScrolledWindow\b|:\s*wxScrolledWindow\s*\(/g) ?? [];
    if (hits.length) offenders.push(`${rel} (${hits.length})`);
  }
  assert.deepEqual(offenders, [], 'build an MD3ScrolledWindow, whose bars are the kit scrollbar');
});

test('no GUI source builds a data view table of its own', async () => {
  const offenders = [];
  for (const file of await sources(gui)) {
    const rel = path.relative(gui, file).replaceAll('\\', '/');
    if (rel.startsWith('Widgets/MD3DataView.')) continue;
    const text = code(await readFile(file, 'utf8'));
    const hits = text.match(/\bnew\s+wxDataView(?:List)?Ctrl\s*\(|\bpublic\s+wxDataView(?:List)?Ctrl\b|:\s*wxDataView(?:List)?Ctrl\s*\(/g) ?? [];
    if (hits.length) offenders.push(`${rel} (${hits.length})`);
  }
  assert.deepEqual(offenders, [], 'build an MD3DataViewCtrl or MD3DataViewListCtrl, whose bars are the kit scrollbar');
  const expectations = [
    ['GUI_ObjectList.hpp', /class ObjectList : public MD3DataViewCtrl/],          // the Objects list
    ['GUI_AuxiliaryList.hpp', /class AuxiliaryList : public MD3DataViewCtrl/],
    ['UnsavedChangesDialog.hpp', /class DiffViewCtrl : public MD3DataViewCtrl/],
  ];
  for (const [file, pattern] of expectations)
    assert.match(code(await read(file)), pattern, file);
});

test('the scrolled surfaces people use most are MD3ScrolledWindow', async () => {
  const expectations = [
    ['Plater.cpp', /p->scrolled\s*=\s*new MD3ScrolledWindow\(/],                         // the Prepare sidebar
    ['Preferences.cpp', /class ScrollPanel : public MD3ScrolledWindow/],                  // every Preferences page
    ['ParamsPanel.cpp', /class PageScrolledWindow : public MD3ScrolledWindow/],           // every settings page
    ['StatusPanel.hpp', /class StatusBasePanel : public MD3ScrolledWindow/],              // the Device tab
    ['MsgDialog.cpp', /new MD3ScrolledWindow\(/],                                         // long message boxes
  ];
  for (const [file, pattern] of expectations)
    assert.match(code(await read(file)), pattern, file);
});

test('MD3ScrollBars keeps Windows from drawing a bar and draws the kit strip instead', async () => {
  const cpp = code(await read('Widgets', 'MD3ScrollBars.cpp'));
  assert.match(cpp, /constexpr int kBarDip = 10;/, 'the strip is 10px, as in base.css');
  assert.match(cpp, /constexpr int\s+kInsetDip\s*=\s*2;/, 'the thumb is inset 2px, as in base.css');
  assert.match(cpp, /style & ~static_cast<WXDWORD>\(WS_HSCROLL \| WS_VSCROLL\)/, 'the window never gets a native scrollbar style');
  assert.doesNotMatch(cpp, /SetScrollInfo|ShowScrollBar|EnableScrollBar|SetScrollRange/, 'nothing hands a bar to Windows');
  assert.match(cpp, /case WM_NCCALCSIZE:[\s\S]*?rc->right -= m_reserved_v;[\s\S]*?rc->bottom -= m_reserved_h;/,
    'each shown bar reserves its strip in the non-client area, where a native bar sits');
  assert.match(cpp, /case WM_NCPAINT:\s*paint\(\);/, 'the strips paint with the frame');
  assert.match(cpp, /MD3::Role::Outline : MD3::Role::OutlineVariant/, 'thumb: OutlineVariant at rest, Outline hovered or dragged');
  assert.match(cpp, /DrawRoundedRectangle\(thumb, std::min\(thumb\.width, thumb\.height\) \/ 2\.0\)/, 'a fully rounded thumb');
  assert.match(cpp, /SPI_GETHIGHCONTRAST/, 'high contrast keeps the system colours');
  for (const type of ['THUMBTRACK', 'THUMBRELEASE', 'PAGEUP', 'PAGEDOWN'])
    assert.match(cpp, new RegExp(`wxEVT_SCROLLWIN_${type}`), `the strip scrolls through wx's own ${type} event`);
  assert.match(cpp, /SWP_FRAMECHANGED/, 'showing or hiding a bar recalculates the frame, as a native bar does');
});

test('MD3ScrolledWindow, the kit ListBox and the MD3 tables route every native scrollbar call to MD3ScrollBars', async () => {
  const pairs = [
    ['MD3ScrolledWindow.hpp', 'MD3ScrolledWindow.cpp', 'wxScrolledWindow'],
    ['ListBox.hpp', 'ListBox.cpp', 'wxVListBox'],
    ['MD3DataView.hpp', 'MD3DataView.cpp', 'wxDataViewCtrl'],
    ['MD3DataView.hpp', 'MD3DataView.cpp', 'wxDataViewListCtrl'],
  ];
  for (const [hpp, cpp, base] of pairs) {
    const header = code(await read('Widgets', hpp));
    const source = code(await read('Widgets', cpp));
    for (const name of ['SetScrollbar', 'SetScrollPos', 'GetScrollPos', 'GetScrollThumb', 'GetScrollRange', 'MSWGetStyle', 'MSWWindowProc'])
      assert.match(header, new RegExp(`\\b${name}\\([^;]*\\)[^;]*override;`), `${hpp} overrides ${name}`);
    assert.doesNotMatch(source, new RegExp(`${base}::(?:SetScrollbar|SetScrollPos)\\(`), `${cpp} never reaches the native scrollbar`);
    assert.match(source, /m_bars\.Before\(msg, wParam, lParam, result\)/, `${cpp} offers every message to the strips first`);
    assert.match(source, /m_bars\.After\(msg, wParam, lParam, result\)/, `${cpp} lets the strips finish every message`);
    assert.match(source, /MD3ScrollBars::WithoutNativeBars\(/, `${cpp} drops the native scrollbar style`);
    assert.match(source, /m_bars\.Abandon\(\)/, `${cpp} ends a drag cut short by destruction`);
  }
  // wxDataViewCtrl's window procedure is private, so the tables call its base's
  // and add the one thing it adds: the arrow keys for the selection.
  const tables = code(await read('Widgets', 'MD3DataView.cpp'));
  assert.doesNotMatch(tables, /wxDataView(?:List)?Ctrl::MSWWindowProc\(/, 'the private wxDataViewCtrl::MSWWindowProc is never called');
  assert.equal((tables.match(/result = wxDataViewCtrlBase::MSWWindowProc\(msg, wParam, lParam\);\s*if \(msg == WM_GETDLGCODE\)\s*result \|= DLGC_WANTARROWS;/g) ?? []).length, 2,
    'both tables keep the arrow keys');
  // The window has to be created from the class's own constructor body: a base
  // constructor creates it before the overrides above exist.
  const list = code(await read('Widgets', 'ListBox.cpp'));
  assert.doesNotMatch(list, /ListBox::ListBox\([^)]*\)\s*:\s*wxVListBox\(/, 'ListBox creates its window through Create()');
  const scrolled = code(await read('Widgets', 'MD3ScrolledWindow.cpp'));
  assert.doesNotMatch(scrolled, /MD3ScrolledWindow::MD3ScrolledWindow\([^)]*\)\s*:\s*wxScrolledWindow\(/,
    'MD3ScrolledWindow creates its window through Create()');
});

test('code that sized a scrolled window for the Windows bar sizes it for the kit bar', async () => {
  for (const file of ['FilamentPickerDialog.cpp', 'TextureImportDialog.cpp', path.join('Widgets', 'RegexBuilderPopup.cpp')]) {
    const text = code(await read(file));
    assert.doesNotMatch(text, /wxSYS_VSCROLL_X/, `${file} still leaves room for the 17px Windows bar`);
    assert.match(text, /MD3ScrolledWindow::BarThickness\(/, `${file} leaves room for the kit bar`);
  }
  const objects = code(await read('GUI_ObjectList.cpp'));
  assert.doesNotMatch(objects, /wxSYS_VSCROLL_X/, 'the Objects list ink editor still leaves room for the Windows bar');
  assert.match(objects, /MD3ScrollBars::Thickness\(this\)/, 'the Objects list ink editor leaves room for the kit bar');
});

test('the scrollbar classes are built and the layout probe reports whose bars a window shows', async () => {
  const cmake = await readFile(path.join(repoDir, 'src', 'slic3r', 'CMakeLists.txt'), 'utf8');
  for (const file of ['MD3ScrollBars.cpp', 'MD3ScrollBars.hpp', 'MD3ScrolledWindow.cpp', 'MD3ScrolledWindow.hpp', 'MD3DataView.cpp', 'MD3DataView.hpp'])
    assert.ok(cmake.includes(`GUI/Widgets/${file}`), `${file} is part of libslic3r_gui`);
  const probe = await read('LayoutProbe.cpp');
  assert.ok(probe.includes('<< ",\\"scrollbars\\":" << scrollbars_json(w)'), 'every window record carries its scrollbars');
  for (const field of ['native_v', 'native_h', 'kit_v', 'kit_h'])
    assert.ok(probe.includes(`\\"${field}\\"`), `the scrollbars record names ${field}`);
});
