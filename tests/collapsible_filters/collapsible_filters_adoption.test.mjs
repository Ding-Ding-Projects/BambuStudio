// Source contract: every native collection surface with a search bar or filter
// row puts it in the shared CollapsibleFilterBar, parents the controls to the
// collapsible body and reports its active filters so a collapsed row never
// hides one silently.
import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
import test from 'node:test';

const read = (path) => readFileSync(new URL(`../../src/slic3r/GUI/${path}`, import.meta.url), 'utf8')
  .replace(/\r\n/g, '\n');
const strip = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

// file, surface id, the search field member, and further filter controls that
// must live in the collapsible body.
const surfaces = [
  ['NotificationCenterPanel.cpp', 'notification_center', 'm_search', ['chip = new Button(', 'm_dismissed_chip = new Button(']],
  ['ChangelogDialog.cpp', 'changelog', 'm_search', ['new TextInput(', 'm_calendar_button = new Button(', 'auto *button = new Button(']],
  ['ProjectHistoryDialog.cpp', 'project_history', 'm_search_field', ['m_category_filter = new wxChoice(', 'm_status_filter = new wxChoice(', 'm_device_filter = new wxTextCtrl(']],
  ['MultiMachineManagerPage.cpp', 'device_farm', 'm_search', []],
  ['ConfigProfilesDialog.cpp', 'config_profiles', 'm_search_field', []],
  ['UserPresetsDialog.cpp', 'user_presets', 'm_search', []],
  ['PrintHostDialogs.cpp', 'upload_queue', 'search_field', ['search_status = new ::Label(']],
  ['LocalSecurity/IdentityHistoryPanel.cpp', 'identity_history', 'm_search', ['new wxCheckBox(']],
  ['StatusHub/StatusHubPanel.cpp', 'status_hub', 'm_search', []],
  ['Export/ExportDialog.cpp', 'export_formats', 'm_search_field', []],
  ['Plater.cpp', 'object_search', 'p->m_search_bar', []],
];

const escape = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

for (const [file, id, search, controls] of surfaces) {
  test(`${file} adopts the collapsible filter bar`, () => {
    const code = strip(read(file));
    assert.match(code, /#include "(?:\.\.\/)?Widgets\/CollapsibleFilterBar\.hpp"/, 'includes the shared widget');
    const create = code.match(new RegExp(`(\\w+(?:->\\w+)?) = new CollapsibleFilterBar\\([^;]*"${escape(id)}"`));
    assert.ok(create, `creates the bar with the persisted id "${id}"`);
    const bar = create[1];
    assert.match(code, new RegExp(`${escape(search)} = new SearchField\\(${escape(bar)}->GetBody\\(\\)`),
      'the search field is a child of the collapsible body');
    assert.match(code, new RegExp(`${escape(bar)}->GetSectionSizer\\(\\)`), 'the section is laid out');
    assert.match(code, new RegExp(`${escape(bar)}->SetActiveFilters\\(`), 'active filters are reported');
    assert.match(code, /CollapsibleFilterBar::SearchFilterLabel\(/, 'a non-empty query counts as an active filter');
    for (const control of controls) {
      const index = code.indexOf(control);
      assert.notEqual(index, -1, `${control} exists`);
      const args = code.slice(index + control.length, index + control.length + 80);
      assert.match(args, /^\s*(?:\w+(?:->\w+)?->GetBody\(\)|body\b|filter_body\b|parent\b)/,
        `${control} is created inside the collapsible body`);
    }
  });
}

test('every surface id is unique', () => {
  const ids = surfaces.map(([, id]) => id);
  assert.equal(new Set(ids).size, ids.length);
});

test('the object search reveals itself for the Ctrl+F shortcut without storing the choice', () => {
  const plater = strip(read('Plater.cpp'));
  const can = plater.slice(plater.indexOf('void Sidebar::priv::can_search()'));
  assert.match(can.slice(0, 600), /m_object_search_filters->SetExpanded\(true, \/\*remember=\*\/false\)/);
  assert.match(plater, /m_object_search_filters->ShowSection\((?:objects|show)\)/);
  assert.doesNotMatch(plater, /p->m_search_bar->Show\(/, 'the bar section, not the bare field, follows the object list');
});
