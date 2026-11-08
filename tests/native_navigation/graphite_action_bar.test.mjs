import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Graphite shell: the Prepare plate, estimate and Slice/Print controls float on
// the window ground under the canvas, with no band colour and no hairline.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const frame = fs.readFileSync(path.join(root, 'src/slic3r/GUI/MainFrame.cpp'), 'utf8').replaceAll('\r\n', '\n');
const style = frame.slice(frame.indexOf('void MainFrame::update_prepare_action_bar_style()'),
                          frame.indexOf('void MainFrame::init_tabpanel()'));
// The ground is named once, by prepare_bar_surface(), so the bar, its Slice and
// Print pill corners and the add-plate button cannot drift apart again.
const helperStart = frame.indexOf('wxColour prepare_bar_surface()');
const helper = helperStart < 0 ? '' : frame.slice(helperStart, frame.indexOf('}', helperStart) + 1);

test('the action bar paints from the window ground', () => {
    assert(style.length > 0);
    assert.match(helper, /return StateColor::semantic\(MD3::Role::Surface\);/);
    assert.match(style, /const wxColour bar_bg = prepare_bar_surface\(\);/);
    assert.doesNotMatch(style, /bar_bg = StateColor::semantic\(MD3::Role::SurfaceContainerLow\)/);
    assert.doesNotMatch(helper, /SurfaceContainerLow/);
});

test('the hairline above the action bar is hidden', () => {
    assert.match(style, /m_prepare_action_bar_divider->Hide\(\);/);
});

test('the add-plate button rests on the ground colour', () => {
    assert.match(style, /SurfaceContainerHigh\), \(int\) StateColor::Hovered\},\n\s*std::pair\{bar_bg, \(int\) StateColor::Normal\}\);\n\s*m_prepare_add_plate_button/);
});
