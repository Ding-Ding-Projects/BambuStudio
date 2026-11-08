// Compiles and runs tests/virtual_disks_test.cpp, the standalone assertions
// for src/slic3r/GUI/VirtualDisks/DiskImage.cpp and DiskPlan.cpp (VHD, VHDX
// and ISO images read back, chains, clashing disk IDs, limits, every
// operation's steps and the library file), with MSVC on Windows and $CXX (or
// c++) elsewhere. A second build defines the negative-fixture switch that
// drops the size limits and calls a parent and its child a clash, and must
// fail: a suite that stayed green there could not notice either.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-virtual-disks-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/virtual_disks_test.cpp'), 'utf8'));
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

const mutated = build('negative', '#define SLIC3R_VIRTUAL_DISKS_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the virtual disk suite cannot notice dropped limits or a false clash.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (limits dropped, a parent and child called a clash).');
