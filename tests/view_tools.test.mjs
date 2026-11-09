// Compiles and runs tests/view_tools_test.cpp, the standalone assertions for
// src/slic3r/GUI/ViewTools (snap to grid and the per-object views), with MSVC
// on Windows and $CXX (or c++) elsewhere. Two more builds each define a
// negative-fixture switch and must fail: one snaps the move instead of where
// the object lands, one lets an object turn invisible.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-view-tools-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/view_tools_test.cpp'), 'utf8'));
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

for (const [macro, what] of [
  ['SLIC3R_SNAP_GRID_NEGATIVE_FIXTURE', 'the move was snapped instead of where the object lands'],
  ['SLIC3R_OBJECT_VIEWS_NEGATIVE_FIXTURE', 'an object could turn invisible'],
]) {
  const mutated = build(macro.toLowerCase(), `#define ${macro} 1\n`);
  const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
  if (red.error) throw red.error;
  if (red.status === 0) {
    console.error(`Negative fixture ${macro} stayed green: the view tools suite cannot notice that ${what}.`);
    process.exit(1);
  }
  console.log(`Negative fixture ${macro} turned red as required (${what}).`);
}
