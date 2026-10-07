import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

// Source geometry checks, not native rendering. The optional revision allows
// the same assertions to demonstrate failure against the preserved baseline.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = name => {
  const file = `src/slic3r/GUI/${name}`;
  return process.env.ATLAS_SOURCE_REF
    ? execFileSync('git', ['show', `${process.env.ATLAS_SOURCE_REF}:${file}`], { cwd: root, encoding: 'utf8' })
    : readFileSync(path.join(root, file), 'utf8');
};
const popup = read('CameraPopup.cpp');
const hud = read('Widgets/CameraHUD.cpp');
const body = (source, name) => {
  const start = source.indexOf(name);
  assert.ok(start >= 0, name);
  const open = source.indexOf('{', start);
  let depth = 1;
  for (let i = open + 1; i < source.length; ++i) {
    if (source[i] === '{') ++depth;
    if (source[i] === '}' && --depth === 0) return source.slice(open + 1, i);
  }
  assert.fail(name);
};

test('resolution choices derive their extent from both real controls and padded sizer rows', () => {
  const row = body(popup, 'wxWindow* CameraPopup::create_item_radiobox(');
  assert.doesNotMatch(row, /SetPosition|wxSize\(-1, FromDIP\(20\)\)/);
  assert.match(row, /row->Add\(radiobox, 0, wxALIGN_CENTER_VERTICAL \| wxTOP \| wxBOTTOM, FromDIP\(8\)\)/);
  assert.match(row, /row->Add\(text, 0, wxALIGN_CENTER_VERTICAL \| wxLEFT \| wxTOP \| wxBOTTOM, FromDIP\(8\)\)/);
  assert.match(row, /item->SetSizerAndFit\(row\)/);
  assert.equal((row.match(/if \(m_obj && allow_alter_resolution\)/g) || []).length, 2);
  assert.equal((row.match(/on_set_resolution\(\)/g) || []).length, 2);
});

test('camera popup uses a shared tonal body and measured content width', () => {
  assert.match(popup, /m_panel->SetBackgroundColour\(StateColor::semantic\(MD3::Role::SurfaceContainerLow\)\)/);
  assert.match(popup, /item->SetBackgroundColour\(StateColor::semantic\(MD3::Role::SurfaceContainerLow\)\)/);
  assert.match(popup, /main_sizer->Add\(top_sizer, 0, wxEXPAND \| wxALL, FromDIP\(16\)\)/);
});

test('temperature extent grows for measured text and paint uses the allocated height', () => {
  const measure = body(hud, 'wxSize CameraHUD::CameraHUDTempChip::DoGetBestSize() const');
  const expression = measure.match(/return wxSize\((.+)\);/)[1].replace(/std::max/g, 'Math.max');
  const minHeight = Number(hud.match(/kTempChipPillH = (\d+)/)[1]);
  const padX = Number(hud.match(/kTempChipPadX\s*=\s*(\d+)/)[1]);
  const evaluate = new Function('tw', 'th', 'FromDIP', 'kTempChipPadX', 'kTempChipPillH', `return [${expression}];`);
  for (const scale of [1, 1.25, 1.5, 2]) {
    const fromDIP = n => Math.round(n * scale);
    for (const textHeight of [fromDIP(13), fromDIP(38)]) {
      const [width, height] = evaluate(90, textHeight, fromDIP, padX, minHeight);
      assert.equal(width, 90 + 2 * fromDIP(padX));
      assert.ok(height >= textHeight + fromDIP(8));
    }
  }
  assert.match(measure, /&::Label::Mono_13/);
  assert.match(body(hud, 'void CameraHUD::CameraHUDTempChip::on_paint('), /const double pillH\s*= sz.y/);
  assert.match(body(hud, 'void CameraHUD::CameraHUDTempChip::msw_rescale()'), /SetFont\(::Label::Mono_13\);\s*InvalidateBestSize\(\)/);
});

test('Atlas camera roles preserve the system high-contrast alternative', () => {
  for (const name of ['CardBg', 'Border', 'ChipBg', 'ChipHover', 'ChipPress', 'Glyph', 'GlyphMuted', 'FocusRing']) {
    const palette = body(hud, `wxColour CameraHUD::${name}()`);
    assert.match(palette, /HighContrastActive\(\) \? wxSystemSettings::GetColour/);
    assert.match(palette, /: MD3::resolve\(MD3::Role::\w+, true/);
  }
});
