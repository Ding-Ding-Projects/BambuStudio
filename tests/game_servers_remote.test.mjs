import {spawnSync} from 'node:child_process';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';
import {randomBytes} from 'node:crypto';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const work=mkdtempSync(path.join(tmpdir(),'bambu-native-target-'));
const exe=path.join(work,'fixture.exe');
const host=process.env.BAMBU_TEST_SSH_PROFILE;
const official=process.env.BAMBU_TEST_OFFICIAL_SERVER_JAR;
const id=randomBytes(16).toString('hex');
const run=(cmd,args,input,expected=0)=>{
  const r=spawnSync(cmd,args,{input,encoding:'utf8',timeout:90000,maxBuffer:1024*1024});
  if(r.error || (expected===0?r.status!==0:r.status===0))throw r.error??new Error(cmd+' returned '+r.status+': '+r.stderr);
  return r.stdout.trim();
};
const ssh=['-T','-o','BatchMode=yes','-o','StrictHostKeyChecking=yes','-o','ClearAllForwardings=yes','-o','ConnectTimeout=8'];
let remote;
try {
  const built=compileFixture({source:path.join(root,'tests/game_servers_remote_commands.cpp'),exe,cwd:work,include:[path.join(root,'src'),path.join(root,'src/nlohmann')]});
  if(built.status!==0)throw Error('Target argument fixture compilation failed');
  const args=(op,h,d,f,e,p,owner)=>JSON.parse(run(exe,[op,h,d,f,e,String(p),owner]));
  if(args('setup','test-host','/tmp/new server','/tmp/distribution.zip','bedrock',19132,id).at(-1).indexOf('python3 -c')<0)throw Error('No native setup entrypoint');
  run(exe,['setup','-bad','/tmp/new','/tmp/server.zip','bedrock','19132',id],undefined,1);
  if(!host || !official){console.log('Native target argument checks passed. Real SSH fixture requires an explicitly selected host and official Java JAR.');}
  else {
    remote=run('ssh',[...ssh,'--',host,'umask 077; mktemp -d /tmp/bambu-native-check.XXXXXXXX']);
    if(!/^\/tmp\/bambu-native-check\.[A-Za-z0-9]{8}$/.test(remote))throw Error('Unexpected test directory');
    run('scp',['-q','-o','BatchMode=yes','-o','StrictHostKeyChecking=yes',official,host+':'+remote+'/source.jar']);
    const script=[
      'import sys,zipfile', 'root=sys.argv[1]',
      'with zipfile.ZipFile(root+"/fixture.zip","w") as z:',
      ' z.writestr("server.properties","server-port=19132\\n")',
      ' z.writestr("bedrock_server", "#!/bin/sh\\nprintf \'fixture ready\\\\n\'\\nwhile IFS= read -r line; do case \\"$line\\" in list) echo fixture-list;; stop) echo fixture-stopped; exit 0;; esac; done\\n")'
    ].join('\n');
    run('ssh',[...ssh,'--',host,'python3 - '+remote],script);
    const remoteCall=(list,input,expected)=>run(list[0],list.slice(1),input,expected);
    const java=args('setup',host,remote+'/java',remote+'/source.jar','java',25597,id);
    const result=JSON.parse(remoteCall(java));
    if(result.edition!=='java'||result.port!==25597||result.owner!==id)throw Error('Java deployment identity mismatch');
    run('ssh',[...ssh,'--',host,'grep -qx eula=false '+remote+'/java/eula.txt && cmp '+remote+'/source.jar '+remote+'/java/server.jar']);
    remoteCall(java,undefined,1);
    const bds=args('setup',host,remote+'/bedrock',remote+'/fixture.zip','bedrock',19197,id);
    if(JSON.parse(remoteCall(bds)).owner!==id)throw Error('Bedrock deployment identity mismatch');
    remoteCall(args('launch',host,remote+'/bedrock','unused','bedrock',19197,'0'.repeat(32)),'stop\n',1);
    const text=remoteCall(args('launch',host,remote+'/bedrock','unused','bedrock',19197,id),'list\nstop\n');
    if(!text.includes('fixture-list')||!text.includes('fixture-stopped'))throw Error('Owned SSH process did not answer both commands');
    console.log('SSH: provisioned real Java JAR with unaccepted EULA, verified source/destination equality, refused existing destination; provisioned inert Bedrock fixture and verified owner-bound launch, console and graceful stop.');
  }
} finally {
  if(remote && /^\/tmp\/bambu-native-check\.[A-Za-z0-9]{8}$/.test(remote)){
    run('ssh',[...ssh,'--',host,`python3 -c "import os,sys,shutil; p=sys.argv[1]; assert os.path.realpath(p)==p and os.stat(p).st_uid==os.getuid(); shutil.rmtree(p)" ${remote}`]);
  }
  rmSync(work,{recursive:true,force:true});
}
