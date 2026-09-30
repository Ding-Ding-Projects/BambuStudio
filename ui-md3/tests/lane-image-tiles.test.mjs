import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Bitmap holders: the thumbnail wells of the colour import and Sync AMS dialogs,
// the colour swatches of the colour import dialog and the tray swatch of the
// Record Factor step. They were native push buttons (owner-drawn, so no system
// face survived, but each was still a button window, a Tab stop with no visible
// focus state, and skipped by the dark-mode pass through its BU_AUTODRAW style).
// They are kit Buttons now: a flat display tile (wells and swatches on a known
// surface) or a kit icon button (a swatch on a page that is remapped by the
// dark-mode pass), never a Tab stop, with the bitmap given through
// Button::SetIconBitmap.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const read = async (name) => code(await readFile(path.join(gui, name), 'utf8'));
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `missing ${signature}`);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

const FILES = [
  'ObjColorDialog.cpp',
  'ObjColorDialog.hpp',
  'SyncAmsInfoDialog.cpp',
  'SyncAmsInfoDialog.hpp',
  'CalibrationWizardSavePage.cpp',
  'CalibrationWizardSavePage.hpp',
];

test('no native push button or auto-draw style is left in the three bitmap-holder surfaces', async () => {
  for (const name of FILES) {
    const text = await read(name);
    assert.doesNotMatch(text, /new\s+wxButton\s*\(/, `${name} must not construct a wxButton`);
    assert.doesNotMatch(text, /\bwxButton\s*\*/, `${name} must not hold a wxButton pointer`);
    assert.doesNotMatch(text, /\bwxBU_AUTODRAW\b/, `${name} must not use the auto-draw marker style`);
  }
});

// A display tile is a plain kit Button that only shows a bitmap. The recipe
// matters: the padding first (the default is 10 x 8), then the minimum size,
// then the bitmap last, because a plain Button does not measure itself when it
// gets an icon and the minimum size set AFTER the bitmap would add the
// icon-to-label gap to the width.
function assertTileRecipe(body, who, label) {
  const at = (re) => {
    const hit = body.search(re);
    assert.notEqual(hit, -1, `${label}: missing ${re}`);
    return hit;
  };
  const padding = at(new RegExp(`${who}->SetPaddingSize\\(wxSize\\(0, 0\\)\\);`));
  const minSize = at(new RegExp(`${who}->SetMinSize\\(`));
  at(new RegExp(`${who}->SetCornerRadius\\(0\\);`));
  at(new RegExp(`${who}->SetCanFocus\\(false\\);`));
  assert.ok(padding < minSize, `${label}: the padding must be set before the minimum size`);
  return { padding, minSize };
}

test('the colour import thumbnails are flat kit tiles on the well surface', async () => {
  const cpp = await read('ObjColorDialog.cpp');
  const ctor = fn(cpp, 'ObjColorPanel::ObjColorPanel(');
  for (const [who, size, fill] of [
    ['m_left_image_button', 'LEFT_THUMBNAIL_SIZE_WIDTH', 'SurfaceContainerHigh'],
    ['m_right_image_button', 'RIGHT_THUMBNAIL_SIZE_WIDTH', 'SurfaceContainerHigh'],
  ]) {
    assert.match(ctor, new RegExp(`${who}\\s*=\\s*new Button\\(m_two_image_panel, wxEmptyString, wxEmptyString, wxBORDER_NONE, 0\\);`), `${who} must be a kit Button`);
    assertTileRecipe(ctor, who, who);
    assert.match(ctor, new RegExp(`${who}->SetMinSize\\(wxSize\\(FromDIP\\(${size}\\), FromDIP\\(${size}\\)\\)\\);`), `${who} must be sized through FromDIP`);
    assert.match(ctor, new RegExp(`${who}->SetBackgroundColorNormal\\(StateColor::semantic\\(MD3::Role::${fill}\\)\\);`), `${who} fill must be the ${fill} role`);
    assert.doesNotMatch(cpp, new RegExp(`${who}->SetBackgroundColour\\(`), `${who}: the window background is not the tile fill`);
    assert.match(cpp, new RegExp(`${who}->SetIconBitmap\\(`), `${who} must show its bitmap through SetIconBitmap`);
    assert.doesNotMatch(cpp, new RegExp(`${who}->SetBitmap\\(`), `${who} must not call the native SetBitmap`);
  }
  const hpp = await read('ObjColorDialog.hpp');
  assert.match(hpp, /\bButton\s*\*\s*m_left_image_button\b/);
  assert.match(hpp, /\bButton\s*\*\s*m_right_image_button\b/);
  assert.match(hpp, /create_sizer_thumbnail\(Button\s*\*\s*image_button, bool left\)/);
  assert.match(cpp, /ObjColorPanel::create_sizer_thumbnail\(Button\s*\*\s*image_button, bool left\)/);
  assert.doesNotMatch(hpp, /\bm_image_button\b/, 'the never-assigned m_image_button member is gone');
});

test('the colour import swatches are kit tiles on the page surface and keep their Tab order clean', async () => {
  const cpp = await read('ObjColorDialog.cpp');
  const hpp = await read('ObjColorDialog.hpp');
  assert.match(hpp, /std::vector<Button\s*\*>\s*m_extruder_icon_list;/);
  assert.match(hpp, /std::vector<Button\s*\*>\s*m_color_cluster_icon_list;/);

  const slot = fn(cpp, 'wxBoxSizer *ObjColorPanel::create_extruder_icon_and_rgba_sizer(');
  const cluster = fn(cpp, 'wxBoxSizer *ObjColorPanel::create_color_icon_map_rgba_sizer(');
  for (const [body, label] of [[slot, 'the slot swatch'], [cluster, 'the cluster swatch']]) {
    assert.match(body, /Button\s*\*\s*icon\s*=\s*new Button\(parent, wxEmptyString, wxEmptyString, wxBORDER_NONE, 0\);/, `${label} must be a kit Button`);
    const { minSize } = assertTileRecipe(body, 'icon', label);
    assert.match(body, /icon->SetMinSize\(ICON_SIZE\);/, `${label} must be the DPI-aware 16 DIP size`);
    assert.match(body, /icon->SetBackgroundColorNormal\(StateColor::semantic\(MD3::Role::SurfaceContainerLowest\)\);/, `${label} fill must be the page role`);
    const bitmap = body.search(/icon->SetIconBitmap\(\*get_extruder_color_icon\(/);
    assert.notEqual(bitmap, -1, `${label} must show its bitmap through SetIconBitmap`);
    assert.ok(minSize < bitmap, `${label}: the bitmap must be set after the minimum size`);
    assert.equal((body.match(/SetCanFocus\(false\)/g) ?? []).length, 1, `${label}: exactly one SetCanFocus(false)`);
  }
  assert.match(cluster, /get_extruder_color_icon\(color\.GetAsString\(wxC2S_HTML_SYNTAX\)\.ToStdString\(\), "", FromDIP\(16\), FromDIP\(16\)\)/, 'the cluster swatch keeps its empty label');

  assert.match(fn(cpp, 'void ObjColorPanel::msw_rescale()'), /m_extruder_icon_list\[i\]->SetIconBitmap\(bitmap\);/, 'a DPI change re-bitmaps the slot swatches through the kit');
  assert.match(fn(cpp, 'void ObjColorPanel::draw_new_table()'), /m_color_cluster_icon_list\[id\]->SetIconBitmap\(\*get_extruder_color_icon\(/, 'the table redraw re-bitmaps the cluster swatches through the kit');
  assert.doesNotMatch(cpp, /(?:m_extruder_icon_list\[i\]|m_color_cluster_icon_list\[id\])->SetBitmap\(/);
});

test('the Sync AMS thumbnails are flat kit tiles on the compare panel surface', async () => {
  const cpp = await read('SyncAmsInfoDialog.cpp');
  const hpp = await read('SyncAmsInfoDialog.hpp');
  assert.match(hpp, /\bButton\s*\*\s*m_left_image_button\s*=\s*nullptr;/);
  assert.match(hpp, /\bButton\s*\*\s*m_right_image_button\s*=\s*nullptr;/);
  assert.match(hpp, /create_sizer_thumbnail\(Button\s*\*\s*image_button, bool left\)/);
  assert.match(cpp, /SyncAmsInfoDialog::create_sizer_thumbnail\(Button\s*\*\s*image_button, bool left\)/);
  for (const [who, size] of [
    ['m_left_image_button', 'LEFT_THUMBNAIL_SIZE_WIDTH'],
    ['m_right_image_button', 'RIGHT_THUMBNAIL_SIZE_WIDTH'],
  ]) {
    assert.match(cpp, new RegExp(`${who}\\s*=\\s*new Button\\(m_two_image_panel, wxEmptyString, wxEmptyString, wxBORDER_NONE, 0\\);`), `${who} must be a kit Button`);
    assertTileRecipe(cpp, who, who);
    assert.match(cpp, new RegExp(`${who}->SetMinSize\\(wxSize\\(FromDIP\\(${size}\\), FromDIP\\(${size}\\)\\)\\);`), `${who} must be sized through FromDIP`);
    const fills = cpp.match(new RegExp(`${who}->SetBackgroundColorNormal\\(StateColor::semantic\\(MD3::Role::SurfaceContainer\\)\\);`, 'g')) ?? [];
    assert.equal(fills.length, 2, `${who}: the fill is set at creation and again each time the dialog is shown`);
    assert.doesNotMatch(cpp, new RegExp(`${who}->SetBackgroundColour\\(`), `${who}: the window background is not the tile fill`);
    assert.match(cpp, new RegExp(`${who}->SetIconBitmap\\(`), `${who} must show its bitmap through SetIconBitmap`);
    assert.doesNotMatch(cpp, new RegExp(`${who}->SetBitmap\\(`), `${who} must not call the native SetBitmap`);
  }
});

test('the Record Factor tray swatch is a kit icon button that follows the plate and is not stretched', async () => {
  const cpp = await read('CalibrationWizardSavePage.cpp');
  const body = fn(cpp, 'void CaliPASaveAutoPanel::sync_cali_result_for_multi_extruder(');
  assert.match(body, /Button\s*\*\s*tray_title\s*=\s*new Button\(m_multi_extruder_grid_panel, wxEmptyString, wxEmptyString, wxBORDER_NONE, 0\);/);
  // SetIconButton takes design pixels and scales them itself: 20, not FromDIP(20).
  assert.match(body, /tray_title->SetIconButton\(Button::IconShape::Circle, 20\);/);
  assert.match(body, /tray_title->SetCanFocus\(false\);/);
  assert.match(body, /tray_title->SetIconBitmap\(\*get_extruder_color_icon\(full_filament_ams_list\[item\.tray_id\]\.opt_string\("filament_colour", 0u\), tray_name\.ToStdString\(\), FromDIP\(20\), FromDIP\(20\)\)\);/);
  // No colour of its own: the plate is the literal white that the dark-mode pass remaps,
  // so a fixed fill would not match it in both themes.
  assert.doesNotMatch(body, /tray_title->Set(?:Background|Border|Text)Colo(?:u)?r(?:Normal)?\(/);
  assert.doesNotMatch(body, /tray_title->SetBitmap\(/);
  assert.doesNotMatch(body, /(?:left|right)_grid_sizer->Add\(tray_title, 1, wxEXPAND\);/, 'the swatch keeps its own size and is centred in its cell');
  assert.match(body, /left_grid_sizer->Add\(tray_title, 0, wxALIGN_CENTER\);/);
  assert.match(body, /right_grid_sizer->Add\(tray_title, 0, wxALIGN_CENTER\);/);
  assert.match(cpp, /#include "Widgets\/Button\.hpp"/);
});

test('the colour import dialog hands its Cancel button to the dark-mode pass as the window it is', async () => {
  const cpp = await read('ObjColorDialog.cpp');
  assert.doesNotMatch(cpp, /static_cast<wxButton\s*\*>/, 'the Cancel button is a kit Button, not a wxButton');
  assert.match(cpp, /update_ui\(this->FindWindowById\(wxID_CANCEL, this\)\);/);
});

test('the Sync AMS loading animation path outlives the call that loads it', async () => {
  const cpp = await read('SyncAmsInfoDialog.cpp');
  assert.doesNotMatch(cpp, /Slic3r::var\([^)]*\)\s*\.c_str\(\)/, 'a pointer into the temporary std::string is dangling once the statement ends');
  assert.match(cpp, /const std::string gif_path = Slic3r::var\("loading\.gif"\);/);
  assert.match(cpp, /m_gif_ctrl->LoadFile\(from_u8\(gif_path\)\)/);
});
