// Compiles and runs tests/preference_plans_test.cpp, the standalone assertions
// for src/slic3r/GUI/PreferencesRemake/PreferencePlans.cpp (Reset this section,
// import, undo, related settings and section counts). A second build defines
// SLIC3R_PREFERENCE_PLANS_NEGATIVE_FIXTURE, in which a section reset also
// changes settings that restart the app or answer a consent question; it must
// fail, or the suite could not notice a reset that closes Preferences or
// answers for you.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-preference-plans-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/preference_plans_test.cpp'), 'utf8'));
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

const mutated = build('negative', '#define SLIC3R_PREFERENCE_PLANS_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the suite cannot notice a section reset that restarts the app or answers consent.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (settings that must be changed by hand were reset).');
