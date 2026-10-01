import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Switching the Process page from Global to Objects with a plate or object selected closed the
// application with a stack overflow. Three calls form a cycle:
//
//   ParamsPanel::set_active_tab(nullptr)  -> Sidebar::show_object_list(...)
//   Sidebar::show_object_list(true)        -> ObjectList::part_selection_changed()
//   part_selection_changed()               -> ObjectSettings::update_settings_list()
//                                          -> ParamsPanel::set_active_tab(nullptr)
//
// Upstream ends the cycle in show_object_list, which returns early when the list already has the
// requested visibility. The early return was lost when the sidebar was split into tabs. This test
// pins it, and pins that it comes before any call that can lead back to set_active_tab.
//
// OBJECT_LIST_SOURCE_ROOT points the test at another copy of the tree, so it can be shown to fail
// on the old source.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = process.env.OBJECT_LIST_SOURCE_ROOT
  ? path.resolve(process.env.OBJECT_LIST_SOURCE_ROOT)
  : path.resolve(testDir, '..', '..');

const read = (...parts) =>
  readFileSync(path.join(repoDir, ...parts), 'utf8')
    .replace(/\r\n/g, '\n')
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/\/\/.*$/gm, '');

function body(text, signature) {
  const start = text.indexOf(signature);
  assert.notEqual(start, -1, `${signature} not found`);
  const open = text.indexOf('{', start);
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === '{') depth++;
    else if (text[i] === '}' && --depth === 0) return text.slice(open + 1, i);
  }
  throw new Error(`${signature} has no closing brace`);
}

test('show_object_list returns early when the visibility does not change', () => {
  const fn = body(read('src', 'slic3r', 'GUI', 'Plater.cpp'), 'bool Sidebar::show_object_list(bool show) const');
  const guard = fn.search(/if\s*\(\s*p->m_object_list->IsShown\(\)\s*==\s*show\s*\)\s*return\s+false\s*;/);
  assert.notEqual(guard, -1, 'show_object_list has no early return for an unchanged visibility');
  for (const call of ['apply_prepare_section', 'part_selection_changed', '->Show(']) {
    const at = fn.indexOf(call);
    assert.ok(at === -1 || at > guard, `${call} runs before the early return`);
  }
});

test('the cycle the early return breaks is still the one described', () => {
  // When these links change, the test above may no longer protect anything: read the cycle again.
  const params = body(read('src', 'slic3r', 'GUI', 'ParamsPanel.cpp'), 'void ParamsPanel::set_active_tab(wxPanel* tab)');
  assert.match(params, /sidebar\(\)\.show_object_list\(/);
  const settings = read('src', 'slic3r', 'GUI', 'GUI_ObjectSettings.cpp');
  assert.match(settings, /set_active_tab\(nullptr\)/);
  const list = body(read('src', 'slic3r', 'GUI', 'GUI_ObjectList.cpp'), 'void ObjectList::part_selection_changed()');
  assert.match(list, /obj_settings\(\)->UpdateAndShow\(/);
});
