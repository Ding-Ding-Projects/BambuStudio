import assert from 'node:assert/strict';
import { readdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// wxMessageBox and wxMessageDialog open the system's own message box: its look and
// its buttons follow Windows, not the theme or the language modes. The same goes for
// a raw Win32 MessageBox, MessageBoxA or MessageBoxW call, which the scan below
// counts too. Every message box of the app is the Material MessageDialog, through
// md3_message_box() where a call site used wxMessageBox's arguments and return
// values. The only system boxes left are the ones that must work before the
// Material layer does.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');
const strip = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '').replace(/\/\/.*$/gm, '');

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}

// Why each remaining system box stays: it fires before or outside the Material layer.
const ALLOWED = {
  'GUI_Init.cpp': { count: 2, why: 'GUI initialisation failed: no app, fonts or theme yet' },
  'GUI_App.cpp': { count: 3, why: 'fatal and critical exceptions, and the first language load before any window exists' },
  'MsgDialog.hpp': { count: 4, why: 'the non-Material wrappers in the disabled #else branch' },
  'StatusPanel.cpp': { count: 2, why: 'the macOS branch, where MessageDialog can block' },
};

test('no system message box outside the few that must precede the Material layer', async () => {
  const found = {};
  for (const file of await sources(guiDir)) {
    const text = strip(await readFile(file, 'utf8'));
    // A raw Win32 call is found by the same scan: MessageBox(, MessageBoxA( and
    // MessageBoxW(, with or without the :: prefix. wxMessageBox( has no word boundary
    // before its MessageBox, and the `#define MessageBox MessageBoxA` line in
    // GLCanvas3D.cpp has no opening bracket, so neither counts twice.
    const hits = (text.match(/\bwxMessageBox\s*\(|\bwxMessageDialog\b|\bwxRichMessageDialog\b|\bMessageBox[AW]?\s*\(/g) || []).length;
    if (hits) found[path.relative(guiDir, file).replace(/\\/g, '/')] = hits;
  }
  const unexpected = Object.entries(found).filter(([file, hits]) => !ALLOWED[file] || hits > ALLOWED[file].count);
  assert.deepEqual(unexpected, [], 'system message boxes left');
  const app = strip(await readFile(path.join(guiDir, 'GUI_App.cpp'), 'utf8'));
  const native = app.split('\n').filter((line) => /\bwxMessageBox\s*\(/.test(line)).join('\n');
  for (const caption of ['_L("Fatal error")', '_L("Critical error")', '_L("Switching language failed")']) {
    assert.ok(native.includes(caption), `GUI_App keeps the system box only for ${caption}`);
  }
});

test('md3_message_box keeps wxMessageBox\'s arguments and return values', async () => {
  const hpp = strip(await readFile(path.join(guiDir, 'MsgDialog.hpp'), 'utf8'));
  assert.match(hpp, /int md3_message_box\(const wxString &message, const wxString &caption = wxEmptyString,\s*long style = wxOK \| wxCENTRE, wxWindow \*parent = nullptr\);/);
  const cpp = strip(await readFile(path.join(guiDir, 'MsgDialog.cpp'), 'utf8'));
  const body = cpp.slice(cpp.indexOf('int md3_message_box('), cpp.indexOf('InfoDialog::InfoDialog('));
  assert.match(body, /MessageDialog dialog\(parent, message, caption, style\);/);
  for (const [id, value] of [['wxID_YES', 'wxYES'], ['wxID_NO', 'wxNO'], ['wxID_OK', 'wxOK']]) {
    assert.ok(body.includes(`case ${id}: return ${value};`), `${id} answers ${value}, as wxMessageBox did`);
  }
  assert.match(body, /default: return wxCANCEL;/);
});

test('the converted call sites use the Material box', async () => {
  const workspace = strip(await readFile(path.join(guiDir, 'WorkspacePanel.cpp'), 'utf8'));
  assert.ok((workspace.match(/\bmd3_message_box\(/g) || []).length >= 14, 'the workspace panel\'s notices and questions');
  const appearance = strip(await readFile(path.join(guiDir, 'Appearance', 'AppearanceEditorPopover.cpp'), 'utf8'));
  assert.equal((appearance.match(/\bmd3_message_box\(/g) || []).length, 4);
  assert.equal((appearance.match(/\bMessageDialog ask\(/g) || []).length, 2, 'reset all and delete preset ask through the Material dialog');
  const map = strip(await readFile(path.join(guiDir, 'FilamentMapPanel.cpp'), 'utf8'));
  assert.match(map, /md3_message_box\(_L\("The destination nozzle/);
});
