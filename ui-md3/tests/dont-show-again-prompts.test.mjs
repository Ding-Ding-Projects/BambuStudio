import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Two prompts used to come back every single time: the question about syncing the nozzle and
// dispenser information that appears before slicing, and the prompt about a new ink that opens
// from the new-ink badge. Each now offers the kit "Don't show again" check box and remembers the
// answer in the app configuration, and each answer can be undone from Preferences.
//
// The checks read the source, not a build. DONT_SHOW_AGAIN_SOURCE_ROOT points the test at another
// copy of the tree (for example a checkout of an older revision) so the test can be shown to fail
// on the old source.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = process.env.DONT_SHOW_AGAIN_SOURCE_ROOT
  ? path.resolve(process.env.DONT_SHOW_AGAIN_SOURCE_ROOT)
  : path.resolve(testDir, '..', '..');

const noComments = (text) => text
  .replace(/\r\n/g, '\n')
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^[ \t]*\/\/.*$/gm, '')
  .replace(/[ \t]+\/\/ .*$/gm, '');
const read = (...parts) => noComments(readFileSync(path.join(repoDir, ...parts), 'utf8'));
const readRaw = (...parts) => readFileSync(path.join(repoDir, ...parts), 'utf8').replace(/\r\n/g, '\n');
const gui = (file) => ['src', 'slic3r', 'GUI', file];

// A block that is missing reads as empty, so each test below fails on its own checks instead of
// the whole file failing to load; the first test names every block that was not found.
const missingBlocks = [];
function between(text, start, end, what) {
  const from = text.search(start);
  if (from === -1) {
    missingBlocks.push(what);
    return '';
  }
  const rest = text.slice(from);
  const to = rest.slice(1).search(end);
  if (to === -1) {
    missingBlocks.push(`${what} (no end)`);
    return '';
  }
  return rest.slice(0, to + 1);
}

