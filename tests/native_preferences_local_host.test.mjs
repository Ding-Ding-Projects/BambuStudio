import {readFileSync,writeFileSync,mkdtempSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {tmpdir} from 'node:os';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
if(process.platform!=='win32') { console.error('UNVERIFIED: native named-pipe fixture requires Windows'); process.exit(2); }
const output=mkdtempSync(path.join(tmpdir(),'bambu-local-host-fixture-'));
const cpp=path.join(output,'fixture.cpp'),exe=path.join(output,'fixture.exe');
writeFileSync(cpp,readFileSync(path.join(root,'tests/native_preferences_local_host_fixture.cpp')));
const compile=spawnSync('cl.exe',['/nologo','/std:c++17','/EHsc','/W4','/I'+root,'/I'+path.join(root,'src'),cpp,'/Fe:'+exe,'/Fo:'+path.join(output,'fixture.obj')],{cwd:output,stdio:'inherit'});
if(compile.error) throw compile.error;
if(compile.status!==0) process.exit(compile.status??1);
const run=spawnSync(exe,[],{cwd:output,stdio:'inherit',timeout:60000});
if(run.error) throw run.error;
process.exit(run.status??1);
