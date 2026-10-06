import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import test from 'node:test';
const read = path => readFileSync(new URL(`../../${path}`, import.meta.url), 'utf8').replace(/\r\n/g, '\n');
const gate = read('src/slic3r/GUI/Widgets/SuperConfirmGate.cpp');
const messages = read('src/slic3r/GUI/MsgDialog.cpp');
const expected = JSON.parse(read('ui-md3/tests/confirmation-atlas-preservation.json'));
const mask = text => text.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, value => value.replace(/[^\n]/g, ' '));
function functionSource(text, marker) {
  const masked = mask(text);
  const start = masked.indexOf(marker);
  assert(start >= 0, `missing function ${marker}`);
  const opening = masked.indexOf('{', start);
  assert(opening > start, `missing body ${marker}`);
  let depth = 1;
  for (let i = opening + 1; i < masked.length; i++) {
    if (masked[i] === '{') depth++;
    if (masked[i] === '}' && --depth === 0) return text.slice(start, i + 1);
  }
  assert.fail(`unclosed body ${marker}`);
}
const hash = text => createHash('sha256').update(text).digest('hex');
// Baseline fingerprints protect actual input/transition/dispatch implementations,
// not merely event-registration spelling. Updating them requires separate semantic review.
for (const [marker, digest] of Object.entries(expected.gateMethods)) {
  test(`confirmation behavior remains unchanged: ${marker}`, () => {
    assert.equal(hash(functionSource(gate, marker)), digest);
  });
}
for (const [marker, digest] of Object.entries(expected.messageMethods)) {
  test(`message default/action behavior remains unchanged: ${marker}`, () => {
    assert.equal(hash(functionSource(messages, marker)), digest);
  });
}
for (const [path, digest] of Object.entries(expected.wholeFiles)) {
  test(`confirmation input/model source remains unchanged: ${path}`, () => {
    assert.equal(hash(read(path)), digest);
  });
}
test('independent key input and accessibility remain unchanged', () => {
  const keyClass = gate.slice(gate.indexOf('class KeySwitch final'), gate.indexOf('wxWindow *resolve_parent'));
  assert.equal(hash(keyClass), expected.keyClass);
});
test('authorization and progress mutations are rejected by the behavior fingerprints', () => {
  for (const [marker, original, replacement] of [
    ['void SuperConfirmGate::finish(', 'authorized && m_state.may_fire()', 'authorized'],
    ['void SuperConfirmGate::on_slide_progress(', 'std::min(99,', 'std::min(100,'],
    ['void SuperConfirmGate::on_key_toggled(', 'm_state.toggle_key(index)', 'm_state.toggle_key(0)'],
  ]) {
    const body = functionSource(gate, marker);
    assert(body.includes(original), marker);
    const mutated = body.replace(original, replacement);
    assert.throws(() => assert.equal(hash(mutated), expected.gateMethods[marker]));
  }
});
test('affected detail is the only scrolled confirmation region', () => {
  const build = functionSource(gate, 'void SuperConfirmGate::build(');
  assert.match(build, /auto \*details = new MD3ScrolledWindow\(this,/);
  assert.match(build, /root->Add\(details, 1, wxEXPAND/);
  assert.match(build, /root->Add\(make_label\(this, Label::Head_16, spec\.consequence/);
  assert.match(build, /detail_sizer->Add\(make_label\(details, Label::Body_13, listed/);
  assert.match(build, /new KeySwitch\(this, key_names\[k\], \[this, k\]\(\) \{ on_key_toggled\(k\); \}\)/);
  assert.match(build, /m_slider = new SlideToConfirm\(this, instruction, done_label\);/);
  assert.match(build, /m_exit = new Button\(this, _L\("Emergency exit"\)/);
  assert.match(build, /SetClientSize\(std::min\(fixed.x, available_w\), std::min\(wanted_h, available_h\)\);/);
  assert.match(build, /m_exit->SetFocus\(\);/);
});
test('message inset and measured footer use matching geometry', () => {
  assert.match(messages, /bounded_message_content_width\(parent, 68 \* em\) - reading_gutter/);
  assert.match(messages, /scrolledWindow->SetMinSize\(wxSize\(info_width \+ reading_gutter, info_height\)\);/);
  assert.match(messages, /m_action_sizer = new wxFlexGridSizer\(1, 0, FromDIP\(MD3::Metrics::active\(\).gap\), FromDIP\(MD3::Metrics::active\(\).gap\)\);/);
  assert.match(messages, /static_cast<int>\(m_button_order.size\(\) - 1\) \* FromDIP\(MD3::Metrics::active\(\).gap\);/);
});
