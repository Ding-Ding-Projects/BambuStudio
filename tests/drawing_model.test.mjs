// Compiles and runs tests/drawing_model_test.cpp, the standalone assertions
// for src/slic3r/GUI/Drawing/DrawingModel.cpp (pens, widths, smoothing,
// direction lock, the eraser, undo, saving and SVG), with MSVC on Windows and
// $CXX (or c++) elsewhere. A second build defines the negative-fixture switch
// whose eraser throws away whole strokes and must fail: a suite that stayed
// green there could not notice an eraser that removes more than it touches.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-drawing-model-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/drawing_model_test.cpp'), 'utf8'));
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

const mutated = build('negative', '#define SLIC3R_DRAWING_MODEL_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the drawing suite cannot notice an eraser removing more than it touches.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (the eraser removed more than it touched).');
