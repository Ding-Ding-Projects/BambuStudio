import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import test from 'node:test';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const source = read('src/slic3r/GUI/Widgets/ListBox.cpp');
function body(text, name) {
    const start = text.indexOf(name);
    assert(start >= 0, name);
    const begin = text.indexOf('{', start);
    const masked = text.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"/g, match => ' '.repeat(match.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return text.slice(start, begin + i + 1);
    }
    assert.fail(name);
}
const geometry = body(source, 'struct AtlasListGeometry') + ';\n' + body(source, 'AtlasListGeometry atlasListGeometry(')
    + '\n' + body(source, 'int atlasListRowHeight(');
const extract = process.argv.indexOf('--extract');
if (extract >= 0) {
    fs.mkdirSync(process.argv[extract + 1], {recursive: true});
    fs.writeFileSync(path.join(process.argv[extract + 1], 'atlas_listbox_geometry.inc'), geometry);
}

test('selection, checks, native keys and scrollbar routes preserve baseline bodies', () => {
    const fixture = JSON.parse(read('tests/native_shared_controls/atlas_listbox_preserved.json'));
    for (const entry of fixture.functions) {
        let actual = body(source, entry.name);
        if (entry.visualReset) actual = actual.replace('    m_hover_motion.Stop();\n    m_previous_hover = -1;\n    m_hover_progress = 1.0;\n', '');
        assert.equal(createHash('sha256').update(actual).digest('hex'), entry.sha256, entry.name);
    }
});
function checkMeasurement(text) {
    const measure = body(text, 'wxCoord ListBox::OnMeasureItem(');
    assert(measure.includes('dc.SetFont(GetFont())'));
    assert(measure.includes('dc.GetMultiLineTextExtent(m_rows[n]).y'));
    assert(measure.includes('atlasListRowHeight('));
    assert(measure.includes('m_checks ? FromDIP(kCheck) : 0'));
}
test('row measurement uses actual caller font and check glyph with density floor', () => checkMeasurement(source));
test('measurement guard rejects a fixed density-only row', () => {
    assert.throws(() => checkMeasurement(source.replace('dc.GetMultiLineTextExtent(m_rows[n]).y', 'FromDIP(13)')));
});
function checkHitConstants(text) {
    for (const [name, value] of [['kInsetX', 4], ['kPadX', 12], ['kCheck', 20], ['kCheckGap', 8]])
        assert(text.match(new RegExp(`constexpr int ${name}\\s*=\\s*${value};`)), name);
}
test('checkbox paint origin and click boundary retain separately scaled dimensions', () => checkHitConstants(source));
test('checkbox contract guard rejects moving the unchanged click region', () => {
    assert.throws(() => checkHitConstants(source.replace('kPadX    = 12;', 'kPadX    = 16;')));
});
test('font and DPI lifecycle refreshes measurements without overwriting caller fonts or sizes', () => {
    assert(body(source, 'bool ListBox::SetFont(').includes('m_custom_font = true'));
    assert(body(source, 'bool ListBox::SetFont(').includes('if (changed) RefreshAll()'));
    assert(body(source, 'void ListBox::Rescale(').includes('if (!m_custom_font)'));
    assert(source.includes('Bind(wxEVT_DPI_CHANGED'));
    assert(!source.includes('SetMinSize('));
    assert(!source.includes('SetSize('));
});
test('focus and selected foreground stay paired and inside the rounded row', () => {
    const paint = body(source, 'void ListBox::OnDrawBackground(');
    // The ring marks the current row. When every kit list was single-selection
    // that was the selected row, so this check read 'selected && HasFocus()'.
    // Extended-selection lists (wxLB_MULTIPLE) move the current row without
    // selecting it, so the ring now follows IsCurrent(n); see the ListBox
    // section of docs/features/design-system/kit-widgets-2026-09.md.
    assert(paint.includes('IsCurrent(n) && HasFocus() && IsEnabled()'));
    assert(!paint.includes('selected && HasFocus()'));
    assert(paint.includes('focus.Deflate(pen_width)'));
    assert(paint.includes('MD3::Role::SecondaryContainer'));
    assert(paint.includes('MD3::Role::SurfaceContainerLow'));
    assert(source.includes('Bind(wxEVT_SET_FOCUS'));
    assert(source.includes('Bind(wxEVT_KILL_FOCUS'));
    const item = body(source, 'void ListBox::OnDrawItem(');
    assert(item.includes('IsSelected(n) ? MD3::Role::OnSecondaryContainer'));
    assert(item.includes('wxDCClipper row_clip(dc, rect)'));
    assert(item.indexOf('if (text.IsEmpty()) return') < item.indexOf('wxControl::Ellipsize'));
});
test('hover motion is owner-bound and reduced motion bypasses intermediate paint only', () => {
    const hover = body(source, 'void ListBox::animateHover(');
    assert(hover.includes('MD3::Motion::short2'));
    assert(hover.includes('&MD3::Motion::easeStandard, this'));
    assert(!/SetSelection|toggle\(|ProcessEvent/.test(hover));
    assert(source.includes('MD3::Motion::reduced() ? 1.0 : m_hover_progress'));
    const motion = body(source, 'void ListBox::onMotion(');
    assert(motion.indexOf('m_hover = row') < motion.indexOf('animateHover(old)'));
    assert(motion.includes('SetToolTip('));
    assert(body(source, 'ListBox::~ListBox(').includes('m_hover_motion.Stop()'));
});
test('all nine existing construction sites and caller-owned sizing remain reachable', () => {
    const callers = [
        ['src/slic3r/GUI/WorkspacePanel.cpp', 1, 'FromDIP(wxSize(1, 160))'],
        ['src/slic3r/GUI/Appearance/AppearanceEditorPopover.cpp', 2, 'kListHeight  = 132'],
        ['src/slic3r/GUI/Schedule/ScheduledSettingsPanel.cpp', 1, 'FromDIP(180)'],
        ['src/slic3r/GUI/SmartHomeDialog.cpp', 1, 'FromDIP(150)'],
        ['src/slic3r/GUI/SettingsDraftPanel.cpp', 1, 'wxSize(520, 440)'],
        ['src/slic3r/GUI/Widgets/TabStripDialogs.cpp', 3, 'work.height / 3'],
    ];
    for (const [file, count, sizing] of callers) {
        const text = read(file);
        assert.equal((text.match(/new ListBox\(/g) || []).length, count, file);
        assert(text.includes(sizing), file);
    }
});
