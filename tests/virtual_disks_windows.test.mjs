// Builds tests/virtual_disks_windows_test.cpp with MSVC and runs it against
// real virtual disks: create, attach, read back, child, merge, grow, compact,
// new disk ID, reconnect and Quick pack, through the app's own planning and
// running code. It needs Windows with administrator rights (a hosted runner
// has them), so anywhere else it says so and stops without failing.
import {mkdtempSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

if (process.platform !== 'win32') {
  console.log('virtual disks on Windows: not run here, this is not Windows');
  process.exit(0);
}
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-virtual-disks-windows-'));
const exe = path.join(output, 'fixture.exe');
const compile = compileFixture({source: path.join(root, 'tests/virtual_disks_windows_test.cpp'), exe, cwd: output,
  include: [root, path.join(root, 'src'), path.join(root, 'src', 'nlohmann')]});
if (compile.error) throw compile.error;
if (compile.status !== 0) process.exit(compile.status ?? 1);
const run = spawnSync(exe, [], {cwd: output, stdio: 'inherit'});
if (run.error) throw run.error;
process.exit(run.status ?? 1);
