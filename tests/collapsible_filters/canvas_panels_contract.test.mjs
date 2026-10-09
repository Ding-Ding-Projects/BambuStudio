// Source contract for the collapsible panels drawn on the 3D canvas: the
// preview legend and statistics dock and the assembly structure panel keep
// their collapsed state, have a keyboard path with a visible focus ring, and
// announce their expanded state through the canvas accessibility bridge.
// Set COLLAPSE_SOURCE_REF to a git revision to check that revision instead.
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
import test from 'node:test';

const read = (path) => (process.env.COLLAPSE_SOURCE_REF
  ? execFileSync('git', ['show', `${process.env.COLLAPSE_SOURCE_REF}:${path}`], { encoding: 'utf8' })
  : readFileSync(new URL(`../../${path}`, import.meta.url), 'utf8')).replace(/\r\n/g, '\n');
const optional = (path) => { try { return read(path); } catch { return ''; } };

const renderer = read('src/slic3r/GUI/GCodeRenderer/BaseRenderer.cpp');
const rendererHeader = read('src/slic3r/GUI/GCodeRenderer/BaseRenderer.hpp');
const assembly = read('src/slic3r/GUI/Overview/AssemblyStepsUtilsImgui.cpp');
const assemblyHeader = read('src/slic3r/GUI/Overview/AssemblyStepsUtils.hpp');
const canvas = read('src/slic3r/GUI/GLCanvas3D.cpp');
const shortcuts = read('src/slic3r/GUI/KBShortcutsDialog.cpp');
const bridge = optional('src/slic3r/GUI/Widgets/CanvasDisclosures.cpp');
const preview = read('src/slic3r/GUI/GUI_Preview.cpp');

test('the legend dock fold is stored instead of living in memory only', () => {
  assert.doesNotMatch(rendererHeader, /bool m_fold\{/);
  assert.match(rendererHeader, /CollapsibleFilters::CanvasDisclosure m_legend_fold\{ "preview_legend"/);
  assert.match(renderer, /m_legend_fold\.restore\(CanvasDisclosures::config_reader\(\)\)/);
  assert.match(renderer, /toggle_from_keyboard\(CanvasDisclosures::config_writer\(\)\)/);
  assert.match(renderer, /toggle_from_pointer\(CanvasDisclosures::config_writer\(\)\)/);
  // The existing "keep the last fold state" preference still decides.
  assert.match(renderer, /use_last_fold_state_gcodeview_option_panel[\s\S]{0,80}m_legend_fold\.reset_to_default\(\)/);
});

test('the assembly structure panel fold is stored', () => {
  assert.doesNotMatch(assemblyHeader, /bool\s+m_structure_panel_collapsed/);
  assert.match(assemblyHeader, /CollapsibleFilters::CanvasDisclosure m_structure_fold\{"assembly_structure"/);
  assert.match(assembly, /m_structure_fold\.restore\(CanvasDisclosures::config_reader\(\)\)/);
  assert.match(assembly, /toggle_structure_panel\(\/\*from_keyboard=\*\/false\)/);
});

test('Shift+L is the keyboard path on both canvases and is listed', () => {
  const lcase = canvas.slice(canvas.indexOf("case 'L':\n        case 'l': {"));
  assert.ok(lcase.length > 0, 'the canvas handles L');
  const body = lcase.slice(0, 1600);
  assert.match(body, /\(evt\.GetModifiers\(\) & shiftMask\) == 0/);
  assert.match(body, /toggle_legend_fold\(true\)/);
  assert.match(body, /m_assembly_steps->toggle_structure_panel\(true\)/);
  assert.match(canvas, /if \(evt\.ButtonDown\(\)\) \{[\s\S]{0,300}on_canvas_pointer_used\(\)/);
  assert.match(shortcuts, /\{ L\("Shift\+L"\), L\("Collapse or expand the legend and statistics panel"\)\}/);
  // The preview one-layer handler (plain L) must let Shift+L through to the canvas.
  const slider = preview.slice(preview.indexOf('void Preview::update_layers_slider_from_canvas'));
  assert.match(slider.slice(0, 600), /if \(event\.HasModifiers\(\) \|\| event\.ShiftDown\(\)\) \{\s*event\.Skip\(\);\s*return;/);
});

test('a keyboard toggle draws a focus ring on the toggle', () => {
  assert.match(renderer, /if \(m_legend_fold\.focus_ring_visible\(\)\) \{[\s\S]{0,400}AddRect\(/);
  assert.match(assembly, /if \(m_structure_fold\.focus_ring_visible\(\)\) \{[\s\S]{0,400}AddRect\(/);
});

test('the canvas exposes each panel header with its expanded state and announces toggles', () => {
  assert.match(bridge, /class CanvasAccessible final : public wxWindowAccessible/);
  assert.match(bridge, /wxROLE_SYSTEM_PUSHBUTTON/);
  assert.match(bridge, /wxACC_STATE_SYSTEM_EXPANDED : wxACC_STATE_SYSTEM_COLLAPSED/);
  assert.match(bridge, /wxACC_EVENT_OBJECT_FOCUS/);
  assert.match(bridge, /wxACC_EVENT_OBJECT_STATECHANGE/);
  assert.match(bridge, /GetFocus\(int \*child_id, wxAccessible \*\*child\)/);
  assert.match(bridge, /DoDefaultAction\(int child_id\)/);
  assert.match(renderer, /CanvasDisclosures::publish\(window, "preview_legend"/);
  assert.match(renderer, /CanvasDisclosures::announce\(m_legend_fold_canvas, "preview_legend"/);
  assert.match(assembly, /CanvasDisclosures::publish\(window, "assembly_structure"/);
  assert.match(assembly, /CanvasDisclosures::announce\(m_structure_fold_canvas, "assembly_structure"/);
});
