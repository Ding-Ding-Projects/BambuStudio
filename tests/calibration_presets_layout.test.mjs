import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
const baseline='7d829820017edf5acd6fa6494dd72d3a1353c56b';
const selected=process.argv.find(x=>x.startsWith('--baseline='))?.slice(11);
const file='src/slic3r/GUI/CalibrationWizardPresetPage.cpp';
const source=rev=>rev?execFileSync('git',['show',rev+':'+file],{encoding:'utf8'}):readFileSync(file,'utf8');
const s=source(selected),old=source(baseline);
const slot=x=>x.slice(x.indexOf('wxSizer* CalibrationPresetPage::create_slot_items_sizer'),x.indexOf('void CalibrationPresetPage::create_multi_extruder_filament_list_panel'));
test('single extruder wraps complete slot rows and both owning panels expand',()=>{
 assert.ok(/slot_ams_items_sizer = new wxWrapSizer\(wxHORIZONTAL\)/.test(slot(s)));
 for(const p of ['panel_sizer->Add(m_single_ams_items_panel, 0, wxEXPAND);','m_top_sizer->Add(m_filament_list_panel, 0, wxEXPAND);']) assert.ok(s.includes(p));
});
test('slot identities and selection callbacks remain exact after only layout changes',()=>{
 const normalize=x=>slot(x).replaceAll('\r\n','\n').replace('new wxFlexGridSizer(2, 2, FromDIP(10), CALIBRATION_FGSIZER_HGAP)','new wxWrapSizer(wxHORIZONTAL)').replace('slot_ams_items_sizer->Add(filament_comboBox_sizer, 0);','slot_ams_items_sizer->Add(filament_comboBox_sizer, 0, wxRIGHT | wxBOTTOM, FromDIP(10));');
 assert.equal(normalize(s),normalize(old));
 assert.notEqual(normalize(s.replace('(i + 4)', '(i + 3)')),normalize(old));
});
test('advice owns measured padded width and refreshes outer scroll extents',()=>{
 assert.ok(s.includes('CalibrationLayout::content_width(GetClientSize().x, FromDIP(20))'));
 assert.ok(s.includes('CalibrationLayout::ReflowPass pass(m_tips_reflow)'));
 assert.ok(s.includes('m_top_sizer->Add(m_tips_panel, 0, wxEXPAND);'));
 assert.ok(s.includes('scroll->FitInside();'));
 assert.ok(!s.includes('Wrap(CALIBRATION_TEXT_MAX_LENGTH * 1.5f)'));
 for(const e of ['SIZE','SHOW','DPI_CHANGED'])assert.ok(s.includes('Bind(wxEVT_'+e));
});
