import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
const baseline='eb07dfe7e6f61f5ae5601c42a179cecd0dbbbe46';
const selected=process.argv.find(x=>x.startsWith('--baseline='))?.slice(11);
const file='src/slic3r/GUI/CalibrationWizardSavePage.cpp';
const read=rev=>(rev?execFileSync('git',['show',rev+':'+file],{encoding:'utf8'}):readFileSync(file,'utf8')).replaceAll('\r\n','\n');
const current=read(selected),old=read(baseline);
function normalize(s){
 return s.replace('#include "Widgets/MD3ScrolledWindow.hpp"\n','')
 .replace(/\n\/\/ An indivisible result table[\s\S]*?(?=\n#define CALIBRATION_SAVE_AMS_NAME_SIZE)/,'')
 .replace(/new CalibrationResultViewport\(parent\)/g,'new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)')
 .replace(/m_top_sizer->Add\((m_(?:multi_extruder_)?grid_panel), 0, wxEXPAND\);/g,'m_top_sizer->Add($1, 0, wxALIGN_CENTER);')
 .replace(/^\s*m_(?:multi_extruder_)?grid_panel->QueueExtent\(\);\n/gm,'');
}
test('three result tables have an expanding horizontal viewport owner',()=>{
 assert.equal((current.match(/new CalibrationResultViewport\(parent\)/g)||[]).length,3);
 assert.equal((current.match(/m_top_sizer->Add\(m_(?:multi_extruder_)?grid_panel, 0, wxEXPAND\)/g)||[]).length,3);
 assert.ok(current.includes('wxTAB_TRAVERSAL | wxHSCROLL | wxBORDER_NONE'));
});
test('complete table construction, child parents, headers, values and action bodies remain exact',()=>{
 assert.equal(normalize(current),normalize(old));
 assert.notEqual(normalize(current.replace('item.extruder_id);','MAIN_EXTRUDER_ID);')),normalize(old));
});
test('table height comes from content sizer and updates on resize, DPI, show and content',()=>{
 assert.ok(current.includes('const wxSize table = GetSizer()->CalcMin();'));
 assert.ok(current.includes('table.x > width ? BarThickness(this) : 0'));
 assert.equal((current.match(/->QueueExtent\(\);/g)||[]).length,6);
 for(const event of ['SIZE','SHOW','DPI_CHANGED'])assert.ok(current.includes('Bind(wxEVT_'+event));
});
