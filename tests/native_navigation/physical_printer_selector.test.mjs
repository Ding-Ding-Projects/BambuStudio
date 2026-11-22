import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';

const read = name => readFileSync(new URL('../../src/slic3r/GUI/' + name, import.meta.url), 'utf8').replaceAll('\r\n', '\n');
const frame = read('MainFrame.cpp');
const selector = frame.slice(frame.indexOf('void MainFrame::show_printer_selector'),
    frame.indexOf('void MainFrame::jump_to_monitor'));
const dialog = read('SelectMachine.cpp');

test('the shared navigation action opens selection without changing the workspace or sending a job', () => {
    assert.match(frame, /AddNavigationAction\(_L\("Select Printer"\), "printer",[\s\S]*?show_printer_selector\(anchor\)/);
    assert.match(selector, /MD3::PopupMenuBelow\(anchor, &menu, true\)/);
    assert.doesNotMatch(selector, /jump_to_monitor|SetSelection|PrintJob|send_gcode/);
});

test('selection is keyed by device ID and connectivity is rechecked after the menu opens', () => {
    assert.match(selector, /manager->get_my_machine_list\(\)/);
    assert.match(selector, /selected->get_dev_id\(\) == id/);
    assert.match(selector, /item->Enable\(machine->is_online\(\) \|\| machine->is_connected\(\)\)/);
    const handler = selector.slice(selector.indexOf('menu.Bind(wxEVT_MENU'));
    assert.doesNotMatch(selector.slice(0, selector.indexOf('menu.Bind(wxEVT_MENU')), /->select_machine\(/);
    assert.match(handler, /get_my_machine\(device_id\)[\s\S]*?!device->is_online\(\) && !device->is_connected\(\)[\s\S]*?return;[\s\S]*?select_machine\(device_id\)/);
});

test('normal Print setup keeps the current physical device ahead of a matching preset', () => {
    const collect = dialog.slice(dialog.indexOf('static void\n_collect_sorted_machines'),
        dialog.indexOf('std::string SelectMachineDialog::get_source_printer_model'));
    assert.match(collect, /get_selected_machine\(\)[\s\S]+_collect_sorted_objs\(cur_selected_obj_list[\s\S]+_collect_sorted_objs\(match_avaliable_list/);
    const refresh = dialog.slice(dialog.indexOf('void SelectMachineDialog::update_user_printer()'));
    assert.match(refresh, /int prefer_idx = 0;[\s\S]*?FROM_NORMAL && m_printer_last_select.empty\(\) && !dev->get_selected_machine\(\)\) \{\s*prefer_idx = find_preset_matched_index\(\)/);
    assert.match(refresh, /m_printer_last_select = m_list\[prefer_idx\]->get_dev_id\(\)/);
});
