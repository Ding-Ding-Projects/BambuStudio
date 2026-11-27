import {mkdtempSync,rmSync,writeFileSync,chmodSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawn,spawnSync} from 'node:child_process';
import {createInterface} from 'node:readline';
import {randomUUID} from 'node:crypto';
import {compileFixture} from './native_fixture_compiler.mjs';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),work=mkdtempSync(path.join(tmpdir(),'bambu-cycle-'));
const exe=path.join(work,'fixture.exe'), ctx=process.env.BAMBU_TEST_LOCAL_DOCKER_CONTEXT;
const engine=(args)=>spawnSync('docker',['--context',ctx,...args],{encoding:'utf8',timeout:20000});
if(!ctx){console.log('Select a disposable local Docker context using BAMBU_TEST_LOCAL_DOCKER_CONTEXT.');rmSync(work,{recursive:true,force:true});process.exit(0);}
const check=engine(['context','inspect',ctx,'--format','{{.Endpoints.docker.Host}}']);
if(check.status!==0 || !/^(npipe:|unix:)/.test(check.stdout.trim())) throw Error('Fixture requires a local engine.');
async function cycle(id,edition,op){
  const child=spawn(exe,[id,edition,op],{stdio:['pipe','pipe','inherit']});
  const finished=new Promise((ok,no)=>{child.on('error',no);child.on('exit',ok);});
  const lines=createInterface({input:child.stdout});let verdict='';
  for await(const line of lines){
    const record=JSON.parse(line);
    if(record.argv){if(!record.argv.includes(id))throw Error('Unowned fixture operation');const r=engine(record.argv);child.stdin.write(JSON.stringify({code:r.status??1,stdout:r.stdout||''})+'\n');}
    else verdict=record.verdict;
  }
  if(await finished!==0)throw Error('Lifecycle driver exited unsuccessfully.');
  return verdict;
}
try{
  const built=compileFixture({source:path.join(root,'tests/game_servers_schedule_run.cpp'),exe,cwd:work,include:[path.join(root,'src'),path.join(root,'src/nlohmann')]});
  if(built.status!==0)throw Error('Fixture compilation failed.');
  const filenames=['rcon-cli','send-command'];
  for(const name of filenames){const f=path.join(work,name);writeFileSync(f,'#!/bin/sh\nprintf "%s\\n" "$1" > /tmp/fixture-input\n');chmodSync(f,0o755);}
  for(const [edition,img] of [['java','itzg/minecraft-server:java21'],['bedrock','itzg/minecraft-bedrock-server:latest']]){
    const cmd='rm -f /tmp/fixture-input; mkfifo /tmp/fixture-input; while true; do IFS= read -r line </tmp/fixture-input; case "$line" in stop) exit 0;; say*) echo warning >/tmp/fixture-warned;; esac; done';
    const made=engine(['create','--pull=never','--name','bambu-schedule-'+randomUUID(),'--network','none','--user','0','--entrypoint','/bin/sh',
      '--mount','type=bind,src='+work+',dst=/fixture,readonly','--env','EULA=FALSE','--env','PATH=/fixture:/usr/local/bin:/usr/bin:/bin',img,'-c',cmd]);
    const id=made.stdout.trim();if(made.status!==0||!/^[a-f0-9]{64}$/.test(id))throw Error('Could not create owned inert fixture: '+made.stderr);
    try{
      if(!(await cycle(id,edition,'restart')).startsWith('Skipped'))throw Error('Stopped target was not skipped.');
      if(engine(['start',id]).status!==0)throw Error('Could not start disposable shell.');
      if(!(await cycle(id,edition,'warning')).startsWith('Warning sent'))throw Error('Warning failed.');
      if(engine(['exec',id,'test','-f','/tmp/fixture-warned']).status!==0)throw Error('Fixture did not receive warning.');
      const verdict=await cycle(id,edition,'restart');if(!verdict.startsWith('Replacement container process'))throw Error(verdict);
      console.log(edition+': real local engine received warning, gracefully stopped and restarted the original immutable fixture ID; stopped instance skipped.');
    }finally{engine(['rm','-f','-v',id]);}
  }
}finally{rmSync(work,{recursive:true,force:true});}
