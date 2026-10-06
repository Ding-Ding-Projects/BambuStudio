import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
const baseline='be5e1205dcdad8f372d2ba63f367dbd7977c29c4';
const paths=['Project.cpp','Preferences.cpp','CalibrationPanel.cpp','CalibrationWizard.cpp','ConfigWizard.cpp','Schedule/ScheduledSettingsPanel.cpp'];
const read=p=>readFileSync('src/slic3r/GUI/'+p,'utf8');
const old=p=>execFileSync('git',['show',baseline+':src/slic3r/GUI/'+p],{encoding:'utf8',maxBuffer:4*1024*1024});
function tokens(source){return [...source.replace(/^\s*#include[^\n]*/gm,'').matchAll(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|::|->|[^\s]/g)].map(m=>m[0]).filter(t=>!t.startsWith('//')&&!t.startsWith('/*'));}
const protectedCalls=new Set(['Bind','Connect','SetOnQuery','SetOnRegexToggle','set_change_listener','register_option_row','add_tab','SetId','SetName','SetSelection','SetValue']);
function calls(source){const t=tokens(source),out=[];for(let i=0;i<t.length;i++){if(!protectedCalls.has(t[i])||t[i+1]!=='(')continue;let depth=0,j=i+1;for(;j<t.length;j++){if(t[j]==='(')depth++;if(t[j]===')'&&!--depth)break;}out.push(t.slice(i,j+1).join(' '));i=j;}return out;}
function literals(source){return tokens(source).filter(t=>t.startsWith('"'));}
function cardContract(project,preferences,schedules){
 assert.match(project,/new StaticBox\(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL\)/);
 assert.match(project,/new Button\(navigation, _L\("Online projects"\)\)/);
 assert.match(project,/new Button\(navigation, _L\("Workspace"\)\)/);
 assert.match(project,/navigation->SetSizer\(view_actions\)/);
 assert.match(preferences,/new wxSimplebook\(content_card, wxID_ANY\)/);
 assert.match(preferences,/new SearchField\(content_card, _L\("Search settings"\)\)/);
 assert.match(preferences,/content_card->SetSizer\(content_pane\)/);
 assert.match(preferences,/m_body_row->Add\(content_card, 1, wxEXPAND/);
 assert.match(schedules,/auto \*card = new StaticBox\(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL\)/);
 for(const type of ['Label','SearchField','ListBox','Button']) assert.match(schedules,new RegExp('new '+type+'\\(card,'));
 assert.match(schedules,/card->SetSizer\(sizer\)/);
 assert.match(schedules,/outer->Add\(card, 0, wxEXPAND/);
 assert.match(schedules,/SetSizer\(outer\)/);
}
test('all six anchors retain bindings, registered options and value routes',()=>{for(const p of paths)assert.deepEqual(calls(read(p)),calls(old(p)),p);});
test('all six anchors retain every literal and localization key',()=>{for(const p of paths)assert.deepEqual(literals(read(p)),literals(old(p)),p);});
test('project, preferences and schedules retain owned child containment and keyboard traversal',()=>{cardContract(read(paths[0]),read(paths[1]),read(paths[5]));});
test('footer and destination action groups wrap without stretch spacers',()=>{
 const p=read('Preferences.cpp');const start=p.indexOf('wxBoxSizer *PreferencesDialog::create_bottom_buttons()');const end=p.indexOf('ResetWarningsDialog::',start);const footer=p.slice(start,end);
 assert.match(footer,/new wxWrapSizer\(wxHORIZONTAL\)/);assert.doesNotMatch(footer,/AddStretchSpacer/);
 assert.match(read('Project.cpp'),/auto \*view_actions = new wxWrapSizer\(wxHORIZONTAL\)/);
});
test('existing project, schedule and calibration engine tails are unchanged',()=>{
 for(const [p,marker] of [['Project.cpp','ProjectPanel::~ProjectPanel'],['Schedule/ScheduledSettingsPanel.cpp','ScheduledSettingsPanel::~ScheduledSettingsPanel'],['CalibrationWizard.cpp','CalibrationWizard::~CalibrationWizard']]){
  const a=read(p).replaceAll('\r\n','\n'),b=old(p).replaceAll('\r\n','\n');assert.ok(a.includes(marker));assert.equal(a.slice(a.indexOf(marker)),b.slice(b.indexOf(marker)),p);
 }
});
test('setup preserves page headings and primary versus secondary action hierarchy',()=>{
 const p=read('ConfigWizard.cpp');assert.match(p,/const auto font = Label::Head_20;/);
 for(const [button,variant] of [['prev','Outlined'],['next','Filled'],['finish','Filled'],['cancel','Text']]) assert.ok(p.includes('p->btn_'+button+'->SetVariant(Button::Variant::'+variant+');'));
 assert.match(read('Schedule/ScheduledSettingsPanel.cpp'),/make_label\(_L\("Schedules"\), Label::Head_20/);
});
test('negative regressions reject callback replacement and missing keyboard traversal',()=>{
 const s=read('Schedule/ScheduledSettingsPanel.cpp');assert.notDeepEqual(calls(s),calls(s.replace('delete_selected();','toggle_selected();')));
 assert.throws(()=>cardContract(read('Project.cpp'),read('Preferences.cpp'),s.replace('wxTAB_TRAVERSAL);','0);')),{name:'AssertionError'});
});
