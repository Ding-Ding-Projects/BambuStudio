// Builds and runs the LAN model drop pure-model test (lan_model_drop_model_test.cpp) with the warning
// flags the repository uses for pure models. Skipped where no g++ is installed (a Windows runner
// without MinGW); the source contracts in app_wiring.test.mjs still run there.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const hasGxx = spawnSync('g++', ['--version'], {encoding: 'utf8'}).status === 0;

test('the pure model compiles warning-free and every assertion passes', {skip: hasGxx ? false : 'g++ is not installed'}, () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'lan-model-drop-'));
  try {
    const exe = path.join(dir, process.platform === 'win32' ? 'model_test.exe' : 'model_test');
    const build = spawnSync('g++', [
      '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Isrc',
      'tests/lan_model_drop/lan_model_drop_model_test.cpp',
      'src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp',
      '-o', exe,
    ], {cwd: repo, encoding: 'utf8'});
    assert.equal(build.status, 0, build.stderr);
    const run = spawnSync(exe, [], {cwd: repo, encoding: 'utf8'});
    assert.equal(run.status, 0, run.stderr || run.stdout);
    assert.match(run.stdout, /lan_model_drop_model_test: \d+ assertions passed/);
  } finally {
    rmSync(dir, {recursive: true, force: true});
  }
});
