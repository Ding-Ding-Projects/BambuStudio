import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import test from 'node:test';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const strip = read('src/slic3r/GUI/Widgets/TabStrip.cpp');
const dialogs = read('src/slic3r/GUI/Widgets/TabStripDialogs.cpp');
const book = read('src/slic3r/GUI/Tabbook.cpp');
const title = read('src/slic3r/GUI/BBLTopbar.cpp');
const frame = read('src/slic3r/GUI/MainFrame.cpp');
const digest = value => createHash('sha256').update(value).digest('hex');
function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g,
        text => ' '.repeat(text.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const extract = process.argv.indexOf('--extract');
if (extract >= 0) {
    const destination = path.resolve(process.argv[extract + 1]);
    fs.mkdirSync(destination, {recursive: true});
    const overflow = body(strip, 'struct AtlasTabOverflow') + ';\n' + body(strip, 'AtlasTabOverflow atlasTabOverflow(');
    const geometry = overflow + '\n' + body(title, 'static int atlasTitleTextBudget(');
    fs.writeFileSync(path.join(destination, 'atlas_tab_overflow.inc'), overflow);
    fs.writeFileSync(path.join(destination, 'atlas_shell_layout.inc'), geometry);
    const drag = body(strip, 'struct AtlasDragTab') + ';\n' + body(strip, 'int atlasTabDragTarget(');
    fs.writeFileSync(path.join(destination, 'atlas_tab_drag.inc'), drag);
    const model = read('src/slic3r/GUI/Widgets/TabStripModel.hpp');
    const operations = ['int add(Tab tab)', 'int index_of(', 'int pinned_count()', 'bool move(int from, int to)']
        .map(name => body(model, name)).join('\n');
    fs.writeFileSync(path.join(destination, 'atlas_tab_model_operations.inc'), operations);
    const focus = strip.match(/constexpr int atlasOverflowFocus = -2;/)[0] + '\n'
        + body(strip, 'std::vector<int> atlasFocusTargets(') + '\n'
        + body(strip, 'enum class AtlasFocusAction') + ';\n'
        + body(strip, 'AtlasFocusAction atlasFocusActivation(') + '\n'
        + body(strip, 'int atlasReconcileFocus(') + '\n' + body(strip, 'int atlasStepFocus(');
    fs.writeFileSync(path.join(destination, 'atlas_tab_focus.inc'), focus);
    console.log(`Production geometry SHA-256: ${digest(geometry)}`);
    console.log(`Production drag SHA-256: ${digest(drag)}; model operations SHA-256: ${digest(operations)}`);
    console.log(`Production focus SHA-256: ${digest(focus)}`);
}

test('horizontal tab measurement includes complete bilingual names and independently scaled markers', () => {
    const measure = body(strip, 'int TabStripButton::PreferredExtent(');
    assert(measure.includes('std::numeric_limits<int>::max()'));
    assert(!measure.includes('kTabMaxWidth'));
    assert(measure.includes('2 * FromDIP(3) + FromDIP(chip_gap)'));
    assert(measure.includes('FromDIP(chip_size) + FromDIP(chip_gap)'));
    assert(measure.includes('FromDIP(dot_size) + FromDIP(content_gap)'));
    assert(measure.includes('m_close->IsShown()'));
    assert(measure.includes('wxFONTWEIGHT_SEMIBOLD'));
});

function checkOverflow(source) {
    const layout = body(source, 'void TabStrip::Relayout(');
    assert(layout.includes('atlasTabOverflow('));
    assert(layout.includes('action_minimum'));
    assert(layout.includes('button->PreferredExtent(true)'));
    assert(layout.includes('active_hidden ? Button::Variant::Tonal'));
    const menu = body(source, 'void TabStrip::OpenOverflowMenu(');
    assert(menu.includes('menu.AppendCheckItem(id, label)'));
    assert(menu.includes('menu.Check(id, m_model.active_index() == m_model.index_of(t.id))'));
    assert(menu.includes('targets.push_back(t.id)'));
    assert(menu.includes('RequestActivate(targets[sel - ID_RESTORE_FIRST])'));
}
test('active overflow retains checked stable identity and reachable existing actions', () => checkOverflow(strip));
test('overflow guard rejects losing active identity', () => {
    assert.throws(() => checkOverflow(strip.replace('menu.Check(id, m_model.active_index() == m_model.index_of(t.id))', 'menu.Check(id, false)')));
});
function checkDragProjection(source) {
    const drag = body(source, 'void TabStrip::OnTabDragEnd(');
    assert(drag.includes('!m_buttons[index]->IsShown()'));
    assert(drag.includes('visible.push_back({m_model.at(index).id,'));
    assert(drag.includes('atlasTabDragTarget(id, from, visible,'));
    assert(drag.includes('m_model.index_of(neighbor_id)'));
    assert(drag.includes('MoveTab(from, target)'));
    assert(!drag.includes('disp[slot]'));
    assert(!drag.includes('m_model.size() - 1'));
}
test('drag insertion maps current shown controls through stable neighbor identities', () => checkDragProjection(strip));
test('drag projection guard rejects counting hidden control rectangles', () => {
    assert.throws(() => checkDragProjection(strip.replace('!m_buttons[index]->IsShown()', 'false')));
});
function checkNativeFocus(source) {
    const targets = body(source, 'std::vector<int> TabStrip::FocusTargets(');
    assert(targets.includes('button->IsShown()'));
    assert(targets.includes('atlasFocusTargets('));
    const reconcile = body(source, 'void TabStrip::ReconcileFocus(');
    assert(reconcile.includes('m_overflow_btn->SetFocus()'));
    assert(reconcile.includes('else if (m_focus_index >= 0) SetFocus()'));
    const keyboard = body(source, 'void TabStrip::OnKeyDown(');
    assert(keyboard.includes('atlasStepFocus(focused, targets, step)'));
    assert(keyboard.includes('targets.front() : targets.back()'));
    assert(keyboard.includes('atlasFocusActivation(focused, targets)'));
    assert(keyboard.includes('AtlasFocusAction::OpenOverflow) OpenOverflowMenu()'));
    assert(source.includes('m_overflow_btn->Bind(wxEVT_KEY_DOWN, &TabStrip::OnKeyDown, this)'));
    assert(body(source, 'void TabStrip::OpenOverflowMenu(').includes('ReconcileFocus(true)'));
    assert(body(source, 'void TabStrip::Relayout(').includes('ReconcileFocus(focus_owned)'));
}
test('roving focus uses visible projection and actual overflow button focus with menu return', () => checkNativeFocus(strip));
test('native focus guard rejects a paint-only overflow state', () => {
    assert.throws(() => checkNativeFocus(strip.replace('m_overflow_btn->SetFocus()', 'm_overflow_btn->Refresh()')));
});
test('accessibility reports hidden tabs off-screen without stale geometry and exposes native overflow focus', () => {
    const accessible = body(strip, 'class TabStripAccessible final');
    const state = body(accessible, 'wxAccStatus GetState(');
    assert(state.includes('wxACC_STATE_SYSTEM_INVISIBLE | wxACC_STATE_SYSTEM_OFFSCREEN'));
    assert(state.indexOf('if (!tab_visible(i))') < state.indexOf('wxACC_STATE_SYSTEM_FOCUSABLE | wxACC_STATE_SYSTEM_SELECTABLE'));
    const location = body(accessible, 'wxAccStatus GetLocation(');
    assert(location.includes('if (!tab_visible(i)) { rect = wxRect(); return wxACC_OK; }'));
    assert(location.includes('m_strip->m_overflow_btn->GetScreenRect()'));
    const focus = body(accessible, 'wxAccStatus GetFocus(');
    assert(focus.includes('m_strip->m_overflow_btn->HasFocus()'));
    assert(focus.includes('*child = m_strip->m_overflow_btn->GetAccessible()'));
    assert(focus.includes('if (!m_strip->HasFocus()) return wxACC_FALSE'));
    assert(accessible.includes('wxROLE_SYSTEM_PUSHBUTTON'));
    assert(accessible.includes('IsShownOnScreen()'));
});
test('tab state is immediate while selection motion honors the shared reduced-motion route', () => {
    const active = body(strip, 'void TabStripButton::SetActive(');
    assert(active.indexOf('m_active = a') < active.indexOf('m_selection_motion.Play'));
    assert(active.includes('MD3::Motion::medium1'));
    assert(active.includes('&MD3::Motion::easeStandard, this'));
    const paint = body(strip, 'void TabStripButton::OnPaint(');
    assert(paint.includes('MD3::Motion::reduced()'));
    assert(paint.includes('MD3::Role::PrimaryContainer'));
    assert(paint.includes('MD3::Role::OnPrimaryContainer'));
    assert(!paint.includes('SetSize('));
});
test('title chip uses matching measured glyph geometry and preserves 46-DIP height routing', () => {
    const draw = body(title, 'void BBLTopbarArt::DrawLabel(');
    const layout = body(title, 'void BBLTopbar::update_responsive_title(');
    for (const source of [draw, layout]) {
        assert(source.includes('MaterialIcon::FolderOpen, 20'));
        assert(source.includes('FromDIP(10)'));
        assert(source.includes('FromDIP(8)'));
    }
    assert(layout.includes('atlasTitleTextBudget('));
    assert(layout.includes('SetShortHelp(m_full_title)'));
    assert(body(title, 'void BBLTopbar::Rescale(').includes('FromDIP(MD3::Metrics::top_bar_height)'));
    assert(read('src/slic3r/GUI/Widgets/MD3Tokens.hpp').match(/top_bar_height\s*=\s*46/));
    const drawButton = body(title, 'void BBLTopbarArt::DrawButton(');
    assert(drawButton.includes('MD3::Role::OnErrorContainer'));
    assert(drawButton.includes('MD3::Role::ErrorContainer'));
});
test('nested book minima, custom padding and dialog containment retain DPI lifecycles', () => {
    const style = body(book, 'void TabButtonsListCtrl::StyleButton(');
    assert(style.includes('if (!m_custom_padding)'));
    assert(style.includes('dc.GetTextExtent(btn->GetLabel())'));
    assert(style.includes('MD3::Role::OnPrimaryContainer'));
    assert(body(book, 'void TabButtonsListCtrl::Rescale(').includes('StyleButton(btn, index == m_selection)'));
    assert(dialogs.includes('new wxNavigationEnabled<StaticBox>()'));
    assert(dialogs.includes('wxTAB_TRAVERSAL'));
    assert(dialogs.includes('work.height / 3'));
    assert(dialogs.includes('wxEVT_DPI_CHANGED'));
    assert.equal((dialogs.match(/LB_AUTO_WRAP/g) || []).length, 3);
    assert(body(frame, 'void MainFrame::on_dpi_changed(').includes('place_project_tabbar()'));
    assert(body(frame, 'void MainFrame::on_sys_color_changed(').includes('m_project_tabbar->Rescale()'));
});
function checkPalette(source) {
    const declarations = read('src/slic3r/GUI/Widgets/MD3Tokens.hpp');
    for (const match of source.matchAll(/MD3::Light::([A-Za-z0-9_]+)/g))
        assert(declarations.includes(`wxColour ${match[1]}{`), match[1]);
}
test('every light-palette reference exists and an invented reference fails', () => {
    checkPalette(strip + dialogs + book + title);
    assert.throws(() => checkPalette('MD3::Light::missingContainer'));
});
test('window, workflow, pin, group, reorder, close and search behavior bodies remain unchanged', () => {
    const fixture = JSON.parse(read('tests/native_shared_controls/atlas_shell_preserved_functions.json'));
    for (const entry of fixture.functions) {
        const source = read(entry.file);
        const section = entry.end ? source.slice(source.indexOf(entry.name), source.indexOf(entry.end)) : body(source, entry.name);
        assert.equal(digest(section), entry.sha256, entry.name);
    }
});
