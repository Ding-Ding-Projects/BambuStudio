// Source checks for three pieces of window chrome that drew the wrong thing:
//
// - The Slice and Print pills on the Prepare action bar painted their corners
//   with SurfaceContainerLow after the bar itself moved to Surface, so every
//   pill sat on a light square. Both now come from prepare_bar_surface().
// - A tab's close button took its corner colour from the tab's window ground,
//   not from the surface the selected or hovered tab paints, so the X sat in
//   a light square. The tab now hands its painted surface to the button.
// - The printer card's title mirrored the printer combo, which shows nothing
//   while the preset in use has no entry in its list, so the card had no name.
//   It now names the preset in use, or says no printer is chosen.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const frame = read('src/slic3r/GUI/MainFrame.cpp');
const strip = read('src/slic3r/GUI/Widgets/TabStrip.cpp');
const plater = read('src/slic3r/GUI/Plater.cpp');

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
const code = text => text.replace(/\/\/[^\n]*/g, '');

function checkActionBar(source) {
    const surface = body(source, 'wxColour prepare_bar_surface(');
    assert.match(surface, /return StateColor::semantic\(MD3::Role::Surface\);/);
    const bar = code(body(source, 'void MainFrame::update_prepare_action_bar_style('));
    assert.match(bar, /const wxColour bar_bg = prepare_bar_surface\(\);/);
    assert.match(bar, /m_prepare_action_bar->SetBackgroundColour\(bar_bg\)/);
    const pills = code(body(source, 'void MainFrame::update_side_button_style('));
    assert.match(pills, /const wxColour\s+bar_bg\s+= prepare_bar_surface\(\);/);
    assert.equal((pills.match(/SetBottomColour\(bar_bg\)/g) || []).length, 1, 'one style helper sets the bar ground');
    const helper = pills.indexOf('auto style_action = [&](SideButton *btn,');
    assert(helper >= 0 && pills.indexOf('SetBottomColour(bar_bg)') > helper, 'the bar ground is set inside the helper');
    for (const group of [/for \(SideButton \*btn : \{m_slice_btn, m_slice_option_btn\}\)\s*style_action\(btn,/,
                         /for \(SideButton \*btn : \{m_print_btn, m_print_option_btn\}\)\s*style_action\(btn,/,
                         /for \(SideButton \*btn : \{m_slice_print_btn, m_slice_send_btn\}\)\s*style_action\(btn,/])
        assert.match(pills, group, 'every action button passes through the helper');
    assert.match(pills, /MD3::Metrics::soft_radius\(btn_height\)/, 'the board radius, not a pill');
    assert.doesNotMatch(pills, /pill_radius/, 'no pill returns to the bar');
    assert.doesNotMatch(pills, /SetBottomColour\((?!bar_bg\))/, 'no pill names another corner colour');
    assert.doesNotMatch(pills, /SurfaceContainerLow\b/, 'the old bar colour is gone from the pills');
    const tools = code(body(source, 'wxSizer* MainFrame::create_side_tools('));
    assert.match(tools, /std::pair\{prepare_bar_surface\(\), \(int\) StateColor::Normal\}/, 'the add-plate rest fill is the bar ground');
}

function checkTabClose(source) {
    const paint = code(body(source, 'void TabStripButton::OnPaint('));
    const painted = paint.indexOf('const bool painted =');
    const draw = paint.indexOf('dc.DrawRoundedRectangle(surface');
    const sync = paint.indexOf('m_close->wxWindow::SetBackgroundColour(behind)');
    assert(painted >= 0 && draw > painted && sync > draw, 'the close button follows the surface just painted');
    assert.match(paint, /const wxColour behind = painted \? fill : base;/);
    assert.match(paint, /if \(m_close->GetBackgroundColour\(\) != behind\)/, 'only a change repaints the button');
    assert.match(paint, /m_close->Refresh\(false\);/);
}

function checkPrinterName(source) {
    const identity = code(body(source, 'void Sidebar::priv::update_printer_identity('));
    const value = identity.indexOf('wxString name = combo_printer->GetValue();');
    const preset = identity.indexOf('if (name.IsEmpty() && !(printer.alias.empty() && printer.name.empty()))');
    const none = identity.indexOf('name = _L("No printer chosen");');
    const label = identity.indexOf('text_printer_name->SetLabel(name);');
    assert(value >= 0 && preset > value && none > preset && label > none, 'the title falls back before it is shown');
    assert.match(identity, /printer\.is_dirty \? Preset::suffix_modified\(\)/, 'a changed preset keeps its marker');
    assert(identity.indexOf('Preset &printer = bundle->printers.get_edited_preset();') < value);
}

test('the action bar pills paint their corners with the bar ground', () => checkActionBar(frame));
test('a tab close button takes the surface its tab paints', () => checkTabClose(strip));
test('the printer card always has a title', () => checkPrinterName(plater));

test('each check fails on the broken source it guards against', () => {
    assert.throws(() => checkActionBar(frame.replace(
        'const wxColour bar_bg     = prepare_bar_surface();',
        'const wxColour bar_bg     = StateColor::semantic(R::SurfaceContainerLow);')));
    assert.throws(() => checkTabClose(strip.replace('m_close->wxWindow::SetBackgroundColour(behind);', '')));
    assert.throws(() => checkPrinterName(plater.replace('name = _L("No printer chosen");', '')));
});
