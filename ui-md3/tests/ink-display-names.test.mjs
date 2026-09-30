import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The product shows "Ink Dispenser" where the printer and the source say "AMS", and
// "ink" where they say "filament". The rename is display only: the catalogues carry the
// wording a person reads, so a name that reaches the screen has to go through the
// catalogue. A dispenser name written as a bare literal in a table, a label or a
// concatenation skips it and shows the old word in every language, and in Cantonese mode
// it shows English. This test refuses each of those places when it goes back to a raw
// literal, and checks that the catalogues carry the message for every marked name.
//
// INK_NAMES_SOURCE_ROOT points the test at another copy of the tree (for example a
// checkout of an older revision) so that the test can be shown to fail on old source.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = process.env.INK_NAMES_SOURCE_ROOT
  ? path.resolve(process.env.INK_NAMES_SOURCE_ROOT)
  : path.resolve(testDir, '..', '..');

const noComments = (text) => text
  .replace(/\r\n/g, '\n')
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/\/\/.*$/gm, '');
const read = (...parts) => noComments(readFileSync(path.join(repoDir, ...parts), 'utf8'));
const readRaw = (root, ...parts) => readFileSync(path.join(root, ...parts), 'utf8').replace(/\r\n/g, '\n');

const gui = (file) => ['src', 'slic3r', 'GUI', file];
const devicePage = (file) => ['src', 'slic3r', 'GUI', 'DeviceWeb', 'device_page', file];

// The calls that mark or translate a message.
const MARKED = /(?:^|[^A-Za-z0-9_])(?:L|_L|_|_u8L|_utf8|_CTX|L_CONTEXT)\(\s*$/;

// Every string literal in the text that names a dispenser or the material and is not the
// first argument of a marking or translating call.
function rawDisplayLiterals(text) {
  const raw = [];
  for (const match of text.matchAll(/"((?:[^"\\\n]|\\.)*)"/g)) {
    if (!/\bAMS\b|\bAMS\d|\bAMS-|[Ff]ilament/.test(match[1])) continue;
    if (!MARKED.test(text.slice(0, match.index))) raw.push(match[1]);
  }
  return raw;
}

function block(text, start, end, what) {
  const from = text.search(start);
  assert.notEqual(from, -1, `${what}: start of the block not found`);
  const rest = text.slice(from);
  const to = rest.search(end);
  assert.notEqual(to, -1, `${what}: end of the block not found`);
  return rest.slice(0, to + 1);
}

test('the dispenser display-name table holds marked message ids, not raw names', () => {
  const source = read(...gui('DeviceCore/DevFilaSystem.cpp'));
  const table = block(source, /s_ams_display_formats\s*=\s*\{/, /\n\};/, 's_ams_display_formats');
  assert.deepEqual(rawDisplayLiterals(table), [], 'every name in the table is marked with L or L_CONTEXT');
  const fallback = source.match(/s_ams_default_display_format\s*=\s*[^;]*;/);
  assert.ok(fallback, 'the fallback name is a marked entry');
  assert.deepEqual(rawDisplayLiterals(fallback[0]), []);
  for (const name of ['AMS(%d)', 'AMS Lite(%d)', 'AMS 2 Pro(%d)', 'AMS HT(%d)'])
    assert.ok(table.includes(`L("${name}")`), `${name} is marked in the table`);
});

