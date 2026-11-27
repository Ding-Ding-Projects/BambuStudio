import {mkdtempSync,rmSync} from 'node:fs';
import {randomBytes,randomUUID,timingSafeEqual} from 'node:crypto';
import {tmpdir} from 'node:os';
import path from 'node:path';
import net from 'node:net';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

if (process.platform!=='win32') {
  console.log('RCON credential fixture requires Windows Credential Manager.');
  process.exit(0);
}
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const work=mkdtempSync(path.join(tmpdir(),'bambu-rcon-'));
const exe=path.join(work,'fixture.exe');
const secret=Buffer.from(randomBytes(32).toString('hex'));
const authSeen=[];
const server=net.createServer(socket=>{
  let buffer=Buffer.alloc(0);
  const packets=[];
  authSeen.push(packets);
  socket.on('error',()=>{});
  socket.on('data',chunk=>{
    buffer=Buffer.concat([buffer,chunk]);
    while(buffer.length>=4) {
      const length=buffer.readInt32LE(0);
      if(length<10 || length>4096){socket.destroy();return;}
      if(buffer.length<length+4)return;
      const id=buffer.readInt32LE(4), type=buffer.readInt32LE(8), body=buffer.subarray(12,length+2);
      const ok=type===3 ? body.length===secret.length && timingSafeEqual(body,secret) : type===2 && body.toString()==='list';
      packets.push(type);
      const reply=Buffer.alloc(14); reply.writeInt32LE(10);reply.writeInt32LE(ok?id:-1,4);reply.writeInt32LE(type===3?2:0,8);
      socket.write(reply);
      buffer=buffer.subarray(length+4);
    }
  });
});
try {
  const compile=compileFixture({source:path.join(root,'tests/game_servers_rcon_test.cpp'),exe,cwd:work,include:[path.join(root,'src'),path.join(root,'src/nlohmann')]});
  if(compile.error || compile.status!==0) throw new Error('RCON fixture did not compile');
  await new Promise((resolve,reject)=>server.once('error',reject).listen(0,'127.0.0.1',resolve));
  const child=spawn(exe,['fixture-'+randomUUID(),String(server.address().port),String(process.pid)],{stdio:['pipe','inherit','inherit']});
  child.stdin.end(Buffer.concat([secret,Buffer.from('\n')]));
  const code=await new Promise((resolve,reject)=>{child.once('error',reject);child.once('exit',resolve);});
  if(code!==0 || authSeen.length!==2 || authSeen[0].length || String(authSeen[1])!=='3,2') throw new Error('Missing expected RCON ownership and protocol results, exit '+code);
} finally {
  secret.fill(0);
  if(server.listening) await new Promise(resolve=>server.close(resolve));
  rmSync(work,{recursive:true,force:true});
}
