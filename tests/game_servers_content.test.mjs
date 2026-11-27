import {spawnSync} from 'node:child_process';
import {mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const work = mkdtempSync(path.join(tmpdir(), 'bambu-server-content-'));
const win = process.platform === 'win32';
const run = (cmd, args) => {
  const result = spawnSync(cmd, args, {cwd:work, stdio:'inherit'});
  if (result.error || result.status !== 0) throw result.error ?? new Error(cmd + ' exited ' + result.status);
};
const includes = ['src', 'src/miniz', 'src/nlohmann'].map(p => (win ? '/I' : '-I') + path.join(root,p));
const model = path.join(root,'src/slic3r/GUI/GameServers/ServerModel.cpp');
const install = path.join(root,'src/slic3r/GUI/GameServers/ContentInstaller.cpp');
const world = path.join(root,'src/slic3r/GUI/GameServers/WorldJarData.cpp');
const bedrockSettings=path.join(root,'src/slic3r/GUI/GameServers/BedrockSettings.cpp');
const localJava=path.join(root,'src/slic3r/GUI/GameServers/LocalJavaInstall.cpp');
const lifecycle=path.join(root,'src/slic3r/GUI/GameServers/ContentLifecycle.cpp');
const recovery=path.join(root,'src/slic3r/GUI/GameServers/LocalBedrockRecovery.cpp');
const archives=path.join(root,'src/slic3r/GUI/GameServers/ServerArchive.cpp');
const bdsLifecycle=path.join(root,'src/slic3r/GUI/GameServers/BedrockLifecycle.cpp');
const test = path.join(root,'tests/game_servers_content_test.cpp');
const miniz = path.join(root,'src/miniz/miniz.c');
const exe = path.join(work, 'content.exe');
try {
  if (win) {
    run('cl.exe', ['/nologo','/c','/O2',...includes,miniz,'/Fominiz.obj']);
    run('cl.exe', ['/nologo','/std:c++17','/EHsc',...includes,test,model,install,world,bedrockSettings,localJava,lifecycle,archives,recovery,bdsLifecycle,'miniz.obj','/Fe:'+exe]);
  } else {
    run(process.env.CC || 'cc', ['-O2','-c',miniz,'-o','miniz.o']);
    run(process.env.CXX || 'c++', ['-std=c++17',...includes,test,model,install,world,bedrockSettings,localJava,lifecycle,archives,recovery,bdsLifecycle,'miniz.o','-o',exe]);
  }
  run(exe,[work]);
} finally {
  rmSync(work,{recursive:true,force:true});
}
