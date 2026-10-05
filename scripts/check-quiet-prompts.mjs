import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const rules = [
  ['src/slic3r/GUI/MainFrame.cpp', /_L\("First Guide"\)/, 'first-use video invitation'],
  ['src/slic3r/GUI/GUI_App.cpp', /DimSumSurprise::maybe_show_after_startup\s*\(/, 'startup random card'],
  ['src/slic3r/GUI/GUI_App.cpp', /DevicePageAmsControlWebVM::NotifyNewRfidFilament\s*\(/, 'automatic RFID badge'],
  ['src/slic3r/GUI/Plater.cpp', /get\("helio_first_time_tutorial"\)/, 'automatic Helio coaching'],
  ['src/slic3r/GUI/Plater.cpp', /get\("show_fila_switch_tips"\)/, 'first switcher introduction'],
  ['src/slic3r/GUI/NotificationManager.cpp', /retrieve_data_from_hint_database\s*\(/, 'automatic slicing tips'],
  ['src/slic3r/GUI/SlicingProgressNotification.cpp', /render_dailytips_panel\(dailytips_pos/, 'slicing education panel'],
  ['src/slic3r/GUI/HelioReleaseNote.cpp', /set\("helio_first_time_tutorial", "active"\)/, 'Helio coaching activation'],
];

function validate(read) {
  for (const [file, forbidden, name] of rules)
    if (forbidden.test(read(file))) throw new Error(`${name} returned in ${file}`);
  const main = read('src/slic3r/GUI/MainFrame.cpp');
  if (!main.includes('play_dual_extruder_print_tpu_video();') || !main.includes('play_dual_extruder_slice_video();'))
    throw new Error('Deliberate tutorial actions are missing');
  const app = read('src/slic3r/GUI/GUI_App.cpp');
  if (!/if \(by_user != 0\) push_auto_update_ready_notification\(tag\)/.test(app))
    throw new Error('Automatic update ready presentation is not restricted to an explicit request');
  if (!/if \(by_user != 0\)\s+CallAfter\(\[this, by_user\]\(\) \{ GUI::wxGetApp\(\).request_new_version/.test(app))
    throw new Error('Portable update offer is not restricted to an explicit request');
  if (!/check_config_updates_from_menu\(\)[\s\S]*?check_updates\(true\)/.test(app))
    throw new Error('Manual preset update action is missing');
  if (!/actions.show_filament_mgr_hint = false;/.test(read('src/slic3r/GUI/DeviceWeb/ViewModels/DevicePage/AmsControlWeb/ViewModelDisplayBuilder.cpp')))
    throw new Error('Embedded slot hints can be resurrected');
}
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
validate(read);
let mutations = 0;
for (const [file, , name] of rules) {
  const examples = {
    'first-use video invitation': '_L("First Guide")',
    'startup random card': 'DimSumSurprise::maybe_show_after_startup(false);',
    'automatic RFID badge': 'DevicePageAmsControlWebVM::NotifyNewRfidFilament();',
    'automatic Helio coaching': 'get("helio_first_time_tutorial")',
    'first switcher introduction': 'get("show_fila_switch_tips")',
    'automatic slicing tips': 'retrieve_data_from_hint_database(',
    'slicing education panel': 'render_dailytips_panel(dailytips_pos',
    'Helio coaching activation': 'set("helio_first_time_tutorial", "active")',
  };
  let rejected = false;
  try { validate(candidate => read(candidate) + (candidate === file ? `\n${examples[name]}` : '')); }
  catch { rejected = true; }
  if (!rejected) throw new Error(`Negative regression missed ${name}`);
  mutations++;
}
console.log(`Quiet prompt source contract passed: ${rules.length} retired producers, ${mutations} negative regressions; native runtime remains separately verified.`);
