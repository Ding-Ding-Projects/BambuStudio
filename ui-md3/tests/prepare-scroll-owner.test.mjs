import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
const root = new URL('../../src/slic3r/GUI/', import.meta.url);
const read = file => readFile(new URL(file, root), 'utf8');
const scroller = await read('Widgets/MD3ScrolledWindow.cpp');
const params = await read('ParamsPanel.cpp');
const plater = await read('Plater.cpp');
test('embedded settings delegate reveal and disable both inner axes', () => {
    assert.match(plater, /params_panel->set_scroll_reveal_owner/);
    assert.match(scroller, /SetScrollRate\(0, 0\)/);
    assert.match(scroller, /EnableScrolling\(false, false\)/);
    assert.match(scroller, /m_reveal_owner->RevealChild\(child\)/);
    assert.match(params, /RevealChild\(win\)/);
});
test('standalone preset pages keep a bounded viewport', () => {
    assert.match(params, /if \(!m_host_height_changed\) \{[\s\S]*?FitInside\(\);[\s\S]*?return;/);
});
test('body layout retains its scroll origin and settles reserved strips', () => {
    assert.match(plater, /wxPoint\(-anchor.x \* unit_x, -anchor.y \* unit_y\)/);
    assert.match(plater, /settled_client = sw->GetClientSize\(\)/);
    assert.match(plater, /updating.insert\(sw\).second/);
});
// Execute the actual scalar algorithm after mechanical C++ syntax conversion.
// This verifies arithmetic, not wx event delivery or rendered geometry.
const body = scroller.match(/auto reveal = \[\]\([^]*?\) \{([^]*?)\n    \};/)[1]
    .replace(/\/\/[^\n]*/g, '')
    .replace(/(?:const )?int /g, 'let ')
    .replace(/std::max/g, 'Math.max')
    .replace(/return \(pixels \+ \(delta > 0 \? unit - 1 : 0\)\) \/ unit;/, 'return Math.trunc((pixels + (delta > 0 ? unit - 1 : 0)) / unit);');
const reveal = new Function('position', 'extent', 'visible', 'unit', 'start', body);
test('reveal reaches final rows, oversized targets, and leading edges', () => {
    assert.equal(reveal(510, 20, 500, 8, 0), 4);
    assert.equal(reveal(-24, 20, 500, 8, 10), 7);
    assert.equal(reveal(50, 600, 500, 8, 0), 7);
    assert.equal(reveal(20, 30, 500, 8, 12), 12);
    assert.equal(reveal(510, 20, 500, 0, 12), 12);
});

test('embedded wheel delivery preserves the event and cannot retain a destroyed owner', async () => {
    const header = await read('Widgets/MD3ScrolledWindow.hpp');
    assert.match(header, /wxWeakRef<MD3ScrolledWindow> m_reveal_owner/);
    assert.match(scroller, /Bind\(wxEVT_MOUSEWHEEL, &MD3ScrolledWindow::OnMouseWheel, this\)/);
    assert.match(scroller, /wxMouseEvent forwarded\(event\)/);
    assert.match(scroller, /ProcessEvent\(forwarded\);\s*return;/);
    assert.match(scroller, /owner == this \|\| \(owner && !owner->IsDescendant\(this\)\)/);
});
test('section switching settles extents before resetting and repainting', () => {
    const section = plater.split('void Sidebar::apply_prepare_section(')[1].split('\n}')[0];
    assert.match(section, /update_scroll_body\(\);\s*p->scrolled->Scroll\(0, 0\);\s*p->scrolled->Refresh\(\);/);
});

test('only focused embedded background forwards plain navigation keys', () => {
    // wxScrollHelperBase only offers DisableKeyboardScrolling() (wx 3.2 and the
    // bundled fork); EnableKeyboardScrolling() does not exist and broke MSVC.
    const delegate = scroller.split('void MD3ScrolledWindow::SetRevealOwner(')[1].split('\n}')[0];
    assert.match(delegate, /if \(owner\) \{[\s\S]*?DisableKeyboardScrolling\(\);/);
    assert.doesNotMatch(scroller, /EnableKeyboardScrolling/);
    const handler = scroller.split('void MD3ScrolledWindow::OnChar(')[1].split('void MD3ScrolledWindow::SetRevealOwner')[0];
    assert.match(handler, /wxWindow::FindFocus\(\) == this && !event.HasAnyModifiers\(\)/);
    assert.match(handler, /case WXK_PAGEUP: case WXK_PAGEDOWN: case WXK_HOME: case WXK_END:/);
    assert.match(handler, /case WXK_UP: case WXK_DOWN: case WXK_LEFT: case WXK_RIGHT:/);
    assert.match(handler, /wxKeyEvent forwarded\(event\)/);
    assert.match(handler, /default: break;/);
    assert.match(handler, /event.Skip\(\)/);
});
test('embedded headers remeasure their natural height without constraining standalone pages', () => {
    const fit = params.split('void ParamsPanel::fit_page_to_content()')[1].split('void ParamsPanel::set_active_tab')[0];
    const standaloneReturn = fit.indexOf('return;', fit.indexOf('!m_host_height_changed'));
    const measuredHeader = fit.indexOf('const int header_height = m_current_tab->GetSizer()->GetMinSize().y');
    assert.ok(standaloneReturn >= 0 && measuredHeader > standaloneReturn);
    assert.match(fit, /SetMinSize\(wxSize\(-1, header_height\)\)/);
    assert.match(fit, /SetMaxSize\(wxSize\(-1, header_height\)\)/);
});
