import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Graphite shell: the workspaces move from the top strip into a left
// navigation rail, and the neutral ramp becomes a green-tinted graphite.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const frame = read('src/slic3r/GUI/MainFrame.cpp');
const header = read('src/slic3r/GUI/Notebook.hpp');
const source = read('src/slic3r/GUI/Notebook.cpp');
const tokens = read('src/slic3r/GUI/Widgets/MD3Tokens.hpp');

const between = (text, start, end) => {
    const a = text.indexOf(start);
    assert.notEqual(a, -1, start);
    const b = text.indexOf(end, a + start.length);
    assert.notEqual(b, -1, end);
    return text.slice(a, b);
};

test('the main workspace book docks left as a rail', () => {
    assert.match(frame, /m_tabpanel = new Notebook\([^;]*wxNB_LEFT[^;]*\);/);
    assert.doesNotMatch(frame, /m_tabpanel = new Notebook\([^;]*wxNB_TOP[^;]*\);/);
});

test('the notebook keeps an explicit dock and only defaults to the top strip', () => {
    assert(header.includes('const long dock = style & wxBK_ALIGN_MASK;'));
    assert(header.includes('dock ? style : (style | wxBK_TOP)'));
    assert(header.includes('new ButtonsListCtrl(this, side_tools, (style & wxBK_LEFT) != 0)'));
});

test('rail geometry comes from tokens and reuses the overflow fit along the height', () => {
    for (const name of ['navigation_rail_width', 'navigation_rail_item_width', 'navigation_rail_item_height', 'radius_panel'])
        assert.match(tokens, new RegExp(`inline constexpr int ${name}\\s*=\\s*\\d+;`), name);
    const rail = between(source, 'void ButtonsListCtrl::LayoutRail()', 'void ButtonsListCtrl::LayoutNavigation()');
    assert(rail.includes('NotebookNavigation::fit(items, available'));
    assert(rail.includes('GetMinSize().y'));
    assert(rail.includes('m_actionButtons.rbegin()'), 'actions are pinned to the bottom');
    const style = between(source, 'void ButtonsListCtrl::StyleButton(', 'void ButtonsListCtrl::ApplyTheme()');
    assert(style.includes('button->SetVertical(true);'));
    assert(style.includes('MD3::Role::SecondaryContainer'));
});

test('the top strip path is unchanged for every other notebook', () => {
    const layout = between(source, 'void ButtonsListCtrl::LayoutNavigation()', 'void ButtonsListCtrl::ShowOverflow()');
    assert(layout.includes('if (m_rail) {'));
    assert(layout.includes('m_side_tools->CalcMin()'));
});

test('dark and light grounds are the Graphite neutrals', () => {
    const dark = between(tokens, 'namespace Dark {', '} // namespace Dark');
    const light = between(tokens, 'namespace Light {', '} // namespace Light');
    assert(dark.includes('surface{"#0e1311"}'));
    assert(dark.includes('scLowest{"#0a0e0c"}'));
    assert(dark.includes('primary{"#6fdc98"}'));
    assert(light.includes('surface{"#f5f8f5"}'));
    assert(light.includes('primary{"#146c2e"}'));
});
