// Compiles and runs tests/base64_files_test.cpp, the standalone assertions for
// src/slic3r/GUI/Base64Toolkit/Base64Files.cpp (whole files encoded and decoded
// beside the originals), with MSVC on Windows and $CXX (or c++) elsewhere. A
// second build defines the negative-fixture switch that drops the encoder's
// last group and must fail: a suite that stayed green there could not notice a
// file that was written short.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-base64-files-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/base64_files_test.cpp'), 'utf8'));
  const exe = path.join(output, 'fixture.exe');
  const compile = compileFixture({source, exe, cwd: output, include: root});
  if (compile.error) throw compile.error;
  if (compile.status !== 0) process.exit(compile.status ?? 1);
  return {exe, output};
}

const normal = build('normal', '');
const green = spawnSync(normal.exe, [], {cwd: normal.output, stdio: 'inherit'});
if (green.error) throw green.error;
if (green.status !== 0) process.exit(green.status ?? 1);

const mutated = build('negative', '#define SLIC3R_BASE64_FILES_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the file suite cannot notice an encoded file written short.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (last group dropped, so verification refused the file).');
