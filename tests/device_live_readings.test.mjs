// Source-pin and behaviour test for the live printer readings on the print monitor and for the
// Print Options / Safety Options entry rules.
//
// Part 1 compiles the real, standard-library-only header DeviceLiveReadings.hpp into a fixture and
// checks its rules. Part 2 pins the production wiring in StatusPanel and SafetyOptionsDialog to the
// protocol fields the application already parses, so a reading cannot be added or kept without its
// parser. Runtime behaviour on a real printer is not covered here.
//
// Pass --negative to confirm the checks fail when a wiring line is removed.
import {readFileSync, writeFileSync, mkdtempSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => readFileSync(path.join(root, rel), 'utf8').replace(/\r\n/g, '\n');
const negative = process.argv.includes('--negative');

let failures = 0;
const check = (ok, name) => {
  console.log((ok ? 'PASS ' : 'FAIL ') + name);
  if (!ok) failures++;
};

// ----- Part 2 inputs ---------------------------------------------------------------------------
let statusPanel = read('src/slic3r/GUI/StatusPanel.cpp');
const statusHeader = read('src/slic3r/GUI/StatusPanel.hpp');
const safetyDialog = read('src/slic3r/GUI/SafetyOptionsDialog.cpp');
const printOptionsDialog = read('src/slic3r/GUI/PrintOptionsDialog.cpp');
const deviceManager = read('src/slic3r/GUI/DeviceManager.cpp');
const devFan = read('src/slic3r/GUI/DeviceCore/DevFan.cpp');
const devFanHeader = read('src/slic3r/GUI/DeviceCore/DevFan.h');
const devPrintOptions = read('src/slic3r/GUI/DeviceCore/DevPrintOptions.cpp');
const deviceHeader = read('src/slic3r/GUI/DeviceManager.hpp');
const cmake = read('src/slic3r/CMakeLists.txt');
const strip = read('src/slic3r/GUI/Widgets/DeviceLiveStrip.cpp');
if (negative) statusPanel = statusPanel.replace('DeviceLive::parse_wifi_dbm(obj->wifi_signal)', 'true');

function method(source, signature) {
  const start = source.indexOf(signature);
  if (start < 0) throw new Error('Missing production method: ' + signature);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let i = open; i < source.length; i++) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}' && --depth === 0) return source.slice(start, i + 1);
  }
  throw new Error('Unterminated method: ' + signature);
}

const live = method(statusPanel, 'void StatusPanel::update_live_readings');

// Every reading is fed from a parsed field.
check(deviceManager.includes('wifi_signal = jj["wifi_signal"].get<std::string>();'), 'wifi_signal is parsed from the status message');
check(live.includes('DeviceLive::parse_wifi_dbm(obj->wifi_signal)'), 'Wi-Fi chip reads the parsed wifi_signal');
check(devFan.includes('airduct["modeCur"]') && live.includes('GetAirDuctData().curren_mode') && live.includes('GetSupportAirduct()'),
      'air duct chip reads modeCur and is gated on the reported air duct');
check(deviceManager.includes('xcam_door_open_check = (DoorOpenCheckState) get_flag_bits(cfg, 20, 2);') && live.includes('get_door_open_check_state()') &&
      live.includes('support_door_open_check()'), 'door detection chip reads the parsed setting and is gated on the support flag');
check(live.includes('GetExtNozzle(ext_id)') && live.includes('nozzle_reading_usable') && live.includes('IsInfoReliable()'),
      'nozzle chips read per-extruder nozzles and require reliable information');
check(read('src/slic3r/GUI/DeviceManager.cpp').includes('door_open_bit = (flag >> 23) & 0x1;') && deviceManager.includes('door_open_bit    = get_flag_bits(stat, 23);'),
      'door sensor chip is fed from home_flag or print.stat bit 23');
check(live.includes('DeviceLive::door_open_reading(obj->door_open_bit, obj->support_door_open_check())') && live.includes('_L("Door (beta)")'),
      'door sensor chip is gated on door support and labelled beta, because the bit comes from community notes');

// Honest labelling and localization.
check(live.includes('_L("Open Door Detection")'), 'door chip is labelled as a detection setting, not a sensor');
for (const literal of ['_L("Wi-Fi signal")', '_L("Strong")', '_L("Fair")', '_L("Weak")', '_L("Air duct")', '_L("Cooling")', '_L("Heating")', '_L("Exhaust")', '_L("Full Cooling")', '_L("Nozzle")'])
  check(live.includes(literal), 'localized literal ' + literal);
