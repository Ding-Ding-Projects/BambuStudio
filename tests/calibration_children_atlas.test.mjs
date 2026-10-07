import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
const baseline='34fa40252bbb9b755fd22eb503280a045d8923ee';
const files=['CalibrationWizardPage.cpp','CalibrationWizardStartPage.cpp','CalibrationWizardPresetPage.cpp','CalibrationWizardCaliPage.cpp','CalibrationWizardSavePage.cpp'];
const read=p=>readFileSync('src/slic3r/GUI/'+p,'utf8').replaceAll('\r\n','\n');
const old=p=>execFileSync('git',['show',baseline+':src/slic3r/GUI/'+p],{encoding:'utf8',maxBuffer:4*1024*1024}).replaceAll('\r\n','\n');
function tokens(s){return [...s.replace(/^\s*#include[^\n]*/gm,'').matchAll(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|[A-Za-z_]\w*|::|->|[^\s]/g)].map(m=>m[0]).filter(t=>!t.startsWith('//')&&!t.startsWith('/*'));}
function behavior(s){
 s=s.replace(/\n\/\/ An indivisible result table[\s\S]*?(?=\n#define CALIBRATION_SAVE_AMS_NAME_SIZE)/,'');
 s=s.replace(/new CalibrationResultViewport\(parent\)/g,'new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)');
 s=s.replace(/^\s*m_(?:multi_extruder_)?grid_panel->QueueExtent\(\);\n/gm,'');

 s=s.replace(/void CaliPresetTipsPanel::queue_tips_reflow[\s\S]*?(?=void CaliPresetTipsPanel::set_params)/,'');
 s=s.replace(/^.*(?:Bind\(wxEVT_(?:SIZE|SHOW|DPI_CHANGED),.*queue_tips_reflow|queue_tips_reflow\(\);|m_top_sizer->Fit\(this\)).*$/gm,'');
 s=s.replace('m_tips_text = new Label','auto m_tips_text = new Label').replaceAll('m_tips_text','preset_panel_tips');
 s=s.replace('preset_panel_tips->SetMinSize(wxSize(0, -1));','preset_panel_tips->Wrap(CALIBRATION_TEXT_MAX_LENGTH * 1.5f);');
 s=s.replace('auto info_sizer = new wxWrapSizer(wxHORIZONTAL);','auto info_sizer = new wxFlexGridSizer(0, 3, 0, FromDIP(10));\n    info_sizer->SetFlexibleDirection(wxBOTH);\n    info_sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);');
 s=s.replace('slot_ams_items_sizer = new wxWrapSizer(wxHORIZONTAL);','slot_ams_items_sizer = new wxFlexGridSizer(2, 2, FromDIP(10), CALIBRATION_FGSIZER_HGAP);');
 s=s.replace('slot_ams_items_sizer->Add(filament_comboBox_sizer, 0, wxRIGHT | wxBOTTOM, FromDIP(10));','slot_ams_items_sizer->Add(filament_comboBox_sizer, 0);');
 s=s.replace('panel_sizer->Add(m_single_ams_items_panel, 0, wxEXPAND);','panel_sizer->Add(m_single_ams_items_panel, 0);');

 // The viewport reflow unit owns these explicit layout-only additions. Its
 // separate regression validates the production helper and every adapter.
 s=s.replace(/void CalibrationWizardPage::register_wrapped_label[\s\S]*?(?=void CalibrationWizardPage::msw_rescale)/,'');
 s=s.replace(/^.*Bind\(wxEVT_(?:SIZE|SHOW|DPI_CHANGED),.*queue_instruction_reflow.*$/gm,'');
 s=s.replace(/^.*queue_instruction_reflow\(\);.*$/gm,'');
 s=s.replace(/^#define CALIBRATION_START_PAGE_TEXT_MAX_LENGTH.*$/gm,'');
 s=s.replace(/^.*(?:register_wrapped_label\([^;]*|->Wrap\(CALIBRATION_START_PAGE_TEXT_MAX_LENGTH\)|->SetMinSize\(\{CALIBRATION_START_PAGE_TEXT_MAX_LENGTH, -1\}\)|->SetMinSize\([^;]*GetSize\(\)[^;]*)\;.*$/gm,'');

 s=s.replace(/        if \(i != m_steps.size\(\) - 1\) \{\n            auto line = new wxPanel\(this, wxID_ANY, wxDefaultPosition\);\n            line->SetBackgroundColour\(\*wxBLACK\);\n            m_step_sizer->Add\(line, 1, wxALIGN_CENTER\);\n        \}\n/g,'');
 s=s.split('\n').filter(line=>!/^\s*(?:(?:this|[A-Za-z_]\w*(?:\[[^\]]+\])?)->)?(?:SetBackgroundColour|SetForegroundColour|SetFont)\([^;]*;\s*$/.test(line))
 .filter(line=>!/^\s*(?:m_step_sizer|m_top_sizer|top_sizer)->(?:Add|AddSpacer)\([^;]*;\s*$/.test(line)).join('\n');
 return tokens(s).map(t=>t==='wxWrapSizer'?'wxBoxSizer':t);
}
test('five child sources retain non-visual algorithms, values and action routes',()=>{for(const p of files)assert.deepEqual(behavior(read(p)),behavior(old(p)),p);});
test('all labels, keys, units, result names and step strings remain exact',()=>{for(const p of files)assert.deepEqual(tokens(read(p)).filter(t=>t.startsWith('"')),tokens(old(p)).filter(t=>t.startsWith('"')),p);});
test('steps keep their actual order and selected identity in the wrapped guide',()=>{const s=read(files[0]);const guide=s.slice(s.indexOf('CaliPageStepGuide::CaliPageStepGuide'),s.indexOf('CaliPagePicture::CaliPagePicture'));assert.match(guide,/m_step_sizer = new wxWrapSizer/);assert.equal((guide.match(/new Label\(this, m_steps\[i\]\)/g)||[]).length,2);assert.match(guide,/m_text_steps\[index\]->SetBackgroundColour\(StateColor::semantic\(MD3::Role::PrimaryContainer\)\)/);assert.match(guide,/text_step->SetBackgroundColour\(GetBackgroundColour\(\)\)/);assert.doesNotMatch(guide,/AddSpacer\(FromDIP\(90\)\)/);});
test('calibration action group wraps without proportional spacers',()=>{const s=read(files[0]);const actions=s.slice(s.indexOf('CaliPageActionPanel::CaliPageActionPanel'),s.indexOf('void CaliPageActionPanel::bind_button'));assert.match(actions,/new wxWrapSizer\(wxHORIZONTAL\)/);assert.doesNotMatch(actions,/Add\(0, 0, 1/);assert.match(actions,/event.SetInt\(\(int\)m_action_btns\[i\]->get_action_type\(\)\)/);});
test('negative regression detects changed printer action and result identity',()=>{const s=read(files[0]);assert.notDeepEqual(behavior(s),behavior(s.replace('get_action_type());','get_action_type() + 1);')));const save=read(files[4]);const altered=save.replace('default_naming','different_naming');assert.notDeepEqual(behavior(save),behavior(altered));});
