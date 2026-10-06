import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import test from 'node:test';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const digest = text => createHash('sha256').update(text).digest('hex');
const spin = read('src/slic3r/GUI/Widgets/SpinInput.cpp');
const check = read('src/slic3r/GUI/Widgets/CheckBox.cpp');
const toggle = read('src/slic3r/GUI/Widgets/SwitchButton.cpp');
export function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, text => ' '.repeat(text.length));
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
    fs.writeFileSync(path.join(destination, 'atlas_selection_geometry.inc'),
        body(spin, 'struct AtlasSpinLayout') + ';\n' + body(spin, 'AtlasSpinLayout atlasSpinLayout('));
}

function preserved(sources) {
    const fixture = JSON.parse(read('tests/native_controls/atlas_selection_preserved.json'));
    for (const entry of fixture.functions)
        assert.equal(digest(body(sources[entry.file], entry.name)), entry.sha256, entry.name);
    assert.equal(digest(sources['SwitchButton.cpp'].slice(sources['SwitchButton.cpp'].indexOf('#if wxUSE_ACCESSIBILITY\nclass SwitchBoard'))), fixture.otherClasses);
}

test('numeric parsing, range, repeat, events and toggle state/motion stay unchanged', () => {
    preserved({'SpinInput.cpp': spin, 'CheckBox.cpp': check, 'SwitchButton.cpp': toggle});
});

test('preservation guard rejects changed range behavior and sibling class changes', () => {
    assert.throws(() => preserved({'SpinInput.cpp': spin.replace('this->max = max;', 'this->max = min;'), 'CheckBox.cpp': check, 'SwitchButton.cpp': toggle}));
    assert.throws(() => preserved({'SpinInput.cpp': spin, 'CheckBox.cpp': check, 'SwitchButton.cpp': toggle + '\n// changed sibling\n'}));
});

function layoutContract(source) {
    const size = body(source, 'void SpinInput::messureSize(');
    assert(size.includes('dc.SetFont(GetFont())'));
    assert(size.includes('dc.SetFont(text_ctrl->GetFont())'));
    assert(size.includes('atlasSpinLayout('));
    assert(size.includes('SetMinSize({layout.minimum_width'));
    assert(size.includes('text_ctrl->SetSize({layout.entry_width'));
    assert(body(source, 'void SpinInput::Rescale(').includes('RescaleDefaultCornerRadius()'));
    assert(body(source, 'void SpinInput::SetCornerRadius(').includes('StaticBox::SetCornerRadius(radius)'));
}
test('numeric field measures actual fonts and preserves explicit caller radii', () => layoutContract(spin));
test('layout guard rejects missing editor measurement and minimum-width reporting', () => {
    assert.throws(() => layoutContract(spin.replace('dc.SetFont(text_ctrl->GetFont())', 'dc.SetFont(GetFont())')));
    assert.throws(() => layoutContract(spin.replace('SetMinSize({layout.minimum_width', 'SetMinSize({GetSize().x')));
});

test('bilingual fitting, caller overrides and bitmap minimum remain connected', () => {
    const paint = body(toggle, 'void SwitchButton::Rescale(');
    for (const text of ['dc.SetFont(GetFont())', 'MD3::Metrics::active().padding', 'segment_inset',
        'm_track_overridden ? track_color', 'm_thumb_overridden ? thumb_color', 'm_text_overridden ? text_color',
        'fit_bilingual(memdc, labels[0]', 'fit_bilingual(memdc, labels[1]',
        'maxWidth > 2 * segment_inset', 'SetMinSize(ScalableBitmap::GetBmpSize(m_on))']) assert(paint.includes(text), text);
    assert(body(spin, 'void SpinInput::render(').includes('fit_bilingual(dc, label'));
});

test('checkbox glyph remains within the native target and retains focus/disabled variants', () => {
    assert(check.includes('constexpr int kCheckBoxPx = 20;'));
    assert(check.includes('px * 0.8'));
    const update = body(check, 'void CheckBox::update(');
    assert(update.includes('SetBitmapDisabled(renderBitmap(v, h, true, false))'));
    assert(update.includes('SetBitmapFocus(renderBitmap(v, h, false, true))'));
    for (const scale of [1, 1.25, 1.5, 2]) {
        const side = Math.ceil(20 * scale), mark = Math.ceil(16 * scale);
        assert((side - mark) / 2 >= 2 * scale);
        const inset = Math.max(0.5, 0.75 * scale), stroke = Math.max(1, 1.5 * scale);
        assert(inset - stroke / 2 >= 0);
    }
});
