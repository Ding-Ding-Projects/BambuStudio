import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(testDir, '..');
const repoDir = path.resolve(uiDir, '..');

const landing = await readFile(path.join(uiDir, 'landing.html'), 'utf8');
const smartHome = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'SmartHomeDialog.cpp'),
  'utf8'
);
const smartHomeHeader = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'SmartHomeDialog.hpp'),
  'utf8'
);
const msgDialog = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'MsgDialog.cpp'),
  'utf8'
);
const msgDialogHeader = await readFile(
  path.join(repoDir, 'src', 'slic3r', 'GUI', 'MsgDialog.hpp'),
  'utf8'
);

test('landing header has explicit desktop, compact, and phone clipping contracts', () => {
  assert.match(
    landing,
    /@media\s*\(max-width:1120px\)\s*\{[\s\S]*?\.nav a\.navlink,\.nav \.tag\{\s*display:none;/
  );
  assert.match(
    landing,
    /@media\s*\(max-width:640px\)\s*\{[\s\S]*?\.wrap\.nav\{\s*padding-inline:12px;[\s\S]*?\.language-select\{\s*width:128px;[\s\S]*?#launchTop\{\s*width:44px;/
  );
  assert.match(
    landing,
    /@media\s*\(max-width:420px\)\s*\{[\s\S]*?\.brand-name\{[\s\S]*?width:1px;[\s\S]*?clip:rect\(0,0,0,0\);/
  );
  assert.match(
    landing,
    /@media\s*\(max-width:360px\)\s*\{[\s\S]*?\.wrap\.nav\{[\s\S]*?flex-wrap:wrap;[\s\S]*?\.nav \.brand,\.nav \.spacer\{\s*display:none;[\s\S]*?\.language-select\{[\s\S]*?flex:1 0 100%;/
  );
  assert.match(
    landing,
    /\.nav a\.navlink\{\s*min-height:44px;[\s\S]*?display:inline-flex;/
  );
});

test('landing compact controls retain accessible names and 44px targets', () => {
  assert.match(landing, /\.iconbtn\{\s*width:44px;\s*height:44px;/);
  assert.match(landing, /\.language-select\{\s*height:44px;/);
  assert.match(
    landing,
    /id="themeToggle"[^>]*aria-label="Toggle light \/ dark"[\s\S]*?id="launchTop"[^>]*aria-label="Launch app"/
  );
  assert.match(landing, /class="logo" aria-hidden="true"/);
  assert.match(landing, /id="themeIcon" aria-hidden="true"/);
  assert.match(landing, /data-icon aria-hidden="true"[^>]*>rocket_launch/);
});

test('Smart Home keeps a fixed footer around a work-area-capped scrolling body', () => {
  assert.match(smartHome, /MD3Dialog::Options\{true, false\}/);
  assert.match(smartHome, /new wxScrolledWindow\([\s\S]*?wxVSCROLL \| wxTAB_TRAVERSAL/);
  assert.match(smartHome, /GetContentSizer\(\)->Add\(m_scroll, 1, wxEXPAND\)/);
  assert.match(smartHome, /wxDisplay\(display_index\)\.GetClientArea\(\)/);
  assert.match(smartHome, /std::min\(target\.GetHeight\(\), available\.GetHeight\(\)\)/);
  assert.match(smartHome, /CenterOnParent\(\);\s*constrain_to_work_area\(false\);/);
  assert.match(smartHome, /auto \*close = new Button\(this,[\s\S]*?GetFooterSizer\(\)->Add\(close/);
  assert.match(smartHomeHeader, /void on_dpi_changed\(const wxRect &suggested_rect\) override;/);
});

test('Smart Home reflows actions and all changing text at the viewport width', () => {
  assert.match(smartHome, /auto \*media = new wxWrapSizer\(wxHORIZONTAL\)/);
  assert.match(smartHome, /auto \*adds = new wxBoxSizer\(wxVERTICAL\)/);
  assert.match(smartHome, /void make_responsive_action\(Button &button\)/);
  assert.ok(
    (smartHome.match(/make_responsive_action\(\*/g) || []).length >= 8,
    'every text action must use the shared 44-DIP target and natural-width policy'
  );
  assert.match(smartHome, /button\.SetAllowShrink\(false\)/);
  assert.doesNotMatch(smartHome, /button\.SetAllowShrink\(true\)/);
  assert.doesNotMatch(
    smartHome,
    /SetMinSize\(FromDIP\(wxSize\((?:300|210|160|140|136|120|110|104|96),/
  );
  assert.doesNotMatch(smartHome, /wxSize\(FromDIP\(140\), -1\)/);
  assert.match(smartHome, /handover_actions->Add\(m_add_printers, 0, wxEXPAND\)/);
  assert.match(smartHome, /adds->Add\(add_speaker, 0, wxEXPAND/);
  assert.match(smartHome, /speakers_row->Add\(clear_speakers, 0, wxEXPAND/);
  assert.match(smartHome, /auto_wrap \? LB_AUTO_WRAP : 0/);
  assert.match(smartHome, /m_status = label\([\s\S]*?true, true\);/);
  assert.match(smartHome, /m_discovery_status\s*=\s*label\([\s\S]*?true, true\);/);
  assert.match(smartHome, /m_speakers_label\s*=\s*label\([\s\S]*?true, true\);/);
  assert.match(smartHome, /m_lights_label = label\([\s\S]*?true, true\);/);
  assert.doesNotMatch(
    smartHome,
    /m_(?:status|discovery_status|speakers_label|lights_label)->SetLabel\(/
  );
  assert.ok(
    (smartHome.match(/update_wrapped_label\(/g) || []).length >= 9,
    'every dynamic status/entity update must use the relayout helper'
  );
});

test('Smart Home keeps long Home Assistant entity names horizontally reachable', () => {
  assert.match(
    smartHome,
    /new wxListBox\([\s\S]*?wxLB_SINGLE\s*\|\s*wxLB_HSCROLL\)/,
    'the native entity list must expose long friendly names instead of clipping them',
  );
});

test('Smart Home names and enlarges every custom toggle target', () => {
  assert.match(
    smartHome,
    /auto add_toggle =[\s\S]*?store->SetMinSize\(FromDIP\(wxSize\(44, 44\)\)\);[\s\S]*?store->SetName\(text\);/
  );
  assert.match(
    smartHome,
    /m_discovery_toggle->SetMinSize\(FromDIP\(wxSize\(44, 44\)\)\);[\s\S]*?m_discovery_toggle->SetName\(discovery_toggle_copy\);/
  );
  assert.match(smartHome, /m_url->SetName\(_L\("Home Assistant URL"\)\);/);
  assert.match(smartHome, /m_token->SetName\(_L\("Token"\)\);/);
  assert.match(smartHome, /m_url->SetMinSize\(wxSize\(-1, FromDIP\(44\)\)\);/);
  assert.match(smartHome, /m_token->SetMinSize\(wxSize\(-1, FromDIP\(44\)\)\);/);
  assert.match(smartHome, /m_volume->SetName\(_L\("Volume"\)\);/);
});

test('Smart Home preserves saved entity lists and bounds native list rendering', () => {
  assert.doesNotMatch(smartHome, /normalize_config_list|joined_config_list/);
  assert.doesNotMatch(
    smartHome,
    /values\.resize\(kMaxConfiguredHomeAssistantEntities\)/
  );
  assert.match(
    smartHome,
    /std::string updated = cfg->get\(key\);[\s\S]*?updated\.back\(\) != ';'[\s\S]*?updated \+= value;[\s\S]*?cfg->set\(key, updated\);/
  );
  assert.match(
    smartHome,
    /index < values\.size\(\) && index < kMaxConfiguredHomeAssistantEntities/
  );
  assert.match(
    smartHome,
    /if \(values\.size\(\) > kMaxConfiguredHomeAssistantEntities\)\s*break;/
  );
  assert.match(
    smartHome,
    /Existing "\s*"entries were kept; only the first 32 valid entries are active\./
  );
  assert.match(smartHome, /kMaxConfiguredHomeAssistantSegments\s*=\s*256/);
  assert.match(smartHome, /kMaxConfiguredHomeAssistantScanBytes\s*=\s*64 \* 1024/);
  assert.match(smartHome, /kMaxConfiguredHomeAssistantValueBytes\s*=\s*256/);
  assert.match(
    smartHome,
    /std::find\(\s*raw\.begin\(\) \+ begin,\s*raw\.begin\(\) \+ scan_end,\s*';'\)/
  );
  assert.match(smartHome, /kMaxVisibleHomeAssistantEntities\s*=\s*256/);
  assert.match(
    smartHome,
    /m_list->Freeze\(\);\s*m_list->Set\(rows\);\s*m_list->Thaw\(\);/
  );
  assert.doesNotMatch(smartHome, /m_list->Append\(/);
  assert.match(
    smartHome,
    /Showing %d of %d matching speakers and lights\./
  );
  assert.match(
    smartHomeHeader,
    /Label\s+\*m_config_limit_status[\s\S]*?Label\s+\*m_results_status/
  );
});

test('message dialogs wrap, refit and stack actions inside the active work area', () => {
  assert.match(msgDialog, /wxDisplay\(display_index\)\.GetClientArea\(\)/);
  assert.match(
    msgDialog,
    /bounded_message_content_width\(parent, 68 \* em\)/
  );
  assert.match(
    msgDialog,
    /void MsgDialog::refit_to_work_area\(bool recenter\)[\s\S]*?available\.GetWidth\(\)[\s\S]*?SetSize\(target\)/
  );
  assert.match(
    msgDialog,
    /void MsgDialog::reflow_footer_for_width\(int available_width\)[\s\S]*?SetCols\(1\)[\s\S]*?SetRows\(1\)/
  );
  assert.match(
    msgDialog,
    /SetButtonLabel[\s\S]*?refit_to_work_area\(!IsShown\(\)\)/
  );
  assert.match(
    msgDialog,
    /show_dsa_button[\s\S]*?refit_to_work_area\(!IsShown\(\)\)/
  );
  assert.match(
    msgDialog,
    /m_footer_content_sizer = new wxBoxSizer\(wxVERTICAL\)/
  );
  assert.match(
    msgDialog,
    /m_dsa_row_sizer->Add\(m_dsa_sizer, 1, wxEXPAND\)/
  );
  assert.match(msgDialog, /m_text_dsa->SetLabel\(m_dsa_text\)/);
  assert.match(msgDialog, /m_text_dsa->Wrap\(text_budget\)/);
  assert.match(msgDialog, /MD3Dialog::on_dpi_changed\(suggested_rect\)/);
  assert.match(msgDialog, /btn->SetMinSize\(FromDIP\(wxSize\(44, 42\)\)\)/);
  assert.match(msgDialog, /btn->SetAllowShrink\(true\)/);
  assert.match(msgDialogHeader, /wxFlexGridSizer \*m_action_sizer/);
});
