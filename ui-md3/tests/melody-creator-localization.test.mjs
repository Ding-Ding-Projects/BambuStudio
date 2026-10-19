import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Melody creator (File > Melody creator...) composes a tune the printer
// plays through M1006. These contracts pin its localized surface and its
// wiring without a build: the dialog source is extracted into the message
// catalogue, every one of its messages has English and Hong Kong Cantonese
// entries, the preset names it shows are translated, it reaches the printer
// through MachineObject::publish_gcode, and the File menu item (which the
// command palette indexes) opens it. The model itself runs in
// tests/melody_model_test.cpp.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (relative) => (await readFile(path.join(repoDir, relative), 'utf8')).replace(/\r\n/g, '\n');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

const DIALOG = 'src/slic3r/GUI/Melody/MelodyCreatorDialog.cpp';
const HEADER = 'src/slic3r/GUI/Melody/MelodyCreatorDialog.hpp';
const MODEL = 'src/slic3r/GUI/Melody/MelodyModel.hpp';

const unescape = (s) => s.replace(/\\(.)/g, (_, c) => ({ n: '\n', t: '\t' }[c] ?? c));

// Minimal PO reader: msgid -> msgstr for entries without a context.
function parsePo(text) {
  const entries = new Map();
  for (const entry of text.split(/\n{2,}/)) {
    const parts = { msgctxt: null, msgid: null, msgstr: null };
    let field = null;
    for (const line of entry.split('\n')) {
      const start = line.match(/^(msgctxt|msgid|msgstr) "(.*)"$/);
      const more = line.match(/^"(.*)"$/);
      if (start) { field = start[1]; parts[field] = unescape(start[2]); }
      else if (more && field) parts[field] += unescape(more[1]);
      else field = null;
    }
    if (parts.msgctxt === null && parts.msgid && parts.msgstr !== null) entries.set(parts.msgid, parts.msgstr);
  }
  return entries;
}

// Every _L("...") source string of a file, with adjacent string literals joined.
function catalogueSources(source) {
  const sources = new Set();
  const re = /_L\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)/g;
  for (const match of source.matchAll(re)) {
    const joined = match[1].match(/"(?:[^"\\]|\\.)*"/g).map((lit) => unescape(lit.slice(1, -1))).join('');
    sources.add(joined);
  }
  return sources;
}

test('the melody creator sources are extracted into the message catalogue', async () => {
  const list = (await read('bbl/i18n/list.txt')).split('\n').map((line) => line.trim());
  assert.ok(list.includes(DIALOG), `${DIALOG} must be listed in bbl/i18n/list.txt`);
  const cmake = await read('src/slic3r/CMakeLists.txt');
  for (const file of ['GUI/Melody/MelodyModel.hpp', 'GUI/Melody/MelodyCreatorDialog.cpp', 'GUI/Melody/MelodyCreatorDialog.hpp'])
    assert.ok(cmake.includes(`    ${file}\n`), `${file} must be built`);
});

test('every melody creator message has English and Hong Kong Cantonese entries', async () => {
  const sources = catalogueSources(stripComments(await read(DIALOG)));
  assert.ok(sources.size >= 60, `the dialog has many messages (${sources.size} found)`);
  const pot = parsePo(await read('bbl/i18n/BambuStudio.pot'));
  const cantonese = parsePo(await read('bbl/i18n/yue_HK/BambuStudio_yue_HK.po'));
  const missingPot = [...sources].filter((s) => !pot.has(s));
  const missingYue = [...sources].filter((s) => !cantonese.get(s));
  assert.deepEqual(missingPot, [], 'every message must be extracted into BambuStudio.pot');
  assert.deepEqual(missingYue, [], 'every message needs a Cantonese entry');
  // Placeholders survive translation exactly.
  for (const s of sources) {
    const holders = (s.match(/%[.\d]*[sdf]/g) ?? []).sort();
    const translated = (cantonese.get(s).match(/%[.\d]*[sdf]/g) ?? []).sort();
    assert.deepEqual(translated, holders, `placeholders of "${s.slice(0, 40)}" must match`);
  }
});

test('every preset the model ships is shown with a translated name', async () => {
  const model = stripComments(await read(MODEL));
  const dialog = stripComments(await read(DIALOG));
  const presets = [...model.matchAll(/\{"([a-z0-9_]+)", "([^"]+)",/g)];
  assert.ok(presets.length >= 7, 'the model ships the stock tunes and a scale');
  for (const [, id, name] of presets) {
    assert.ok(dialog.includes(`if (id == "${id}") return _L("${name}");`), `preset ${id} needs its translated title in preset_title()`);
  }
});

test('the dialog plays through the printer gcode route, guards an active print, and is reachable from the File menu', async () => {
  const dialog = stripComments(await read(DIALOG));
  assert.match(dialog, /if \(obj->is_in_printing\(\)\) \{ update_status\(_L\("The printer is printing\. Play the melody when it is idle\."\), true\); return; \}/);
  assert.match(dialog, /obj->publish_gcode\(melody_to_gcode\(m_melody\)\)/);
  assert.match(dialog, /Tab::validate_custom_gcode\(field_name, gcode\)/);
  assert.match(dialog, /wxGetApp\(\)\.get_tab\(Preset::TYPE_PRINTER\)/);
  // The MD3 scrolled body and the kit dialog shell are included directly (dialog-header-includes rule).
  assert.match(dialog, /#include "\.\.\/Widgets\/MD3ScrolledWindow\.hpp"/);
  assert.match(await read(HEADER), /#include "\.\.\/Widgets\/MD3Dialog\.hpp"/);
  const mainframe = stripComments(await read('src/slic3r/GUI/MainFrame.cpp'));
  assert.match(mainframe, /append_menu_item\(fileMenu, wxID_ANY, _L\("Melody creator"\) \+ dots,[\s\S]{0,400}MelodyCreatorDialog\(this\)\.ShowModal\(\);/);
});

test('each contract rejects the defect it pins', async () => {
  const dialog = stripComments(await read(DIALOG));
  const cantonese = parsePo(await read('bbl/i18n/yue_HK/BambuStudio_yue_HK.po'));
  // A message without Cantonese is caught.
  const sources = catalogueSources(dialog);
  const one = [...sources][0];
  cantonese.set(one, '');
  assert.ok([...sources].some((s) => !cantonese.get(s)), 'a blank Cantonese entry must be reported');
  // A placeholder dropped by a translation is caught.
  const holders = (s) => (s.match(/%[.\d]*[sdf]/g) ?? []).sort();
  assert.notDeepEqual(holders('Saved %s.'), holders('Saved.'));
  // The printing guard removed is caught.
  const mutated = dialog.replace('if (obj->is_in_printing()) { update_status(_L("The printer is printing. Play the melody when it is idle."), true); return; }', '');
  assert.notEqual(mutated, dialog);
  assert.doesNotMatch(mutated, /if \(obj->is_in_printing\(\)\) \{ update_status/);
});