check(strip.includes('_L("Live printer readings")') && strip.includes('SetName(full)') && strip.includes('SetToolTip(full)') && strip.includes('AcceptsFocus() const override { return false; }'),
      'strip and chips expose an accessible name and tooltip');
check(strip.includes('Show(!m_readings.empty())'), 'strip hides when nothing is reported');

// Wiring.
check(statusHeader.includes('DeviceLiveStrip *m_live_strip') && statusHeader.includes('void update_live_readings(MachineObject *obj);'), 'header declares the strip and updater');
check(statusPanel.includes('update_live_readings(obj);') && statusPanel.includes('m_live_strip = new DeviceLiveStrip(parent);'), 'strip is created and refreshed');
check(cmake.includes('GUI/Widgets/DeviceLiveStrip.cpp') && cmake.includes('GUI/DeviceLiveReadings.hpp'), 'new files are registered with the build');

// Enumeration mirrors.
check(/AIR_DUCT_NONE\s*=\s*-1,\s*AIR_DUCT_COOLING_FILT\s*=\s*0,\s*AIR_DUCT_HEATING_INTERNAL_FILT,\s*AIR_DUCT_EXHAUST,\s*AIR_DUCT_FULL_COOLING/.test(devFanHeader.replace(/\s+/g, ' ')),
      'air duct ids mirror DevFan.h');
check(/DOOR_OPEN_CHECK_DISABLE\s*=\s*0[\s\S]*DOOR_OPEN_CHECK_ENABLE_WARNING\s*=\s*1[\s\S]*DOOR_OPEN_CHECK_ENABLE_PAUSE_PRINT\s*=\s*2/.test(deviceHeader), 'door check states mirror DeviceManager.hpp');

// Print Options entry covers every capability the dialog can show.
const optionsGate = statusPanel.slice(statusPanel.indexOf('print_option_entries[]'), statusPanel.indexOf('m_options_btn->Show();'));
const enumsInDialog = [...printOptionsDialog.matchAll(/PrintOptionEnum::(\w+)/g)].map((m) => m[1]);
const dialogOnly = new Set(enumsInDialog);
let missing = [];
for (const name of dialogOnly) {
  // Open_Door and Idle_Heating are routed through the door/safety rules, and the save-to-storage row is gated on SupportSaveRemotePrintFileToStorage.
  if (['Open_Door_Detection', 'Idle_Heating_Protect_Detection', 'Save_Remote_Print_File_To_Storage', 'First_Layer_Detection'].includes(name)) continue;
  if (!optionsGate.includes('PrintOptionEnum::' + name)) missing.push(name);
}
check(missing.length === 0, 'Print Options entry lists every dialog capability' + (missing.length ? ' (missing: ' + missing.join(', ') + ')' : ''));
check(optionsGate.includes('SupportSaveRemotePrintFileToStorage()') && optionsGate.includes('SupportFirstLayerInspect()') && optionsGate.includes('SupportAIMonitor()'),
      'Print Options entry keeps the config-flag capabilities');
check(devPrintOptions.includes('m_idel_heating_protect_detection.is_support_detect = DevUtil::get_flag_bits(fun, 62);'), 'idle heating support is parsed from fun bit 62');

// Safety entry: idle heating no longer depends on the door-check flag or the config switch.
const showSafety = method(statusPanel, 'void StatusPanel::on_show_safety_options');
check(showSafety.includes('DeviceLive::safety_entry_visible(') && statusPanel.includes('DeviceLive::safety_entry_visible(supports_safety, obj->support_door_open_check(), idle_heating_supported)'),
      'Safety entry uses the shared visibility rule in the button and the handler');
check(safetyDialog.includes('DeviceLive::safety_dialog_shows_door_check('), 'Safety dialog hides the duplicate door control where Print Options owns it');
check(printOptionsDialog.includes('// Hide door open check for printers that support safety options') && printOptionsDialog.includes('if (supports_safety) {'),
      'Print Options still owns the door control on printers without the safety dialog');

