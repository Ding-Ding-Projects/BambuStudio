import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Plate Settings sidebar page: at the default 344 DIP sidebar the row beside a
// 14 em label left the plate type dropdown about 17 em, and "Smooth PEI Plate / High
// Temp Plate" (about 21 em) and "Bambu Cool Plate SuperTack" (about 26.5 em) ended in
// an ellipsis (clipping inventory CJ-036). A group may now stack its single row-wide
// option rows: the label takes a line of its own and the field the next line, across
// the row. These checks pin the pieces that layout needs to agree on.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const read = async (...parts) => code(await readFile(path.join(guiDir, ...parts), 'utf8'));
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, signature);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

test('an options group can opt its row-wide option rows into the stacked layout', async () => {
  assert.match(await read('OptionsGroup.hpp'), /^\s*bool\s+stack_full_width_label\s*\{\s*false\s*\};/m,
    'the flag is off unless a page sets it');
  const header = await read('OG_CustomCtrl.hpp');
  assert.match(header, /^\s*wxCoord\s+label_band\s*\{\s*0\s*\};/m, 'a line remembers the height of its label band');
  assert.match(header, /^\s*bool\s+is_stacked\(\) const;/m);
  assert.match(header, /^\s*void\s+update_stacked_height\(\);/m);
  assert.match(header, /^\s*int\s+get_label_band\(const Line& line\);/m);
});

test('a stacked line is exactly a labelled single row-wide option with nothing beside it', async () => {
  const panel = await read('OG_CustomCtrl.cpp');
  const stacked = fn(panel, 'bool OG_CustomCtrl::CtrlLine::is_stacked() const');
  for (const needle of [
    /!ctrl->opt_group->stack_full_width_label/,
    /ctrl->opt_group->label_width == 0/,
    /draw_just_act_buttons/,
    /og_line\.label\.IsEmpty\(\)/,
    /og_line\.widget != nullptr/,
    /option_set\.size\(\) == 1/,
    /option_set\.front\(\)\.opt\.full_width/,
    /option_set\.front\(\)\.opt\.sidetext\.empty\(\)/,
    /option_set\.front\(\)\.side_widget == nullptr/,
    /og_line\.get_extra_widgets\(\)\.empty\(\)/,
  ])
    assert.match(stacked, needle);
});

test('the stacked height is the label band plus the field, measured again once the field exists', async () => {
  const panel = await read('OG_CustomCtrl.cpp');
  const update = fn(panel, 'void OG_CustomCtrl::CtrlLine::update_stacked_height()');
  assert.match(update, /^\s*label_band = ctrl->GetTextExtent\(og_line\.label\)\.y \+ ctrl->m_v_gap;/m);
  assert.match(update, /field->getWindow\(\)->GetSize\(\)\.GetHeight\(\)/, 'the real field height once it is built');
  assert.match(update, /^\s*height = label_band \+ field_h \+ ctrl->m_v_gap;/m);
  // init_ctrl_lines runs before any field exists, so the first height is a fallback...
  const init = fn(panel, 'void OG_CustomCtrl::init_ctrl_lines()');
  assert.match(init, /ctrl_lines\.emplace_back\(CtrlLine\(height, this, line, false, opt_group->staticbox\)\);\s*if \(ctrl_lines\.back\(\)\.is_stacked\(\)\)\s*ctrl_lines\.back\(\)\.update_stacked_height\(\);/);
  // ...corrected where the fields are known to exist, and on a rescale.
  const visibility = fn(panel, 'void OG_CustomCtrl::CtrlLine::update_visibility(ConfigOptionMode mode)');
  assert.match(visibility, /if \(draw_just_act_buttons\)\s*return;\s*if \(is_stacked\(\)\)\s*update_stacked_height\(\);/);
  const rescale = fn(panel, 'void OG_CustomCtrl::CtrlLine::msw_rescale()');
  assert.match(rescale, /if \(is_stacked\(\)\) \{\s*update_stacked_height\(\);\s*correct_items_positions\(\);\s*return;\s*\}/);
});

test('the field of a stacked line starts at the row start, on the line under the label', async () => {
  const panel = await read('OG_CustomCtrl.cpp');
  const pos = fn(panel, 'wxPoint OG_CustomCtrl::get_pos(const Line& line, Field* field_in');
  assert.match(pos, /if \(opt_group->label_width != 0 && !ctrl_line\.is_stacked\(\)\)\s*add_label_width\(ctrl_line, label, opt_group->label_width \* m_em_unit\);/,
    'no label column is reserved beside a stacked field');
  const band = fn(panel, 'int OG_CustomCtrl::get_label_band(const Line& line)');
  assert.match(band, /return ctrl_line\.label_band;/);
  const place = fn(panel, 'void OG_CustomCtrl::correct_window_position(wxWindow* win, const Line& line, Field* field');
  assert.match(place, /const int label_band = get_label_band\(line\);\s*pos\.y \+= label_band;\s*line_height -= label_band;\s*pos\.y \+= std::max\(0, int\(0\.5 \* \(line_height - win->GetSize\(\)\.y\)\)\);/,
    'the window is centred in the band under the label');
});

