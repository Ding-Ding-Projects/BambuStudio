import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import test from 'node:test';
const expected = JSON.parse(readFileSync(new URL('./reader-details-atlas-preservation.json', import.meta.url)));
const files = Object.fromEntries(Object.keys(expected.files).map(path => [path, readFileSync(new URL('../../' + path, import.meta.url), 'utf8').replace(/\r\n/g, '\n')]));
const [regex, exp] = Object.values(files);
const mask = s => s.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, v => v.replace(/[^\n]/g, ' '));
function block(s, start, open = '{', close = '}') {
  const m = mask(s); const begin = m.indexOf(open, start); let depth = 0;
  assert(start >= 0 && begin >= 0);
  for (let i = begin; i < m.length; i++) {
    if (m[i] === open) depth++;
    if (m[i] === close && !--depth) return s.slice(start, i + 1);
  }
  assert.fail('unterminated source');
}
const hash = s => createHash('sha256').update(s).digest('hex');
function preserved(s, contract) {
  // These two reviewed additions perform layout only. Preserve the complete
  // evaluation, data, validation, clipboard, launch and motion implementations.
  const normalized = s.replace(/^\s*refreshDiagnosticLayout\(m_(?:ref_)?scroll\);\n/gm, '');
  for (const [name, digest] of Object.entries(contract.methods))
    assert.equal(hash(block(normalized, mask(normalized).indexOf(name))), digest, name);
  const callbacks = [...mask(s).matchAll(/(?:->|\b)Bind\(/g)].map(m => block(s, m.index, '(', ')')).join('\n');
  assert.equal(hash(callbacks), contract.callbacks);
}
for (const [path, contract] of Object.entries(expected.files))
  test('behavior and callback preservation: ' + path, () => preserved(files[path], contract));
test('changed export password validation is rejected', () => {
  const broken = exp.replace('m_password_input->GetTextCtrl()->GetValue() != m_password_confirm_input', 'm_password_input->GetTextCtrl()->GetValue() == m_password_confirm_input');
  assert.notEqual(broken, exp);
  assert.throws(() => preserved(broken, Object.values(expected.files)[1]));
});
test('changed regex evaluation budget is rejected', () => {
  const broken = regex.replace('kMaxMatches, options);', 'kMaxMatches + 1, options);');
  assert.notEqual(broken, regex);
  assert.throws(() => preserved(broken, Object.values(expected.files)[0]));
});
function wrapping(s, name) {
  const assignment = s.indexOf(name + ' = new Label(');
  const call = block(s, assignment, '(', ')');
  assert.match(call, /LB_AUTO_WRAP \| wxST_NO_AUTORESIZE/);
  assert(s.includes(name + '->SetMinSize(wxSize(0, -1));'));
}
test('live diagnostics and loss notice keep wrapping content', () => {
  wrapping(regex, 'm_status'); wrapping(regex, 'm_ref_status'); wrapping(exp, 'm_format_badge_label');
  const broken = exp.replace('Label::Head_14, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE', 'Label::Head_14, wxEmptyString');
  assert.throws(() => wrapping(broken, 'm_format_badge_label'));
});
test('sample and result readers expand within the existing test section', () => {
  for (const member of ['m_sample', 'm_results']) {
    assert(regex.includes(member + '->SetFont(Label::Mono_13);'));
    assert(regex.includes('test_sizer->Add(' + member + ', 0, wxEXPAND | wxTOP'));
  }
  assert.match(regex, /sizer->Add\(m_test_panel, 0, wxEXPAND/);
  assert.match(block(regex, regex.indexOf('m_results = new TextAreaEditor('), '(', ')'), /wxTE_READONLY/);
});
test('tool location detail gets a full row above its measured action', () => {
  assert.match(exp, /auto \*seven_zip_row = new wxBoxSizer\(wxVERTICAL\)/);
  assert.match(exp, /seven_zip_row->Add\(m_seven_zip_status_label, 0, wxEXPAND \| wxBOTTOM/);
  assert.match(exp, /seven_zip_row->Add\(m_locate_seven_zip_button, 0, wxALIGN_LEFT/);
});
function diagnosticLifecycle(s) {
  const helper = block(s, s.indexOf('void refreshDiagnosticLayout('));
  const status = block(s, s.indexOf('auto setStatus = '));
  const js = cpp => cpp.replace(/->/g, '.').replace(/const wxPoint /g, 'const ').replace(/int pass/g, 'let pass');
  const run = (cpp, params) => new Function(...params, js(cpp.slice(cpp.indexOf('{') + 1, -1)));
  let text = '', extent = 0, view = {x: 0, y: 7}, focus = 'pattern';
  const scroll = {GetSizer: () => ({}), GetViewStart: () => ({...view}),
    Layout() {}, FitInside() {extent = Math.ceil(text.length / 24) * 20;}, Scroll(x,y) {view = {x,y};}};
  const label = {SetForegroundColour() {}, SetLabel(value) {text=value;}, SetToolTip() {}, Refresh() {}};
  const reflow = run(helper, ['scroll']);
  const update = run(status, ['text','colour','m_status','m_scroll','refreshDiagnosticLayout']);
  const set = value => update(value, 'semantic', label, scroll, reflow);
  set('Valid'); const small=extent;
  set('A diagnostic with details '.repeat(20)); assert(extent>small, 'content hook must refresh the virtual extent');
  set('Valid'); assert.equal(extent,small);
  assert.deepEqual(view,{x:0,y:7}); assert.equal(focus,'pattern');
}
test('actual diagnostic callback updates short-long-short content without resizing', () => diagnosticLifecycle(regex));
test('missing diagnostic content hook fails the lifecycle check', () => {
  const broken=regex.replace('        refreshDiagnosticLayout(m_scroll);\n','');
  assert.notEqual(broken,regex);
  assert.throws(() => diagnosticLifecycle(broken), /content hook must refresh/);
});
