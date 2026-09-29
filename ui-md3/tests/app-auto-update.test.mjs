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
// console window, the restart waits for the app to exit, and a cancelled close
// never restarts. They read the sources as text, so they run without a build.

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
  const route = check.search(/get_bool\("auto_update"\)\s*&&\s*squirrel_update_exe\(/);
  const updater = check.indexOf('start_auto_update(tag, name, by_user)');
  const dialog = check.lastIndexOf('request_new_version(by_user)');
  assert.ok(skip >= 0, 'a skipped version must still be honoured');
  assert.ok(route > skip, 'the route is chosen after the skip check, from the preference and the install layout');
  assert.ok(updater > route, 'an installed copy with the preference on starts the automatic update');
  assert.ok(dialog > updater, 'every other copy keeps the download dialog, unchanged');
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
  assert.match(start, /Slic3r::create_thread\(/, 'the wait happens off the UI thread');
  assert.match(start, /CallAfter\(\[this, tag, by_user, updated\]/, 'the outcome is handled on the UI thread');
  assert.match(start, /if \(updated\)\s*push_auto_update_ready_notification\(tag\);/, 'success tells the user the update is ready');
  assert.match(
    start,
    /else\s*request_new_version\(by_user\);/,
    'a failed update falls back to the download dialog on every check, so a broken update never hides a new release'
  );
  assert.doesNotMatch(start, /else if \(by_user != 0\)\s*request_new_version/, 'the automatic check is not silenced on failure');
});

test('a success needs a newer app folder as well as exit code 0', () => {
  const run = bodyOf(guiApp, 'static bool run_squirrel_update(');
  assert.match(run, /exit_code == 0\s*&&\s*squirrel_newer_version_staged\(update_exe\)/, 'Update.exe also exits 0 when there is nothing to install');
  const staged = bodyOf(guiApp, 'static bool squirrel_newer_version_staged(');
  assert.match(staged, /squirrel_folder_version\(/);
  assert.match(staged, /directory_iterator/);
});

test('the ready notification is a non-blocking notification with a Restart now link', () => {
  const ready = bodyOf(guiApp, 'static void push_auto_update_ready_notification(');
  assert.match(ready, /NotificationType::AppUpdateReady/);
  assert.match(ready, /_u8L\("Restart now"\)/);
  assert.match(ready, /Bambu Studio %s is ready\. It starts the next time you open the app\./);
  assert.match(
    ready,
    /CallAfter\(\[\]\s*\{\s*wxGetApp\(\)\.restart_after_update\(\);\s*\}\);/,
    'the link is clicked while the canvas renders, so the window closes on the next turn of the event loop'
  );
  assert.match(notificationHeader, /AppUpdateReady,\s*NotificationTypeCount/, 'a dedicated notification type, added before the count');
  assert.match(notificationSource, /case NotificationType::AppUpdateReady:\s*return "AppUpdateReady";/);
});

test('"Restart now" hands over to Squirrel and waits for the app to exit', () => {
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
    'Restart now',
    'Bambu Studio %s is ready. It starts the next time you open the app.',
    'Downloading Bambu Studio %s in the background.',
  ]) {
    assert.ok(ids.has(id), `${id} must be in bbl/i18n/BambuStudio.pot (run scripts/i18n/update_catalogs.py)`);
  }
});

test('the feature article describes the preference, the restart and the fallbacks', async () => {
  const doc = await read('docs', 'features', 'windows', 'app-updates.md');
  for (const needle of ['Update automatically', '`auto_update`', 'Update.exe', 'Restart now', '--processStartAndWait']) {
    assert.ok(doc.includes(needle), `app-updates.md must mention ${needle}`);
  }
});
