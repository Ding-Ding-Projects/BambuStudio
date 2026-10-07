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
// The separately tested presentation conjunction is the only authorized change
// to these terminal methods. Removing it recovers the exact normal-path baseline.
const normalPath = body => body.startsWith('bool SuperConfirmGate::Run(') || body.startsWith('void SuperConfirmGate::finish(')
  ? body.replaceAll('gate->m_presentation_available && ', '')
    .replaceAll(' && gate->m_presentation_available', '').replaceAll(' && m_presentation_available', '')
  : body;
// Baseline fingerprints protect actual input/transition/dispatch implementations,
// not merely event-registration spelling. Updating them requires separate semantic review.
for (const [marker, digest] of Object.entries(expected.gateMethods)) {
  test(`confirmation behavior remains unchanged: ${marker}`, () => {
    assert.equal(hash(normalPath(functionSource(gate, marker))), digest);
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
    ['void SuperConfirmGate::finish(', 'authorized && m_presentation_available && m_state.may_fire()', 'authorized'],
    ['void SuperConfirmGate::on_slide_progress(', 'std::min(99,', 'std::min(100,'],
    ['void SuperConfirmGate::on_key_toggled(', 'm_state.toggle_key(index)', 'm_state.toggle_key(0)'],
  ]) {
    const body = functionSource(gate, marker);
    assert(body.includes(original), marker);
    const mutated = body.replace(original, replacement);
    assert.throws(() => assert.equal(hash(normalPath(mutated)), expected.gateMethods[marker]));
  }
});
test('affected detail is the only scrolled confirmation region', () => {
  const build = functionSource(gate, 'void SuperConfirmGate::build(');
  assert.match(build, /auto \*details = new MD3ScrolledWindow\(this,/);
  assert.match(build, /root->Add\(details, 1, wxEXPAND/);
  assert.match(build, /auto \*consequence = make_label\(this, Label::Head_16, spec\.consequence/);
  assert.match(build, /detail_sizer->Add\(make_label\(details, Label::Body_13, listed/);
  assert.match(build, /new KeySwitch\(this, key_names\[k\], \[this, k\]\(\) \{ on_key_toggled\(k\); \}\)/);
  assert.match(build, /m_slider = new SlideToConfirm\(this, instruction, done_label\);/);
  assert.match(build, /m_exit = new Button\(this, _L\("Emergency exit"\)/);
  assert.match(build, /confirmation_controls_fit\(required, available\)/);
  assert.match(build, /m_exit->SetFocus\(\);/);
});
test('actual fit predicate rejects height and width overflow including tiny work areas', () => {
  const body = functionSource(gate, 'bool confirmation_controls_fit(');
  const expression = body.match(/return\s+([\s\S]*?);/)[1];
  const fits = new Function('required', 'available', `return (${expression});`);
  assert.equal(fits({ x: 440, y: 580 }, { x: 460, y: 700 }), true);
  assert.equal(fits({ x: 440, y: 701 }, { x: 460, y: 700 }), false);
  assert.equal(fits({ x: 461, y: 580 }, { x: 460, y: 700 }), false);
  assert.equal(fits({ x: 320, y: 440 }, { x: 160, y: 100 }), false);
  assert.equal(fits({ x: 320, y: 440 }, { x: 0, y: 0 }), false);
  assert.equal(fits({ x: 0, y: 0 }, { x: 460, y: 700 }), false);
});
test('measurement follows zero detail allocation and wrapping at final width', () => {
  const body = functionSource(gate, 'void SuperConfirmGate::build(');
  const zero = body.indexOf('details->SetMinSize(wxSize(0, 0));');
  const width = body.indexOf('SetClientSize(available);', zero);
  const wrap = body.indexOf('consequence->Wrap(wrap_w);', width);
  const measure = body.indexOf('wxSize required = root->GetMinSize();', wrap);
  assert(zero >= 0 && width > zero && wrap > width && measure > wrap);
  assert.match(body, /const wxRect bounds = control->GetRect\(\);/);
  assert.match(body, /best.x > bounds.width \|\| best.y > bounds.height/);
});
test('all terminal authorization expressions reject unavailable presentation', () => {
  const finish = functionSource(gate, 'void SuperConfirmGate::finish(');
  const expression = finish.match(/const bool fire = ([^;]+);/)[1];
  const fire = new Function('authorized', 'm_presentation_available', 'm_state', `return (${expression});`);
  assert.equal(fire(true, false, { may_fire: () => true }), false);
  assert.equal(fire(true, true, { may_fire: () => false }), false);
  assert.equal(fire(true, true, { may_fire: () => true }), true);
  const unsafeFire = new Function('authorized', 'm_presentation_available', 'm_state',
    `return (${expression.replace(' && m_presentation_available', '')});`);
  assert.throws(() => assert.equal(unsafeFire(true, false, { may_fire: () => true }), false));
  const run = functionSource(gate, 'bool SuperConfirmGate::Run(');
  const returns = [...run.matchAll(/authorized = ([^;]+);/g)].map(m => m[1]).filter(s => s.includes('gate->'));
  assert.equal(returns.length, 2);
  for (const value of returns) {
    const result = new Function('gate', 'wxID_OK', `return (${value.replaceAll('->', '.')});`);
    let shown = 0;
    const instance = { m_presentation_available: false, m_state: { may_fire: () => true }, ShowModal: () => { shown++; return 1; } };
    assert.equal(result(instance, 1), false);
    if (value.includes('ShowModal')) assert.equal(shown, 1, 'cancel-only modal still opens');
    const unsafeValue = value.replace('gate->m_presentation_available && ', '').replace(' && gate->m_presentation_available', '');
    const unsafeResult = new Function('gate', 'wxID_OK', `return (${unsafeValue.replaceAll('->', '.')});`);
    assert.throws(() => assert.equal(unsafeResult(instance, 1), false));
  }
});
test('unavailable presentation cancels before replacing content and retains full disclosure', () => {
  const body = functionSource(gate, 'void SuperConfirmGate::build(');
  const fallback = body.slice(body.indexOf('if (!fits) {'));
  assert.match(fallback, /m_presentation_available = false;\s*m_state.cancel\(\);\s*refresh_stage\(\);/);
  assert.match(fallback, /for \(const wxString &name : spec.affected\) disclosure \+= "\\n" \+ name;/);
  assert.match(fallback, /spec.action \+ "\\n\\n" \+ spec.consequence \+ "\\n\\n" \+ count_text/);
  assert.match(fallback, /root->Add\(readback, 1, wxEXPAND\);/);
  assert.match(fallback, /cancel_row->Add\(m_exit, 1, wxEXPAND\);/);
  assert.match(body, /e.GetKeyCode\(\) == WXK_ESCAPE\) \{\s*finish\(false\);/);
});
test('message inset and measured footer use matching geometry', () => {
  assert.match(messages, /bounded_message_content_width\(parent, 68 \* em\) - reading_gutter/);
  assert.match(messages, /scrolledWindow->SetMinSize\(wxSize\(info_width \+ reading_gutter, info_height\)\);/);
  assert.match(messages, /m_action_sizer = new wxFlexGridSizer\(1, 0, FromDIP\(MD3::Metrics::active\(\).gap\), FromDIP\(MD3::Metrics::active\(\).gap\)\);/);
  assert.match(messages, /static_cast<int>\(m_button_order.size\(\) - 1\) \* FromDIP\(MD3::Metrics::active\(\).gap\);/);
});
