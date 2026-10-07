import test from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync, spawnSync} from 'node:child_process';
import {readFileSync, writeFileSync, mkdtempSync, rmSync, existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const source=process.env.BAMBU_CONTINUATION_SOURCE_REF
 ? execFileSync('git',['show',`${process.env.BAMBU_CONTINUATION_SOURCE_REF}:src/slic3r/GUI/ReleaseNote.cpp`],{cwd:root,encoding:'utf8'})
 : readFileSync(path.join(root,'src/slic3r/GUI/ReleaseNote.cpp'),'utf8');
const header=readFileSync(path.join(root,'src/slic3r/GUI/ReleaseNote.hpp'),'utf8');
const captionSource=readFileSync(path.join(root,'src/slic3r/GUI/Widgets/MD3DialogChrome.cpp'),'utf8');
function definition(text,signature){
 const start=text.indexOf(signature); assert.ok(start>=0,signature);
 const body=text.indexOf('{',start);
 const masked=text.slice(body).replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g,s=>' '.repeat(s.length));
 let depth=0;
 for(let i=0;i<masked.length;i++){if(masked[i]==='{')depth++;if(masked[i]==='}'&&--depth===0)return text.slice(start,body+i+1);}
 throw new Error('Unbalanced definition');
}
const allowedAvailabilityChecks=[
 'if (!m_presentation_available || !m_button_ok->IsEnabled() || !m_button_ok->IsShown()) return;',
 'if (!m_presentation_available || !m_button_update_nozzle->IsEnabled() || !m_button_update_nozzle->IsShown()) return;',
 'if (!m_presentation_available || !m_button_ok->IsEnabled()) return;',
 'if (!m_presentation_available || !m_button_retry->IsEnabled() || !m_button_retry->IsShown()) return;',
 'if (!m_presentation_available || !m_button_input->IsEnabled() || !m_button_input->IsShown()) return;'
];
const normalise=s=>{
 for(const check of allowedAvailabilityChecks)s=s.replace(check,'');
 return s.replace(/\bfit_content\(\)/g,'Fit()')
  .replace(/\s+/g,'');
};
const hash=s=>createHash('sha256').update(normalise(s)).digest('hex');
const expected=JSON.parse(readFileSync(path.join(root,'ui-md3/tests/print-continuations-preservation.json'),'utf8'));
test('connection and confirmation actions preserve the reviewed baseline',()=>{
 for(const entry of expected.functions) assert.equal(hash(definition(source,entry.signature)),entry.sha256,entry.signature);
 for(const entry of expected.callbacks) assert.equal(hash(definition(source.slice(source.indexOf(entry.class+"::"+entry.class+"(")),entry.signature)),entry.sha256,entry.signature);
 // A changed modal result must be detected by the same source-bound comparison.
 const entry=expected.callbacks.find(x=>x.signature.startsWith('m_button_retry->Bind'));
 const changed=definition(source.slice(source.indexOf(entry.class+"::"+entry.class+"(")),entry.signature).replace('EndModal(wxYES)','EndModal(wxCANCEL)');
 assert.notEqual(hash(changed),entry.sha256);
});
const legacy=!source.includes('bool fit_continuation_body(');
const helper=definition(source,legacy?'void fit_continuation_body(':'bool fit_continuation_body(');
const ip=definition(source,'bool InputIpAddressDialog::isIp(');
const state=definition(header,'struct ContinuationDisclosureLayout')+';';
const button=legacy?'using ContinuationButton=Button;':definition(source,'class ContinuationButton')+';';
const fixture=readFileSync(path.join(root,'ui-md3/tests/print-continuations-fixture.cpp'),'utf8');
function ownerMethods(helperText){
 let methods=legacy?'ContinuationDisclosureLayout::~ContinuationDisclosureLayout() {}':
  definition(source,'ContinuationDisclosureLayout::~ContinuationDisclosureLayout()')+'\n'+
  definition(source,'void restore_continuation_layout(')+'\n'+definition(source,'void show_continuation_readback(');
 if(!legacy) methods+='\n'+definition(source,'void set_continuation_action_available(');
 methods+='\n'+(legacy?'void bind_continuation_refresh(wxDialog*,const function<void()>&) {}':definition(source,'void bind_continuation_refresh('));
 methods+='\n'+definition(captionSource,'void MD3DialogCaption::Adopt(');
 methods+='\n'+helperText+'\n'+ip;
 for(const cls of ['ConfirmBeforeSendDialog','InputIpAddressDialog','SendFailedConfirm']){
  const ctor=definition(source,cls+'::'+cls+'(');
  const refresh=legacy?'':ctor.match(/bind_continuation_refresh\(this, \[this\] \{ fit_content\(\); \}\);/)[0];
  // Compile the actual completed layout/adoption tail, in source order.
  const finish=cls==='SendFailedConfirm'?'auto* m_sizer_main=root;SetSizer(nullptr,false);'+ctor.slice(ctor.indexOf('SetSizer(m_sizer_main);'),ctor.lastIndexOf('}')):refresh;
  methods+='\n'+cls+'::'+cls+'() {'+finish+'}';
  methods+='\n'+definition(source,'void '+cls+'::fit_content()');
  const signature='bool '+cls+'::Show(bool show)';
  methods+='\n'+(source.includes(signature)?definition(source,signature):signature+' { return MD3Dialog::Show(show); }');
 }
 methods+='\n'+definition(source,'void ConfirmBeforeSendDialog::disable_button_ok(');
 methods+='\n'+definition(source,'void ConfirmBeforeSendDialog::enable_button_ok(');
 for(const [cls,button,method] of [
  ['ConfirmBeforeSendDialog','m_button_ok','click_ok'],['ConfirmBeforeSendDialog','m_button_update_nozzle','click_nozzle'],
  ['SendFailedConfirm','m_button_retry','click_retry'],['SendFailedConfirm','m_button_input','click_input']]){
  const ctor=source.slice(source.indexOf(cls+'::'+cls+'('));const callback=definition(ctor,button+'->Bind(wxEVT_LEFT_DOWN');
  methods+='\nvoid '+cls+'::'+method+'(wxMouseEvent& e)'+callback.slice(callback.indexOf('{'));
 }
 // Execute the exact production entry prefix; the unchanged transport suffix is
 // replaced by an observation counter so this fixture cannot perform networking.
 for(const [signature,boundary] of [['void InputIpAddressDialog::on_ok(wxMouseEvent& evt)','    if (!m_need_input_sn)'],['void InputIpAddressDialog::on_send_retry()','    m_test_right_msg->Hide();']]){
  const body=definition(source,signature);methods+='\n'+body.slice(0,body.indexOf(boundary))+'    ++attempts;\n}';
 }
 return methods;
}
function compileAndRun(helperText){
 const directory=mkdtempSync(path.join(tmpdir(),'print-continuations-'));
 try{
  writeFileSync(path.join(directory,'PrintSetupLayout.hpp'),readFileSync(path.join(root,'src/slic3r/GUI/PrintSetupLayout.hpp')));
  writeFileSync(path.join(directory,'test.cpp'),fixture.replace('// PRODUCTION_BUTTON',button).replace('// PRODUCTION_STATE',state).replace('// PRODUCTION_METHODS',ownerMethods(helperText)));
  if(process.platform==='win32'){
   const vswhere=path.join(process.env['ProgramFiles(x86)']??'C:/Program Files (x86)','Microsoft Visual Studio/Installer/vswhere.exe');
   assert.ok(existsSync(vswhere));
   const installation=execFileSync(vswhere,['-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],{encoding:'utf8'}).trim();
   const setup=path.join(installation,'VC/Auxiliary/Build/vcvars64.bat');assert.ok(existsSync(setup));
   writeFileSync(path.join(directory,'compile.cmd'),`@echo off\r\ncall "${setup}" >nul\r\nif errorlevel 1 exit /b %errorlevel%\r\ncl /nologo /std:c++17 /EHsc /W4 test.cpp /Fe:test.exe /Fo:test.obj\r\nexit /b %errorlevel%\r\n`);
   try{execFileSync(process.env.ComSpec??'cmd.exe',['/d','/c','compile.cmd'],{cwd:directory,stdio:'pipe'});}catch(e){throw new Error(String(e.stdout)+String(e.stderr));}
  }else{execFileSync(process.env.CXX??'c++',['-std=c++17','test.cpp','-o','test'],{cwd:directory});}
  return spawnSync(path.join(directory,process.platform==='win32'?'test.exe':'test'),[],{encoding:'utf8'});
 }finally{assert.equal(path.dirname(directory),path.resolve(tmpdir()));assert.ok(path.basename(directory).startsWith('print-continuations-'));rmSync(directory,{recursive:true,force:true});}
}
test('actual owners retain disclosure, cancellation and requested capability across layout changes',()=>{
 const result=compileAndRun(helper);assert.equal(result.status,0,result.stderr);assert.match(result.stdout,/172 continuation owner assertions passed/);console.log(result.stdout.trim());
});
test('removing the production display clamp is detected',()=>{
 const mutated=helper.replace(/const int height = PrintSetupLayout::bounded_body_height\([\s\S]*?FromDIP\(12\)\);/,'const int height = preferred_height;');
 assert.notEqual(mutated,helper);
 const result=compileAndRun(mutated);assert.equal(result.status,1);assert.match(result.stderr,/constructor saves the adopted caption root before readback/);
});
