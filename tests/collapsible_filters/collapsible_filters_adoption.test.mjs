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
  ['ProjectHistoryDialog.cpp', 'project_history', 'm_search_field', ['m_category_filter = new ComboBox(', 'm_status_filter = new ComboBox(', 'device_field = new TextInput(']],
  ['MultiMachineManagerPage.cpp', 'device_farm', 'm_search', []],
  ['ConfigProfilesDialog.cpp', 'config_profiles', 'm_search_field', []],
  ['UserPresetsDialog.cpp', 'user_presets', 'm_search', []],
  ['PrintHostDialogs.cpp', 'upload_queue', 'search_field', ['search_status = new ::Label(']],
  ['LocalSecurity/IdentityHistoryPanel.cpp', 'identity_history', 'm_search', ['new LabeledCheckBox(']],
  ['StatusHub/StatusHubPanel.cpp', 'status_hub', 'm_search', []],
  ['Export/ExportDialog.cpp', 'export_formats', 'm_search_field', []],
  ['Plater.cpp', 'object_search', 'p->m_search_bar', []],
];

const escape = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

for (const [file, id, search, controls] of surfaces) {
  test(`${file} adopts the collapsible filter bar`, () => {
    const code = strip(read(file));
    assert.match(code, /#include "(?:\.\.\/|slic3r\/GUI\/)?Widgets\/CollapsibleFilterBar\.hpp"/, 'includes the shared widget');
    const create = code.match(new RegExp(`(\\w+(?:->\\w+)?)\\s*=\\s*new CollapsibleFilterBar\\([^;]*"${escape(id)}"`));
    assert.ok(create, `creates the bar with the persisted id "${id}"`);
    // Members created through a pimpl (p->m_x) are used as m_x inside it.
    const bar = create[1];
    const member = `(?:\\w+->)?${escape(bar.replace(/^\w+->/, ''))}`;
    assert.match(code, new RegExp(`${escape(search)}\\s*=\\s*new SearchField\\(\\s*${escape(bar)}->GetBody\\(\\)`),
      'the search field is a child of the collapsible body');
    assert.match(code, new RegExp(`${member}->GetSectionSizer\\(\\)`), 'the section is laid out');
    assert.match(code, new RegExp(`${member}->SetActiveFilters\\(`), 'active filters are reported');
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

test('the sidebar ink slot search is collapsible and reports its query', () => {
  const plater = strip(read('Plater.cpp'));
  assert.match(plater, /p->m_filament_search_filters\s*=\s*new CollapsibleFilterBar\(p->m_filament_area_wrapper, "filament_search"/);
  assert.match(plater, /p->m_filament_search\s*=\s*new SearchField\(p->m_filament_search_filters->GetBody\(\)/);
  assert.match(plater, /p->m_filament_search_filters->SetActiveFilters\(active\)/);
  assert.match(plater, /wrapper_sizer->Add\(p->m_filament_search_filters->GetSectionSizer\(\)/);
});

test('the two sidebar settings searches are collapsible', () => {
  const plater = strip(read('Plater.cpp'));
  assert.match(plater, /p->m_process_search_filters\s*=\s*new CollapsibleFilterBar\(p->m_process_card, "settings_search"/);
  assert.match(plater, /p->m_process_search\s*=\s*new SearchField\(p->m_process_search_filters->GetBody\(\)/);
  assert.match(plater, /card_sizer->Add\(p->m_process_search_filters->GetSectionSizer\(\)/);
  assert.match(plater, /p->m_process_search_adv_filters\s*=\s*new CollapsibleFilterBar\(p->m_process_simple_bar, "settings_search_full"/);
  assert.match(plater, /p->m_process_search_adv\s*=\s*new SearchField\(p->m_process_search_adv_filters->GetBody\(\)/);
  assert.match(plater, /simple_sizer->Add\(p->m_process_search_adv_filters->GetSectionSizer\(\)/);
});

test('every surface id is unique', () => {
  const ids = [...surfaces.map(([, id]) => id), 'filament_search', 'settings_search', 'settings_search_full'];
  assert.equal(new Set(ids).size, ids.length);
});

test('the object search reveals itself for the Ctrl+F shortcut without storing the choice', () => {
  const plater = strip(read('Plater.cpp'));
  const can = plater.slice(plater.indexOf('void Sidebar::priv::can_search()'));
  // Comments are stripped, so the named "remember" argument reads as a bare false.
  assert.match(can.slice(0, 600), /m_object_search_filters->SetExpanded\(true,\s*false\)/);
  assert.match(plater, /m_object_search_filters->ShowSection\((?:objects|show)\)/);
  assert.doesNotMatch(plater, /p->m_search_bar->Show\(/, 'the bar section, not the bare field, follows the object list');
});
