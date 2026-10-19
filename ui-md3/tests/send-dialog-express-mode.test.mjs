import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Express print mode is one process option (`express_print_mode`) with two
// halves: the start G-code guards (pinned by express-start-gcode.test.mjs)
// and the send half pinned here. The Send print job dialog shows an Express
// print mode row that mirrors the process preset: switching it writes the
// preset through the process Tab, which invalidates the G-code export and
// makes the dialog offer Slice again, and while it is on the bed leveling,
// flow calibration and nozzle offset calibration rows are forced off. The
// job then carries every preparation flag off. The multi-machine page and
// the automation bridge follow the same preset. The decisions themselves run
// in tests/send_print_readiness_test.cpp.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (...parts) => readFileSync(path.join(root, ...parts), 'utf8').replace(/\r\n/g, '\n');
const stripComments = (text) => text
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^[ \t]*\/\/.*$/gm, '')
  .replace(/[ \t]+\/\/[^"\n]*$/gm, '');
const sources = {
  'SelectMachine.cpp': stripComments(read('src', 'slic3r', 'GUI', 'SelectMachine.cpp')),
  'SelectMachine.hpp': stripComments(read('src', 'slic3r', 'GUI', 'SelectMachine.hpp')),
  'SendPrintReadiness.hpp': stripComments(read('src', 'slic3r', 'GUI', 'SendPrintReadiness.hpp')),
  'SendMultiMachinePage.cpp': stripComments(read('src', 'slic3r', 'GUI', 'SendMultiMachinePage.cpp')),
  'AutomationBridge.cpp': stripComments(read('src', 'slic3r', 'GUI', 'AutomationBridge.cpp')),
  'Print.cpp': stripComments(read('src', 'libslic3r', 'Print.cpp')),
  'Preset.cpp': stripComments(read('src', 'libslic3r', 'Preset.cpp')),
  'PrintConfig.cpp': stripComments(read('src', 'libslic3r', 'PrintConfig.cpp')),
  'GCodeProcessor.cpp': stripComments(read('src', 'libslic3r', 'GCode', 'GCodeProcessor.cpp')),
  'Tab.cpp': stripComments(read('src', 'slic3r', 'GUI', 'Tab.cpp')),
  'send_print_readiness_test.cpp': stripComments(read('tests', 'send_print_readiness_test.cpp')),
  'BambuStudio_yue_HK.po': read('bbl', 'i18n', 'yue_HK', 'BambuStudio_yue_HK.po'),
  'BambuStudio.pot': read('bbl', 'i18n', 'BambuStudio.pot'),
};

// Brace-balanced body that starts at the first `{` after `signature`.
const block = (text, signature) => {
  const at = text.indexOf(signature);
  assert.ok(at >= 0, `${signature} must exist`);
  const open = text.indexOf('{', at);
  let depth = 0;
  for (let i = open; i < text.length; i += 1) {
    if (text[i] === '{') depth += 1;
    else if (text[i] === '}') { depth -= 1; if (depth === 0) return text.slice(at, i + 1); }
  }
  assert.fail(`${signature} body does not close`);
};

const poEntries = (text) => {
  const entries = new Map();
  for (const entry of text.split(/\n{2,}/)) {
    const id = entry.match(/^msgid "(.*)"$/m);
    const str = entry.match(/^msgstr "(.*)"$/m);
    if (id && str && !/^msgctxt/m.test(entry)) entries.set(id[1], str[1]);
  }
  return entries;
};

const strings = [
  'Express print mode',
  'Turned off by Express print mode.',
  'Change Express print mode in the process settings while printing all plates.',
  'Skips bed leveling, the nozzle wipe, the vibration check, flow and nozzle offset calibration, the detections and the start sound. Heating, homing, filament loading, the purge line and the temperature waits are kept. Turning it on or off changes the process preset, so the plate is sliced again before it is sent.',
];

const checks = {
  optionDeclared(s) {
    assert.match(s['PrintConfig.cpp'], /this->add\("express_print_mode", coBool\);[\s\S]{0,1400}set_default_value\(new ConfigOptionBool\(false\)\)/);
    assert.match(s['Preset.cpp'], /"express_print_mode"/);
    // The key re-runs only the G-code export.
    assert.match(block(s['Print.cpp'], 'static std::unordered_set<std::string> steps_gcode'), /"express_print_mode"/);
    assert.match(s['Tab.cpp'], /append_single_option_line\("timelapse_type", "Timelapse"\);\s*optgroup->append_single_option_line\("express_print_mode"\);/);
  },
  estimatorAllowance(s) {
    const g = s['GCodeProcessor.cpp'];
    assert.match(g, /static const float EXPRESS_PREPARE_COMPENSATION_TIME = 60\.0f;/);
    assert.match(block(g, 'void GCodeProcessor::apply_config(const PrintConfig& config)'), /config\.express_print_mode\.value\)\s*\? EXPRESS_PREPARE_COMPENSATION_TIME/);
    assert.match(block(g, 'void GCodeProcessor::apply_config(const DynamicPrintConfig& config)'), /express_print_mode->value\)\s*m_time_processor\.prepare_compensation_time = EXPRESS_PREPARE_COMPENSATION_TIME/);
  },
  rowMirrorsPreset(s) {
    const d = s['SelectMachine.cpp'];
    assert.match(s['SelectMachine.hpp'], /PrintOption\*\s+m_option_express\{nullptr\};/);
    assert.match(d, /m_option_express = new PrintOption\(\s*m_options_other, _L\("Express print mode"\)/);
    // Never a per-printer AppConfig row.
    assert.doesNotMatch(d, /m_checkbox_list\["express_print_mode"\]/);
    assert.match(d, /m_checkbox_list_order\.push_front\(m_option_express\);/);
    assert.match(block(d, 'bool SelectMachineDialog::express_print_mode_in_preset() const'), /prints\.get_edited_preset\(\)\.config\.option\("express_print_mode"\)/);
    assert.match(block(d, 'void SelectMachineDialog::update_option_opts(MachineObject *obj)'), /m_option_express->Show\(m_print_type == FROM_NORMAL\);/);
    // The forced-off pass is the last thing the dynamic state does.
    assert.match(block(d, 'void SelectMachineDialog::update_option_dynamic_state(MachineObject *obj)'), /apply_express_print_mode_to_options\(obj\);\s*\}$/);
  },
  forcedOffRows(s) {
    const body = block(s['SelectMachine.cpp'], 'void SelectMachineDialog::apply_express_print_mode_to_options(MachineObject *obj)');
    assert.match(body, /for \(const char\* key : \{"bed_leveling", "flow_cali", "nozzle_offset_cali"\}\)/);
    assert.match(body, /row->setValue\("off"\);\s*row->enable\(false\);\s*row->update_tooltip_options_area\(forced_tip\);/);
    assert.match(body, /can_toggle_express_print_mode\(\)/);
  },
  toggleWritesPreset(s) {
    const body = block(s['SelectMachine.cpp'], 'void SelectMachineDialog::on_express_option_changed(wxCommandEvent &event)');
    assert.match(body, /if \(!can_toggle_express_print_mode\(\)\) \{\s*m_option_express->restoreValue\(\);\s*return;/);
    assert.match(body, /cfg\.set_key_value\("express_print_mode", new ConfigOptionBool\(on\)\);/);
    assert.match(body, /wxGetApp\(\)\.get_tab\(Preset::TYPE_PRINT\)\)\s*tab->load_config\(cfg\);/);
    assert.match(body, /load_option_vals\(obj\);/);
    assert.match(body, /CallAfter\(\[this\] \{ update_by_obj\(get_current_machine\(\)\); \}\);/);
    assert.match(block(s['SelectMachine.cpp'], 'bool SelectMachineDialog::can_toggle_express_print_mode() const'), /express_toggle_enabled\(m_print_type == FROM_NORMAL, m_is_in_sending_mode,/);
  },
  jobCarriesFlagsOff(s) {
    const d = s['SelectMachine.cpp'];
    assert.match(d, /const PrintPrepareChoice prepare = apply_express_print_mode\(express, PrintPrepareChoice\{/);
    assert.match(d, /m_print_job->set_print_config\(\s*MachineBedTypeString\[0\],\s*prepare\.bed_leveling_on,\s*prepare\.flow_cali_on,\s*false,\s*timelapse_option,\s*true,\s*m_ext_change_assist,\s*prepare\.bed_leveling,\s*prepare\.flow_cali,\s*prepare\.nozzle_offset_cali,/);
    const h = s['SendPrintReadiness.hpp'];
    assert.match(h, /inline PrintPrepareChoice apply_express_print_mode\(bool express, const PrintPrepareChoice &chosen\)\s*\{\s*return express \? PrintPrepareChoice\{\} : chosen;/);
    assert.match(h, /inline bool express_toggle_enabled\(bool from_normal, bool sending, bool current_plate_selected\)\s*\{\s*return from_normal && !sending && current_plate_selected;/);
    assert.match(s['send_print_readiness_test.cpp'], /express_assertions\(\);/);
  },
  otherSendersFollow(s) {
    const m = block(s['SendMultiMachinePage.cpp'], 'BBL::PrintParams SendMultiMachinePage::request_params(MachineObject* obj)');
    assert.match(m, /if \(express_print_mode_in_preset\(\)\) \{\s*bed_leveling = false;\s*flow_cali\s*= false;/);
    assert.match(block(s['SendMultiMachinePage.cpp'], 'void SendMultiMachinePage::on_timer(wxTimerEvent& event)'), /it->second->Enable\(!express\);/);
    assert.match(s['AutomationBridge.cpp'], /params\.task_bed_leveling=a\.value\("bedLeveling",!express\);/);
  },
  stringsTranslated(s) {
    const yue = poEntries(s['BambuStudio_yue_HK.po']);
    for (const id of strings) {
      assert.ok(s['BambuStudio.pot'].includes(`"${id.slice(0, 60)}`), `${id.slice(0, 40)} must be extracted into the catalog template`);
      assert.ok(yue.get(id), `${id.slice(0, 40)} needs a Cantonese entry`);
    }
  },
};

for (const [name, check] of Object.entries(checks))
  test(name, () => check(sources));

test('each contract rejects the defect it pins', () => {
  const mutations = [
    ['optionDeclared', 'Print.cpp', '        "express_print_mode",\n', ''],
    ['estimatorAllowance', 'GCodeProcessor.cpp', 'EXPRESS_PREPARE_COMPENSATION_TIME = 60.0f;', 'EXPRESS_PREPARE_COMPENSATION_TIME = 260.0f;'],
    ['rowMirrorsPreset', 'SelectMachine.cpp', 'm_checkbox_list_order.push_front(m_option_express);', 'm_checkbox_list["express_print_mode"] = m_option_express;'],
    ['rowMirrorsPreset', 'SelectMachine.cpp', '    apply_express_print_mode_to_options(obj);\n}\n\nbool SelectMachineDialog::express_print_mode_in_preset', '}\n\nbool SelectMachineDialog::express_print_mode_in_preset'],
    ['forcedOffRows', 'SelectMachine.cpp', 'row->setValue("off");\n            row->enable(false);', 'row->enable(false);'],
    ['toggleWritesPreset', 'SelectMachine.cpp', 'm_option_express->restoreValue();\n        return;', 'return;'],
    ['toggleWritesPreset', 'SelectMachine.cpp', 'tab->load_config(cfg);', 'tab->get_config()->apply(cfg);'],
    ['jobCarriesFlagsOff', 'SelectMachine.cpp', 'prepare.bed_leveling_on,\n        prepare.flow_cali_on,', '(m_checkbox_list["bed_leveling"]->getValue() == "on"),\n        prepare.flow_cali_on,'],
    ['jobCarriesFlagsOff', 'SendPrintReadiness.hpp', 'return express ? PrintPrepareChoice{} : chosen;', 'return chosen;'],
    ['otherSendersFollow', 'SendMultiMachinePage.cpp', 'bed_leveling = false;\n        flow_cali    = false;', 'flow_cali    = false;'],
    ['otherSendersFollow', 'AutomationBridge.cpp', 'a.value("bedLeveling",!express)', 'a.value("bedLeveling",true)'],
    ['stringsTranslated', 'BambuStudio_yue_HK.po', 'msgid "Turned off by Express print mode."\nmsgstr "', 'msgid "Turned off by Express print mode."\nmsgstr ""\n#'],
  ];
  for (const [name, file, from, to] of mutations) {
    assert.ok(sources[file].includes(from), `mutation source for ${name} must exist: ${from.slice(0, 60)}`);
    const mutated = { ...sources, [file]: sources[file].replace(from, to) };
    assert.throws(() => checks[name](mutated), `${name} must reject: ${from.trim().slice(0, 60)}`);
  }
});
