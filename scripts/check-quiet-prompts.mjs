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

// A function that starts at column 0 ends at the first closing brace at column 0.
function bodyOf(source, signature) {
  const start = source.indexOf(signature);
  if (start < 0) throw new Error(`${signature} is missing`);
  const end = source.indexOf('\n}', start);
  return source.slice(start, end < 0 ? source.length : end + 2);
}

// Every call of the modal download dialog in the update route, each one guarded by an explicit request.
const updateRoutes = ['void GUI_App::check_new_version(', 'void GUI_App::start_auto_update('];
const guardedOffer = /if \(by_user != 0(?: && \w+)?\)\s+(?:CallAfter\(\[this, by_user\]\(\) \{ GUI::wxGetApp\(\)\.)?request_new_version\(by_user\)/g;
// Background updates may only use these non-blocking notices.
const backgroundNotices = ['static void push_auto_update_ready_notification(', 'static void push_auto_update_failed_notification('];

function validate(read) {
  for (const [file, forbidden, name] of rules)
    if (forbidden.test(read(file))) throw new Error(`${name} returned in ${file}`);
  const main = read('src/slic3r/GUI/MainFrame.cpp');
  if (!main.includes('play_dual_extruder_print_tpu_video();') || !main.includes('play_dual_extruder_slice_video();'))
    throw new Error('Deliberate tutorial actions are missing');
  const app = read('src/slic3r/GUI/GUI_App.cpp');
  for (const route of updateRoutes) {
    const body = bodyOf(app, route);
    const offers = body.split('request_new_version(').length - 1;
    const guarded = (body.match(guardedOffer) || []).length;
    if (offers === 0 || guarded !== offers)
      throw new Error(`Update download dialog in ${route} is not restricted to an explicit request`);
    if (/ShowModal\(|wxMessageBox\(|MessageDialog/.test(body))
      throw new Error(`Update route ${route} opens a modal dialog of its own`);
  }
  for (const notice of backgroundNotices) {
    const body = bodyOf(app, notice);
    if (!/manager->push_(?:app_update_ready_)?notification\(/.test(body) || /ShowModal\(|wxMessageBox\(|MessageDialog|request_new_version\(/.test(body))
      throw new Error(`Background update notice ${notice} is not a non-blocking notification`);
  }
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
// The update route: an unguarded download dialog, a modal in the route, or a modal behind a
// background notice must each be rejected.
const appFile = 'src/slic3r/GUI/GUI_App.cpp';
const updateMutations = {
  'unguarded background download dialog': app => app.replace('if (by_user != 0) request_new_version(by_user);\n                break;', 'request_new_version(by_user);\n                break;'),
  'unguarded portable download dialog': app => app.replace(/if \(by_user != 0\)\s+CallAfter\(\[this, by_user\]\(\) \{ GUI::wxGetApp\(\)\.request_new_version/, 'CallAfter([this, by_user]() { GUI::wxGetApp().request_new_version'),
  'modal in the update route': app => app.replace('push_auto_update_failed_notification(tag);\n', 'push_auto_update_failed_notification(tag);\n                wxMessageBox("update");\n'),
  'modal behind the failure notice': app => app.replace('const std::string page_url = release_page_url(tag);', 'const std::string page_url = release_page_url(tag);\n    UpdateVersionDialog(nullptr).ShowModal();'),
};
for (const [name, mutate] of Object.entries(updateMutations)) {
  const mutated = mutate(read(appFile));
  if (mutated === read(appFile)) throw new Error(`Negative regression could not apply ${name}`);
  let rejected = false;
  try { validate(candidate => candidate === appFile ? mutated : read(candidate)); }
  catch { rejected = true; }
  if (!rejected) throw new Error(`Negative regression missed ${name}`);
  mutations++;
}
console.log(`Quiet prompt source contract passed: ${rules.length} retired producers, ${mutations} negative regressions; native runtime remains separately verified.`);
