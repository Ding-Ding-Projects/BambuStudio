// Compiles and runs tests/extension_builtins_test.cpp against
// resources/extensions, with MSVC on Windows and $CXX (or c++) elsewhere.
// Everywhere it checks the built-in manifests and scripts; on Windows it also
// runs every built-in extension under Windows PowerShell through the app's own
// request, process and answer code, and expects a broken copy to be caught.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-extension-builtins-'));
const source = path.join(output, 'fixture.cpp');
writeFileSync(source, readFileSync(path.join(root, 'tests/extension_builtins_test.cpp'), 'utf8'));
const exe = path.join(output, 'fixture.exe');
const compile = compileFixture({source, exe, cwd: output, include: [root, path.join(root, 'src')]});
if (compile.error) throw compile.error;
if (compile.status !== 0) process.exit(compile.status ?? 1);
const run = spawnSync(exe, [path.join(root, 'resources', 'extensions')], {cwd: output, stdio: 'inherit'});
if (run.error) throw run.error;
process.exit(run.status ?? 1);
