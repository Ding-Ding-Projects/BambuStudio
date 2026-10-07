import {readFileSync,writeFileSync,mkdtempSync} from 'node:fs';import {spawnSync} from 'node:child_process';import {tmpdir} from 'node:os';import path from 'node:path';import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');const source=readFileSync(path.join(root,'src/slic3r/GUI/AMSDryControl.cpp'),'utf8');
function method(signature){const start=source.indexOf(signature),open=source.indexOf('{',start);if(start<0)throw new Error('Missing production method');const re=/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]/g;re.lastIndex=open;let depth=0;for(let m;(m=re.exec(source));){if(m[0]==='{')depth++;if(m[0]==='}'&&!--depth)return source.slice(start,m.index+1);}throw new Error('Unterminated method');}
let methods=method('Button* AMSDryCtrWin::create_button')+'\n'+method('void AMSDryCtrWin::update_button_size');
if(process.argv.includes('--negative'))methods=methods.replace('Button::Variant::Danger','Button::Variant::Filled');
const fixture=`#include <algorithm>
#include <iostream>
#include <string>
using wxString=std::string;using wxColour=int;struct wxPanel{};
namespace ThemeColor{constexpr int Danger=1,Grey200=2;}
struct wxSize{int x,y;wxSize(int a,int b):x(a),y(b){}int GetWidth()const{return x;}int GetHeight()const{return y;}};
struct Button{enum class Variant{Danger,Outlined,Filled};Variant variant=Variant::Outlined;wxString text;wxSize minimum{-1,-1};int scale=1;Button(wxPanel*,const wxString&s):text(s){}void SetVariant(Variant v){variant=v;}void SetMinSize(wxSize s){minimum=s;}wxSize GetBestSize()const{return {std::max(minimum.x,int(text.size()*8+32)*scale),std::max(minimum.y,42*scale)};}};
struct AMSDryCtrWin{int scale=1;int FromDIP(int x)const{return x*scale;}Button* create_button(wxPanel*,const wxString&,const wxColour&,const wxColour&,const wxColour&);void update_button_size(Button*);};
${methods}
int main(){int failures=0;auto check=[&](bool ok,const char*name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\\n';failures+=!ok;};AMSDryCtrWin owner;wxPanel panel;auto* start=owner.create_button(&panel,"Start",3,3,3);auto* stop=owner.create_button(&panel,"Stop",1,1,1);auto* back=owner.create_button(&panel,"Back",2,2,2);
check(start->variant==Button::Variant::Filled,"primary action");check(stop->variant==Button::Variant::Danger,"destructive action");check(back->variant==Button::Variant::Outlined,"secondary action");int initial=stop->minimum.x;stop->text="Stopping";owner.update_button_size(stop);int longer=stop->minimum.x;stop->text="Stop";owner.update_button_size(stop);check(longer>initial&&stop->minimum.x==initial,"changed action text grows and shrinks");owner.scale=2;stop->scale=2;owner.update_button_size(stop);check(stop->minimum.x==initial*2&&stop->minimum.y==88,"DPI preserves measured target");delete start;delete stop;delete back;std::cout<<5-failures<<"/5 cases passed\\n";return failures?1:0;}
`;
const output=mkdtempSync(path.join(tmpdir(),'bambustudio-drying-buttons-')),cpp=path.join(output,'fixture.cpp'),exe=path.join(output,'fixture.exe');writeFileSync(cpp,fixture);console.log('Task-owned output: '+output);
const compile=spawnSync('cl.exe',['/nologo','/std:c++17','/EHsc','/W4',cpp,'/Fe:'+exe,'/Fo:'+path.join(output,'fixture.obj')],{cwd:output,stdio:'inherit'});if(compile.error)throw compile.error;if(compile.status!==0)process.exit(compile.status??1);const run=spawnSync(exe,[],{cwd:output,stdio:'inherit'});if(run.error)throw run.error;process.exit(run.status??1);