test('GetDisplayName translates the name when it is built, never at static initialisation', () => {
  const source = read(...gui('DeviceCore/DevFilaSystem.cpp'));
  const fn = block(source, /wxString DevAms::GetDisplayName\(/, /\n\}\n/, 'GetDisplayName');
  assert.match(fn, /_CTX\(\s*names->narrow\s*,\s*"NarrowBlock"\s*\)/, 'the narrow form uses the NarrowBlock context');
  assert.match(fn, /if \(narrow\)\s*ams_display_format\.Replace\("Ink Dispenser", "Ink"\);/,
    'a language without the short wording still gets the short form');
  // L_CONTEXT drops its context at run time; only the extraction sees it. Every entry must carry the
  // context the lookup asks for, or the catalogues and the lookup stop matching.
  const table = block(source, /s_ams_display_formats\s*=\s*\{/, /\n\};/, 's_ams_display_formats');
  const fallback = source.match(/s_ams_default_display_format\s*=\s*[^;]*;/)[0];
  assert.equal((table + fallback).match(/L_CONTEXT\("[^"]+",\s*"NarrowBlock"\)/g)?.length, 5, 'four table entries and the fallback use NarrowBlock');
  assert.equal((table + fallback).match(/L_CONTEXT\(/g)?.length, 5, 'no entry uses another context');
  assert.match(fn, /_L\(\s*names->full\s*\)/, 'the wide form is translated');
  assert.match(fn, /wxString::Format\(\s*ams_display_format\s*,\s*loc\s*\)/);
  const header = read(...gui('DeviceCore/DevFilaSystem.h'));
  assert.match(header, /GetDisplayName\(\s*bool\s+narrow\s*=\s*false\s*\)\s*const/);
});

test('the one-slot blocks ask for the narrow name', () => {
  assert.match(read(...gui('AmsMappingPopupUpdate.cpp')), /GetDisplayName\(\s*ams_iter->second->GetSlotCount\(\)\s*==\s*1\s*\)/,
    'the mapping popup block is 74 DIP wide for one slot');
  assert.match(read(...gui('DeviceTab/uiAMSBestPositionPopup.cpp')), /GetDisplayName\(\s*ams->second->GetSlotCount\(\)\s*==\s*1\s*\)/,
    'the reselect dialog panel is 92 DIP wide for one slot');
});

test('the multi-device mapping popup does not hard-code a dispenser name', () => {
  const source = read(...gui('AmsMappingPopupUpdate.cpp'));
  assert.doesNotMatch(source, /MappingContainer\([^;,]*,\s*"AMS/, 'the name argument is not a raw literal');
  assert.match(source, /MappingContainer\([^;]*wxString::Format\(_L\("AMS\(%d\)"\), 1\)/);
});

test('the printer settings dispenser-type names are marked and translated where shown', () => {
  const config = read('src', 'libslic3r', 'PrintConfig.cpp');
  const fn = block(config, /std::string get_ams_type_display_name\(/, /\n\}\n/, 'get_ams_type_display_name');
  assert.deepEqual(rawDisplayLiterals(fn), [], 'each returned name is marked with L');
  for (const name of ['AMS', 'AMS Lite', 'AMS 2 Pro/AMS HT'])
    assert.ok(fn.includes(`return L("${name}");`), `${name} is returned as a marked message id`);
  const tab = read(...gui('Tab.cpp'));
  assert.match(tab, /items\.push_back\(\{\s*_L\(display_name\)\s*,\s*ams_type\s*\}\)/, 'the dropdown shows the translated name');
  // The row label is built with the Line constructor, which translates its label.
  assert.match(readFileSync(path.join(repoDir, ...gui('OptionsGroup.hpp')), 'utf8'), /label\(_\(label\)\)/, 'Line translates its label');
  assert.doesNotMatch(tab, /items\.push_back\(\{\s*wxString::FromUTF8\(display_name/, 'the dropdown does not show the raw name');
});

test('the firmware page translates every dispenser name it builds', () => {
  const source = read(...gui('UpgradePanel.cpp'));
  const table = block(source, /ACCESSORY_DISPLAY_STR\s*=\s*\{/, /\n\};/, 'ACCESSORY_DISPLAY_STR');
  assert.ok(table.includes('{"N3F", L("AMS 2 Pro")}'));
  assert.ok(table.includes('{"N3S", L("AMS HT")}'));
  // The keys are module names the printer uses ("AMS", "N3F"); only the values are shown.
  assert.deepEqual(rawDisplayLiterals(table.replace(/\{"[^"]*",/g, '{')), [], 'no dispenser name in the accessory table is a raw literal');
  assert.match(source, /result\s*=\s*_L\(str_it->second\)/, 'the table value is translated where the name is built');
  assert.match(source, /ams_device_name\s*=\s*_L\("AMS-%s"\)/);
  assert.match(source, /name_text\s*=\s*_L\("AMS Lite"\)/);
  assert.match(source, /ams_name\s*=\s*reported_product_name_text\(iter_ams->second\.product_name\)/);
  assert.match(source, /name_text\s*=\s*reported_product_name_text\(extra_ams_it->second\.product_name\)/);
  const helper = block(source, /static wxString reported_product_name_text\(/, /\n\}\n/, 'reported_product_name_text');
  for (const pair of helper.matchAll(/\{"([^"]+)",\s*([^}]*)\}/g))
    assert.match(pair[2], /^L\("[^"]+"\)$/, `the reported name ${pair[1]} maps to a marked message id`);
  assert.match(helper, /return it == known\.end\(\) \? I18N::vocabulary\(product_name\) : _L\(it->second\);/,
    'an unknown name keeps its words but reads in the product wording');
  // The module names ams/<n> and ams_f1/<n> reach the table as AMS and AMS_F1 when no product name is reported.
  assert.ok(table.includes('{"AMS", L("AMS")}'), 'the plain dispenser module has a display name');
  assert.ok(table.includes('{"AMS_F1", L("AMS Lite")}'), 'the Lite module has a display name');
});

test('the drying limits name each dispenser through the catalogue', () => {
  const source = read(...gui('AMSDryControl.cpp'));
  const table = block(source, /static const AmsTempLimit ams_limits\[\]\s*=\s*\{/, /\n\s*\};/, 'ams_limits');
  assert.deepEqual(rawDisplayLiterals(table), [], 'the names in ams_limits are marked with L');
  assert.match(source, /wxString msg = _L\(lim\.name\) \+ _L\(" maximum drying temperature is "\)/);
  assert.match(source, /wxString msg = _L\(lim\.name\) \+ _L\(" minimum drying temperature is "\)/);
});

test('the colour-swatch label is a short context message, not the raw word', () => {
  const source = read(...gui('AMSMaterialsSetting.cpp'));
  assert.doesNotMatch(source, /set_label\(\s*"AMS"\s*\)/, 'the swatch label is no longer a raw AMS');
  assert.match(source, /wxString swatch_label = _CTX\("AMS", "ColorSwatch"\);\s*swatch_label\.Replace\("Ink Dispenser", "Ink"\);\s*cp->set_label\(swatch_label\);/,
    'a language without the short wording still gets a label that fits the disc');
});

test('the developer switch labels and the flush texts use the catalogue or the product wording', () => {
  assert.match(read(...gui('StatusPanel.cpp')), /new SwitchBoard\(parent, _L\("AMS C\+\+"\), _L\("AMS Web"\)/);
  const flush = read(...gui('WipeTowerDialog.cpp'));
  assert.match(flush, /auto_flush_tip = I18N::vocabulary\(wxString\("Studio would re-calculate/);
  assert.match(flush, /volume_desp_panel = I18N::vocabulary\(wxString::FromUTF8\("Flushing volume/);
});

test('the fallback names for an unresolved ink go through the catalogue', () => {
  assert.doesNotMatch(read(...gui('Plater.cpp')), /"Filament "\s*\+\s*std::to_string/);
  assert.doesNotMatch(read(...gui('TextureImportDialog.cpp')), /"Filament "\s*\+\s*std::to_string/);
});

test('the undo snapshot names that say filament are marked and translated in the history', () => {
  assert.match(read(...gui('GUI_ObjectList.cpp')), /take_snapshot\(L\("Change Filament"\)\)/);
  assert.match(read(...gui('GUI_ObjectList.cpp')), /take_snapshot\(L\("Change Filaments"\)\)/);
  assert.match(read(...gui('Plater.cpp')), /take_snapshot\(L\("Change Filaments"\)\)/);
  const history = read(...gui('ProjectHistoryDialog.cpp'));
  assert.match(history, /"Change Filament",/);
  assert.match(history, /"Change Filaments",/);
});

test('the device page names its dispenser types and unit chips through t()', () => {
  const constants = read(...devicePage('src/features/filament-manager/constants.ts'));
  const names = block(constants, /export const AMS_TYPE_NAMES/, /\n\};/, 'AMS_TYPE_NAMES');
  for (const key of ['AMS', 'AMS Lite', 'AMS 2 Pro', 'AMS HT'])
    assert.ok(names.includes(`t('${key}')`), `${key} is a translation key`);
  assert.doesNotMatch(constants, /`AMS\(\$\{/, 'the fallback name is translated');
  assert.match(constants, /t\('AMS\(\{\{n\}\}\)', \{ n: amsType \}\)/);

  assert.doesNotMatch(read(...devicePage('src/features/filament-manager/AddEditDialog.tsx')), /title=\{`AMS \$\{/);
  assert.match(read(...devicePage('src/features/filament-manager/AddEditDialog.tsx')), /title=\{t\('AMS \{\{n\}\}'/);

  const chips = read(...devicePage('src/features/device-page/ams-control-web/components/AmsPreviewBar.tsx'));
  assert.doesNotMatch(chips, /aria-label=\{`AMS \$\{/);
  assert.match(chips, /aria-label=\{t\('AMS \{\{n\}\}'/);

  const slot = read(...devicePage('src/features/device-page/ams-control-web/components/SlotCard.tsx'));
  assert.doesNotMatch(slot, /aria-label=\{?[^>]*'(Edit|View|New) filament'/, 'no raw filament accessible name');
  assert.doesNotMatch(slot, /aria-label="New filament"/);
  assert.match(slot, /aria-label=\{showEdit \? t\('Edit Filament'\) : t\('View Filament'\)\}/);
  assert.match(slot, /aria-label=\{t\('New Filament'\)\}/);
});

// A minimal PO reader: entries are separated by blank lines; a value can continue over lines.
function poEntries(text) {
  const entries = new Map();
  for (const chunk of text.replace(/\r\n/g, '\n').split(/\n\n+/)) {
    const fields = { msgctxt: null, msgid: '', msgstr: '' };
    let current = null;
    for (const line of chunk.split('\n')) {
      const start = line.match(/^(msgctxt|msgid|msgstr)\s+"(.*)"$/);
      if (start) { current = start[1]; fields[current] = start[2]; continue; }
      const more = line.match(/^"(.*)"$/);
      if (more && current) fields[current] += more[1];
    }
    if (fields.msgid) entries.set(`${fields.msgctxt ?? ''}\u0004${fields.msgid}`, fields.msgstr);
  }
  return entries;
}

test('the catalogues carry every marked dispenser message, and the narrow names are shorter', () => {
  const english = poEntries(readRaw(repoDir, 'bbl', 'i18n', 'en', 'BambuStudio_en.po'));
  const cantonese = poEntries(readRaw(repoDir, 'bbl', 'i18n', 'yue_HK', 'BambuStudio_yue_HK.po'));
  const key = (ctx, id) => `${ctx ?? ''}\u0004${id}`;

  const wide = ['AMS(%d)', 'AMS Lite(%d)', 'AMS 2 Pro(%d)', 'AMS HT(%d)'];
  const plain = ['AMS', 'AMS Lite', 'AMS 2 Pro', 'AMS HT', 'AMS 2 Pro/AMS HT', 'AMS-%s', 'AMS C++', 'AMS Web', 'Change Filaments', 'Filament Buffer'];
  const contexts = [['ColorSwatch', 'AMS'], ...wide.map((id) => ['NarrowBlock', id])];

  for (const id of [...wide, ...plain]) {
    assert.ok(english.get(key(null, id)), `English override for ${id}`);
    assert.ok(cantonese.get(key(null, id)), `Cantonese translation for ${id}`);
    assert.doesNotMatch(english.get(key(null, id)), /\bAMS\b|[Ff]ilament/, `English override for ${id} uses the ink wording`);
  }
  for (const [ctx, id] of contexts) {
    assert.ok(english.get(key(ctx, id)), `English override for ${ctx} ${id}`);
    assert.ok(cantonese.get(key(ctx, id)), `Cantonese translation for ${ctx} ${id}`);
  }
  for (const id of wide) {
    assert.ok(english.get(key('NarrowBlock', id)).length < english.get(key(null, id)).length, `the narrow English name for ${id} is shorter`);
    assert.doesNotMatch(english.get(key('NarrowBlock', id)), /Dispenser/, 'the narrow name drops the word Dispenser');
  }
  assert.ok(english.get(key('ColorSwatch', 'AMS')).length <= 3, 'the colour swatch English label fits a 25 DIP disc');
  assert.ok([...cantonese.get(key('ColorSwatch', 'AMS'))].length <= 2, 'the colour swatch Cantonese label is at most two characters');
});

test('the device page keys the components ask for exist in both locale files in the ink wording', () => {
  const locales = ['en.json', 'yue_HK.json'].map((file) => JSON.parse(readRaw(repoDir, ...devicePage(`locales/${file}`))));
  const components = [
    'src/features/device-page/ams-control-web/components/SlotCard.tsx',
    'src/features/device-page/ams-control-web/components/AmsPreviewBar.tsx',
    'src/features/filament-manager/AddEditDialog.tsx',
    'src/features/filament-manager/constants.ts',
  ];
  // The keys are the English source text, so a key that names the old word is expected; its value is what shows.
  const asked = new Set(['AMS', 'AMS Lite', 'AMS 2 Pro', 'AMS HT', 'AMS({{n}})', 'AMS {{n}}', 'View Filament', 'New Filament']);
  for (const file of components) {
    const text = readRaw(repoDir, ...devicePage(file));
    for (const m of text.matchAll(/\bt\(\s*'([^']*)'/g))
      if (/\bAMS\b|[Ff]ilament/.test(m[1])) asked.add(m[1]);
  }
  for (const k of asked)
    for (const [index, locale] of locales.entries()) {
      assert.equal(typeof locale[k], 'string', `${index ? 'yue_HK' : 'en'} has the key ${k}`);
      assert.doesNotMatch(locale[k], /\bAMS\b|[Ff]ilament/, `${index ? 'yue_HK' : 'en'} value for ${k} uses the ink wording`);
    }
});
