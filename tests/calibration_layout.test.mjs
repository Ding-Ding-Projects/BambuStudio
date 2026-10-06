import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
const revision=process.argv.find(x=>x.startsWith('--baseline='))?.slice(11);
const read=p=>revision?execFileSync('git',['show',revision+':src/slic3r/GUI/'+p],{encoding:'utf8'}):readFileSync('src/slic3r/GUI/'+p,'utf8');
test('page minimum no longer imposes a viewport-independent 1100 DIP floor',()=>{
 assert.match(read('CalibrationWizardPage.hpp'),/#define MIN_CALIBRATION_PAGE_WIDTH\s+-1\b/);
});
test('all ten instruction owners register for measured reflow and expand',()=>{
 const s=read('CalibrationWizardStartPage.cpp');
 for(const name of ['m_when_title','m_when_content','m_about_title','m_about_content','extra_text','auto_cali_title','auto_cali_content','recommend_title','recommend_text1','recommend_text2']){
  assert.ok(s.includes('register_wrapped_label('+name+');'),name);
  assert.ok(s.includes('m_top_sizer->Add('+name+', 0, wxEXPAND);'),name);
 }
 assert.doesNotMatch(s,/CALIBRATION_START_PAGE_TEXT_MAX_LENGTH/);
 assert.doesNotMatch(s,/SetMinSize\([^;]*GetSize\(\)/);
});
test('shell allocates viewport width and instruction reflow refreshes outer scroll extent',()=>{
 const shell=read('CalibrationWizard.cpp'),page=read('CalibrationWizardPage.cpp');
 assert.match(shell,/padding_sizer->Add\(m_all_pages_sizer, 1, wxEXPAND\)/);
 assert.doesNotMatch(shell,/padding_sizer->Add\(0, 0, 1\)/);
 assert.match(page,/CalibrationLayout::content_width\(GetClientSize\(\).x, 0\)/);
 assert.match(page,/m_instruction_reflow.request\(\)/);
 assert.match(page,/CalibrationLayout::ReflowPass pass\(m_instruction_reflow\)/);
 assert.match(page,/label->GetWindowStyle\(\) \| LB_AUTO_WRAP/);
 assert.match(page,/scroll->FitInside\(\)/);
 for(const event of ['wxEVT_SIZE','wxEVT_SHOW','wxEVT_DPI_CHANGED']) assert.ok(page.includes('Bind('+event),event);
});
