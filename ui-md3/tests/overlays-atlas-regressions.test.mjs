import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
const root = fileURLToPath(new URL('../../', import.meta.url));
const source = path => (process.env.OVERLAY_SOURCE_REF
  ? execFileSync('git', ['show', `${process.env.OVERLAY_SOURCE_REF}:${path}`], { cwd: root, encoding: 'utf8' })
  : readFileSync(new URL(`../../${path}`, import.meta.url), 'utf8')).replace(/\r\n/g, '\n');
const history = source('src/slic3r/GUI/ProjectHistoryDialog.cpp');
const palette = source('src/slic3r/GUI/CommandPalette.cpp');
const header = source('src/slic3r/GUI/CommandPalette.hpp');
const label = source('src/slic3r/GUI/Widgets/Label.hpp');
// Keep offsets stable while hiding delimiters and flag names in comments/literals.
const mask = text => text.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, value => value.replace(/[^\n]/g, ' '));
function callArguments(text, call) {
  const masked = mask(text);
  const start = masked.indexOf(call);
  assert.notEqual(start, -1, `missing call ${call}`);
  let depth = 1, argumentStart = start + call.length;
  const args = [];
  for (let i = argumentStart; i < masked.length; i++) {
    if (masked[i] === '(') depth++;
    else if (masked[i] === ')' && --depth === 0) {
      args.push({ start: argumentStart, end: i, text: text.slice(argumentStart, i) });
      return args;
    } else if (masked[i] === ',' && depth === 1) {
      args.push({ start: argumentStart, end: i, text: text.slice(argumentStart, i) });
      argumentStart = i + 1;
    }
  }
  assert.fail(`unclosed call ${call}`);
}
function assertConstructorWrapping(text, name) {
  const args = callArguments(text, `${name} = new Label(`);
  assert(args.length >= 4, `${name}: missing style argument`);
  const style = mask(args[3].text);
  assert.match(style, /\bLB_AUTO_WRAP\b/, `${name}: missing wrapping style`);
  assert.match(style, /\bwxST_NO_AUTORESIZE\b/, `${name}: missing size ownership style`);
}
test('responsive history setters match the actual one-argument Label override', () => {
  assert.match(label, /void SetLabel\(const wxString\s*&\s*label\) override;/);
  const responsive = history.slice(history.indexOf('void ProjectHistoryDialog::update_responsive_layout('));
  for (const name of ['m_subtitle_label', 'm_safety_label']) {
    assert.equal(callArguments(responsive, `${name}->SetLabel(`).length, 1, `${name}: constructor flags are not setter arguments`);
    assert.match(responsive, new RegExp(`${name}->Wrap\\(content_width\\);`));
  }
});
test('history keeps constructor wrapping styles on both labels', () => {
  for (const name of ['m_subtitle_label', 'm_safety_label'])
    assertConstructorWrapping(history, name);
});
for (const name of ['m_subtitle_label', 'm_safety_label']) {
  test(`omitting ${name}'s own styles is rejected independently`, () => {
    const style = callArguments(history, `${name} = new Label(`)[3];
    assert(style, `${name}: baseline style argument is required`);
    const mutated = history.slice(0, style.start) + '0' + history.slice(style.end);
    assert.throws(() => assertConstructorWrapping(mutated, name), /missing wrapping style/);
    const other = name === 'm_subtitle_label' ? 'm_safety_label' : 'm_subtitle_label';
    assertConstructorWrapping(mutated, other);
  });
}
test('constructor bounds ignore semicolons and parentheses in literals and comments', () => {
  const fixture = 'subject = new Label(this, font, _L("text; ) ,"), /* ); , */ LB_AUTO_WRAP | wxST_NO_AUTORESIZE);';
  assert.equal(callArguments(fixture, 'subject = new Label(').length, 4);
  assertConstructorWrapping(fixture, 'subject');
  const commentOnly = fixture.replace('/* ); , */ LB_AUTO_WRAP | wxST_NO_AUTORESIZE', '0 /* LB_AUTO_WRAP | wxST_NO_AUTORESIZE */');
  assert.throws(() => assertConstructorWrapping(commentOnly, 'subject'), /missing wrapping style/);
});
test('selection recolors only the explicit decorative icon plate, never all panels', () => {
  const selection = palette.slice(palette.indexOf('void CommandPalette::select_row('), palette.indexOf('void CommandPalette::run_selected('));
  assert.match(selection, /child == icon_plate/);
  assert.doesNotMatch(selection, /wxCLASSINFO\(wxPanel\)|dynamic_cast<wxPanel\s*\*>/);
  assert.match(palette, /m_icon_plates\[row\] = icon;/);
  assert.match(header, /std::map<wxPanel \*, wxPanel \*> m_icon_plates;/);
  assert.match(palette, /sw->SetBackgroundColour\(wxColour\(hex\)\);/);
  assert.match(palette, /\[hex\]\(wxMouseEvent &\)/);
});
test('icon identity is cleared before rows are destroyed during rebuild', () => {
  const rebuild = palette.slice(palette.indexOf('void CommandPalette::rebuild_rows('), palette.indexOf('void CommandPalette::select_row('));
  assert(rebuild.indexOf('m_icon_plates.clear();') >= 0);
  assert(rebuild.indexOf('m_icon_plates.clear();') < rebuild.indexOf('m_list->GetSizer()->Clear(true);'));
});
