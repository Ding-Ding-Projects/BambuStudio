// Builds tests/multi_slice_names_test.cpp against the real MultiSliceNames.cpp
// and runs it, then proves the check can fail: with the separator rule broken
// the same fixture must report a failure.
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const fixture = readFileSync(path.join(root, 'tests/multi_slice_names_test.cpp'), 'utf8');
const names = readFileSync(path.join(root, 'src/slic3r/GUI/MultiSlice/MultiSliceNames.cpp'), 'utf8');

function run(namesSource) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-multi-slice-'));
  const header = readFileSync(path.join(root, 'src/slic3r/GUI/MultiSlice/MultiSliceNames.hpp'), 'utf8');
  writeFileSync(path.join(output, 'MultiSliceNames.hpp'), header);
  writeFileSync(path.join(output, 'names.cpp'), namesSource);
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, fixture.replace('#include "src/slic3r/GUI/MultiSlice/MultiSliceNames.cpp"', '#include "names.cpp"'));
  const exe = path.join(output, 'fixture.exe');
  const compile = compileFixture({source, exe, cwd: output, include: output});
  if (compile.error) throw compile.error;
  assert.equal(compile.status, 0, 'fixture compiles');
  return spawnSync(exe, [], {cwd: output, encoding: 'utf8'});
}

test('multi-slice file names are safe, readable and unique', () => {
  const result = run(names);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /all checks passed/);
});

test('a broken separator rule is caught', () => {
  const broken = names.replace("if (pending_gap) out += '_';", "if (pending_gap) out += ' ';");
  assert.notEqual(broken, names, 'the mutation applies');
  const result = run(broken);
  assert.notEqual(result.status, 0);
  assert.match(result.stdout, /FAIL spaces become one separator/);
});
