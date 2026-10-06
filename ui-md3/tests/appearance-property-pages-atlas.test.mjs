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
    .map(m => balanced(text, m.index, '(', ')'));
  const values = [...hidden.matchAll(/->on_change\s*=/g)].map(m => balanced(text, m.index, '{', '}'));
  return [...events, ...values].join('\n');
}
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
