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

// MSVC looks names up from lambdas through the using-directives the app's
// headers bring in (boost::asio, boost::system and nlohmann all have a
// `detail` namespace), so a page member named after one of them is ambiguous
// there even though GCC and Clang accept it. The Windows build once failed on
// exactly that; keep the page's members clear of those names.
const panel = readFileSync(path.join(root, 'src/slic3r/GUI/GameServers/ServersPanel.cpp'), 'utf8');
function clashes(text) {
  return [...text.matchAll(/^\s+[A-Za-z_:<>]+\s*\*?\s*(detail|impl|placeholders)\s*(?:=[^;]*)?;/gm)].map(m => m[1]);
}
if (clashes(panel).length) {
  console.error('ServersPanel declares a name MSVC also sees as a namespace from lambdas: ' + clashes(panel).join(', '));
  process.exit(1);
}
if (!clashes('struct X {\n    wxPanel             *detail = nullptr;\n};\n').length) {
  console.error('Negative check: the namespace-name guard no longer notices a member named detail.');
  process.exit(1);
}
console.log('ServersPanel: no member is named after detail, impl or placeholders.');
