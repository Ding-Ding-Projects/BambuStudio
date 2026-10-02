import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Preview canvas draws several ImGui overlays that used to ignore each
// other: notifications lay under the legend dock with their text cut, the
// slicing card hid under it, the "Sliced" pill sat on the plate strip, the
// All Plates Stats tile ghosted its own label, the view-mode combo and the
// statistics card ended halfway across the dock, and the grouping card cut its
// last link off behind a scrollbar.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const read = async (...parts) => (await readFile(path.join(gui, ...parts), 'utf8')).replace(/\r\n/g, '\n');
const code = (text) => text.replace(/\/\*[\s\S]*?\*\//g, ' ').replace(/^[ \t]*\/\/.*$/gm, ' ');

const canvas = code(await read('GLCanvas3D.cpp'));
const canvasHeader = code(await read('GLCanvas3D.hpp'));
const base = code(await read('GCodeRenderer', 'BaseRenderer.cpp'));
const notifications = code(await read('NotificationManager.cpp'));
const slicing = code(await read('SlicingProgressNotification.cpp'));

const body = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `${signature} exists`);
  const next = source.indexOf('\nvoid ', start + signature.length);
  return source.slice(start, next === -1 ? undefined : next);
};

test('the notification column in Preview stops left of the expanded legend dock', async () => {
  assert.match(base, /float BaseRenderer::get_legend_dock_width\(\) const\s*\{\s*return m_legend_expanded \? m_legend_width : 0\.0f;\s*\}/,
    'the dock reports its width only while it is expanded');
  assert.match(code(await read('GCodeViewer.cpp')), /float GCodeViewer::get_legend_dock_width\(\) const[\s\S]*?p_renderer->get_legend_dock_width\(\) : 0\.0f;/);
  assert.match(canvas, /if \(m_canvas_type == ECanvasType::CanvasPreview\) \{\s*right_margin = SLIDER_RIGHT_MARGIN;\s*bottom_margin = SLIDER_BOTTOM_MARGIN;\s*if \(m_render_preview\) \{\s*const float dock_w = get_gcode_viewer\(\)\.get_legend_dock_width\(\);\s*if \(dock_w > 0\.0f\)\s*right_margin \+= dock_w \/ std::max\(get_scale\(\), 0\.5f\) \+ 16\.0f;/,
    'the canvas adds the dock to the right margin, only while the preview (and its dock) was drawn this frame');
  // An early return before render_legend() must not leave last frame's dock behind.
  for (const file of ['AdvancedRenderer.cpp', 'LegacyRenderer.cpp'])
    assert.match(code(await read('GCodeRenderer', file)), /m_legend_height = 0\.0f;\s*m_legend_width = 0\.0f;\s*m_legend_expanded = false;\s*if \(m_roles\.empty\(\)\)\s*return;/,
      `${file} clears the dock width with the legend height`);

  const toast = body(notifications, 'void NotificationManager::PopNotification::render(');
  assert.doesNotMatch(toast, /\(void\) right_margin/, 'a toast no longer throws the right margin away');
  assert.match(toast, /const bool beside_preview = right_margin > 16\.0f \* scale;/);
  assert.match(toast, /PreviewLayout::notification_column\(cnv_w, scale, 560\.0f \* scale, right_margin\)/,
    'the toast uses the production bounded-column helper');
  assert.match(toast, /m_window_width = column\.width;[\s\S]*?count_lines\(\);[\s\S]*?set_next_window_size\(imgui\);[\s\S]*?fit_to_stack\(initial_y\)/,
    'the final width determines text wrapping before stack placement');
  assert.match(toast, /ImVec2 win_pos\(cnv_w - right_gap,/, 'the toast is anchored at that gap');
  assert.match(toast, /if \(beside_preview && !ImGui::IsPopupOpen\("", ImGuiPopupFlags_AnyPopup\)\)\s*ImGui::BringWindowToDisplayFront\(ImGui::GetCurrentWindow\(\)\);\s*imgui\.end\(\);/,
    'in Preview a toast stays in front of the G-code window and, on a narrow canvas, of the dock, but never of an open popup');

  const banner = body(notifications, 'void NotificationManager::PopNotification::bbl_render_block_notification(');
  assert.match(banner, /PreviewLayout::notification_column\(float\(cnv_size\.get_width\(\)\)/,
    'error banners use the same canvas bounds');
  assert.match(slicing, /PreviewLayout::notification_column\(float\(cnv_size\.get_width\(\)\)/,
    'the slicing card uses the production bounded-column helper');
  assert.match(slicing, /const bool over_dock = column\.right < right_margin;/);
  assert.match(slicing, /if \(over_dock && !ImGui::IsPopupOpen\("", ImGuiPopupFlags_AnyPopup\)\)\s*ImGui::BringWindowToDisplayFront\(ImGui::GetCurrentWindow\(\)\);\s*imgui\.end\(\);/,
    'and is lifted in front when it cannot sit beside the dock');
});

test('overflow keeps live notifications accessible and preserves their timers', () => {
  assert.match(notifications, /notification->set_stack_bounds\(stack_bottom, stack_top\)/);
  assert.match(notifications, /wrapped_button\("###next_notifications", next\)/);
  assert.match(notifications, /wrapped_button\("###notification_history", history_label\)/);
  assert.match(notifications, /topbar\(\)->OnNotificationBell\(event\)/);
  assert.match(notifications, /notification->update_state\(hover \|\| notification->stack_deferred\(\), time_since_render\)/);
  assert.match(notifications, /m_deferred_timer\.set_deferred\(item\.deferred, canvas_timestamp_now\(\)\)/);
  assert.match(notifications, /const int64_t deferred_elapsed = m_deferred_timer\.consume\(now\)/);
  assert.match(notifications, /m_notification_start \+= deferred_elapsed/);
  assert.match(notifications, /m_fading_start \+= deferred_elapsed/);
  assert.doesNotMatch(notifications, /m_(notification_start|fading_start) \+= std::max<int64_t>\(0, delta\)/,
    'time since the last render must not be added again by each idle update');
  assert.match(notifications, /if \(m_overflow_rendered && point\.x >= m_overflow_min\.x/);
  // Numeric boundary cases call PreviewLayout.hpp directly in
  // tests/preview_layout/geometry_tests.cpp, not a JavaScript copy of its math.
});

test('the Sliced status stays beside the strip or in the dock header', () => {
  assert.match(canvasHeader, /float get_select_plate_toolbar_width\(\) const \{ return m_sel_plate_toolbar_width; \}/);
  assert.match(canvasHeader, /float m_sel_plate_toolbar_width\{ 0\.0f \};/);
  const strip = body(canvas, 'void GLCanvas3D::_render_imgui_select_plate_toolbar(');
  assert.match(strip, /if \(!m_sel_plate_toolbar\.is_enabled\(\)\) \{\s*m_sel_plate_toolbar_width = 0\.0f;/, 'no strip publishes no width');
  assert.match(strip, /m_sel_plate_toolbar_width = m_sel_plate_toolbar\.icon_width \+ margin_size \* 2 \+ 28\.0f \* f_scale;/,
    'the strip publishes its width, scrollbar variant included');
  assert.match(canvas, /m_sel_plate_toolbar\.set_enabled\(enable\);\s*if \(!enable\)\s*m_sel_plate_toolbar_width = 0\.0f;/);

  assert.match(base, /float pill_left = 16\.0f \* m_scale;[\s\S]*?const float strip_width = preview_canvas->get_select_plate_toolbar_width\(\);\s*if \(strip_width > 0\.0f\)\s*pill_left = strip_width \+ 12\.0f \* m_scale;/,
    'the pill starts 12 px right of the strip');
  assert.match(base, /if \(pill_left \+ pill_width \+ 8\.0f \* m_scale <= dock_left\) \{\s*imgui\.set_next_window_pos\(pill_left, 16\.0f \* m_scale,/,
    'and is drawn only when it ends before the dock');
  assert.match(base, /dock_status_text = pill_text;/);
  assert.match(base, /ImGui::TextUnformatted\(dock_status_text\.c_str\(\)\)/);
  assert.doesNotMatch(base, /forced_compact/, 'a narrow dock still permits expansion');
  assert.doesNotMatch(base, /imgui\.set_next_window_pos\(16\.0f \* m_scale, 16\.0f \* m_scale,/, 'never at the fixed corner over the strip');
});

test('the dock blocks span the dock and the grouping card is sized from its content', () => {
  assert.match(base, /const float dock_edge = ImGui::GetCurrentWindow\(\)->WindowBorderSize;\s*const float dock_x0   = ImGui::GetWindowPos\(\)\.x \+ ImGui::GetWindowContentRegionMin\(\)\.x \+ dock_edge;\s*const float dock_span = std::max\(1\.0f, ImGui::GetWindowContentRegionWidth\(\) - 2\.0f \* dock_edge\);/,
    'one span, inside the window border, for the full-width blocks');
  assert.match(base, /ImGui::SetCursorScreenPos\(ImVec2\(dock_x0, ImGui::GetCursorScreenPos\(\)\.y\)\);\s*ImGui::SetNextItemWidth\(dock_span \+ 2\.0f \* ImGui::GetFrameHeight\(\)\);\s*if \(ImGui::BBLBeginCombo\("##preview_view_overflow"/,
    'the view-mode combo gets the dock span, not the default 65% item width');
  assert.doesNotMatch(base, /pop_combo_style\(\);\s*ImGui::SameLine\(\);\s*ImGui::Dummy\(/, 'no spacer after the full-width combo: it would switch the horizontal scrollbar on');
  assert.match(base, /const ImVec2 stats_min\(dock_x0, stats_rect_min\.y - 4\.0f \* m_scale\);\s*const ImVec2 stats_max\(std::max\(dock_x0 \+ dock_span, stats_rect_max\.x \+ 6\.0f \* m_scale\),/,
    'the statistics card spans the dock instead of hugging its widest line');
  assert.doesNotMatch(base, /stats_rect_min\.x - 6\.0f \* m_scale/, 'its left outline no longer starts outside the clip rect');

  const cardStart = base.indexOf('void BaseRenderer::render_legend_color_arr_recommen(');
  const cardEnd = base.indexOf('void BaseRenderer::update_moves_slider(');
  assert.ok(cardStart !== -1 && cardEnd > cardStart, 'the grouping card function exists');
  const card = base.slice(cardStart, cardEnd);
  assert.match(card, /if \(m_ams_nozzle_box_content_height > 0\.0f\)\s*ams_item_height = std::max\(ams_item_height, m_ams_nozzle_box_content_height\);/);
  assert.match(card, /if \(m_ams_card_content_height > 0\.0f\)\s*AMS_container_height = m_ams_card_content_height;/,
    'the card height is the measured content height once it is known, not the fixed line count');
  assert.match(card, /const bool ams_card_open = ImGui::BeginChild\("#AMS", ImVec2\(0, AMS_container_height\)/);
  assert.match(card, /if \(ams_card_open\) \{\s*const ImGuiWindow\* card_win = ImGui::GetCurrentWindowRead\(\);\s*const float ams_card_needed = card_win->DC\.CursorMaxPos\.y - card_win->Pos\.y \+ card_win->Scroll\.y \+ window_padding \* 2\.0f;/,
    'nothing is measured while the card is clipped out and its items are skipped');
  assert.match(card, /if \(!box_open\)\s*return;\s*const ImGuiWindow\* box_win = ImGui::GetCurrentWindowRead\(\);\s*const float box_needed = box_win->DC\.CursorMaxPos\.y - box_win->Pos\.y \+ box_win->Scroll\.y \+ window_padding \* 2\.0f \+ box_win->ScrollbarSizes\.y;\s*nozzle_box_needed = std::max\(nozzle_box_needed, box_needed\);/,
    'a nozzle box is measured from CursorMaxPos, which a trailing SameLine does not move back');
  // ImGui decides a child's scrollbar from the previous frame's size, so the frame that first
  // applies the measured height can still draw one. It must ask for one more frame, or the
  // settled picture keeps the scrollbar the measurement was written to remove.
  assert.match(card, /\} else if \(card_win->ScrollbarY && ams_card_needed <= card_win->Size\.y\) \{\s*request_card_relayout\(\);\s*\}/,
    'a scrollbar left on a card whose content fits gets one more frame');
  assert.match(card, /if \(box_win->ScrollbarY && box_needed <= box_win->Size\.y\)\s*nozzle_box_stale_bar = true;/);
  assert.match(card, /\} else if \(nozzle_box_stale_bar\) \{\s*request_card_relayout\(\);\s*\}/);
  for (const side of ['left', 'right'])
    assert.match(card, new RegExp(`measure_nozzle_box\\(${side}_box_open\\);\\s*ImGui::EndChild\\(\\);`), `${side} box`);
  assert.doesNotMatch(card, /GetCursorPosY\(\)/, 'no measurement reads the cursor, which SameLine rewinds');
  assert.match(card, /PreviewLayout::inline_link_fits\(/,
    'the help link only shares a row when both labels fit');
  assert.match(card, /ImGui::Button\("###grouping_help"/,
    'the wrapped help link retains keyboard activation');

});

test('the grouping card arithmetic: the fixed line count was short once the sentence wrapped', () => {
  // 100% scale: frame height 21, item spacing 4, text 15, window padding 4.
  const line = 21, gap = 4, text = 15, pad = 4, boxes = 89;
  const guessed = boxes + line * 5 + line / 2; // tips_count 5, the only height the card had
  const content = (sentenceLines) => 6 * pad + text + 1 + boxes + sentenceLines * text + text + 10 * gap;
  assert.ok(content(1) <= guessed, 'a one-line sentence fitted the guess');
  assert.ok(content(2) > guessed, 'the two lines the 344 px dock wraps it to did not, which cut the last link off');
  const measured = (sentenceLines) => content(sentenceLines) + 2 * pad; // CursorMaxPos + 2 * window_padding
  for (const n of [1, 2, 3, 5])
    assert.ok(measured(n) >= content(n), `the measured height holds ${n} sentence lines without a scrollbar`);
});

test('the All Plates Stats tile draws its glyph and label above the wash in opaque roles', async () => {
  const strip = body(canvas, 'void GLCanvas3D::_render_imgui_select_plate_toolbar(');
  assert.doesNotMatch(strip, /MD3::Role::Primary,\s*0\.2f/, 'no label at 20% alpha');
  assert.match(strip, /const ImVec4 text_clr  = stats_sliced \? md3_imvec4\(MD3::Role::Primary\) : md3_imvec4\(MD3::Role::OnSurface\);/);
  assert.match(strip, /const ImVec4 btn_tint = ImVec4\(1\.0f, 1\.0f, 1\.0f, 0\.0f\);/, 'no glyph is painted under the wash, in any state');
  assert.match(strip, /btn_texture_id = \(ImTextureID\)\(intptr_t\)\(all_plates_stats_item->image_texture_transparent\.get_id\(\)\);/,
    'every state tints the one white glyph, so the sliced glyph matches its Primary label');
  assert.match(strip, /if \(stats_busy\)\s*glyph_clr\.w \*= 0\.5f;/, 'the glyph still dims while the tile is disabled');
  assert.match(strip, /md3_scrim_imu32\(0\.125f\)\);\s*\}\s*ImGui::GetWindowDrawList\(\)->AddImage\(btn_texture_id, start_pos,[\s\S]*?ImGui::GetColorU32\(glyph_clr\)\);/,
    'the glyph is redrawn right after the wash, for every state');
  const svg = await readFile(path.join(repoDir, 'resources', 'images', 'im_all_plates_stats_transparent.svg'), 'utf8');
  assert.doesNotMatch(svg, /#C8EBD5/i, 'the glyph is white, so the tint gives it its role colour');
  assert.equal((svg.match(/fill="#FFFFFF"/g) ?? []).length, 5);

  // Contrast of the label and glyph on the darkest wash band (the unsliced part while slicing).
  const tokens = await read('Widgets', 'MD3Tokens.hpp');
  const luminance = ([r, g, b]) => {
    const [x, y, z] = [r, g, b].map((c) => c / 255).map((c) => (c <= 0.03928 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4));
    return 0.2126 * x + 0.7152 * y + 0.0722 * z;
  };
  const contrast = (a, b) => {
    const [hi, lo] = [luminance(a), luminance(b)].sort((p, q) => q - p);
    return (hi + 0.05) / (lo + 0.05);
  };
  const rgb = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
  for (const theme of ['Light', 'Dark']) {
    const block = tokens.slice(tokens.indexOf(`namespace ${theme} {`), tokens.indexOf(`} // namespace ${theme}`));
    const hex = (name) => block.match(new RegExp(`inline const wxColour ${name}\\{"(#[0-9a-fA-F]{6})"\\};`))[1];
    const scrim = Number(block.match(/inline const wxColour scrim\{0, 0, 0, (\d+)\};/)[1]);
    const dim = (colour, alpha) => colour.map((c) => c * (1 - alpha / 255));
    const band = dim(dim(rgb(hex('scLowest')), Math.round(scrim * 0.125)), scrim);
    assert.ok(contrast(rgb(hex('onSurface')), band) >= 4.5, `${theme}: the label reads on the darkest band`);
    assert.ok(contrast(rgb(hex('onSurfaceVariant')), band) >= 3.0, `${theme}: the glyph reads on the darkest band`);
  }
});
