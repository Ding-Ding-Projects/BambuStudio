// Source contract for the shared collapsible filter and statistics container.
// The rules are unit tested in collapsible_filters_tests.cpp; this checks that
// the wxWidgets layer wires them to persistence, keyboard and accessibility.
import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
import test from 'node:test';

const read = (path) => readFileSync(new URL(`../../${path}`, import.meta.url), 'utf8');
const widget = read('src/slic3r/GUI/Widgets/CollapsibleFilterBar.cpp');
const header = read('src/slic3r/GUI/Widgets/CollapsibleFilterBar.hpp');
const cmake = read('src/slic3r/CMakeLists.txt');
const i18nList = read('bbl/i18n/list.txt');

test('the widget is built and its strings are extracted', () => {
  assert.match(cmake, /GUI\/Widgets\/CollapsibleFilterBar\.cpp/);
  assert.match(cmake, /GUI\/Widgets\/CollapsibleFilterState\.hpp/);
  assert.match(i18nList, /^src\/slic3r\/GUI\/Widgets\/CollapsibleFilterBar\.cpp$/m);
});

test('the header is a keyboard-focusable button with a disclosure accessible object', () => {
  assert.match(header, /class CollapsibleFilterBar : public Button/);
  assert.match(widget, /SetAccessible\(new CollapsibleFilterBarAccessible\(this\)\)/);
  assert.match(widget, /wxACC_STATE_SYSTEM_EXPANDED/);
  assert.match(widget, /wxACC_STATE_SYSTEM_COLLAPSED/);
  assert.match(widget, /wxACC_STATE_SYSTEM_FOCUSABLE/);
  assert.match(widget, /wxACC_EVENT_OBJECT_STATECHANGE/);
  assert.match(widget, /wxACC_EVENT_OBJECT_DESCRIPTIONCHANGE/);
  assert.match(widget, /GetDescription\(int child_id, wxString \*description\)/);
  assert.match(widget, /DoDefaultAction[\s\S]*?m_bar->Toggle\(\)/);
});

test('the state is restored from and written to the application config', () => {
  assert.match(widget, /m_state\.restore\(config_reader\(\)\)/);
  assert.match(widget, /m_state\.set_expanded\(expanded, remember \? config_writer\(\) : CF::Section::Write\(\)\)/);
  assert.match(widget, /config->set\(section, key, value\)/);
});

test('the bar follows theme and DPI changes on its own', () => {
  assert.match(widget, /Bind\(wxEVT_PAINT, \[this\]\(wxPaintEvent &event\) \{\s*SyncTheme\(\);/);
  assert.match(widget, /Bind\(wxEVT_DPI_CHANGED, \[this\]\(wxDPIChangedEvent &event\) \{\s*Rescale\(\);/);
});

test('collapsing hides only the body panel and rescues focus', () => {
  assert.match(widget, /m_body->Show\(m_section_shown && expanded\)/);
  assert.match(widget, /focus_is_inside\(m_body\)\)\s*SetFocus\(\)/);
  // Never ShowItems()/Show(sizer): that would overwrite the shown state the
  // host gives its own controls.
  assert.doesNotMatch(widget, /ShowItems\(/);
});

test('a collapsed bar discloses active filters in visible text', () => {
  assert.match(widget, /CF::disclose\(labels, m_state\.expanded\(\)\)/);
  assert.match(widget, /_L\("Active filters \(%d\): %s"\)/);
  assert.match(widget, /_L\("\+%d more"\)/);
  assert.match(widget, /m_summary->Show\(m_section_shown && disclosure\.visible\)/);
});
