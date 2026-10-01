import assert from 'node:assert/strict';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

// The material type "TPU-AMS" and the preset names "Bambu TPU for AMS" and "Generic TPU for AMS"
// are read as "TPU for Ink Dispenser" (Cantonese: 墨水機用 TPU). The rename is display only. The
// value of the filament_type setting stays "TPU-AMS" in presets, 3MF files and every comparison in
// code, and the preset names keep their spelling, so old projects and profiles still load.
//
// This test pins both halves. The first tests prove the stored value and the code that compares it
// did not move. The others fail when a place that shows the type or the name goes back to showing it
// raw, and when the text a person reads could be written back as the value.
//
// TPU_DISPLAY_SOURCE_ROOT points the test at another copy of the tree (for example a checkout of an
// older revision) so that the test can be shown to fail on old source.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = process.env.TPU_DISPLAY_SOURCE_ROOT
  ? path.resolve(process.env.TPU_DISPLAY_SOURCE_ROOT)
  : path.resolve(testDir, '..', '..');

const readRaw = (...parts) => readFileSync(path.join(repoDir, ...parts), 'utf8').replace(/\r\n/g, '\n');
const noComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const read = (...parts) => noComments(readRaw(...parts));
const gui = (file) => ['src', 'slic3r', 'GUI', file];

function between(text, start, end, what) {
  const from = text.search(start);
  assert.notEqual(from, -1, `${what}: start not found`);
  const rest = text.slice(from);
  const to = rest.slice(1).search(end);
  assert.notEqual(to, -1, `${what}: end not found`);
  return rest.slice(0, to + 1);
}

// The source of one named function, from its first line to its closing brace at column zero.
function functionSource(text, name) {
  const match = text.match(new RegExp(`function ${name}\\([^)]*\\) \\{[\\s\\S]*?\\n\\}`));
  assert.ok(match, `function ${name} is found`);
  return match[0];
}

