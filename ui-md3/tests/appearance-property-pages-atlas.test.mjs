import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import test from 'node:test';
const source = readFileSync(new URL('../../src/slic3r/GUI/Appearance/AppearanceEditorPopover.cpp', import.meta.url), 'utf8').replace(/\r\n/g, '\n');
const expected = {
  "callbacks": "654f26e533cf4860cbae9272c2d4e35f4cf9cc0b5ee99be54d6402f90e8ef418",
  "methods": {
    "void AppearanceEditorPopover::write_number(": "5d869e189d5e88de174b0a576e295cb0372f210704543a74b8688f2171dc5e76",
    "void AppearanceEditorPopover::write_bool(": "6b572419c86014ef0b0bf398d6514d04d298d8ed9dbbcb69d1456849bdd1b679",
    "void AppearanceEditorPopover::write_string(": "7e153fe7c51168c2a599f1478d5d5c504c91ce8e24809ae5422ff638642c7ce4",
    "void AppearanceEditorPopover::reset_property(": "543b7d7cbf4e831c87783a09c3156ece454f7693e2dff8d2a9297cc022152238",
    "void AppearanceEditorPopover::persist(": "dd5acd35cceba00bff0ae6b92d125d614bb7978cfe6239bc4c30c67fc02dec81",
    "void AppearanceEditorPopover::close_and_return_focus(": "7ddddfd74553b39232bcbc4c033af5cdfe84ec3d793193299477c9220a1cef0d",
    "void AppearanceEditorPopover::show_section(": "4322cd16ccc6d38feeb3741c6700d0b42d70906d12db8771ca02a4639ee5fec2",
    "void AppearanceEditorPopover::refresh_from_registry(": "a8d9877438796260fc8703f4b7d5145d1f005185813321e655728285855d5664",
    "Button *AppearanceEditorPopover::make_reset(": "145a9c0ee01ec9c09890bbaff27e4708e745797171ed06627478b3dba41af5c7",
    "Button *AppearanceEditorPopover::make_swatch(": "a91165601cbcc691ce3c3881c35416dabd693589192939ddf41eab38a6eafbd5"
  },
  "declarations": "c5a09d7990da6e5c1a69c083cf7c81e0445f3558dde95120f5499811efb36b2f",
  "colours": "6c60a1b3850092076e43c61e9e5b33a8538ee30c597a4354db9471912d6531c7"
};
const mask = text => text.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, v => v.replace(/[^\n]/g, ' '));
const hash = text => createHash('sha256').update(text).digest('hex');
function balanced(text, start, open, close) {
  const hidden = mask(text);
  const first = hidden.indexOf(open, start);
  assert(first >= 0);
  let depth = 1;
  for (let i = first + 1; i < hidden.length; i++) {
    if (hidden[i] === open) depth++;
    if (hidden[i] === close && --depth === 0) return text.slice(start, i + 1);
  }
  assert.fail('unterminated source block');
}
function method(name) {
  const start = mask(source).indexOf(name);
  assert(start >= 0, name);
  return balanced(source, start, '{', '}');
}
function callbacks(text) {
  const hidden = mask(text);
  const events = [...hidden.matchAll(/->(?:Bind|SetOnQuery|SetOnRegexToggle)\(/g)]
    .map(m => balanced(text, m.index, '(', ')'))
    .filter(call => call !== presetSizeBinding);
  const values = [...hidden.matchAll(/->on_change\s*=/g)].map(m => balanced(text, m.index, '{', '}'));
  return [...events, ...values].join('\n');
}
// Only this new presentation callback is excluded from the original behavior
// fingerprint. Its source is executed by the lifecycle adapter below.
const presetSizeBinding = `->Bind(wxEVT_SIZE, [this, page](wxSizeEvent &event) {
        event.Skip();
        if (page->GetClientSize().x != m_preset_page_width)
            reflow_preset_page();
    })`;
test('all existing bound event implementations remain unchanged', () => {
  assert.equal(hash(callbacks(source)), expected.callbacks);
  const changed = source.replace('write_number(StyleProp::font_weight,', 'write_number(StyleProp::font_size,');
  assert.notEqual(changed, source);
  assert.throws(() => assert.equal(hash(callbacks(changed)), expected.callbacks));
});
for (const [name, digest] of Object.entries(expected.methods)) {
  test('preserved registry and focus method: ' + name, () => assert.equal(hash(method(name)), digest));
}
test('property declarations retain keys, ranges and defaults', () => {
  const declarations = source.match(/^\s*(?:row\(_L|spin_row\(|add_check\(|m_(?:size|letter_spacing|line_height) = new AppearanceDecimalField).*$/gm).join('\n');
  assert.equal(hash(declarations), expected.declarations);
  const colours = source.match(/const Row rows\[\] = \{[\s\S]*?\n    \};/)[0];
  assert.equal(hash(colours), expected.colours);
});
test('all three property builders stack wrapping labels above real value/reset rows', () => {
  for (const name of ['build_typography', 'build_colours', 'build_shape']) {
    const body = method('void AppearanceEditorPopover::' + name + '(');
    assert.match(body, /new Label\(page, Label::Body_13, (?:r.label|label), LB_AUTO_WRAP \| wxST_NO_AUTORESIZE\)/);
    assert.match(body, /auto \*value_row = new wxBoxSizer\(wxHORIZONTAL\);/);
    assert.match(body, /value_row->Add\(make_reset\(/);
    assert.doesNotMatch(body, /new Label\([^;]*wxSize\(FromDIP\(110\)/);
  }
});
test('decorations wrap as complete checkbox label reset groups', () => {
  const body = method('void AppearanceEditorPopover::build_typography(');
  assert.match(body, /auto \*deco = new wxWrapSizer\(wxHORIZONTAL\);/);
  for (const item of ['slot', 'l', 'make_reset']) assert(body.includes('group->Add(' + item));
  assert.match(body, /deco->Add\(group, 0, wxRIGHT \| wxBOTTOM/);
});
test('preset action groups receive available width and active names wrap', () => {
  const body = method('void AppearanceEditorPopover::build_presets(');
  for (const group of ['actions', 'io']) {
    assert(body.includes('auto *' + group + ' = new wxWrapSizer(wxHORIZONTAL);'));
    assert(new RegExp('s->Add\\(' + group + ', 0, wxEXPAND').test(body));
  }
  assert.match(body, /m_preset_active = new Label\([^;]*LB_AUTO_WRAP \| wxST_NO_AUTORESIZE\);/);
});

// Execute the actual C++ presentation statements against a deterministic
// non-window adapter. This checks lifecycle wiring, not wx font metrics.
function presetLifecycle(text = source) {
  const getMethod = name => balanced(text, mask(text).indexOf('void AppearanceEditorPopover::' + name + '('), '{', '}');
  const body = getMethod('reflow_preset_page');
  const translate = cpp => cpp
    .replace(/auto \*page = dynamic_cast<wxScrolledWindow \*>\(m_preset_active->GetParent\(\)\);/, 'const page = m_preset_active.GetParent();')
    .replace(/const wxPoint /g, 'const ').replace(/int pass/g, 'let pass')
    .replace(/->/g, '.').replace(/std::max/g, 'Math.max')
    .replace(/wxString::Format/g, 'format').replace(/wxString::FromUTF8/g, 'String');
  const run = cpp => new Function('state', 'with (state) {' + translate(cpp) + '}');
  const state = { m_preset_reflowing: false, m_preset_page_width: -1,
    focus: 'preset-apply', width: 180, height: 240, virtualHeight: 240,
    view: {x: 0, y: 0}, labelWidth: 180, labelHeight: 20, labelText: '',
    layouts: 0, fits: 0, depth: 0, maxDepth: 0, name: 'Short',
    _L: text => text, format: (format, value) => format.replace('%s', value),
  };
  const page = {
    GetClientSize: () => ({x: state.width - (state.virtualHeight > state.height ? 16 : 0)}),
    GetSizer: () => ({}), InvalidateBestSize() {}, GetViewStart: () => ({...state.view}),
    Layout() { state.layouts++; state.labelWidth = page.GetClientSize().x; },
    FitInside() { state.fits++; state.virtualHeight = 210 + state.labelHeight; resize(); },
    Scroll(x, y) { state.view = {x, y: Math.min(y, Math.max(0, state.virtualHeight - state.height))}; },
  };
  state.m_preset_active = {
    GetParent: () => page, GetSize: () => ({x: state.labelWidth}),
    SetLabel(value) { state.labelText = value; },
    Wrap(width) { state.labelHeight = Math.ceil(state.labelText.length * 8 / width) * 20; },
  };
  const reflow = run(body.slice(body.indexOf('{') + 1, -1));
  state.reflow_preset_page = () => {
    state.maxDepth = Math.max(state.maxDepth, ++state.depth);
    assert(state.depth < 4, 'layout reentry was not bounded');
    reflow(state); state.depth--;
  };
  const builder = getMethod('build_presets');
  const sizeStart = builder.indexOf('page->Bind(wxEVT_SIZE,');
  assert(sizeStart >= 0, 'missing width hook');
  const sizeCall = balanced(builder, sizeStart, '(', ')');
  const sizeBody = balanced(sizeCall, sizeCall.indexOf('{'), '{', '}');
  const widthHook = run(sizeBody.slice(1, -1));
  const resize = () => widthHook({...state, page, event: {Skip() {}}});
  const refresh = getMethod('refresh_preset_list');
  const labelStart = refresh.indexOf('    if (m_preset_active)');
  const labelEnd = refresh.indexOf('    const int sel', labelStart);
  const enableEnd = refresh.indexOf(';', refresh.indexOf('m_preset_delete->Enable')) + 1;
  const contentHook = run(refresh.slice(labelStart, labelEnd) + refresh.slice(enableEnd, -1));
  const update = name => {
    state.name = name;
    state.reg = {active_preset: () => state.name};
    contentHook(state);
  };
  return {state, update, resize};
}

function assertContentLifecycle(text) {
  const {state, update} = presetLifecycle(text);
  update('Short');
  const shortHeight = state.virtualHeight;
  update('Long preset name '.repeat(30));
  assert(state.virtualHeight > shortHeight, 'changed content must enlarge the virtual extent without a size event');
  assert.equal(state.width, 180);
  assert.equal(state.labelWidth, 164, 'wrap must use the post-scrollbar width');
  state.view.y = 40;
  update('Long preset name '.repeat(31));
  assert.equal(state.view.y, 40, 'valid scroll position must survive reflow');
  update('Short');
  assert.equal(state.virtualHeight, shortHeight, 'short content must release stale virtual height');
  assert.equal(state.view.y, 0, 'a vanished scroll range is clamped');
  assert.equal(state.focus, 'preset-apply');
  assert(state.maxDepth <= 2);
  assert.equal(state.m_preset_reflowing, false);
}

test('preset content short-long-short lifecycle reflows at unchanged page dimensions', () => assertContentLifecycle(source));
test('removing the content hook rejects fixed-size preset updates', () => {
  const broken = source.replace('    reflow_preset_page();\n}\n\nvoid AppearanceEditorPopover::reflow_preset_page', '\n}\n\nvoid AppearanceEditorPopover::reflow_preset_page');
  assert.notEqual(broken, source);
  assert.throws(() => assertContentLifecycle(broken), /changed content must enlarge/);
});
test('actual width changes reflow current content without stealing focus', () => {
  const {state, update, resize} = presetLifecycle();
  update('A long preset '.repeat(20));
  const before = state.virtualHeight;
  state.width = 360;
  resize();
  assert(state.virtualHeight < before);
  assert.equal(state.labelWidth, 344);
  assert.equal(state.focus, 'preset-apply');
});
test('removing the reentry guard rejects recursive fit notifications', () => {
  const broken = source.replace('m_preset_reflowing || !m_preset_active', '!m_preset_active');
  assert.notEqual(broken, source);
  assert.throws(() => assertContentLifecycle(broken), /layout reentry was not bounded/);
});