// The body of one function: from its signature to the next top-level definition.
const plater = read(...gui('Plater.cpp'));
const syncCheck = between(plater, /bool Plater::priv::check_ams_status_impl\(/, /\nbool Plater::priv::get_machine_sync_status\(/, 'check_ams_status_impl');

const statusPanel = read(...gui('StatusPanel.cpp'));
const showDialog = between(statusPanel, /void StatusPanel::show_new_official_filament_dlg\(/, /\nvoid StatusPanel::on_new_official_filament_hint\(/, 'show_new_official_filament_dlg');
const openHint = between(statusPanel, /void StatusPanel::open_new_official_filament_hint\(/, /\n\}\n/, 'open_new_official_filament_hint');
const dismissAll = between(statusPanel, /void StatusPanel::dismiss_all_filament_hint_ui\(/, /\nvoid StatusPanel::show_new_official_filament_dlg\(/, 'dismiss_all_filament_hint_ui');

const inkHpp = read(...gui('AMSMaterialsSetting.hpp'));
const inkCpp = read(...gui('AMSMaterialsSetting.cpp'));
const inkDialogCreate = between(inkCpp, /void AMSNewOfficialFilamentDlg::create\(\)/, /\nvoid AMSNewOfficialFilamentDlg::on_record_new\(/, 'AMSNewOfficialFilamentDlg::create');
const inkDialogContext = between(inkCpp, /void AMSNewOfficialFilamentDlg::SetTrayContext\(/, /\nvoid AMSNewOfficialFilamentDlg::SetSoftMatchData\(/, 'AMSNewOfficialFilamentDlg::SetTrayContext');

const guiAppHpp = read(...gui('GUI_App.hpp'));
const guiAppCpp = read(...gui('GUI_App.cpp'));
const notify = between(guiAppCpp, /void GUI_App::notify_new_rfid_filament\(/, /\n\}\n/, 'notify_new_rfid_filament');

const filaSync = read(...gui('fila_manager/wgtFilaManagerSync.cpp'));
const drainHints = between(filaSync, /void wgtFilaManagerSync::drain_filament_hints\(/, /\n\}\n/, 'drain_filament_hints');

const preferences = read(...gui('Preferences.cpp'));
const resetAll = between(preferences, /void PreferencesDialog::on_reset_all_warnings\(\)/, /\n\}\n/, 'on_reset_all_warnings');
const resetDialog = between(preferences, /ResetWarningsDialog::ResetWarningsDialog\(/, /\nvoid ResetWarningsDialog::toggle_details\(/, 'ResetWarningsDialog');

// One entry of a catalogue: the message and what it shows, read whole.
function catalogValue(text, msgid) {
  const escaped = msgid.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const match = text.match(new RegExp(`^msgid "${escaped}"\\nmsgstr "((?:[^"\\\\\\n]|\\\\.)*)"$`, 'm'));
  return match ? match[1] : undefined;
}

test('every function the checks below read is in the source', () => {
  assert.deepEqual(missingBlocks, [], 'the blocks the prompts are built from');
});

test('the sync prompt offers the kit "Don\'t show again" check box', () => {
  assert.match(syncCheck, /struct SyncInfoDialog : MessageDialog/, 'the prompt is still the sync prompt');
  assert.match(syncCheck, /dlg\.show_dsa_button\(\);/, 'the check box is added to the prompt');
  assert.match(syncCheck, /add_button\(wxID_YES, true, _L\("Sync now"\)\);/, 'Sync now stays');
  assert.match(syncCheck, /add_button\(wxID_NO, true, _L\("Later"\)\);/, 'Later stays');
});

test('the sync prompt remembers Sync now as "sync" and Later or close as "later", only when ticked', () => {
  assert.match(syncCheck, /if \(dlg\.get_checkbox_state\(\)\)\s*wxGetApp\(\)\.app_config->set\("sync_ams_info_choice", sync_now \? "sync" : "later"\);/);
  assert.match(syncCheck, /sync_now = dlg\.ShowModal\(\) == wxID_YES;/, 'only the Sync now button means sync');
});

test('a remembered "later" skips the prompt and slices; a remembered "sync" syncs without asking', () => {
  assert.match(syncCheck, /wxGetApp\(\)\.app_config->get\("sync_ams_info_choice"\)/);
  const later = syncCheck.search(/remembered_choice == "later"/);
  const dialog = syncCheck.search(/struct SyncInfoDialog/);
  assert.notEqual(later, -1, 'the stored "later" is read');
  assert.ok(later < dialog, 'the stored "later" is read before the prompt can open');
  assert.match(syncCheck, /remembered_choice == "later"[^;]*\)\s*return true;/, '"later" lets slicing go on');
  assert.match(syncCheck, /bool sync_now = remembered_choice == "sync";\s*if \(!sync_now\) \{/, '"sync" never opens the prompt');
  // Both answers reach the same sync as the Sync now button.
  const syncCalls = syncCheck.match(/sidebar\(\)\.sync_extruder_list\(\)/g) ?? [];
  assert.equal(syncCalls.length, 1, 'one sync call serves the button and the stored answer');
  assert.match(syncCheck, /if \(synced && wxGetApp\(\)\.check_slice_version_policy\(\)\)/);
  assert.match(syncCheck, /wxPostEvent\(q, SimpleEvent\(EVT_GLTOOLBAR_SLICE_ALL\)\);/);
  assert.match(syncCheck, /wxPostEvent\(q, SimpleEvent\(EVT_GLTOOLBAR_SLICE_PLATE\)\);/);
});

test('a remembered answer never blocks slicing and never loops', () => {
  // The slice re-posted after an automatic sync comes back through the check once; the mark is
  // read and cleared first thing, so it cannot outlive that visit or stay set after an early exit.
  assert.match(plater, /bool m_auto_sync_ams_pending\{false\};/, 'the mark is a member of the plater');
  const mark = syncCheck.search(/const bool came_back_from_auto_sync = m_auto_sync_ams_pending;\s*m_auto_sync_ams_pending = false;/);
  const firstExit = syncCheck.search(/return true;/);
  assert.notEqual(mark, -1, 'the mark is read and cleared');
  assert.ok(mark < firstExit, 'the mark is cleared before any early return');
  assert.match(syncCheck, /remembered_choice == "sync" && came_back_from_auto_sync/, 'a second visit after the sync slices as it is');
  assert.match(syncCheck, /m_auto_sync_ams_pending = wxGetApp\(\)\.app_config->get\("sync_ams_info_choice"\) == "sync";/, 'only a remembered sync sets the mark');
  assert.match(syncCheck, /else if \(!synced && remembered_choice == "sync"\)\s*\{\s*return true;\s*\}/, 'a stored sync that cannot run carries on unsynced');
});

test('the new-ink prompt has the kit check box and keeps its two buttons', () => {
  assert.match(inkHpp, /::CheckBox\*\s+m_chk_dont_show\s*\{\s*nullptr\s*\};/);
  assert.match(inkHpp, /bool GetDontShowAgain\(\) const \{ return m_chk_dont_show && m_chk_dont_show->GetValue\(\); \}/);
  assert.match(inkDialogCreate, /m_chk_dont_show = new ::CheckBox\(this\);/);
  assert.match(inkDialogCreate, /new Label\(this, _L\("Don't show again"\)\)/, 'the label is the translated "Don\'t show again"');
  assert.match(inkDialogCreate, /new Button\(this, _L\("Add as new filament"\)\)/, 'Add as new stays');
  assert.match(inkDialogCreate, /new Button\(this, _L\("Confirm"\)\)/, 'Confirm stays');
  assert.match(inkDialogContext, /if \(m_chk_dont_show\)\s*m_chk_dont_show->SetValue\(false\);/, 'each opening starts with the box clear');
});

test('a ticked new-ink prompt is remembered however it closes, and takes the badge down', () => {
  const tick = showDialog.search(/m_new_official_filament_dlg->GetDontShowAgain\(\)/);
  const okBranch = showDialog.search(/if \(rc == wxID_OK\)/);
  assert.notEqual(tick, -1, 'the tick is read');
  assert.ok(tick < okBranch, 'the tick is read outside the OK branch, so the close button counts too');
  assert.match(showDialog, /wxGetApp\(\)\.app_config->set\("hide_new_filament_prompt", "1"\);/);
  assert.match(showDialog, /dismiss_filament_hint_ui\(dev_id, ams_id, slot_id\);\s*dismiss_all_filament_hint_ui\(\);/, 'this badge and every other one go');
  assert.match(showDialog, /Choice::LinkExisting/, 'the buttons still do what they did');
  assert.match(showDialog, /Choice::RecordNew/);
  // The sweep reaches the pending record, the native slot badge and the Web page badge.
  assert.match(dismissAll, /fila_manager_sync\(\)/);
  assert.match(dismissAll, /dismiss_pending_badge\(/);
  assert.match(dismissAll, /m_ams_control->dismiss_filament_hint\(/);
  assert.match(dismissAll, /DevicePageAmsControlWebVM::DismissFilamentMgrHint\(/);
});

test('while the setting stands no new-ink badge or prompt is offered, native or Web', () => {
  assert.match(guiAppHpp, /bool is_new_filament_prompt_hidden\(\) const \{ return app_config && app_config->get\("hide_new_filament_prompt"\) == "1"; \}/);
  // The one place a pending badge is shown, on the native slot and on the Web page.
  const hidden = notify.search(/if \(is_new_filament_prompt_hidden\(\)\)\s*return;/);
  const webShow = notify.search(/NotifyNewRfidFilament\(/);
  const nativeShow = notify.search(/show_ams_filament_hint\(/);
  assert.notEqual(hidden, -1, 'the badge is skipped while the setting stands');
  assert.ok(hidden < webShow && hidden < nativeShow, 'before either badge is shown');
  assert.match(drainHints, /if \(wxGetApp\(\)\.is_new_filament_prompt_hidden\(\)\)\s*return;/);
  // A badge is not even recorded, but the new spool still reaches the Ink Manager.
  assert.match(filaSync, /if \(!wxGetApp\(\)\.is_new_filament_prompt_hidden\(\)\)\s*m_pending_badges\[/);
  assert.match(filaSync, /client\.sync_ams\(/, 'registering the spool is unchanged');
  // And the prompt itself never opens, from a native badge or from the Web page.
  const hiddenOpen = openHint.search(/if \(wxGetApp\(\)\.is_new_filament_prompt_hidden\(\)\)/);
  const created = openHint.search(/new AMSNewOfficialFilamentDlg\(/);
  assert.notEqual(hiddenOpen, -1, 'opening the prompt is refused while the setting stands');
  assert.ok(hiddenOpen < created, 'before the dialog is created');
  assert.match(openHint, /dismiss_filament_hint_ui\(obj \? obj->get_dev_id\(\) : std::string\(\), ams_id, slot_id\);\s*return;/);
});

test('Reset warnings clears both answers and lists them for the person', () => {
  assert.match(resetAll, /app_config->erase\("app", "sync_ams_info_choice"\);/);
  assert.match(resetAll, /app_config->erase\("app", "hide_new_filament_prompt"\);/);
  assert.match(resetDialog, /_L\("- Sync nozzle and AMS information before slicing"\)/);
  assert.match(resetDialog, /_L\("- New filament badge and prompt"\)/);
  // The existing list keeps its own message, so its translations survive.
  assert.match(resetDialog, /_L\("- Sync printer presets after loading a file\\n"/);
});

test('every new message has its English override and its Cantonese translation', () => {
  const template = readRaw('bbl', 'i18n', 'BambuStudio.pot');
  const english = readRaw('bbl', 'i18n', 'en', 'BambuStudio_en.po');
  const cantonese = readRaw('bbl', 'i18n', 'yue_HK', 'BambuStudio_yue_HK.po');

  const syncLine = '- Sync nozzle and AMS information before slicing';
  const badgeLine = '- New filament badge and prompt';
  for (const msgid of [syncLine, badgeLine, "Don't show again"])
    assert.notEqual(catalogValue(template, msgid), undefined, `the template extracts ${msgid}`);

  assert.equal(catalogValue(english, syncLine), '- Sync nozzle and Ink Dispenser information before slicing');
  assert.equal(catalogValue(english, badgeLine), '- New ink badge and prompt');
  assert.equal(catalogValue(english, "Don't show again"), "Don't show again");

  assert.equal(catalogValue(cantonese, syncLine), '- 切片前同步噴嘴同墨水機資訊');
  assert.equal(catalogValue(cantonese, badgeLine), '- 新墨水角標同提示');
  assert.equal(catalogValue(cantonese, "Don't show again"), '唔再顯示');
  for (const msgid of [syncLine, badgeLine]) {
    const entry = cantonese.slice(cantonese.indexOf(`msgid "${msgid}"`) - 120, cantonese.indexOf(`msgid "${msgid}"`));
    assert.match(entry, /#\. reviewed-category: preferences\n#\. review-status: agent-drafted\n$/, `${msgid} carries the review comments`);
  }
});
