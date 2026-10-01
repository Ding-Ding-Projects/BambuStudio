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
// show_object_list now returns at once when it is called again during its own refresh of the
// selection. The switch also stays on the Process page: it moved to the Objects tab before, which
// hides the settings panel, so the plate settings it had just picked could never be seen.
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

const showObjectList = () =>
  body(read('src', 'slic3r', 'GUI', 'Plater.cpp'), 'bool Sidebar::show_object_list(bool show) const');

test('show_object_list returns at once when it is called during its own selection refresh', () => {
  const fn = showObjectList();
  const flag = fn.match(/static\s+bool\s+(\w+)\s*=\s*false\s*;/);
  assert.ok(flag, 'show_object_list has no re-entrancy flag');
  const name = flag[1];
  const early = fn.search(new RegExp(`if\\s*\\(\\s*${name}\\s*\\)\\s*return\\s+false\\s*;`));
  assert.notEqual(early, -1, 'the flag does not end a nested call');
  const set = fn.indexOf(`${name} = true;`);
  const refresh = fn.indexOf('part_selection_changed()');
  assert.ok(early < set && set < refresh, 'the flag is not set before the selection refresh');
  assert.match(fn, new RegExp(`struct\\s+\\w+\\s*\\{\\s*bool\\s*&\\s*flag;\\s*~\\w+\\(\\)\\s*\\{\\s*flag\\s*=\\s*false;`),
    'the flag is not cleared when the refresh ends');
});

test('the Objects switch does not move the sidebar to another tab', () => {
  assert.doesNotMatch(showObjectList(), /apply_prepare_section\(/);
});

test('the cycle the flag breaks is still the one described', () => {
  // When these links change, the tests above may no longer protect anything: read the cycle again.
  const params = body(read('src', 'slic3r', 'GUI', 'ParamsPanel.cpp'), 'void ParamsPanel::set_active_tab(wxPanel* tab)');
  assert.match(params, /sidebar\(\)\.show_object_list\(/);
  assert.match(read('src', 'slic3r', 'GUI', 'GUI_ObjectSettings.cpp'), /set_active_tab\(nullptr\)/);
  const list = body(read('src', 'slic3r', 'GUI', 'GUI_ObjectList.cpp'), 'void ObjectList::part_selection_changed()');
  assert.match(list, /obj_settings\(\)->UpdateAndShow\(/);
});