// A minimal PO reader: entries are separated by blank lines; a value can continue over lines.
function poEntries(text) {
  const entries = new Map();
  for (const chunk of text.split(/\n\n+/)) {
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

// text.js, run the way the locale validator runs it, with the page language switchable.
function loadTextJs(initialLang) {
  const sandbox = {
    lang: initialLang,
    GetQueryString: () => sandbox.lang,
    localStorage: { getItem: () => null, setItem: () => {} },
  };
  vm.createContext(sandbox);
  vm.runInContext(readRaw('resources', 'web', 'data', 'text.js'), sandbox);
  return sandbox;
}

// The table of LanguageMode.cpp's vocabulary(), run with the same whole-word algorithm.
function vocabularyFromSource() {
  const mode = read(...gui('LanguageMode.cpp'));
  const table = between(mode, /VOCABULARY_RULES\[\]\s*=\s*\{/, /\n\};/, 'VOCABULARY_RULES');
  const rules = [...table.matchAll(/\{\s*L"([^"]+)"\s*,\s*L"([^"]+)"\s*\}/g)].map((m) => [m[1], m[2]]);
  const wordChar = (c) => /[\p{L}\p{N}_]/u.test(c);
  const apply = (input) => {
    if (input.includes('://')) return input;
    if (input.includes('_') && !input.includes(' ')) return input;
    let value = input;
    for (const [from, to] of rules) {
      let pos = 0;
      while ((pos = value.indexOf(from, pos)) !== -1) {
        const end = pos + from.length;
        const starts = pos === 0 || !wordChar(value[pos - 1]);
        const ends = end >= value.length || !wordChar(value[end]);
        if (starts && ends) { value = value.slice(0, pos) + to + value.slice(end); pos += to.length; }
        else pos += from.length;
      }
    }
    return value;
  };
  return { rules, apply };
}

// ---------------------------------------------------------------------------------------------
// The stored value did not move.
// ---------------------------------------------------------------------------------------------

test('the filament_type setting still lists and stores TPU-AMS, with no separate label', () => {
  const config = read('src', 'libslic3r', 'PrintConfig.cpp');
  const def = between(config, /def = this->add\("filament_type", coStrings\);/, /def = this->add\("filament_soluble"/, 'filament_type');
  assert.ok(def.includes('def->enum_values.push_back("TPU-AMS");'), 'TPU-AMS is still a value of the setting');
  assert.ok(def.includes('ConfigOptionDef::GUIType::f_enum_open'), 'the dropdown is still an editable one');
  assert.doesNotMatch(def, /enum_labels/, 'the setting has no label list: the text shown is the text stored');
});

test('the system profiles that carry the type and the names are untouched', () => {
  for (const vendor of ['Bambu', 'Generic']) {
    const profile = JSON.parse(readRaw('resources', 'profiles', 'BBL', 'filament', `${vendor} TPU for AMS @base.json`));
    assert.equal(profile.name, `${vendor} TPU for AMS @base`);
    assert.deepEqual(profile.filament_type, ['TPU-AMS']);
  }
  const order = read('src', 'libslic3r', 'Preset.cpp');
  assert.ok(order.includes('"Bambu TPU for AMS",'), 'the sort order still names the preset as written');
});

test('the code that compares the type still compares the value as written', () => {
  const expectations = [
    [['src', 'libslic3r', 'Brim.cpp'], 'materialName == "TPU-AMS"'],
    [['src', 'libslic3r', 'Model.cpp'], 'materialName == "TPU-AMS"'],
    [gui('Tab.cpp'), 'has_filaments({"TPU", "TPU-AMS"})'],
    [gui('GUI_App.cpp'), '{"PETG", "TPU", "TPU-AMS"}'],
    [gui('AMSMaterialsSetting.cpp'), 'fila_item->filament_type == "TPU-AMS"'],
  ];
  for (const [file, text] of expectations)
    assert.ok(read(...file).includes(text), `${file.join('/')} still compares ${text}`);
});

test('the display helpers never take the stored value from display text', () => {
  const source = read(...gui('I18N.cpp'));
  assert.match(source, /INK_DISPENSER_MATERIAL_TYPE\s*=\s*"TPU-AMS"/);
  assert.match(source, /is_ink_dispenser_material_type\(const std::string &value\)\s*\{\s*return value == INK_DISPENSER_MATERIAL_TYPE;\s*\}/,
    'the type is recognised by the stored value, never by what is shown');
  const header = read(...gui('I18N.hpp'));
  for (const name of ['is_ink_dispenser_material_type', 'display_material_type', 'display_material_type_utf8', 'display_material_name'])
    assert.ok(header.includes(name), `I18N.hpp declares ${name}`);
});

// ---------------------------------------------------------------------------------------------
// The translation layer.
// ---------------------------------------------------------------------------------------------

test('the vocabulary rewrite turns TPU-AMS into the display name ahead of the plain AMS rule', () => {
  const { rules, apply } = vocabularyFromSource();
  const at = (word) => rules.findIndex(([from]) => from === word);
  assert.notEqual(at('TPU-AMS'), -1, 'the rules table has a TPU-AMS rule');
  assert.ok(at('TPU-AMS') < at('AMS'), 'the TPU-AMS rule runs before the AMS rule');
  assert.deepEqual(rules[at('TPU-AMS')], ['TPU-AMS', 'TPU for Ink Dispenser']);
  assert.equal(apply('TPU-AMS'), 'TPU for Ink Dispenser');
  assert.equal(apply('Bambu TPU for AMS'), 'Bambu TPU for Ink Dispenser');
  assert.equal(apply('Slot 1 (TPU-AMS)'), 'Slot 1 (TPU for Ink Dispenser)');
  // Nothing else moves.
  assert.equal(apply('AMS Lite(1)'), 'Ink Dispenser Lite(1)');
  assert.equal(apply('TPU-AMSX'), 'TPU-AMSX');
  assert.equal(apply('TPU-AMS_CF'), 'TPU-AMS_CF', 'an identifier is never rewritten');
  assert.equal(apply('PLA'), 'PLA');
});

test('the catalogues carry the display name and the short name for the type', () => {
  const english = poEntries(readRaw('bbl', 'i18n', 'en', 'BambuStudio_en.po'));
  const cantonese = poEntries(readRaw('bbl', 'i18n', 'yue_HK', 'BambuStudio_yue_HK.po'));
  const key = (ctx, id) => `${ctx ?? ''}\u0004${id}`;
  assert.equal(english.get(key(null, 'TPU-AMS')), 'TPU for Ink Dispenser');
  assert.equal(english.get(key('NarrowBlock', 'TPU-AMS')), 'TPU for Ink');
  assert.equal(cantonese.get(key(null, 'TPU-AMS')), '墨水機用 TPU');
  assert.equal(cantonese.get(key('NarrowBlock', 'TPU-AMS')), '墨水用 TPU');
  // The message id is the stored type, so it is the lookup key and is never translated away.
  const pot = readRaw('bbl', 'i18n', 'BambuStudio.pot');
  assert.ok(pot.includes('msgid "TPU-AMS"\nmsgstr ""'), 'the template lists the message');
  assert.ok(pot.includes('msgctxt "NarrowBlock"\nmsgid "TPU-AMS"\nmsgstr ""'), 'the template lists the short message');
  assert.ok(readRaw('bbl', 'i18n', 'list.txt').split('\n').includes('src/slic3r/GUI/I18N.cpp'),
    'the extraction reads the file that marks the message');
});

test('the helper marks the display name and the short name, and is written for display only', () => {
  const source = read(...gui('I18N.cpp'));
  assert.match(source, /return _L\("TPU-AMS"\);/, 'the display name is a marked message');
  assert.match(source, /_CTX\("TPU-AMS", "NarrowBlock"\)/, 'the short name is a marked message with the context the catalogue uses');
  assert.match(source, /shown\.Replace\("Ink Dispenser", "Ink"\);/, 'a language without the short wording still gets a short form');
  const name = between(source, /wxString display_material_name\(/, /\n\}\n/, 'display_material_name');
  assert.ok(source.includes('INK_DISPENSER_MATERIAL_NAME = "TPU for AMS"'), 'the preset names are matched as written');
  assert.match(name, /starts && ends/, 'the name is matched as a whole word');
});

// ---------------------------------------------------------------------------------------------
// The Type dropdown: the text shown is the text stored, so it has to be mapped back.
// ---------------------------------------------------------------------------------------------

test('the Type dropdown shows the ink wording and maps it back to the stored type', () => {
  const field = read(...gui('Field.cpp'));
  assert.match(field, /static wxString enum_item_text\(const ConfigOptionDef &opt, const std::string &value\)\s*\{\s*if \(opt\.opt_key == "filament_type" && I18N::is_ink_dispenser_material_type\(value\)\)\s*return I18N::display_material_type\(value\);\s*return wxString\(value\);\s*\}/);
  const build = between(field, /void Choice::BUILD\(\)/, /\n\}\n/, 'Choice::BUILD');
  assert.ok(build.includes('temp->Append(enum_item_text(m_opt, el));'), 'the list items are built through enum_item_text');
  assert.doesNotMatch(build, /temp->Append\(el\);/, 'no list item is appended as the raw value');

  const back = between(field, /static wxString stored_enum_text\(/, /\n\}\n/, 'stored_enum_text');
  assert.ok(back.includes('opt.opt_key != "filament_type" || !opt.enum_labels.empty()'), 'only the filament type list is mapped back');
  assert.ok(back.includes('shown == field.GetString(static_cast<unsigned int>(i))'), 'the text is matched against the item this list showed');
  assert.ok(back.includes('return wxString(opt.enum_values[i]);'), 'the stored value is the list value');

  const get = between(field, /boost::any& Choice::get_value\(\)/, /\n\}\n/, 'Choice::get_value');
  assert.ok(get.includes('wxString ret_str = stored_enum_text(m_opt, *field, field->GetValue());'),
    'the text read from the control is mapped before anything stores it');
  assert.doesNotMatch(get, /wxString ret_str = field->GetValue\(\);/, 'the control text is never stored as it is shown');
});

test('mapping every type through display and back gives the stored type, in every language', () => {
  const config = read('src', 'libslic3r', 'PrintConfig.cpp');
  const def = between(config, /def = this->add\("filament_type", coStrings\);/, /def = this->add\("filament_soluble"/, 'filament_type');
  const values = [...def.matchAll(/enum_values\.push_back\("([^"]+)"\)/g)].map((m) => m[1]);
  assert.ok(values.includes('TPU-AMS') && values.includes('TPU') && values.includes('PLA'));

  const english = 'TPU for Ink Dispenser';
  const cantonese = '墨水機用 TPU';
  for (const shownAs of [english, cantonese]) {
    // The two functions of Field.cpp, as a model: the list shows display text for one value, and a
    // text that equals a list item's display text is stored as that item's value.
    const items = values.map((v) => (v === 'TPU-AMS' ? shownAs : v));
    const stored = (text) => { const i = items.indexOf(text); return i >= 0 ? values[i] : text; };
    for (const v of values) assert.equal(stored(items[values.indexOf(v)]), v, `${v} survives a round trip as ${shownAs}`);
    assert.equal(stored('TPU-AMS'), 'TPU-AMS', 'a type typed as written is stored as written');
    assert.equal(stored('My own type'), 'My own type', 'a custom type is stored as typed');
    assert.notEqual(stored(shownAs), shownAs, 'the display text is never what gets stored');
  }
});

// ---------------------------------------------------------------------------------------------
// The other places a person reads the type or a preset name.
// ---------------------------------------------------------------------------------------------

test('the temperature calibration chips show the ink wording and pick temperatures by index', () => {
  const source = read(...gui('calib_dlg.cpp'));
  const labels = source.match(/wxString filamentLabels\[\]\s*=\s*\{[^}]*\};/);
  assert.ok(labels, 'the label array is found');
  assert.ok(labels[0].includes('I18N::display_material_type("TPU-AMS")'), 'the chip shows the display name');
  assert.doesNotMatch(labels[0], /"TPU-AMS"\s*,\s*"PA-CF"/, 'the chip is not the raw type');
  assert.match(source, /case tTPU_AMS:/, 'the temperatures are still picked by index');
});

test('the Ink Grouping card shows the short ink wording for the type', () => {
  const source = read(...gui('GCodeRenderer/BaseRenderer.cpp'));
  const lambda = between(source, /auto get_filament_display_type = /, /\};/, 'get_filament_display_type');
  assert.ok(lambda.includes('return I18N::display_material_type_utf8(filament.type, true);'));
  assert.doesNotMatch(lambda, /return filament\.type;/, 'the chip is not the raw type');
});

test('the mixed ink dialog shows the ink wording in its message and its names', () => {
  const source = read(...gui('MixedFilamentDialog.cpp'));
  assert.ok(source.includes('wxString::Format(_L("Slot %s (%s)"), slots, I18N::display_material_type(it->first))'));
  assert.doesNotMatch(source, /_L\("Slot %s \(%s\)"\),\s*slots,\s*wxString::FromUTF8\(it->first\)/, 'the mismatch message does not show the raw type');
  assert.equal((source.match(/m_physical_names\(display_preset_names\(physical_names\)\)/g) || []).length, 2, 'both constructors list display names');
  assert.match(source, /shown\.push_back\(std::string\(I18N::display_material_name\(name\)\.ToUTF8\(\)\.data\(\)\)\);/);
  // The groups and the comparison still use the stored types.
  assert.ok(source.includes('type_groups[m_physical_types[phys - 1]].push_back(phys);'), 'types are grouped as stored');
});

test('the changes dialog and the support suggestion show the ink wording', () => {
  const unsaved = read(...gui('UnsavedChangesDialog.cpp'));
  assert.match(unsaved, /if \(opt_key == "filament_type"\)\s*return I18N::display_material_type\(strings->get_at\(opt_idx\)\);/);
  const tab = read(...gui('Tab.cpp'));
  assert.ok(tab.includes('slot_str += " " + I18N::display_material_type(filament->config.option<ConfigOptionStrings>("filament_type")->values[0]);'));
  assert.ok(tab.includes('I18N::display_material_name(support_material_display_name), I18N::display_material_name(model_material_display_name)'));
});

test('the ink slot picker lists system presets in the ink wording and still returns the alias', () => {
  const source = read(...gui('FilamentSelectDialog.cpp'));
  assert.ok(source.includes('auto* lbl = new Label(row, I18N::display_material_name(std::string(alias.ToUTF8().data())));'),
    'the row shows the preset alias in the ink wording');
  assert.doesNotMatch(source, /auto\* lbl = new Label\(row, alias\);/, 'the row is not the raw alias');
  assert.ok(source.includes('m_checked_alias = alias;'), 'the selection is still the alias as written');
});

// ---------------------------------------------------------------------------------------------
// The web pages.
// ---------------------------------------------------------------------------------------------

test('the page text table carries the display name in English and Cantonese, and the helper uses it', () => {
  const sandbox = loadTextJs(null);
  assert.equal(sandbox.LangText.en.t299, 'TPU for Ink Dispenser');
  assert.equal(sandbox.LangText.yue_HK.t299, '墨水機用 TPU');
  for (const [lang, type, name] of [
    [null, 'TPU for Ink Dispenser', 'Bambu TPU for Ink Dispenser'],
    ['en', 'TPU for Ink Dispenser', 'Bambu TPU for Ink Dispenser'],
    ['yue_HK', '墨水機用 TPU', 'Bambu 墨水機用 TPU'],
    ['bilingual_en_yue_HK', 'TPU for Ink Dispenser', 'Bambu TPU for Ink Dispenser'],
    ['de_DE', 'TPU for Ink Dispenser', 'Bambu TPU for Ink Dispenser'],
  ]) {
    sandbox.lang = lang;
    assert.equal(sandbox.DisplayInkWording('TPU-AMS'), type, `${lang}: the type`);
    assert.equal(sandbox.DisplayInkWording('Bambu TPU for AMS'), name, `${lang}: the preset name`);
    assert.equal(sandbox.DisplayInkWording('Generic TPU for AMS'), name.replace('Bambu', 'Generic'));
  }
  sandbox.lang = 'en';
  for (const same of ['TPU', 'PLA', 'TPU-AMSX', 'Bambu PLA Basic', 'Bambu TPU 95A HF', 'AMSX', '']) {
    assert.equal(sandbox.DisplayInkWording(same), same, `${same || '(empty)'} is shown as written`);
  }
  assert.equal(sandbox.DisplayInkWordingTitle('TPU-AMS'), '', 'only the bilingual mode adds a tooltip');
  sandbox.lang = 'bilingual_en_yue_HK';
  assert.equal(sandbox.DisplayInkWordingTitle('TPU-AMS'), 'TPU for Ink Dispenser ／ 粵語：墨水機用 TPU');
  assert.equal(sandbox.DisplayInkWordingTitle('Bambu TPU for AMS'), 'Bambu TPU for Ink Dispenser ／ 粵語：Bambu 墨水機用 TPU');
  assert.equal(sandbox.DisplayInkWordingTitle('PLA'), '');
});

test('the ink picker pages show the ink wording and keep the raw type and name in their data', () => {
  const current = readRaw('resources', 'web', 'guide', '23', '23.js');
  assert.ok(current.includes("$('<span>').text(DisplayInkWording(shortName))"), 'the row shows the ink wording');
  assert.ok(current.includes("$('<span>').text(DisplayInkWording(value))"), 'the filter options show the ink wording');
  assert.ok(current.includes("vendor: vendor, filatype: type, name: shortName"), 'the row keeps the raw type and name in its attributes');
  assert.ok(current.includes("appendFilterOption('#FilatypeList', type, 'filatype', '', FilaClick)"), 'the filter keeps the raw type as its value');
  assert.ok(current.includes("DisplayInkWording(String(item.type || ''))"), 'the custom ink list shows the ink wording');
  assert.ok(current.includes("$(FilaSelectedList[n]).closest('.filament-row').data('filamentKeys')"), 'the result is still sent by profile key');
  assert.doesNotMatch(current, /\$\('<span>'\)\.text\(shortName\)/, 'the row is not the raw name');
  assert.doesNotMatch(current, /\$\('<span>'\)\.text\(value\)\)/, 'the filter option is not the raw value');

  const legacy = readRaw('resources', 'web', 'guide', '22', '22.js');
  assert.ok(legacy.includes('filatype="\'+fType+\'" onChange="FilaClick()"   />\'+DisplayInkWording(fType)+\'</div>'), 'the type row shows the ink wording');
  assert.ok(legacy.includes('name="\'+fShortName+\'" />\'+DisplayInkWording(fShortName)+\'</div>'), 'the name row shows the ink wording');
  assert.ok(legacy.includes('filalist="\'+fWholeName+\';\''), 'the row keeps the whole preset name for the result');
});

