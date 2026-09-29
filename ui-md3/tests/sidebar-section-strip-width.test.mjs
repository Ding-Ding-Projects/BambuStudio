import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Prepare section strip (Ink / Process / Objects) docks to the left of the
// sidebar body by default, 128 DIP wide, inside the same AUI pane. Every pane
// width counted only the body, so on md3-v151 the full process-settings tree got
// 334 px of a 480 px pane while it needs about 417: every value field ended past
// the sidebar edge, in every language, and the Cantonese category pills wrapped
// at the hidden width. The Process title itself was pinned to 56 DIP and read
// "打印設…" in Cantonese.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const plater = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Plater.cpp'), 'utf8');
const platerHpp = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Plater.hpp'), 'utf8');
const params = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'ParamsPanel.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const code = stripComments(plater);

test('the strip width is one constant, used by the strip and by every pane width', () => {
  assert.match(code, /static constexpr int PREPARE_SECTION_RAIL_WIDTH = 128;/);
  assert.match(code, /prepare_tabs_options\.vertical_width_dip = PREPARE_SECTION_RAIL_WIDTH;/);
  const strip = code.match(/int Sidebar::section_strip_width\(\) const[\s\S]*?\n\}/)[0];
  assert.match(strip, /MD3::Tabs::is_vertical\(p->m_prepare_tabs->GetDockEdge\(\)\) \? FromDIP\(PREPARE_SECTION_RAIL_WIDTH\) : 0/);
  const def = code.match(/int Sidebar::default_width\(\) const[\s\S]*?\n\}/)[0];
  assert.match(def, /FromDIP\(MD3::Metrics::active\(\)\.sidebar_width\) \+ section_strip_width\(\)/);
  assert.match(platerHpp, /int section_strip_width\(\) const;/);
  assert.match(platerHpp, /int default_width\(\) const;/);
});

test('the pane minimum, default and every width request include the strip', () => {
  // The density width alone is only the body's: the scroller minimum and the
  // panel's first size. Every other use goes through default_width().
  const bodyOnly = [...code.matchAll(/FromDIP\(MD3::Metrics::active\(\)\.sidebar_width\)/g)].length;
  assert.equal(bodyOnly, 3, 'sidebar_width is read raw only by default_width(), the scroller minimum and the panel constructor');
  assert.match(code, /\.MinSize\(wxSize\(this->sidebar->default_width\(\), 90 \* wxGetApp\(\)\.em_unit\(\)\)\)/);
  assert.match(code, /\.BestSize\(wxSize\(this->sidebar->default_width\(\), 90 \* wxGetApp\(\)\.em_unit\(\)\)\)/);
  assert.match(code, /pane\.MinSize\(wxSize\(this->sidebar->default_width\(\), 90 \* em\)\);/);
  assert.match(code, /const int def_w = p->sidebar->default_width\(\);/);
  const advanced = [...code.matchAll(/FromDIP\(ADVANCED_SIDEBAR_WIDTH\)( \+ [^,;)]*section_strip_width\(\))?/g)];
  assert.ok(advanced.length >= 3, 'the tree width is requested from the toggle, the startup size event and a strip move');
  for (const use of advanced) assert.ok(use[1], 'a tree width request without the strip: ' + use[0]);
});

test('moving the strip updates the sidebar minimum and asks for the width again', () => {
  const place = code.match(/void Sidebar::place_prepare_strip\(\)[\s\S]*?\n\}/)[0];
  assert.match(place, /SetMinSize\(wxSize\(default_width\(\), -1\)\);/);
  const dock = code.match(/Bind\(EVT_TABSTRIP_DOCK_CHANGED, \[this\]\(wxCommandEvent &\) \{[\s\S]*?\n    \}\);/)[0];
  assert.match(dock, /place_prepare_strip\(\);/);
  assert.match(dock, /request_sidebar_width\(/);
});

test('the Process title is as wide as its text', () => {
  const create = stripComments(params.match(/m_title_label = new Label\(m_top_panel, _L\("Process"\)[\s\S]*?m_mode_region = new SwitchButton/)[0]);
  assert.doesNotMatch(create, /m_title_label->SetMinSize/, 'a fixed minimum cut "打印設定"');
  assert.match(params, /m_mode_sizer->Add\( m_title_label, 0, wxALIGN_CENTER \);/, 'still a fixed item (CJ-012)');
});
