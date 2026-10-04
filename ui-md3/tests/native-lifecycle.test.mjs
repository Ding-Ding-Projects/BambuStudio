import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
const root = new URL('../../', import.meta.url);
const read = path => readFile(new URL(path, root), 'utf8');
const menu = await read('src/slic3r/GUI/Widgets/MD3Menu.cpp');
const dropdown = await read('src/slic3r/GUI/Widgets/DropDown.cpp');
const combo = await read('src/slic3r/GUI/Widgets/ComboBox.cpp');
const plater = await read('src/slic3r/GUI/Plater.cpp');
const part = (s, start, end) => s.slice(s.indexOf(start), s.indexOf(end, s.indexOf(start)));
function menuContract(source) {
  const run = part(source, 'int run_blocking(', 'wxRect point_anchor(');
  assert.ok(run.includes('popup->SetSendEvents(false);'));
  assert.ok(run.indexOf('loop.Run();') < run.indexOf('source->SendEvent('));
  assert.ok(run.indexOf('popup_ref->SetCloseCallback({});') < run.indexOf('popup_ref->Destroy();'));
  assert.ok(run.indexOf('popup_ref->Destroy();') < run.indexOf('source->SendEvent('));
  assert.ok(run.includes('wxWeakRef<wxMenu> source;'));
  assert.ok(run.includes('AppearanceEditor::open_for(appearance_anchor.get(), appearance_element)'));
}
function dropdownContract(source) {
  const dispatch = part(source, 'void DropDown::sendDropDownEvent()', 'void DropDown::Dismiss()');
  assert.ok(dispatch.indexOf('event.SetString(items[index].text);') < dispatch.indexOf('DismissAndNotify();'));
  assert.ok(dispatch.lastIndexOf('DismissAndNotify();') < dispatch.indexOf('ProcessEvent(event);'));
  assert.ok(dispatch.includes('wxWeakRef<DropDown> target(root);'));
  assert.ok(dispatch.includes('event.SetExtraLong(static_cast<long>(root->item_revision));'));
}
test('blocking menu completes teardown before dispatch', () => menuContract(menu));
test('menu contract fails when popup-stack dispatch returns', () => assert.throws(() => menuContract(menu.replace('popup->SetSendEvents(false);', 'popup->SetSendEvents(true);'))));
test('dropdown snapshots before close and dispatches after close', () => dropdownContract(dropdown));
test('dropdown contract rejects loss of surviving target', () => assert.throws(() => dropdownContract(dropdown.replace('wxWeakRef<DropDown> target(root);', 'auto target = root;'))));
test('combo rejects rebuilt item generations', () => assert.ok(combo.includes('static_cast<unsigned long>(e.GetExtraLong()) != drop.item_revision')));
test('printer card opens explicitly and canceled switch stops plate changes', () => {
  assert.equal((plater.match(/combo_printer->OpenDropDown\(p->panel_printer_preset\)/g) ?? []).length, 3);
  assert.ok(plater.includes('if (selection_applied && !*selection_applied) return;'));
  assert.ok(plater.includes('if (!selection_applied) return;'));
});
test('filament menu reads live config slot and deletion uses confirmed index', () => {
  assert.ok(plater.includes('const int config_slot = menu_combo->get_filament_idx();'));
  const deletion = part(plater, 'void Sidebar::delete_filament_with_confirm(', 'void Sidebar::delete_mixed_filament_at(');
  assert.ok(deletion.includes('delete_filament(resolved);'));
  assert.ok(deletion.includes('if (presets != wxGetApp().preset_bundle->filament_presets) return;'));
});
