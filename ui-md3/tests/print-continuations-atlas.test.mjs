import test from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync, spawnSync} from 'node:child_process';
import {readFileSync, writeFileSync, mkdtempSync, rmSync, existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const source=readFileSync(path.join(root,'src/slic3r/GUI/ReleaseNote.cpp'),'utf8');
function definition(text,signature){
 const start=text.indexOf(signature); assert.ok(start>=0,signature);
 const body=text.indexOf('{',start);
 const masked=text.slice(body).replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g,s=>' '.repeat(s.length));
 let depth=0;
 for(let i=0;i<masked.length;i++){if(masked[i]==='{')depth++;if(masked[i]==='}'&&--depth===0)return text.slice(start,body+i+1);}
 throw new Error('Unbalanced definition');
}
const normalise=s=>s.replace(/\bfit_content\(\)/g,'Fit()').replace(/\s+/g,'');
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
const helper=definition(source,'void fit_continuation_body(');
const ip=definition(source,'bool InputIpAddressDialog::isIp(');
const fixture=String.raw`
#include "PrintSetupLayout.hpp"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
namespace PrintSetupLayout=Slic3r::GUI::PrintSetupLayout;
static int scale=100, screen_height=600, display_owner=-1;
static int dip(int n){return (n*scale+50)/100;}
namespace MD3::Metrics { struct Values{int padding;}; static Values metrics{16}; static const Values& active(){return metrics;} }
struct wxSize{int x,y;wxSize(int a=-1,int b=-1):x(a),y(b){}};
static const wxSize wxDefaultSize;
constexpr int wxNOT_FOUND=-1;
struct wxWindow{int display=0;virtual ~wxWindow()=default;};
struct Label:wxWindow{
 int chars=2000, width=400, wraps=0;
 void SetMaxSize(wxSize){} void SetMinSize(wxSize s){width=s.x;}
 void Wrap(int n){width=n;++wraps;}
 int height()const{return ((chars*dip(8)+width-1)/width)*dip(20);}
};
struct wxScrolledWindow;
struct Sizer{function<wxSize()> measure;wxSize CalcMin(){return measure();}};
struct wxScrolledWindow:wxWindow{
 Label label; vector<wxWindow*> children{&label}; wxSize minimum{480,0};int allocated=480,virtual_height=0,inside=0;
 Sizer sizer{[this]{return wxSize(minimum.x,label.height()+2*dip(MD3::Metrics::active().padding));}};
 auto& GetChildren(){return children;} Sizer* GetSizer(){return &sizer;}
 void SetMinSize(wxSize s){minimum=s;}
 wxSize GetClientSize(){return wxSize(allocated-(virtual_height>minimum.y?dip(16):0),minimum.y);}
 void FitInside(){virtual_height=sizer.CalcMin().y;++inside;}
};
struct wxDialog:wxWindow{
 wxScrolledWindow* body;bool shown=false;wxWindow parent;int fits=0;wxSize client{528,100};function<void()> on_fit;
 Sizer sizer{[this]{return wxSize(body->minimum.x+dip(48),body->minimum.y+dip(170));}};
 explicit wxDialog(wxScrolledWindow* b):body(b){parent.display=1;}
 Sizer* GetSizer(){return &sizer;}int FromDIP(int n){return dip(n);}void SetMinSize(wxSize){}
 void Fit(){client=sizer.CalcMin();++fits;if(fits==1&&on_fit)on_fit();}
 void Layout(){body->allocated=client.x-dip(48);}
 bool IsShown(){return shown;}wxWindow* GetParent(){return &parent;}
 wxSize GetSize(){return wxSize(client.x,client.y+dip(20));}wxSize GetClientSize(){return client;}
};
struct wxDisplay{
 explicit wxDisplay(int){} static int GetFromWindow(wxWindow* w){display_owner=w->display;return 0;}
 struct Area{int height;};Area GetClientArea()const{return {dip(screen_height)};}
};
class InputIpAddressDialog{public:bool isIp(string);};
`;
const cases=String.raw`
int main(){
 int checks=0;auto require=[&](bool ok,const char* text){++checks;if(!ok)fprintf(stderr,"FAILED: %s\n",text);return ok;};
 for(int pct:{100,125,150,200})for(int padding:{10,16})for(int work:{360,600,1000}){
  scale=pct;screen_height=work;MD3::Metrics::metrics.padding=padding;
  wxScrolledWindow body;wxDialog dialog(&body);bool fitting=false;
  dialog.on_fit=[&]{fit_continuation_body(&dialog,&body,dip(480),dip(480),fitting);};
  fit_continuation_body(&dialog,&body,dip(480),dip(480),fitting);
  if(!require(dialog.GetSize().y<=dip(work)-2*dip(12),"measured chrome and body fit display"))return 1;
  if(!require(body.virtual_height>body.minimum.y,"full message stays scrollable"))return 1;
  if(!require(body.label.width<=body.GetClientSize().x-2*dip(padding),"text fits scrollbar-adjusted width"))return 1;
  if(!require(dialog.fits==3&&!fitting&&body.inside==2,"nested fit does not recurse"))return 1;
  if(!require(display_owner==1,"hidden dialog uses parent display"))return 1;
 }
 scale=100;screen_height=100;wxScrolledWindow body;wxDialog dialog(&body);bool fitting=false;
 fit_continuation_body(&dialog,&body,480,480,fitting);
 if(!require(body.minimum.y==0,"nonpositive available height has no forced body minimum"))return 1;
 InputIpAddressDialog input;
 for(auto value:{"127.0.0.1","192.168.50.2","0.0.0.0","255.255.255.255"})if(!require(input.isIp(value),"accepted address stays accepted"))return 1;
 for(auto value:{"256.0.1.1","1.2.3.256","1-2.3.4","1.2.3.4extra"})if(!require(!input.isIp(value),"rejected address stays rejected"))return 1;
 printf("%d continuation assertions passed\n",checks);
}
`;
function compileAndRun(helperText){
 const directory=mkdtempSync(path.join(tmpdir(),'print-continuations-'));
 try{
  writeFileSync(path.join(directory,'PrintSetupLayout.hpp'),readFileSync(path.join(root,'src/slic3r/GUI/PrintSetupLayout.hpp')));
  writeFileSync(path.join(directory,'test.cpp'),fixture+helperText+'\n'+ip+'\n'+cases);
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
test('actual body fitter bounds wrapping and preserves address validation',()=>{
 const result=compileAndRun(helper);assert.equal(result.status,0,result.stderr);assert.match(result.stdout,/129 continuation assertions passed/);console.log(result.stdout.trim());
});
test('removing the production display clamp is detected',()=>{
 const mutated=helper.replace(/const int height = PrintSetupLayout::bounded_body_height\([\s\S]*?FromDIP\(12\)\);/,'const int height = preferred_height;');
 assert.notEqual(mutated,helper);
 const result=compileAndRun(mutated);assert.equal(result.status,1);assert.match(result.stderr,/measured chrome and body fit display/);
});
