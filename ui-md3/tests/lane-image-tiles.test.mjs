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
  'TextureImportDialog.cpp',
  'TextureImportDialog.hpp',
  'SyncAmsDialog.cpp',
  'SyncAmsDialog.hpp',
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

// The colour import thumbnail wells and swatches (ObjColorDialog) and the Sync AMS compare-panel
// thumbnail tiles (SyncAmsInfoDialog) have no equivalent surface any more: upstream removed
// ObjColorDialog in be763c901 (standard 3MF colour import goes through TextureImportDialog) and
// the old Sync AMS compare panel in 42d6cea63 (SyncAmsDialog uses ModelPreview cards instead).

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

test('the texture import dialog Cancel is a kit Button the dark-mode pass sees as a window', async () => {
  const cpp = await read('TextureImportDialog.cpp');
  assert.doesNotMatch(cpp, /static_cast<wxButton\s*\*>/, 'the Cancel button is a kit Button, not a wxButton');
  assert.match(cpp, /auto\*\s*cancel_btn\s*=\s*new Button\(this, _L\("Cancel"\), "", 0, 0, wxID_CANCEL\);/);
  assert.match(cpp, /cancel_btn->SetVariant\(Button::Variant::Outlined\);/);
  assert.match(cpp, /wxGetApp\(\)\.UpdateDlgDarkUI\(this\);/);
});

test('the Sync AMS dialog is a kit caption shell with kit footer variants and no dangling resource path', async () => {
  const cpp = await read('SyncAmsDialog.cpp');
  // The old loading.gif animation (SyncAmsInfoDialog, removed in 42d6cea63) is gone; the lifetime
  // rule it protected still applies to any resource path taken in this file.
  assert.doesNotMatch(cpp, /Slic3r::var\([^)]*\)\s*\.c_str\(\)/, 'a pointer into the temporary std::string is dangling once the statement ends');
  assert.match(cpp, /MD3DialogCaption::Adopt\(this, _L\("Nozzle and Filament Sync"\)\);/);
  assert.match(cpp, /ok->SetVariant\(::Button::Variant::Filled\);/);
  assert.match(cpp, /cancel->SetVariant\(::Button::Variant::Outlined\);/);
  assert.match(cpp, /wxGetApp\(\)\.UpdateDlgDarkUI\(this\);/);
  assert.doesNotMatch(cpp, /new\s+wxButton\s*\(/);
});
