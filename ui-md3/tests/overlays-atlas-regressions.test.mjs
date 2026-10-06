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
const mask = text => text.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"/g, value => value.replace(/[^\n]/g, ' '));
function argumentCount(text, call) {
  const start = text.indexOf(call);
  assert.notEqual(start, -1, `missing call ${call}`);
  const masked = mask(text);
  let depth = 1, count = 1;
  for (let i = start + call.length; i < masked.length; i++) {
    if (masked[i] === '(') depth++;
    else if (masked[i] === ')' && --depth === 0) return count;
    else if (masked[i] === ',' && depth === 1) count++;
  }
  assert.fail(`unclosed call ${call}`);
}
test('responsive history setters match the actual one-argument Label override', () => {
  assert.match(label, /void SetLabel\(const wxString\s*&\s*label\) override;/);
  const responsive = history.slice(history.indexOf('void ProjectHistoryDialog::update_responsive_layout('));
  for (const name of ['m_subtitle_label', 'm_safety_label']) {
    assert.equal(argumentCount(responsive, `${name}->SetLabel(`), 1, `${name}: constructor flags are not setter arguments`);
    assert.match(responsive, new RegExp(`${name}->Wrap\\(content_width\\);`));
  }
});
test('history keeps constructor wrapping styles on both labels', () => {
  for (const name of ['m_subtitle_label', 'm_safety_label'])
    assert.match(history, new RegExp(`${name} = new Label\\([\\s\\S]*?LB_AUTO_WRAP \\| wxST_NO_AUTORESIZE\\);`));
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
