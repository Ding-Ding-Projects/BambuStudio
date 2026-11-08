// Source checks for locks on rows and the lock badge
// (docs/features/windows/element-locks.md):
//
// - The filter stops a click on a locked row (an object in the object list or
//   the 3D view) with the hint and offers only Unlock on a right-click; the
//   wheel and the keys go on, and a release is held back only after its press
//   was, so a drag that ends over a locked row still ends.
// - Ink rows are locked one by one under the number they have now; their
//   menu offers the row's lock and no Delete or Merge while it is locked, and
//   deleting a locked row is refused wherever it is asked for.
// - A locked object stays out of the selection in the list and in the 3D
//   view, carries a badge on its row, is arranged around by Arrange all, keeps
//   its plate from being arranged alone or emptied, and stops Delete all.
// - Locked elements carry a badge that follows them; the notes clean up.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const service = read('src/slic3r/GUI/ElementLocks/LockService.cpp');
const plater = read('src/slic3r/GUI/Plater.cpp');
const factories = read('src/slic3r/GUI/GUI_Factories.cpp');
const list = read('src/slic3r/GUI/GUI_ObjectList.cpp');
const model = read('src/slic3r/GUI/ObjectDataViewModel.cpp');
const arrange = read('src/slic3r/GUI/Jobs/ArrangeJob.cpp');