test('the create-ink wizard shows the ink wording and sends the stored type and preset names', () => {
  const step1 = readRaw('resources', 'web', 'filament_create', 'step1.js');
  const setter = functionSource(step1, 'setTypeValue');
  const getter = functionSource(step1, 'getTypeValue');
  assert.ok(step1.includes("'<div class=\"dropdown-item\" data-val=\"' + t + '\">' + DisplayInkWording(t) + '</div>'"), 'the list row shows the ink wording and keeps data-val');
  // Every read of the type goes through getTypeValue, every write through setTypeValue.
  const outside = step1.replace(setter, '').replace(getter, '');
  assert.doesNotMatch(outside, /\$\('#input-type'\)\.val\(/, 'the field text is never read or written directly');
  assert.ok((outside.match(/getTypeValue\(\)/g) || []).length >= 3, 'the next button, the payload and the dropdown read the stored type');
  assert.match(outside, /sessionStorage\.setItem\('step1', JSON\.stringify\(\{ vendor, type, serial: serial, mode \}\)\);/, 'the saved step keeps the stored type');

  // Run the two functions: the field shows the ink wording and the value stays the stored type.
  const sandbox = loadTextJs('en');
  const field = { data: {}, text: '' };
  const element = {
    data(key, value) { if (value === undefined) return field.data[key]; field.data[key] = value; return element; },
    val(value) { if (value === undefined) return field.text; field.text = value; return element; },
  };
  sandbox.$ = () => element;
  vm.runInContext(`${setter}\n${getter}`, sandbox);
  for (const [lang, shown] of [['en', 'TPU for Ink Dispenser'], ['yue_HK', '墨水機用 TPU']]) {
    sandbox.lang = lang;
    sandbox.setTypeValue('TPU-AMS');
    assert.equal(field.text, shown, `${lang}: the field shows the ink wording`);
    assert.equal(sandbox.getTypeValue(), 'TPU-AMS', `${lang}: the stored type is the type as written`);
  }
  sandbox.setTypeValue('PLA');
  assert.equal(field.text, 'PLA');
  assert.equal(sandbox.getTypeValue(), 'PLA');
  sandbox.setTypeValue('');
  assert.equal(field.text, '');
  assert.equal(sandbox.getTypeValue(), '');

  const base = readRaw('resources', 'web', 'filament_create', 'step2.js');
  assert.ok(base.includes("DisplayInkWording(p.name) + '</div>'"), 'the base preset rows show the ink wording');
  assert.ok(base.includes("$('#input-base-preset').val(DisplayInkWording(p.name));"));
  assert.ok(base.includes('base_preset: selectedPreset ? selectedPreset.name : \'\''), 'the confirmed preset is sent by its own name');
  const typed = readRaw('resources', 'web', 'filament_create', 'step2_type.js');
  assert.ok(typed.includes("$('#input-base-preset').val(DisplayInkWording(selectedPreset));"));
  assert.ok(typed.includes("data-val=\"' + p + '\">' + DisplayInkWording(p) + '</div>'"));
  assert.ok(typed.includes('base_preset: selectedPreset,'), 'the confirmed preset is sent by its own name');
});

test('the old create-ink script that still lists the raw type is not loaded by any page', () => {
  const dir = path.join(repoDir, 'resources', 'web', 'filament_create');
  if (!existsSync(path.join(dir, 'main.js'))) return;
  for (const file of readdirSync(dir).filter((name) => name.endsWith('.html')))
    assert.doesNotMatch(readRaw('resources', 'web', 'filament_create', file), /src="main\.js"/, `${file} does not load main.js`);
});
