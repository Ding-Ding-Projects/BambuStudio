import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import test from 'node:test';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const sources = Object.fromEntries(['Button', 'SearchField', 'StaticBox', 'MD3Menu'].map(name =>
    [name, read(`src/slic3r/GUI/Widgets/${name}.cpp`)]));

function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, `${name} is present`);
    const begin = source.indexOf('{', start);
    assert(begin >= 0);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g,
        match => ' '.repeat(match.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const digest = text => createHash('sha256').update(text).digest('hex');
const helpers = ['double relativeLuminance(', 'double contrastRatio(', 'wxColour buttonStateLayer('];
const extracted = helpers.map(name => body(sources.Button, name)).join('\n\n');
const outputIndex = process.argv.indexOf('--extract');
if (outputIndex >= 0) {
    assert(process.argv[outputIndex + 1], 'Supply a temporary extraction directory');
    const destination = path.resolve(process.argv[outputIndex + 1]);
    fs.mkdirSync(destination, { recursive: true });
    fs.writeFileSync(path.join(destination, 'atlas_button_state_functions.inc'), extracted);
    console.log(`Production paint helper SHA-256: ${digest(extracted)}`);
}

test('button state layers preserve contrast and remain bounded calculations', () => {
    const helper = body(sources.Button, 'wxColour buttonStateLayer(');
    assert(helper.includes('std::min(4.5, contrastRatio(surface, foreground))'));
    assert(helper.includes('for (int step = 12; step >= 0; --step)'));
    assert(helper.includes('contrastRatio(candidate, foreground) + 1e-9 >= minimum'));
    assert(helper.includes('return surface;'));
    assert(!helper.includes('Refresh('));
    assert(!helper.includes('Play('));
});
function checkButtonStates(button) {
    const source = body(button, 'void Button::applyMD3Style(');
    for (const variant of ['Filled', 'Tonal', 'Outlined', 'Text', 'Danger']) {
        const part = body(source, `case Variant::${variant}:`);
        assert(part.includes('StateColor::Pressed'), variant);
        assert(part.indexOf('StateColor::Disabled') < part.indexOf('StateColor::Pressed'), variant);
        assert(part.indexOf('StateColor::Pressed') < part.indexOf('StateColor::Hovered'), variant);
    }
    assert(source.includes('StateColor::Pressed | StateColor::Checked'));
    assert(source.includes('MD3::Metrics::active().radius'));
    assert(source.includes('MD3::Metrics::active().small_radius'));
}
test('shared variants distinguish pressed feedback and preserve checked precedence', () => checkButtonStates(sources.Button));
test('search backing and focus paint retain existing child target dimensions', () => {
    assert(sources.SearchField.includes('constexpr int kHeight    = 44;'));
    assert(sources.SearchField.includes('constexpr int kActionPx  = 40;'));
    const paint = body(sources.SearchField, 'void SearchField::doRender(');
    assert(paint.includes('FromDIP(m_focused ? 2 : 1)'));
    assert(paint.includes('inset  = std::max(1, FromDIP(1))'));
    assert(!paint.includes('SetSize('));
    assert(body(sources.SearchField, 'void SearchField::applyTextCtrlTheme(').includes('SetBackgroundColor(StateColor(background))'));
    for (const scale of [1, 1.25, 1.5, 2]) {
        const fieldHeight = 44 * scale, actionHeight = 40 * scale;
        const actionTop = (fieldHeight - actionHeight) / 2;
        const ringBottom = 1 * scale + 2 * scale / 2;
        assert(ringBottom <= actionTop);
    }
});
test('menu state surfaces change paint only and keep motion reduction', () => {
    const paint = body(sources.MD3Menu, 'void MD3MenuList::paintRow(');
    assert(paint.includes('wxRect state_rect = r;'));
    assert(paint.includes('state_rect.Deflate(FromDIP(4), FromDIP(2))'));
    assert(paint.includes('dc.DrawRoundedRectangle(state_rect, state_radius)'));
    assert(paint.includes('MD3::Motion::reduced()'));
    assert(!paint.includes('SetSize('));
    assert(!paint.includes('ActivateItem('));
});
test('card rounding remains bounded and explicit caller radii remain authoritative', () => {
    assert(sources.StaticBox.includes('HOVER_ANIM_MS  = MD3::Motion::short2'));
    assert(body(sources.StaticBox, 'void StaticBox::doRender(').includes('std::clamp(radius - border_width, 0.0,'));
    assert(body(sources.StaticBox, 'void StaticBox::SetCornerRadius(').includes('m_uses_default_radius = false;'));
    assert(body(sources.StaticBox, 'void StaticBox::RescaleDefaultCornerRadius(').includes('if (m_uses_default_radius)'));
    assert(body(sources.StaticBox, 'void StaticBox::onHoverTick(').includes('!IsShownOnScreen() || !IsEnabled() || MD3::Motion::reduced()'));
});

// Fingerprints bind this appearance-only change to the reviewed interaction and
// measurement baseline. They are not claims that those functions are bug-free.
const preserved = JSON.parse(read('tests/native_shared_controls/atlas_preserved_functions.json'));
test('registered input, callback and measured-geometry functions are unchanged', () => {
    for (const [source, functions] of Object.entries(preserved))
        for (const [name, expected] of Object.entries(functions))
            assert.equal(digest(body(sources[source], name)), expected, `${source}: ${name}`);
});
test('source checks notice missing pressed feedback and focus-geometry changes', () => {
    assert.throws(() => checkButtonStates(sources.Button.replaceAll('StateColor::Pressed', 'StateColor::Normal')));
    const mutated = sources.SearchField.replace('m_text->SetSize(lead, y, w, th);', 'm_text->SetSize(lead, y, 0, th);');
    assert.throws(() => assert.equal(digest(body(mutated, 'void SearchField::layoutText(')),
        preserved.SearchField['void SearchField::layoutText(']));
    checkButtonStates(sources.Button);
    assert.equal(digest(body(sources.SearchField, 'void SearchField::layoutText(')), preserved.SearchField['void SearchField::layoutText(']);
});
