// Compiles and runs tests/game_servers_test.cpp, the standalone assertions for
// src/slic3r/GUI/GameServers/ServerModel.cpp (versions, server.properties kept
// exactly as written, the settings table, the server folder's files, console
// lines, the Mojang version list, starting, backups, schedules, the world
// generator and its batches, the library and SHA-1), with MSVC on Windows and
// $CXX (or c++) elsewhere. A second build defines the negative-fixture switch
// that re-encodes every line of server.properties and plans batches of 17 x 17
// chunks, and must fail: a suite that stayed green there could not notice a
// rewritten file or a batch the server refuses.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-game-servers-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/game_servers_test.cpp'), 'utf8'));
  const exe = path.join(output, 'fixture.exe');
  const compile = compileFixture({source, exe, cwd: output, include: [root, path.join(root, 'src'), path.join(root, 'src', 'nlohmann')]});
  if (compile.error) throw compile.error;
  if (compile.status !== 0) process.exit(compile.status ?? 1);
  return {exe, output};
}

const normal = build('normal', '');
const green = spawnSync(normal.exe, [], {cwd: normal.output, stdio: 'inherit'});
if (green.error) throw green.error;
if (green.status !== 0) process.exit(green.status ?? 1);

const mutated = build('negative', '#define SLIC3R_GAME_SERVERS_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the suite cannot notice a rewritten server.properties or an oversized batch.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (a rewritten file, a batch over 256 chunks).');