// ----- Part 1: compile the real header ---------------------------------------------------------
const fixture = `#include <iostream>
#include <string>
#include "DeviceLiveReadings.hpp"
using namespace Slic3r::GUI::DeviceLive;
int main() {
  int failures = 0;
  auto check = [&](bool ok, const char* name) { std::cout << (ok ? "PASS " : "FAIL ") << name << '\\n'; failures += !ok; };
  check(parse_wifi_dbm("-61dBm") == -61, "parses -61dBm");
  check(parse_wifi_dbm("-61 dBm") == -61, "parses -61 dBm");
  check(parse_wifi_dbm(" -45 ") == -45, "parses a bare number");
  check(parse_wifi_dbm("-61DBM") == -61, "unit is case-insensitive");
  check(!parse_wifi_dbm("").has_value(), "empty text is unknown");
  check(!parse_wifi_dbm("dBm").has_value(), "unit only is unknown");
  check(!parse_wifi_dbm("-61dB").has_value(), "wrong unit is unknown");
  check(!parse_wifi_dbm("-61dBmx").has_value(), "trailing text is unknown");
  check(!parse_wifi_dbm("-200dBm").has_value(), "out of range is unknown");
  check(!parse_wifi_dbm("12dBm").has_value(), "positive value is unknown");
  check(classify_wifi_dbm(-60) == WifiQuality::Strong && classify_wifi_dbm(-30) == WifiQuality::Strong, "strong at or above -60");
  check(classify_wifi_dbm(-61) == WifiQuality::Fair && classify_wifi_dbm(-75) == WifiQuality::Fair, "fair between -61 and -75");
  check(classify_wifi_dbm(-76) == WifiQuality::Weak, "weak below -75");
  check(air_duct_mode_label(0) == AirDuctModeLabel::Cooling && air_duct_mode_label(1) == AirDuctModeLabel::Heating &&
        air_duct_mode_label(2) == AirDuctModeLabel::Exhaust && air_duct_mode_label(3) == AirDuctModeLabel::FullCooling, "air duct modes map");
  check(!air_duct_mode_label(-1).has_value() && !air_duct_mode_label(255).has_value() && !air_duct_mode_label(4).has_value(), "unreported or unknown air duct mode is hidden");
  check(door_check_setting(0) == DoorCheckSetting::Off && door_check_setting(1) == DoorCheckSetting::Warning && door_check_setting(2) == DoorCheckSetting::PausePrint, "door setting maps");
  check(!door_check_setting(3).has_value() && !door_check_setting(-1).has_value(), "unknown door setting is hidden");
  check(door_open_reading(1, true) == true && door_open_reading(0, true) == false, "door sensor bit maps to open and closed");
  check(!door_open_reading(1, false).has_value() && !door_open_reading(-1, true).has_value(), "door sensor hidden without support or report");
  check(nozzle_reading_usable(0.4f, true, true, false), "reliable nozzle is shown");
  check(!nozzle_reading_usable(0.0f, true, true, false), "zero diameter is hidden");
  check(!nozzle_reading_usable(0.4f, false, true, false), "unknown material is hidden");
  check(!nozzle_reading_usable(0.4f, true, false, false), "unreliable nozzle is hidden");
  check(!nozzle_reading_usable(0.4f, true, true, true), "unknown or abnormal nozzle is hidden");
  check(format_nozzle_diameter(0.4f) == "0.4 mm" && format_nozzle_diameter(0.6f) == "0.6 mm", "diameter formats with one decimal");
  check(safety_entry_visible(true, true, false), "safety entry: configured and door check");
  check(!safety_entry_visible(true, false, false), "safety entry: configured but nothing to show stays hidden");
  check(!safety_entry_visible(false, true, false), "safety entry: door check alone stays in Print Options");
  check(safety_entry_visible(false, false, true), "safety entry: idle heating reaches printers without the safety switch");
  check(safety_entry_visible(false, true, true), "safety entry: idle heating plus door check");
  check(safety_dialog_shows_door_check(true, true) && !safety_dialog_shows_door_check(false, true) && !safety_dialog_shows_door_check(true, false), "safety dialog door control rule");
  std::cout << (failures ? "FAILED" : "all header checks passed") << '\\n';
  return failures ? 1 : 0;
}
`;
const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-live-readings-'));
const cpp = path.join(output, 'fixture.cpp');
const exe = path.join(output, 'fixture.exe');
writeFileSync(cpp, fixture);
const include = path.join(root, 'src/slic3r/GUI');
let compile = compileFixture({source: cpp, exe, cwd: output, include});
if (compile.error && compile.error.code === 'ENOENT') {
  // Windows host without a developer prompt: fall back to the GNU toolchain if present.
  compile = spawnSync(process.env.CXX || 'g++', ['-std=c++17', '-Wall', '-Wextra', '-I' + include, cpp, '-o', exe], {cwd: output, stdio: 'inherit'});
}
if (compile.error) {
  check(false, 'compile fixture (' + compile.error.message + ')');
} else if (compile.status !== 0) {
  check(false, 'compile fixture');
} else {
  const run = spawnSync(exe, [], {cwd: output, stdio: 'inherit'});
  check(run.status === 0, 'header fixture passes');
}

console.log(failures ? failures + ' check(s) failed' : 'all checks passed');
process.exit(failures ? 1 : 0);
