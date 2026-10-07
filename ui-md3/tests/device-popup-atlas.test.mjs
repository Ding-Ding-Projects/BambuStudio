import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

// Source-derived geometry and lifecycle checks, not rendered-window evidence.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = name => {
  const file = `src/slic3r/GUI/${name}`;
  return process.env.ATLAS_SOURCE_REF
    ? execFileSync('git', ['show', `${process.env.ATLAS_SOURCE_REF}:${file}`], { cwd: root, encoding: 'utf8' })
    : readFileSync(path.join(root, file), 'utf8');
};
const fan = read('Widgets/FanControl.cpp');
const mapping = read('AmsMappingPopup.cpp');
const materials = read('AMSMaterialsSetting.cpp');
const settings = read('AMSSetting.cpp');

test('fan focus stroke remains inside the existing hit rectangle at every supported scale', () => {
  const paint = fan.slice(fan.indexOf('void FanOperate::doRender('), fan.indexOf('void FanOperate::msw_rescale()'));
  const insetExpression = paint.match(/const int inset = ([^;]+);/);
  assert.ok(insetExpression);
  assert.match(paint, /DrawRoundedRectangle\(inset, inset, std::max\(0, size.x - 2 \* inset\)/);
  const insetAt = new Function('FromDIP', `return ${insetExpression[1].replace(/std::max/g, 'Math.max')};`);
  for (const scale of [1, 1.25, 1.5, 2]) {
    const fromDIP = n => Math.round(n * scale);
    const inset = insetAt(fromDIP);
    const strokeWidth = Math.max(fromDIP(2), 1);
    for (const extent of [fromDIP(24), fromDIP(160)]) {
      const paintedStart = inset - strokeWidth / 2;
      const paintedEnd = inset + Math.max(0, extent - 2 * inset) + strokeWidth / 2;
      assert.ok(paintedStart >= 0);
      assert.ok(paintedEnd <= extent);
    }
  }
  assert.match(paint, /dc.SetFont\(::Label::Mono_13\)/);
});

test('mapping header grows from its actual padded label instead of a fixed height', () => {
  const header = mapping.slice(mapping.indexOf('auto title_panel ='), mapping.indexOf('m_left_marea_panel ='));
  assert.doesNotMatch(header, /title_panel->Set(?:Min)?Size\(/);
  assert.match(header, /m_title_text->SetFont\(::Label::Head_16\)/);
  assert.match(header, /title_sizer_v->Add\(m_title_text, 0, wxALIGN_LEFT \| wxALL, FromDIP\(12\)\)/);
  assert.match(header, /title_panel->Fit\(\)/);
});

test('filament field retains its Atlas radius after DPI changes and leaves input semantics intact', () => {
  const radiusCalls = materials.match(/m_filament_box->SetCornerRadius\([^;]+/g);
  assert.equal(radiusCalls.length, 2);
  for (const call of radiusCalls) assert.match(call, /MD3::Metrics::active\(\).small_radius/);
  assert.match(materials, /m_filament_arrow->SetIconButton\(Button::IconShape::Circle, 20\)/);
  assert.match(materials, /m_filament_arrow->Bind\(wxEVT_BUTTON, \[this\]\(wxCommandEvent&\) \{ on_open_filament_select_dialog\(\); \}\)/);
  assert.match(materials, /m_nozzle_temp_label->SetFont\(::Label::Mono_13\)/);
});

test('AMS settings body has one padded sizer owner and its real content determines the dialog fit', () => {
  assert.match(settings, /body_frame->Add\(m_sizerl_body, 1, wxEXPAND \| wxALL, FromDIP\(16\)\)/);
  assert.match(settings, /m_panel_body->SetSizer\(body_frame\)/);
  assert.match(settings, /body_frame->Fit\(m_panel_body\)/);
  assert.doesNotMatch(settings, /m_panel_body->SetSizer\(m_sizerl_body\)/);
});
