// Compiles and runs tests/preference_history_test.cpp, the standalone
// assertions for the Preferences change record and provenance rules
// (src/slic3r/GUI/PreferencesRemake/PreferenceHistory.cpp). A second build
// defines SLIC3R_PREFERENCE_HISTORY_NEGATIVE_FIXTURE, in which no key counts as
// secret; it must fail, or the suite could not notice tokens leaking into the
// record.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-preference-history-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/preference_history_test.cpp'), 'utf8'));
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

const mutated = build('negative', '#define SLIC3R_PREFERENCE_HISTORY_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the suite cannot notice secrets written to the record.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (secret keys no longer redacted).');

// Source contract for the Preferences dialog: every registered row is
// decorated, provenance lines stay out of search, the record lives in the data
// folder and the change timer stops with the dialog. A copy with the
// decoration call removed must be reported.
const preferences = readFileSync(path.join(root, 'src/slic3r/GUI/Preferences.cpp'), 'utf8');
function contract(source) {
  const problems = [];
  const register = source.slice(source.indexOf('void PreferencesDialog::register_option_row('));
  const registerBody = register.slice(0, register.indexOf('\n}\n'));
  if (!registerBody.includes('decorate_row(key, sizer)')) problems.push('register_option_row no longer decorates rows');
  const collector = source.slice(source.indexOf('static void collect_search_labels_from_window('));
  if (!collector.slice(0, collector.indexOf('\n}\n')).includes('"preferences-provenance"')) problems.push('provenance lines are searchable again');
  if (!/build_search_index\(\);[\s\S]{0,600}start_change_tracking\(\);/.test(source)) problems.push('change tracking does not start after the search index');
  if (!source.includes('"preferences-history.jsonl"')) problems.push('the record file name changed');
  const destructor = source.slice(source.indexOf('PreferencesDialog::~PreferencesDialog()'));
  if (!destructor.slice(0, destructor.indexOf('\n}\n')).includes('m_change_timer.Stop()')) problems.push('the change timer is not stopped with the dialog');
  return problems;
}
const found = contract(preferences);
const broken = contract(preferences.replace('if (sizer != nullptr) decorate_row(key, sizer);', ''));
if (!broken.includes('register_option_row no longer decorates rows')) found.push('negative contract: removing the decoration call was not noticed');
if (found.length) {
  for (const problem of found) console.error(problem);
  process.exit(1);
}
console.log('Preferences contract: rows decorated, provenance kept out of search, record in the data folder, timer stopped.');