test('a stacked line paints its label across the row and its buttons and field in the band below', async () => {
  const panel = await read('OG_CustomCtrl.cpp');
  const render = fn(panel, 'void OG_CustomCtrl::CtrlLine::render(wxDC& dc, wxCoord h_pos, wxCoord v_pos)');
  assert.match(render, /^\s*const wxCoord row_start = h_pos;/m);
  assert.match(render, /if \(label_band > 0\) \{\s*const int label_room = std::max\(ctrl->GetSize\(\)\.x - ctrl->m_em_unit \* 3 - \(h_pos \+ indent\), int\(ctrl->opt_group->label_width \* ctrl->m_em_unit\)\);\s*draw_text\(dc, wxPoint\(h_pos \+ indent, v_pos\), label, text_clr, label_room, is_url_string, true\);\s*h_pos = row_start;\s*v_pos \+= label_band;\s*\} else \{/,
    'the label gets the row less the margin the row-wide field keeps, then the row continues below it');
  // The row-wide field keeps its old minimum and its old sizing line.
  assert.match(render, /const int row_width = ctrl->GetSize\(\)\.x - h_pos2 \+ h_pos3 - h_pos - ctrl->m_em_unit \* 3;\s*field->getWindow\(\)->SetSize\(std::max\(row_width, Field::def_width_wider\(\) \* ctrl->m_em_unit\), -1\);/);
  // Text and buttons centre in the band they sit in, which is the whole row when nothing is stacked.
  const text = fn(panel, 'wxCoord OG_CustomCtrl::CtrlLine::draw_text(');
  assert.match(text, /\} else if \(is_main && label_band > 0\) \{\s*pos\.y = pos\.y \+ lround\(\(label_band - size\.y\) \/ 2\);\s*\} else \{\s*pos\.y = pos\.y \+ lround\(\(height - label_band - size\.y\) \/ 2\);/);
  const blink = fn(panel, 'wxPoint OG_CustomCtrl::CtrlLine::draw_blinking_bmp(wxDC& dc, wxPoint pos, bool is_blinking)');
  assert.match(blink, /lround\(\(height - label_band - get_bitmap_size\(bmp_blinking\)\.GetHeight\(\)\) \/ 2\)/);
  const act = fn(panel, 'wxCoord OG_CustomCtrl::CtrlLine::draw_act_bmps(');
  assert.match(act, /\} else \{\s*pos\.y \+= lround\(\(height - label_band - get_bitmap_size\(bmp_undo\)\.GetHeight\(\)\) \/ 2\);/);
});

test('the Plate Settings page stacks its dropdown rows', async () => {
  const build = fn(await read('Tab.cpp'), 'void TabPrintPlate::build()');
  assert.match(build, /auto optgroup = page->new_optgroup\("", wxEmptyString, 14\);\s*optgroup->stack_full_width_label = true;\s*auto append_select = /,
    'the flag is set on the group before the first dropdown row is added');
});

test('the stacked arithmetic leaves the longest plate name whole at the default sidebar', () => {
  // 100 % scale: 10 px em, 19 px text, a 32 px kit dropdown, 12 px row gap.
  const em = 10, text = 19, field = 32, v_gap = Math.round(1.2 * em);
  const label_band = text + v_gap;
  const height = label_band + field + v_gap;
  assert.equal(height, 75);
  const label_y = Math.round((label_band - text) / 2);
  const field_y = label_band + Math.max(0, Math.round(0.5 * (height - label_band - field)));
  assert.ok(label_y + text <= label_band, 'the label stays inside its band');
  assert.ok(field_y >= label_band && field_y + field <= height, 'the field stays inside the band under it');
  // Beside the 14 em label column (and its 0.2 em gap) the row left the dropdown about 17 em at a
  // 344 DIP sidebar; stacked, the dropdown gets that column back. Its chevron takes about 2 em.
  const beside_label_em = 17, label_column_em = 14 + 0.2, chevron_em = 2;
  const field_em = beside_label_em + label_column_em;
  assert.ok(field_em - chevron_em >= 26.5, `"Bambu Cool Plate SuperTack" needs 26.5 em, the face offers ${field_em - chevron_em}`);
  assert.ok(field_em >= 12, 'never below the 12 em minimum of a row-wide field');
});
