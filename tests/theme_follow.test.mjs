// Light, Dark or Follow Windows, and Undo for printers added in setup.
//
// 1. Compiles and runs tests/theme_follow_test.cpp, the rules of the theme
//    choice. A second build defines SLIC3R_THEME_FOLLOW_NEGATIVE_FIXTURE, in
//    which the look follows Windows even when following is off; it must fail.
// 2. Checks the sources: the look Windows wants is set before the first window
//    and the watcher starts after startup and stops at exit; Preferences and
//    setup write both keys through the shared rules; the watcher changes the
//    look only when the rules say so. Undo in setup asks first, lets unsaved
//    printer changes be saved, puts back the printer guide's choices as they
//    were, and acts once. Each source check also runs on a broken copy and
//    must fail.
import assert from 'node:assert/strict';
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => readFileSync(path.join(root, rel), 'utf8').replaceAll('\r\n', '\n');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-theme-follow-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + read('tests/theme_follow_test.cpp'));
  const exe = path.join(output, 'fixture.exe');
  const compile = compileFixture({source, exe, cwd: output, include: [root, path.join(root, 'src')]});
  if (compile.error) throw compile.error;
  if (compile.status !== 0) process.exit(compile.status ?? 1);
  return {exe, output};
}

const normal = build('normal', '');
const green = spawnSync(normal.exe, [], {cwd: normal.output, stdio: 'inherit'});
if (green.error) throw green.error;
if (green.status !== 0) process.exit(green.status ?? 1);
const mutated = build('negative', '#define SLIC3R_THEME_FOLLOW_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the suite cannot notice Windows changing the look while following is off.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (the look followed Windows while following was off).');

function body(source, name) {
  const start = source.indexOf(name);
  assert(start >= 0, name);
  const begin = source.indexOf('{', start);
  const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, (t) => ' '.repeat(t.length));
  let depth = 0;
  for (let i = 0; i < masked.length; ++i) {
    if (masked[i] === '{') ++depth;
    if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
  }
  assert.fail(`Unbalanced ${name}`);
}
const code = (text) => text.replace(/\/\/[^\n]*/g, '');

function checkApp(app, watcher) {
  const startup = app.indexOf('ThemeFollow::apply_at_startup();');
  const first = app.indexOf('bool init_dark_color_mode = dark_mode();');
  assert(startup >= 0 && first > startup, 'the look Windows wants is set before it is first applied');
  assert.match(app, /PreferencesDialog::remember_values_in_use\(\);[\s\S]{0,200}ThemeFollow::start_watching\(\);/);
  assert.match(app, /m_update_check_timer\.Stop\(\);\s*ThemeFollow::stop_watching\(\);/, 'the watcher stops at exit');
  const notify = code(body(watcher, 'void Notify() override'));
  const change = notify.indexOf('follow_change(');
  const set = notify.indexOf('cfg->set("dark_color_mode"');
  assert(change >= 0 && set > change && /if \(!change\) return;/.test(notify), 'the look changes only when the rules say so');
}

function checkChoices(prefs, setup) {
  for (const [name, source, fn] of [['Preferences', prefs, 'wxWindow *PreferencesDialog::create_appearance_tab('], ['setup', setup, 'wxWindow *SetupWizardDialog::build_look(']]) {
    const look = code(body(source, fn));
    assert.match(look, /ThemeFollow::store\(choice, ThemeFollow::windows_prefers_dark\(\)/, `${name} writes through the shared rules`);
    assert.match(look, /set\(ThemeFollow::kFollowKey, stored\.follow\)/, `${name} stores whether Windows decides`);
  }
}

function checkUndo(setup) {
  const choose = code(body(setup, 'wxWindow *SetupWizardDialog::build_printers('));
  const kept = choose.indexOf('state.vendors   = cfg->vendors();');
  const guide = choose.indexOf('wxGetApp().run_wizard(ConfigWizard::RR_USER, ConfigWizard::SP_PRINTERS)');
  assert(kept >= 0 && guide > kept, 'the guide\'s choices are kept before it opens');
  const undo = code(body(setup, 'void SetupWizardDialog::undo_added_printers('));
  const once = undo.indexOf('if (*undone) {');
  const ask = undo.indexOf('ShowModal() != wxID_OK) return;');
  const unsaved = undo.indexOf('check_and_keep_current_preset_changes(');
  const vendors = undo.indexOf('cfg->set_vendors(state.vendors);');
  assert(once >= 0 && ask > once && unsaved > ask && vendors > unsaved, 'once, after asking and after unsaved changes are handled');
  assert.match(undo, /cfg->set_section\(AppConfig::SECTION_FILAMENTS, state\.filaments\);/);
  assert.match(undo, /\*undone = true;/);
}

function broken(source, from, to) {
  assert(source.includes(from), `fixture anchor: ${from}`);
  return source.replace(from, to);
}

const app = read('src/slic3r/GUI/GUI_App.cpp');
const watcher = read('src/slic3r/GUI/Appearance/ThemeFollowWatcher.cpp');
const prefs = read('src/slic3r/GUI/Preferences.cpp');
const setup = read('src/slic3r/GUI/SetupWizard/SetupWizardDialog.cpp');
checkApp(app, watcher);
checkChoices(prefs, setup);
checkUndo(setup);
assert.throws(() => checkApp(broken(app, '    ThemeFollow::apply_at_startup();\n', ''), watcher));
assert.throws(() => checkApp(app, broken(watcher, '        if (!change) return;\n', '')));
assert.throws(() => checkChoices(prefs, broken(setup, 'config->set(ThemeFollow::kFollowKey, stored.follow);', '(void) stored.follow;')));
assert.throws(() => checkUndo(broken(setup, '    if (MessageDialog(this, question, _L("Undo"), wxOK | wxCANCEL | wxICON_QUESTION).ShowModal() != wxID_OK) return;\n', '')));
assert.throws(() => checkUndo(broken(setup, '    *undone = true;\n', '')));
console.log('Theme choices, the watcher and Undo in setup are wired as required.');
