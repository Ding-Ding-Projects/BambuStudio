import {readFileSync, writeFileSync, mkdtempSync} from 'node:fs';
import {execFileSync, spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

// Run from a supported MSVC developer environment. This builds only a tiny
// non-window test executable, never the application or its dependencies.
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const revision=process.argv.find(arg=>arg.startsWith('--baseline='))?.slice(11);
const only=process.argv.find(arg=>arg.startsWith('--case='))?.slice(7) ?? 'all';
if(!['all','rows','radius'].includes(only)) throw new Error('Unknown fixture case');
function source(p) {
  return revision ? execFileSync('git',['show',revision+':'+p],{cwd:root,encoding:'utf8',maxBuffer:4*1024*1024}) : readFileSync(path.join(root,p),'utf8');
}
function method(text,signature) {
  const start=text.indexOf(signature);
  if(start<0) throw new Error('Missing production method: '+signature);
  const open=text.indexOf('{',start);
  let depth=0;
  const tokens=/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]/g;
  tokens.lastIndex=open;
  for(let m;(m=tokens.exec(text));) {
    if(m[0]==='{') depth++;
    if(m[0]==='}' && --depth===0) return text.slice(start,m.index+1);
  }
  throw new Error('Unterminated production method: '+signature);
}
const gui='src/slic3r/GUI/';
const prefs=source(gui+'Preferences.cpp');
const staticBox=source(gui+'Widgets/StaticBox.cpp');
const methods=[method(prefs,'void PreferencesDialog::build_search_index()')];
for(const name of ['SetCornerRadius','SetDefaultCornerRadius','RescaleDefaultCornerRadius','SetDensity'])
  methods.push(method(staticBox,'void StaticBox::'+name+'('));
const initializers=[['Project.cpp','navigation'],['Preferences.cpp','content_card'],['Schedule/ScheduledSettingsPanel.cpp','card']].map(([p,name])=>{
  const text=source(gui+p);
  const hits=[...text.matchAll(new RegExp(name+'->Set(?:CornerRadius|Density)\\([^;]+;','g'))];
  if(hits.length!==1) throw new Error('Expected one actual card radius initializer: '+p);
  return hits[0][0].replace(name+'->','box.');
});
methods.push('void initialize_card(StaticBox &box,int index) { switch(index) { '+initializers.map((line,i)=>'case '+i+': '+line+' break;').join(' ')+' } }');
const fixture=readFileSync(path.join(root,'tests/native_preferences_behavior_fixture.cpp'),'utf8');
const output=mkdtempSync(path.join(tmpdir(),'bambustudio-preferences-fixture-'));
const cpp=path.join(output,'fixture.cpp'),exe=path.join(output,'fixture.exe');
writeFileSync(cpp,fixture.replace('// PRODUCTION_METHODS',methods.join('\n\n')));
console.log('Fixture source: '+(revision ?? 'current working source'));
console.log('Task-owned output: '+output);
const compile=spawnSync('cl.exe',['/nologo','/std:c++17','/EHsc','/W4','/I'+root,cpp,'/Fe:'+exe,'/Fo:'+path.join(output,'fixture.obj')],{cwd:output,stdio:'inherit'});
if(compile.error) throw compile.error;
if(compile.status!==0) process.exit(compile.status ?? 1);
const run=spawnSync(exe,[only],{cwd:output,stdio:'inherit'});
if(run.error) throw run.error;
process.exit(run.status ?? 1);
