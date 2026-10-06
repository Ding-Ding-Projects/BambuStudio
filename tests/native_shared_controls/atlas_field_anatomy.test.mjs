import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import test from 'node:test';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const input = read('src/slic3r/GUI/Widgets/TextInput.cpp');
const header = read('src/slic3r/GUI/Widgets/TextInput.hpp');
const combo = read('src/slic3r/GUI/Widgets/ComboBox.cpp');
const field = read('src/slic3r/GUI/Field.cpp');
const tab = read('src/slic3r/GUI/Tab.cpp');
const bitmap = read('src/slic3r/GUI/BitmapComboBox.cpp');
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
    fs.mkdirSync(destination, { recursive: true });
    const geometry = body(input, 'struct AtlasFieldLayout') + ';\n' + body(input, 'AtlasFieldLayout atlasFieldLayout(');
    fs.writeFileSync(path.join(destination, 'atlas_field_layout.inc'), geometry);
    console.log(`Production geometry SHA-256: ${digest(geometry)}`);
}

test('layout, drawing and minimum size share one set of font-correct measurements', () => {
    for (const name of ['void TextInput::DoSetSize(', 'void TextInput::render(', 'void TextInput::messureSize(']) {
        const source = body(input, name);
        assert(source.includes('measureContent()'));
        assert(source.includes('atlasFieldLayout('));
    }
    const measure = body(input, 'TextInput::ContentMetrics TextInput::measureContent(');
    assert(measure.includes('dc.SetFont(GetFont())'));
    assert(measure.includes('dc.SetFont(Label::Body_12)'));
    assert(measure.includes('dc.SetFont(text_ctrl->GetFont())'));
    assert(!body(input, 'AtlasFieldLayout atlasFieldLayout(').includes('FromDIP'));
    assert(input.includes('dc.DrawText(static_tips, label_x, label_y + m.label.y + support_gap)'));
    assert(!input.includes('pt.x += static_tips_size.x'));
});

function checkLifecycle(source, declarations) {
    assert(declarations.includes('wxTextCtrl * text_ctrl = nullptr'));
    assert(body(source, 'bool TextInput::SetFont(').includes('if (text_ctrl)'));
    assert(body(source, 'bool TextInput::SetFont(').includes('messureSize()'));
    assert(body(source, 'void TextInput::SetCornerRadius(').includes('StaticBox::SetCornerRadius(radius)'));
    assert(body(source, 'void TextInput::Rescale(').includes('RescaleDefaultCornerRadius()'));
    assert(body(source, 'void TextInput::messureSize(').includes('DoSetSize(wxDefaultCoord'));
}
test('font, explicit radius, density and same-size label changes retain a live layout path', () => checkLifecycle(input, header));
test('lifecycle guard rejects removal of same-size relayout', () => {
    assert.throws(() => checkLifecycle(input.replace('DoSetSize(wxDefaultCoord', 'MissingRelayout(wxDefaultCoord'), header));
});

test('focus stays inside padding, validation restores caller state, no timer is introduced', () => {
    const paint = body(input, 'void TextInput::render(');
    assert(paint.includes('focus.Deflate(inset)'));
    assert(paint.includes('wxDCClipper clip(dc, GetClientRect())'));
    assert(!paint.includes('SetSize('));
    assert(!input.includes('wxTimer'));
    for (const scale of [1, 1.25, 1.5, 2]) for (const padding of [5, 8])
        assert(2 * scale + 2 * scale / 2 <= padding * scale);
    const validation = body(input, 'bool TextInput::CheckValid(');
    assert(validation.includes('MD3::Role::OnErrorContainer'));
    assert(validation.includes('background_color.colorForStates(state_handler.states())'));
    assert(validation.includes('text_color.colorForStates(state_handler.states())'));
});

test('choice state colors are paired and native bitmap item bounds remain native', () => {
    const palette = body(combo, 'void ComboBox::SetColorScheme(');
    assert(palette.includes('MD3::Role::SecondaryContainer, scheme'));
    assert(palette.includes('MD3::Role::OnSecondaryContainer, scheme'));
    assert(palette.indexOf('StateColor::Disabled') < palette.indexOf('StateColor::Pressed'));
    assert(palette.indexOf('StateColor::Pressed') < palette.indexOf('StateColor::Focused'));
    const paint = body(bitmap, 'void BitmapComboBox::DrawBackground_(');
    assert(paint.includes('wxRect selection = rect'));
    assert(paint.includes('selection.Deflate(FromDIP(1))'));
    assert(!paint.includes('SetSize('));
    assert(!paint.includes('SetSelection('));
});

test('field and preset callers remeasure once-scaled geometry after density and DPI changes', () => {
    assert(body(field, 'void TextCtrl::BUILD(').includes('static_cast<::TextInput*>(temp)->Rescale()'));
    assert(body(field, 'void MultiVariantTextCtrl::msw_rescale(').includes('std::max(current_width, win->GetMinWidth())'));
    assert(body(tab, 'void Tab::msw_rescale(').includes('applyPresetHeaderAnatomy('));
    assert(body(tab, 'void Tab::sys_color_changed(').includes('applyPresetHeaderAnatomy('));
    const spacing = body(tab, 'static void applyPresetHeaderAnatomy(');
    assert(spacing.includes('panel->FromDIP(metrics.gap)'));
    assert(!spacing.includes('GetBorder()'));
});

test('value, inheritance, validation and selection functions remain byte-identical to the baseline', () => {
    const fixture = JSON.parse(read('tests/native_shared_controls/atlas_field_preserved_functions.json'));
    for (const entry of fixture.functions)
        assert.equal(digest(body(read(entry.file), entry.name)), entry.sha256, entry.name);
});
