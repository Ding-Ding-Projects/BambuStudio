import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=readFileSync('src/slic3r/GUI/CalibrationWizardSavePage.cpp','utf8');
const header=readFileSync('src/slic3r/GUI/CalibrationWizardSavePage.hpp','utf8');
function tokens(s){return [...s.matchAll(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|[A-Za-z_]\w*|->|::|[^\s]/g)].filter(m=>!m[0].startsWith('//')&&!m[0].startsWith('/*')).map(m=>({text:m[0],index:m.index}));}
function block(s,startPattern){const start=s.search(startPattern);assert.ok(start>=0,'scope exists: '+startPattern);const t=tokens(s.slice(start));let depth=0,begin=-1;for(const x of t){if(x.text==='{'){if(begin<0)begin=x.index;depth++;}if(x.text==='}'&&--depth===0)return s.slice(start+begin+1,start+x.index);}throw Error('unterminated scope');}
function members(className){const body=block(header,new RegExp('class '+className+'\\b'));const t=tokens(body).map(x=>x.text);const names=new Set();for(let i=0;i<t.length-2;i++)if(t[i]==='Label'&&t[i+1]==='*')names.add(t[i+2]);return names;}
function validate(body,memberNames){const t=tokens(body).map(x=>x.text),scopes=[new Set()],initialized=new Set();let uses=0;for(let i=0;i<t.length;i++){
 if(t[i]==='{')scopes.push(new Set());else if(t[i]==='}')scopes.pop();
 if(t[i]==='auto'&&t[i+2]==='='&&t[i+3]==='new'&&t[i+4]==='Label')scopes.at(-1).add(t[i+1]);
 if(memberNames.has(t[i])&&t[i+1]==='='&&t[i+2]==='new'&&t[i+3]==='Label')initialized.add(t[i]);
 if(['complete_text','m_complete_text'].includes(t[i])&&t[i+1]==='->'&&t[i+2]==='SetForegroundColour'){
  assert.ok(scopes.some(scope=>scope.has(t[i]))||(memberNames.has(t[i])&&initialized.has(t[i])),'undeclared or uninitialized receiver in actual scope: '+t[i]);uses++;
 }
 }assert.equal(uses,1,'one completion-label foreground call per inventoried scope');}
const scopes=[['CaliPASaveAutoPanel','create_panel'],['CaliPASaveManualPanel','create_panel'],['CaliPASaveP1PPanel','create_panel'],['CalibrationFlowX1SavePage','create_page'],['CalibrationFlowCoarseSavePage','create_page'],['CalibrationFlowFineSavePage','create_page']];
for(const [owner,method] of scopes)test(owner+' foreground receiver resolves in its own declaration scope',()=>validate(block(source,new RegExp('void '+owner+'::'+method+'\\(')),members(owner)));
test('out-of-scope or later local declarations cannot validate a receiver',()=>{
 for(const body of ['{ auto complete_text = new Label(); } complete_text->SetForegroundColour(1);','complete_text->SetForegroundColour(1); auto complete_text = new Label();'])assert.throws(()=>validate(body,new Set()),{name:'AssertionError'});
});
test('member must be declared on the owning class and initialized in this method',()=>{
 assert.throws(()=>validate('m_complete_text = new Label(); m_complete_text->SetForegroundColour(1);',new Set()),{name:'AssertionError'});
 assert.throws(()=>validate('m_complete_text->SetForegroundColour(1);',new Set(['m_complete_text'])),{name:'AssertionError'});
});
