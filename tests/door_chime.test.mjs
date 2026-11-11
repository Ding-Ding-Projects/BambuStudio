// Compiles and runs tests/door_chime_test.cpp, the standalone assertions
// for src/slic3r/GUI/DoorChime/DoorChime.hpp, with MSVC on Windows and $CXX
// (or c++) elsewhere.
import {mkdtempSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=mkdtempSync(path.join(tmpdir(),'bambustudio-door-chime-'));
const exe=path.join(output,'fixture.exe');
console.log('Task-owned output: '+output);
const compile=compileFixture({source:path.join(root,'tests/door_chime_test.cpp'),exe,cwd:output,include:root});
if(compile.error) throw compile.error;
if(compile.status!==0) process.exit(compile.status ?? 1);
const run=spawnSync(exe,[],{cwd:output,stdio:'inherit'});
if(run.error) throw run.error;
process.exit(run.status ?? 1);
