// Compiles and runs tests/extension_context_test.cpp, the standalone
// assertions for src/slic3r/GUI/Extensions/ExtensionContext.cpp (the slice,
// printer and model sections, the sample requests, and comment lines added to
// an exported G-code file), with MSVC on Windows and $CXX (or c++) elsewhere.
// A second build defines the negative-fixture switch that puts the start
// lines above Bambu Studio's header block and must fail: a suite that stayed
// green there could not notice exported files whose header printers no
// longer find first.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-extension-context-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/extension_context_test.cpp'), 'utf8'));
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

const mutated = build('negative', '#define SLIC3R_EXTENSION_CONTEXT_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the context suite cannot notice comment lines landing above the header block.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (start lines went above the header block).');

const unsourced = build('source-negative', '#define SLIC3R_EXTENSION_CONTEXT_SOURCE_NEGATIVE_FIXTURE 1\n');
const wrong = spawnSync(unsourced.exe, [], {cwd: unsourced.output, encoding: 'utf8'});
if (wrong.error) throw wrong.error;
if (wrong.status === 0) {
  console.error('Source negative fixture stayed green: the suite cannot notice a file on the printer getting this project\'s slice.');
  process.exit(1);
}
console.log('Source negative fixture turned red as required (a file on the printer counted as sliced here).');
