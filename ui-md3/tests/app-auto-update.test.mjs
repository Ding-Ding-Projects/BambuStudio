import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A copy installed by the Squirrel.Windows installer updates itself
// (docs/features/windows/app-updates.md). These contracts pin what has to stay
// true for that to be safe and honest: only a Squirrel install takes the route,
// Squirrel is pointed at one fixed feed and nothing else reaches its command
// line, the app downloads and runs nothing itself, the helper never opens a
// console window, the restart waits for the app to exit, a cancelled close
// never restarts, the release check compares UTC times, and a background check
// reports through non-blocking notices only. They read the sources as text, so
// they run without a build. tests/app_update_check_policy_test.cpp executes the
// decisions themselves.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');

// "must contain" assertions run against code only, so a comment that merely
// names a construct cannot satisfy them.
const stripComments = (source) => source
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^[ \t]*\/\/.*$/gm, '')
  .replace(/[ \t]+\/\/[^"\n]*$/gm, '');

const escapeRegExp = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

// A function or method that starts at column 0 ends at the first closing brace
// at column 0.
const bodyOf = (source, signature) => {
  const start = source.indexOf(signature);
  assert.ok(start >= 0, `${signature} must exist`);
  const end = source.indexOf('\n}', start);
  assert.ok(end > start, `${signature} must end with a closing brace at column 0`);
  return source.slice(start, end + 2);
};

// The msgids of a gettext catalogue, with wrapped lines joined.
const catalogueIds = (po) => {
  const ids = new Set();
  const lines = po.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    if (!lines[i].startsWith('msgid ')) continue;
    let text = '';
    let line = lines[i].slice('msgid '.length);
    for (;;) {
      const quoted = line.match(/^"(.*)"$/);
      if (!quoted) break;
      text += quoted[1];
      const next = lines[i + 1];
      if (next === undefined || !next.startsWith('"')) break;
      line = next;
      i++;
    }
    ids.add(text.replace(/\\"/g, '"'));
  }
  return ids;
};

const guiApp = stripComments(await read('src', 'slic3r', 'GUI', 'GUI_App.cpp'));
const mainFrame = stripComments(await read('src', 'slic3r', 'GUI', 'MainFrame.cpp'));
const appConfig = stripComments(await read('src', 'libslic3r', 'AppConfig.cpp'));
const preferences = stripComments(await read('src', 'slic3r', 'GUI', 'Preferences.cpp'));
const paletteIndex = stripComments(await read('src', 'slic3r', 'GUI', 'CommandPaletteIndex.cpp'));
const notificationHeader = stripComments(await read('src', 'slic3r', 'GUI', 'NotificationManager.hpp'));
const notificationSource = stripComments(await read('src', 'slic3r', 'GUI', 'NotificationManager.cpp'));
const policy = stripComments(await read('src', 'slic3r', 'GUI', 'AppUpdateCheckPolicy.hpp'));
const guiAppHeader = stripComments(await read('src', 'slic3r', 'GUI', 'GUI_App.hpp'));

const FEED = 'https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download';

test('only a Squirrel install takes the automatic route', () => {
  const helper = bodyOf(guiApp, 'static bool squirrel_update_exe(boost::filesystem::path &out)');
  assert.match(helper, /starts_with\([^;]*"app-"\)/, 'the running copy must sit in an app-<version> folder');
  assert.match(helper, /parent_path\(\)\s*\/\s*"Update\.exe"/, 'Update.exe sits in the parent of the app folder');
  assert.match(helper, /is_regular_file\(/, 'the copy counts as installed only when Update.exe exists');
  assert.match(helper, /return false;[\s\S]*return true;/, 'anything else is not installed');
});

test('the release check hands an installed copy with the preference on to the updater and everyone else to the dialog', () => {
  const check = bodyOf(guiApp, 'void GUI_App::check_new_version(');
  const skip = check.indexOf('"skip_version"');
  const preference = check.search(/auto_update\s*=\s*this->app_config->get_bool\("auto_update"\);/);
  const layout = check.search(/installed\s*=\s*squirrel_update_exe\(update_exe\);/);
  const route = check.search(/switch \(AppUpdateCheckPolicy::decide\(auto_update, installed, newer, by_user != 0, skipped\)\)/);
  const updater = check.search(/case AppUpdateCheckPolicy::Action::RunSquirrelUpdate: \{[^}]*CallAfter\(\[this, tag, name, by_user, newer\]\(\) \{ this->start_auto_update\(tag, name, by_user, newer\); \}\);/);
  const dialog = check.search(/case AppUpdateCheckPolicy::Action::OfferDownload:\s*if \(by_user != 0\)\s*CallAfter\(\[this, by_user\]\(\) \{ GUI::wxGetApp\(\)\.request_new_version\(by_user\); \}\);\s*break;/);
  const none = check.search(/case AppUpdateCheckPolicy::Action::ShowNoNewVersion:\s*if \(show_tips\) this->no_new_version\(\);\s*break;/);
  const nothing = check.search(/case AppUpdateCheckPolicy::Action::Nothing:\s*BOOST_LOG_TRIVIAL\(info\)[^;]*;\s*break;/);
  assert.ok(skip >= 0, 'a skipped version must still be honoured');
  assert.ok(preference > skip && layout > skip, 'the route is chosen after the skip check, from the preference and the install layout');
  assert.ok(route > preference && route > layout, 'one policy decision routes every copy');
  assert.ok(updater > route, 'an installed copy with the preference on starts the automatic update with the time verdict');
  assert.ok(dialog > route, 'every other copy keeps the download dialog, for an explicit check only');
  assert.ok(none > route, 'a manual check with nothing newer says so');
  assert.ok(nothing > route, 'a background check with nothing to do stays silent');
  assert.equal(check.split('request_new_version(').length - 1, 1, 'the release check opens the download dialog in one place');
});

test('the release check compares UTC times without a time-zone API', () => {
  const check = bodyOf(guiApp, 'void GUI_App::check_new_version(');
  assert.match(check, /const std::string built\s*=\s*SLIC3R_BUILD_TIME_UTC;/, 'the build time is the UTC stamp');
  assert.match(check, /const bool newer = AppUpdateCheckPolicy::release_is_newer\(published, built, AppUpdateCheckPolicy::kMarginSeconds\);/);
  assert.doesNotMatch(check, /\bSLIC3R_BUILD_TIME\b|mktime|_mkgmtime|timegm|get_time|difftime/, 'the local-time build stamp and time-zone APIs are gone');
  assert.match(guiApp, /#include "AppUpdateCheckPolicy\.hpp"/);
  assert.match(policy, /kMarginSeconds = 3 \* 60 \* 60;/, 'the three-hour margin stays');
  assert.match(policy, /published - built > margin_seconds;/, 'exactly the margin is not newer');
  assert.doesNotMatch(policy, /mktime|timegm|gmtime|localtime|strptime|get_time|#include <(?:ctime|chrono|iomanip)>|\bwx|boost/, 'the policy is plain arithmetic');
  assert.match(policy, /stamp\.size\(\) != 20/, 'only the exact YYYY-MM-DDTHH:MM:SSZ form is read');
});

test('an installed copy always asks Update.exe, and only a background check honours a skipped release', () => {
  assert.match(
    policy,
    /if \(auto_update_enabled && squirrel_installed\)\s*return manual_check \|\| !tag_is_skipped \? Action::RunSquirrelUpdate : Action::Nothing;/,
    'Update.exe compares package versions, so it runs whatever the time verdict says'
  );
  assert.match(policy, /if \(!manual_check\)\s*return Action::Nothing;/, 'a background check never opens the download dialog');
  assert.match(policy, /return newer_by_time \? Action::OfferDownload : Action::ShowNoNewVersion;/);
});

test('a release check that cannot read the release still lets an installed copy ask Update.exe', () => {
  const check = bodyOf(guiApp, 'void GUI_App::check_new_version(');
  const fallback = bodyOf(guiApp, 'void GUI_App::check_without_release(');
  const onError = check.slice(check.indexOf('.on_error('));
  assert.match(onError, /^\.on_error\(\[this, show_tips, by_user\]\([^)]*\) \{[^}]*this->check_without_release\(show_tips, by_user, /, 'a refused or failed request (a rate-limited 403 or 429, no network) takes the fallback');
  assert.doesNotMatch(onError, /no_new_version\(/, 'a failed request no longer just says nothing is newer');
  assert.match(check, /catch \(\.\.\.\) \{\s*this->check_without_release\(show_tips, by_user, /, 'an unreadable answer takes the fallback');
  assert.match(check, /if \(!j\.contains\("tag_name"\) \|\| !j\.contains\("published_at"\)\) \{\s*this->check_without_release\(show_tips, by_user, /, 'an answer without a tag takes the fallback');
  assert.match(fallback, /skipping\s*=\s*!app_config->get\("app", "skip_version"\)\.empty\(\);/, 'any skipped version keeps a background check quiet, since the feed tag is unknown');
  assert.match(fallback, /switch \(AppUpdateCheckPolicy::decide_without_release\(auto_update, installed, by_user != 0, skipping\)\)/, 'one policy decision routes every copy');
  assert.match(
    fallback,
    /case AppUpdateCheckPolicy::Action::RunSquirrelUpdate:[^;]*;\s*CallAfter\(\[this, by_user\]\(\) \{ this->start_auto_update\(std::string\(\), std::string\(\), by_user, false\); \}\);\s*break;/,
    'Update.exe runs from the UI thread with no tag and the "not newer" verdict, so staging nothing is not a failure'
  );
  assert.match(fallback, /case AppUpdateCheckPolicy::Action::ShowNoNewVersion:\s*if \(show_tips\) no_new_version\(\);\s*break;/, 'every other copy behaves as when nothing is newer');
  assert.doesNotMatch(fallback, /request_new_version\(|push_auto_update_(?:failed|started)_notification\(/, 'without a release nothing is offered or reported as failed');
  assert.match(
    policy,
    /static Action decide_without_release\(bool auto_update_enabled, bool squirrel_installed, bool manual_check, bool a_version_is_skipped\) \{\s*return decide\(auto_update_enabled, squirrel_installed, false, manual_check, a_version_is_skipped\);\s*\}/,
    'an unknown release is never newer'
  );
  const start = bodyOf(guiApp, 'void GUI_App::start_auto_update(');
  assert.match(start, /if \(updated && tag\.empty\(\)\)\s*tag = squirrel_newest_folder_version\(update_exe\);/, 'the ready banner names the version Squirrel staged when no tag was known');
  const newest = bodyOf(guiApp, 'static std::string squirrel_newest_folder_version(');
  assert.match(newest, /squirrel_folder_version\(folder\)/, 'only app-<version> folders count');
  assert.match(newest, /version > newest/, 'the highest version wins');
});

test('Squirrel is pointed at one fixed feed and nothing else reaches its command line', () => {
  assert.equal(guiApp.split(FEED).length - 1, 1, 'the feed URL is written exactly once');
  assert.match(guiApp, new RegExp(`kSquirrelFeedUrl\\s*=\\s*"${escapeRegExp(FEED)}"`), 'the feed is one named constant');

  const run = bodyOf(guiApp, 'static bool run_squirrel_update(');
  assert.match(
    run,
    /arguments\s*=\s*L"--update="\s*\+\s*boost::nowide::widen\(kSquirrelFeedUrl\);/,
    'the update command line is --update= plus the fixed feed, nothing else'
  );
  assert.match(run, /spawn_hidden_process\(update_exe,\s*arguments,/);
  assert.match(guiApp, /kSquirrelUpdateTimeoutMs\s*=\s*30ull\s*\*\s*60ull\s*\*\s*1000ull;/, 'the wait is limited to 30 minutes');
  assert.match(run, /GetTickCount64\(\)\s*\+\s*kSquirrelUpdateTimeoutMs/, 'and the wait honours the limit');
  assert.doesNotMatch(run, /TerminateProcess|kill/i, 'Update.exe is never terminated, a half-staged folder is worse than a slow update');
});

test('the app downloads and runs nothing itself', () => {
  assert.equal(guiApp.split('CreateProcessW(').length - 1, 1, 'one process is ever created, in the hidden-process helper');
  for (const name of ['static bool run_squirrel_update(', 'void GUI_App::start_auto_update(', 'static void launch_squirrel_restart()']) {
    const body = bodyOf(guiApp, name);
    assert.doesNotMatch(body, /Http::|wxExecute|ShellExecute|system\(|URLDownload|wxLaunchDefaultBrowser|_wsystem/, `${name} must not fetch or run anything itself`);
  }
});

test('the helper process never opens a console window', () => {
  const spawn = bodyOf(guiApp, 'static bool spawn_hidden_process(');
  assert.match(spawn, /CreateProcessW\(/);
  assert.match(spawn, /FALSE,\s*CREATE_NO_WINDOW,\s*nullptr/, 'created with CREATE_NO_WINDOW and no inherited handles');
  assert.doesNotMatch(spawn, /CREATE_NEW_CONSOLE|DETACHED_PROCESS/, 'no flag that would override CREATE_NO_WINDOW');
});

test('the update runs once at a time on a worker thread and reports back on the UI thread', () => {
  const start = bodyOf(guiApp, 'void GUI_App::start_auto_update(');
  assert.match(start, /m_auto_update_running\.compare_exchange_strong\(/, 'an atomic flag lets one update run at a time');
  assert.match(start, /Slic3r::create_thread\(\[this, tag, by_user, newer_by_time, update_exe\]/, 'the wait happens off the UI thread');
  assert.match(start, /CallAfter\(\[this, tag, by_user, updated, newer_by_time\]/, 'the outcome is handled on the UI thread');
  assert.match(start, /const bool reported = m_auto_update_failed_tag == tag;/, 'a failure is remembered per release');
  assert.match(
    start,
    /switch \(AppUpdateCheckPolicy::after_squirrel_update\(updated, newer_by_time, by_user != 0, reported\)\)/,
    'one policy decides the outcome from the staging result, the time verdict and the kind of check'
  );
  const outcome = (name) => {
    const match = start.match(new RegExp(`case AppUpdateCheckPolicy::Outcome::${name}:([\\s\\S]*?)break;`));
    assert.ok(match, `the ${name} outcome must be handled`);
    assert.equal(start.split(`case AppUpdateCheckPolicy::Outcome::${name}:`).length - 1, 1, `the ${name} outcome is handled once`);
    return match[1];
  };
  assert.match(outcome('ShowReady'), /^\s*push_auto_update_ready_notification\(tag\);\s*$/, 'success always shows the ready banner, after a manual or a background check');
  assert.match(
    outcome('OfferDownload'),
    /^\s*m_auto_update_failed_tag = tag;\s*if \(by_user != 0\) request_new_version\(by_user\);\s*$/,
    'a real failure after a manual check always opens the download dialog'
  );
  assert.match(
    outcome('NotifyFailure'),
    /^\s*m_auto_update_failed_tag = tag;\s*push_auto_update_failed_notification\(tag\);\s*$/,
    'a real failure after a background check shows the non-blocking failure notice and remembers the release'
  );
  assert.match(outcome('ShowNoNewVersion'), /if \(by_user != 0\) no_new_version\(\);\s*$/, 'nothing newer and nothing staged: a manual check says so');
  assert.match(outcome('Nothing'), /^\s*$/, 'a background check with nothing to install stays silent');
  assert.doesNotMatch(start, /if \(by_user != 0\) push_auto_update_ready_notification/, 'the ready banner is not limited to a manual check');
  assert.doesNotMatch(start, /if \(by_user != 0\)\s*push_auto_update_failed_notification/, 'the background failure is not silenced');

  assert.match(policy, /if \(updated\)\s*return Outcome::ShowReady;/, 'a staged version always shows the banner');
  assert.match(policy, /if \(!newer_by_time\)\s*return manual_check \? Outcome::ShowNoNewVersion : Outcome::Nothing;/, 'nothing staged and nothing newer is not a failure');
  assert.match(policy, /if \(manual_check\)\s*return Outcome::OfferDownload;/, 'a manual check always falls back to the dialog');
  assert.match(policy, /return failure_already_reported \? Outcome::Nothing : Outcome::NotifyFailure;/, 'a background check reports a failure once per release');
});

test('a failed background update shows one non-blocking notice that links to the release page', () => {
  const failed = bodyOf(guiApp, 'static void push_auto_update_failed_notification(');
  assert.match(
    failed,
    /manager->push_notification\(NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel,/,
    'a warning-level notification never fades and never blocks'
  );
  assert.match(failed, /auto_update_message\(L\("The background update to Bambu Studio %s did not finish\."\), tag\)/);
  assert.match(failed, /_u8L\("Download from the release page"\)/);
  assert.match(failed, /page_url = release_page_url\(tag\);[\s\S]*wxLaunchDefaultBrowser\(wxString::FromUTF8\(page_url\)\);/, 'the link opens the release page in the browser');
  assert.doesNotMatch(failed, /request_new_version|ShowModal|wxMessageBox|MessageDialog/, 'no dialog');
  assert.match(guiAppHeader, /std::string\s+m_auto_update_failed_tag;/, 'the reported release lives on the application, UI thread only');
  assert.match(
    guiAppHeader,
    /void\s+start_auto_update\(const std::string &tag, const std::string &name, int by_user, bool newer_by_time\);/,
    'the updater receives the time verdict'
  );
});

test('a success needs a newer app folder as well as exit code 0', () => {
  const run = bodyOf(guiApp, 'static bool run_squirrel_update(');
  assert.match(run, /exit_code == 0\s*&&\s*squirrel_newer_version_staged\(update_exe\)/, 'Update.exe also exits 0 when there is nothing to install');
  const staged = bodyOf(guiApp, 'static bool squirrel_newer_version_staged(');
  assert.match(staged, /squirrel_folder_version\(/);
  assert.match(staged, /directory_iterator/);
});

test('the ready banner stays until the user acts, says the update is unsigned, and offers restart and release notes', () => {
  const ready = bodyOf(guiApp, 'static void push_auto_update_ready_notification(');
  assert.match(ready, /push_app_update_ready_notification\(/);
  assert.match(ready, /Bambu Studio %s is ready\. It starts the next time you open the app\. Updates from this fork are not code-signed\./);
  assert.match(ready, /_u8L\("Restart to install update"\)/);
  assert.match(ready, /_u8L\("Release notes"\)/);
  assert.match(
    ready,
    /CallAfter\(\[\]\s*\{\s*wxGetApp\(\)\.restart_after_update\(\);\s*\}\);/,
    'the link is clicked while the canvas renders, so the window closes on the next turn of the event loop'
  );
  assert.match(ready, /wxLaunchDefaultBrowser\(/, 'release notes open in the browser');
  assert.match(ready, /notes_url = release_page_url\(tag\);/, 'release notes open the release page');
  const page = bodyOf(guiApp, 'static std::string release_page_url(');
  assert.match(page, /"https:\/\/github\.com\/Ding-Ding-Projects\/BambuStudio\/releases"/, 'the fork\'s own releases');
  assert.match(page, /releases \+ "\/tag\/" \+ tag/, 'on the page of that release');
  assert.match(page, /std::regex_match\(tag, std::regex\("md3-v\[0-9\]\+"\)\)/, 'only a well-formed tag reaches the link; anything else opens the release list');

  const push = bodyOf(notificationSource, 'void NotificationManager::push_app_update_ready_notification(');
  assert.match(push, /NotificationType::AppUpdateReady,\s*NotificationLevel::ImportantNotificationLevel,\s*0,/, 'duration 0: the banner never fades');
  assert.match(push, /\.second_hypertext\s*=\s*notes_text;/);
  assert.match(push, /\.second_callback\s*=\s*std::move\(notes_callback\);/);
  assert.match(notificationHeader, /void push_app_update_ready_notification\(/);
  assert.match(notificationHeader, /AppUpdateReady,\s*NotificationTypeCount/, 'a dedicated notification type, added before the count');
  assert.match(notificationSource, /case NotificationType::AppUpdateReady:\s*return "AppUpdateReady";/);
});

test('an installed copy with the preference on checks again every six hours while it runs', () => {
  assert.match(guiApp, /kUpdateCheckIntervalMs\s*=\s*6 \* 60 \* 60 \* 1000;/);
  const start = bodyOf(guiApp, 'void GUI_App::start_periodic_update_check()');
  assert.match(start, /app_config->get_bool\("auto_update"\)\s*\|\|\s*!squirrel_update_exe\(/, 'only an installed copy with the preference on');
  assert.match(start, /m_update_check_timer\.Start\(kUpdateCheckIntervalMs\);/);
  assert.match(start, /check_new_version\(\);/, 'the timer runs the same check as the startup');
  assert.match(start, /if \(!app_config->get_bool\("auto_update"\)\)\s*\{\s*m_update_check_timer\.Stop\(\);/, 'turning the preference off stops the checks');
  const onExit = bodyOf(guiApp, 'int GUI_App::OnExit()');
  assert.match(onExit, /m_update_check_timer\.Stop\(\);/);
  assert.match(guiApp, /this->check_new_version\(\);\s*this->start_periodic_update_check\(\);/, 'started right after the startup check');
});

test('"Restart to install update" hands over to Squirrel and waits for the app to exit', () => {
  assert.match(guiApp, /kSquirrelRestartArguments\s*=\s*L"--processStartAndWait bambu-studio\.exe"/);
  const launch = bodyOf(guiApp, 'static void launch_squirrel_restart()');
  assert.match(launch, /squirrel_update_exe\(/, 'only a Squirrel install can restart this way');
  assert.match(launch, /spawn_hidden_process\(update_exe,\s*kSquirrelRestartArguments,/);

  const onExit = bodyOf(guiApp, 'int GUI_App::OnExit()');
  assert.match(onExit, /m_auto_update_cancel\s*=\s*true;[\s\S]*m_auto_update_thread\.join\(\)/, 'a running update is not waited for and the worker is joined');
  assert.match(onExit, /if \(take_restart_after_update\(\)\)\s*launch_squirrel_restart\(\);/, 'the restart is launched only when the application is really exiting');

  const restart = bodyOf(guiApp, 'void GUI_App::restart_after_update()');
  assert.match(restart, /m_restart_after_update\s*=\s*true;\s*[\s\S]*mainframe->Close\(\);/, 'the close goes through the normal path, so the unsaved-project prompt applies');
});

test('a cancelled close never restarts', () => {
  const handler = mainFrame.match(/\n    Bind\(wxEVT_CLOSE_WINDOW, \[this\]\(wxCloseEvent& event\) \{[\s\S]*?\n    \}\);/);
  assert.ok(handler, 'the main frame close handler must exist');
  const code = handler[0];
  const take = code.indexOf('take_restart_after_update()');
  const firstVeto = code.indexOf('event.Veto()');
  const lastVeto = code.lastIndexOf('event.Veto()');
  const rearm = code.lastIndexOf('set_restart_after_update(true)');
  const skip = code.lastIndexOf('event.Skip()');
  assert.ok(take >= 0 && take < firstVeto, 'the request is taken before any check can veto the close');
  assert.ok(rearm > lastVeto && rearm < skip, 'it is handed back only after the last check, when the close is accepted');
  assert.equal(code.split('set_restart_after_update(true)').length - 1, 2, 'the project page replay of a deferred close keeps the request too');
});

test('auto_update is a preference that defaults to on', () => {
  assert.match(
    appConfig,
    /if \(get\("auto_update"\)\.empty\(\)\) \{\s*set_bool\("auto_update", true\);\s*\}/,
    'a config without the key updates automatically'
  );
  assert.match(preferences, /create_item_checkbox\(_L\("Update automatically"\),[\s\S]{0,400}?50,\s*"auto_update"\);/, 'Preferences has a switch for it');
  assert.match(preferences, /sizer->Add\(item_auto_update, flags\);/, 'and shows it');
  assert.match(preferences, /kPrefKeys\[\] = \{[\s\S]*?"auto_update",[\s\S]*?\};/, 'Reset preferences restores the default');
  assert.match(paletteIndex, /\{"auto_update", L\("Update automatically"\),/, 'the command palette can find it');
});

test('the new messages are extracted into the source catalogue', async () => {
  const ids = catalogueIds(await read('bbl', 'i18n', 'BambuStudio.pot'));
  for (const id of [
    'Update automatically',
    'Restart to install update',
    'Release notes',
    'Bambu Studio %s is ready. It starts the next time you open the app. Updates from this fork are not code-signed.',
    'Downloading Bambu Studio %s in the background.',
    'The background update to Bambu Studio %s did not finish.',
    'Download from the release page',
  ]) {
    assert.ok(ids.has(id), `${id} must be in bbl/i18n/BambuStudio.pot (run scripts/i18n/update_catalogs.py)`);
  }
});

test('the feature article describes the preference, the restart and the fallbacks', async () => {
  const doc = await read('docs', 'features', 'windows', 'app-updates.md');
  for (const needle of ['Update automatically', '`auto_update`', 'Update.exe', 'Restart to install update', 'Release notes', 'not code-signed', 'every six hours', '--processStartAndWait', 'SLIC3R_BUILD_TIME_UTC', 'did not finish', 'reinstall', '429', 'which reads its own']) {
    assert.ok(doc.includes(needle), `app-updates.md must mention ${needle}`);
  }
});
