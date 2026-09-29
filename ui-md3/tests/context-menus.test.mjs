import assert from 'node:assert/strict';
import { readdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Every context menu of the Windows app is the Material menu (MD3::PopupMenu):
// it follows the theme, the density and the three language modes. The native
// ones did not: a text field's Undo / Cut / Copy / Paste menu came from the
// system, a copyable device label opened a stock wx menu, and a web page could
// open the browser's own menu. These contracts keep every one of them Material.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');
const stripComments = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '').replace(/\/\/.*$/gm, '');
const read = async (...parts) => stripComments(await readFile(path.join(guiDir, ...parts), 'utf8'));

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}
const all = await Promise.all((await sources(guiDir)).map(async (file) => ({
  file: path.relative(guiDir, file).replace(/\\/g, '/'),
  text: stripComments(await readFile(file, 'utf8')),
})));

test('every popup menu goes through the Material menu', () => {
  // Plater::PopupMenu is itself a wrapper around MD3::PopupMenu (checked below),
  // so calls through the plater are Material too; anything else is wxWindow::PopupMenu.
  const allowed = /(?:MD3::|plater->|plater\(\)->|q->|this->|Plater::|bool\s+)$/;
  const offenders = [];
  for (const { file, text } of all) {
    for (const match of text.matchAll(/(?<![A-Za-z_])PopupMenu\s*\(/g)) {
      const before = text.slice(Math.max(0, match.index - 40), match.index);
      if (!allowed.test(before)) offenders.push(`${file}: ...${before.trim().slice(-30)}PopupMenu(`);
    }
    if (/GetPopupMenuSelectionFromUser\s*\(/.test(text)) offenders.push(`${file}: GetPopupMenuSelectionFromUser`);
  }
  assert.deepEqual(offenders, [], 'native popup menus left');
  const plater = all.find((s) => s.file === 'Plater.cpp').text;
  const wrapper = plater.slice(plater.indexOf('bool Plater::PopupMenu(wxMenu *menu, const wxPoint& pos)'));
  assert.match(wrapper.slice(0, wrapper.indexOf('\n}\n')), /MD3::PopupMenu\(/, 'the plater\'s menus are Material');
});

test('every text entry gets the Material edit menu, from the mouse and the keyboard', async () => {
  const menu = await read('Widgets', 'MD3Menu.cpp');
  const filter = menu.slice(menu.indexOf('class TextContextMenus'), menu.indexOf('std::unique_ptr<TextContextMenus> &text_context_menus()'));
  assert.match(filter, /if \(event\.GetEventType\(\) != wxEVT_CONTEXT_MENU\)\s*return Event_Skip;/);
  assert.match(filter, /if \(text_entry_of\(window\) == nullptr\)\s*return Event_Skip;/);
  assert.match(filter, /window->CallAfter\(\[ref, at\]\(\) \{\s*if \(ref\)\s*show_text_menu\(ref\.get\(\), at\);\s*\}\);\s*return Event_Processed;/,
    'handled in the filter, so the native menu never opens');
  const entry = menu.slice(menu.indexOf('wxTextEntryBase *text_entry_of(wxWindow *window)'), menu.indexOf('bool is_masked(wxWindow *window)'));
  assert.match(entry, /return dynamic_cast<wxTextEntryBase \*>\(window\);/, 'wxTextCtrl, combo boxes, search and rich text controls');
  assert.match(entry, /combo->HasFlag\(wxCB_READONLY\)/, 'a read-only combo box is a list, not a field');
  const show = menu.slice(menu.indexOf('void show_text_menu(wxWindow *window, wxPoint screen_pos)'), menu.indexOf('class TextContextMenus'));
  for (const [id, label] of [['wxID_UNDO', 'Undo'], ['wxID_CUT', 'Cut'], ['wxID_COPY', 'Copy'], ['wxID_PASTE', 'Paste'], ['wxID_DELETE', 'Delete'], ['wxID_SELECTALL', 'Select all']]) {
    assert.ok(show.includes(`menu.Append(${id}, _L("${label}"))`), `${label} is a catalogue string, so all three modes translate it`);
  }
  assert.match(show, /menu\.Append\(wxID_CUT, _L\("Cut"\)\)->Enable\(editable && !masked && entry->CanCut\(\)\);/);
  assert.match(show, /menu\.Append\(wxID_COPY, _L\("Copy"\)\)->Enable\(!masked && entry->CanCopy\(\)\);/, 'a password never goes to the clipboard');
  assert.match(show, /if \(screen_pos == wxDefaultPosition\)\s*screen_pos = window->ClientToScreen/, 'the Menu key and Shift+F10 open it under the field');
  assert.match(show, /MD3::PopupMenu\(window, &menu, screen_pos\);/, 'sent events, so "Edit appearance..." keeps working');
  const masked = menu.slice(menu.indexOf('bool is_masked(wxWindow *window)'), menu.indexOf('void show_text_menu('));
  assert.match(masked, /window->HasFlag\(wxTE_PASSWORD\)/);
  assert.match(masked, /::SendMessage\(hwnd, EM_GETPASSWORDCHAR, 0, 0\) != 0/, 'a field masked after creation (the Smart home token)');

  const app = await read('GUI_App.cpp');
  assert.match(app, /wxInitAllImageHandlers\(\);\s*MD3::EnableTextContextMenus\(true\);/, 'installed before any window exists');
  assert.match(app, /I18N::enable_bilingual_decorator\(false\);\s*MD3::EnableTextContextMenus\(false\);/, 'removed while the event loop exists');
});

test('the kit fields let a right-click reach that menu', async () => {
  for (const [file, control] of [['TextInput.cpp', 'text_ctrl'], ['SpinInput.cpp', 'text_ctrl'], ['TempInput.cpp', 'text_ctrl'], ['SearchField.cpp', 'm_text']]) {
    const text = await read('Widgets', file);
    assert.doesNotMatch(text, new RegExp(`${control}->Bind\\(wxEVT_RIGHT_DOWN, \\[[^\\]]*\\]\\([^)]*\\) \\{\\}\\)`), `${file} swallows the right-click`);
  }
});

test('a copyable label opens the Material menu', async () => {
  const ext = await read('wxExtensions.cpp');
  const copy = ext.slice(ext.indexOf('void enable_static_text_copy_menu(wxStaticText* label)'));
  assert.match(copy.slice(0, copy.indexOf('\n}\n')), /MD3::PopupMenu\(label, &menu, label->ClientToScreen\(evt\.GetPosition\(\)\)\);/);
});

test('no web page opens the browser\'s own menu in a public build', () => {
  for (const { file, text } of all) {
    for (const match of text.matchAll(/(\w+)\s*=\s*wxWebView::New\(/g)) {
      const rest = text.slice(match.index);
      const fnEnd = rest.indexOf('\n}\n');
      const body = rest.slice(0, fnEnd < 0 ? rest.length : fnEnd);
      assert.match(body, new RegExp(`${match[1]}->EnableContextMenu\\((?:false|enable_devtools)\\)`), `${file}: ${match[1]} keeps the browser menu`);
    }
    for (const match of text.matchAll(/EnableContextMenu\(true\)/g)) {
      const before = text.slice(0, match.index);
      const opened = before.lastIndexOf('#if !BBL_RELEASE_TO_PUBLIC');
      const closed = before.lastIndexOf('#endif');
      assert.ok(opened > closed, `${file}: the browser menu is switched on outside the internal-build block`);
    }
  }
});

test('the What\'s new year field is the kit spin field', async () => {
  const changelog = await read('ChangelogDialog.cpp');
  assert.doesNotMatch(changelog, /new wxSpinCtrl\(/, 'the native spin control drew a system box and opened the system edit menu');
  assert.match(changelog, /m_year_spin = new SpinInput\(this, /);
  assert.match(changelog, /m_year_spin->Bind\(wxEVT_SPINCTRL, \[this\]\(wxCommandEvent &\)/, 'SpinInput sends a plain command event');
});