function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, (t) => ' '.repeat(t.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const code = (text) => text.replace(/\/\/[^\n]*/g, '');

function checkFilter(source) {
    const filter = code(body(source, 'int FilterEvent(wxEvent &event) override'));
    assert.match(filter, /const bool\s+mouse = left \|\| t == wxEVT_MIDDLE_DOWN \|\| t == wxEVT_MOUSEWHEEL;/);
    // Rows: only clicks are looked up, so the wheel scrolls and the keys work.
    assert.match(filter, /if \(left \|\| right\) \{/);
    assert.match(filter, /const Identity row = it->second \? it->second\(screen\) : Identity\{\};/);
    assert.match(filter, /if \(t == wxEVT_RIGHT_DOWN\) m_held_right = window;\s*else m_held_left = window;/);
    // A release is held back only after its press was.
    assert.match(filter, /wxWindow \*&held = t == wxEVT_LEFT_UP \? m_held_left : m_held_right;\s*if \(held != window\) \{/);
    assert.match(filter, /if \(t == wxEVT_RIGHT_UP && !m_held_id\.empty\(\)\)[\s\S]*hint_now\(at\.get\(\), id, true\);/, 'a right-click offers Unlock');
    // Window-backed: an element's own id, or a row identity worked out now.
    assert.match(filter, /const Identity mine = identity->second\(\);\s*if \(!mine\.id\.empty\(\) && state\(\)\.store\.find\(mine\.id\)\)/);
}

function checkBadges(source) {
    const refresh = code(body(source, 'void refresh_badges()'));
    assert.match(refresh, /for \(wxWindow \*w : ElementStyle::windows_of\(id\)\) want\[w\] = id;/);
    assert.match(refresh, /badge->target\(\) == it->first && badge->id\(\) == wanted->second/);
    assert.match(code(body(source, 'void changed()\n{')), /refresh_badges\(\);/);
    assert.match(code(body(source, 'bool lock_element(')), /if \(done\) changed\(\);/);
    assert.match(code(body(source, 'bool unlock_element(')), /if \(done\) changed\(\);/);
    assert.match(code(body(source, 'void place()')), /const bool show = t->IsShown\(\);/);
    assert.match(code(body(source, 'void hint_now(')), /if \(!e\.IsShown\(\)\) popup->CallAfter\(\[popup\]\(\) \{ popup->Destroy\(\); \}\);/);
}

function checkInkRows(platerSource, factoriesSource) {
    assert.match(platerSource, /const int slot = row_combo->get_filament_idx\(\) \+ 1;\s*mine\.id\s*= ElementLocks::ink_row_lock_id\(slot\);/,
                 'the identity follows the row number');
    for (const target of ['combobox', 'combobox->clr_picker', 'diameter_btns'])
        assert.match(platerSource, new RegExp(`ElementLocks::set_lock_identity\\(${target.replace(/[>-]/g, (c) => '\\' + c)}, identity\\);`));
    const del = code(body(platerSource, 'void Sidebar::delete_filament(size_t filament_id, int replace_filament_id)'));
    assert.match(del, /ElementLocks::ink_row_lock_id\(int\(filament_id\) \+ 1\); ElementLocks::is_locked\(lock\)\) \{[\s\S]*?return;/);
    const menu = code(body(factoriesSource, 'void MenuFactory::create_filament_action_menu('));
    assert.match(menu, /delete_item->Enable\(!row_locked &&/);
    assert.match(menu, /merge_item->Enable\(!row_locked &&/);
    assert.match(code(body(factoriesSource, 'wxMenu *MenuFactory::filament_action_menu(')),
                 /ElementLocks::append_lock_item\(\*m_filament_popup_menu, ElementLocks::ink_row_lock_id\(slot\), row\);/);
}

function checkObjects(listSource, modelSource, platerSource, arrangeSource) {
    assert.match(code(body(listSource, 'void ObjectList::selection_changed()')), /if \(m_prevent_list_events\) return;\s*drop_locked_from_selection\(\);/);
    assert.match(code(body(listSource, 'void ObjectList::update_selections()')), /^void ObjectList::update_selections\(\)\s*\{\s*drop_locked_from_scene\(\);/);
    assert.match(code(body(listSource, 'void ObjectList::drop_locked_from_scene()')), /for \(int obj_idx : locked\) selection\.remove_object\(unsigned\(obj_idx\)\);/);
    assert.match(code(body(listSource, 'bool ObjectList::drop_locked_from_selection()')), /if \(!locked_row\(item\)\) continue;\s*Unselect\(item\);/);
    assert.match(listSource, /ElementLocks::set_row_lookup\(this, \[this\]\(const wxPoint &screen\)/);
    assert.match(code(body(listSource, 'void ObjectList::show_context_menu(')), /offer_lock\(menu\);/);
    assert.match(platerSource, /sidebar->obj_list\(\)->offer_lock\(menu\);\s*show_right_click_menu\(evt\.data\.first, menu\);/);
    assert.match(platerSource, /ElementLocks::set_row_lookup\(view3D_canvas,[\s\S]*?gizmo->get_hover_id\(\) != -1\) return row;/,
                 'a gizmo handle under the mouse is never taken for the object');
    assert.match(modelSource, /if \(m_locked_rows && m_locked_rows\(item\)\)\s*variant << DataViewBitmapText\(node->m_name, locked_icon\(node->m_bmp\)\);/);
    assert.match(code(body(platerSource, 'void Plater::delete_all_objects_from_model()')),
                 /if \(obj && ElementLocks::object_locked\(obj->name\)\) \{[\s\S]*?return;\s*\}\s*if \(!p->model\.objects\.empty\(\)\)/);
    assert.match(code(body(platerSource, 'void Plater::priv::remove_curr_plate_all()')), /ElementLocks::object_locked\(model\.objects\[obj_idx\]->name\)[\s\S]*?return;/);
    const all = code(body(arrangeSource, 'void ArrangeJob::prepare_all()'));
    assert.match(all, /plate_list\.preprocess_arrange_polygon\(oidx, i, ap, !object_locked\);/);
    assert.match(all, /!mo->instances\[i\]->printable \? m_unprintable : object_locked \? m_unselected : m_selected;/);
    assert.match(code(body(arrangeSource, 'void ArrangeJob::prepare_partplate()')), /ElementLocks::object_locked\(model\.objects\[oidx\]->name\)[\s\S]*?return;/);
}

test('the filter stops clicks on locked rows and lets drags end', () => checkFilter(service));
test('badges follow locked elements and notes clean up', () => checkBadges(service));
test('ink rows are locked one by one', () => checkInkRows(plater, factories));
test('a locked object stays out of the selection, arrange and Delete all', () => checkObjects(list, model, plater, arrange));

test('each checker fails on a broken copy', () => {
    assert.throws(() => checkFilter(service.replace('if (held != window) {', 'if (false) {')));
    assert.throws(() => checkBadges(service.replace('if (done) changed();', '(void) done;')));
    assert.throws(() => checkInkRows(plater.replace('row_combo->get_filament_idx() + 1', '1'), factories));
    assert.throws(() => checkInkRows(plater, factories.replace('delete_item->Enable(!row_locked &&', 'delete_item->Enable(')));
    assert.throws(() => checkObjects(list.replace('drop_locked_from_scene();\n    const Selection& selection', 'const Selection& selection'), model, plater, arrange));
    assert.throws(() => checkObjects(list, model, plater, arrange.replace('object_locked ? m_unselected : m_selected', 'm_selected')));
});
