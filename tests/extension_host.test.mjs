// Compiles and runs tests/extension_host_test.cpp, the standalone assertions
// for src/slic3r/GUI/Extensions/ExtensionHost.cpp (built-in extensions,
// adding folders, broken and duplicate manifests, consent through the list,
// the master switch, limits and real runs), with MSVC on Windows and $CXX (or
// c++) elsewhere. A second build defines the negative-fixture switch that
// updates a built-in you changed and must fail: a suite that stayed green
// there could not notice your edits being overwritten.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-extension-host-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/extension_host_test.cpp'), 'utf8'));
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

const mutated = build('negative', '#define SLIC3R_EXTENSION_HOST_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the host suite cannot notice a changed built-in being overwritten.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (a changed built-in was overwritten).');
