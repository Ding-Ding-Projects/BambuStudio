import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';
// Windows: an MSVC developer environment with --wx-root <installed-wx-prefix>.
// Elsewhere --wx-root may be omitted: wx-config (WX_CONFIG) names the installed
// wx headers and the host C++ compiler (CXX, default c++) compiles the probe.
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const arg=name=>{const i=process.argv.indexOf(name);return i<0?null:process.argv[i+1]};
const msvc=process.platform==='win32';
const wxRoot=arg('--wx-root');assert.ok(wxRoot||!msvc,'Pass the installed wx prefix with --wx-root');
function hostWx(){
let flags;
try{flags=execFileSync(process.env.WX_CONFIG??'wx-config',['--cxxflags'],{encoding:'utf8'}).trim().split(/\s+/);}
catch(error){assert.fail('Pass --wx-root, or provide wx-config for the installed wx headers: '+error.message);}
const dirs=flags.filter(f=>f.startsWith('-I')).map(f=>f.slice(2));
const include=dirs.find(d=>fs.existsSync(path.join(d,'wx/sizer.h')));
assert.ok(include&&dirs.some(d=>fs.existsSync(path.join(d,'wx/setup.h'))),'wx-config must name real installed wx headers');
return {flags,include};
}
const wx=wxRoot?{flags:null,include:path.join(wxRoot,'include')}:hostWx();
const revision=arg('--source-revision');
const file='src/slic3r/GUI/Tab.cpp';
const source=(revision?execFileSync('git',['show',revision+':'+file],{cwd:root,encoding:'utf8'}):fs.readFileSync(path.join(root,file),'utf8')).replaceAll('\r\n','\n');
const helper=source.slice(source.indexOf('static void applyPresetHeaderAnatomy('),source.indexOf('static const std::vector<std::string> plate_keys'));
const statement=helper.match(/if \(last->IsSpacer\(\)\) last->[^;]+;/)?.[0];assert.ok(statement);
const directory=fs.mkdtempSync(path.join(os.tmpdir(),'preset-spacer-'));
const probe=path.join(directory,'spacer.cpp');
fs.writeFileSync(probe,'#include <wx/sizer.h>\nvoid update(wxSizerItem *last, int padding) { '+statement+' }\n');
const result=wx.flags
?spawnSync(process.env.CXX??'c++',['-std=c++17','-c','-DUNICODE','-D_UNICODE',...wx.flags,probe,'-o',path.join(directory,'spacer.o')],{encoding:'utf8'})
:spawnSync('cl.exe',['/nologo','/std:c++17','/EHsc','/MD','/c', '/D_UNICODE','/DUNICODE', '/I'+path.join(wxRoot,'include'),'/I'+path.join(wxRoot,'lib/vc_x64_lib/mswu'),probe,'/Fo'+path.join(directory,'spacer.obj')],{encoding:'utf8'});
if(result.error)throw result.error;
const output=result.stdout+result.stderr;
if(process.argv.includes('--expect-missing-api')){assert.notEqual(result.status,0);assert.match(output,wx.flags?/no member named/:/C2039/);assert.match(output,/SetSpacer/);console.log('Old production call rejected by actual wx header: '+(wx.flags?'no member':'C2039')+' SetSpacer.');}
else{assert.equal(result.status,0,output);assert.match(statement,/last->SetMinSize\(padding, 1\)/);assert.doesNotMatch(helper,/AssignSpacer|Remove\(|Detach\(|Replace\(|delete /);const header=fs.readFileSync(path.join(wx.include,'wx/sizer.h'),'utf8');assert.match(header,/void SetMinSize\(const wxSize& size\)[\s\S]*?m_minSize = size;/);console.log('Production spacer call compiles against installed wx; item-preserving SetMinSize verified.');}
