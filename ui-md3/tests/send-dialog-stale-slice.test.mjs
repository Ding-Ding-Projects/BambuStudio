import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Send print job dialog sometimes said "Cannot send the print job for
// empty plate" for a plate with a model on it, on a two-nozzle printer
// without a filament switcher. Opening the dialog checks a nozzle move for
// every material (Plater::validate_print_setup_filament_maps). The check wrote
// the proposed maps to the plate and undid them through
// PartPlate::set_filament_map_mode(), which erases filament_map,
// filament_nozzle_map and filament_volume_map whenever the effective mode
// changes. The maps the slice had written back were lost, the next background
// apply saw a changed configuration and discarded the slice, and the dialog
// read "no valid G-code" as "empty plate". The apply came whenever some
// timer or edit ran it; one source was a catch-all timer handler on the
// Plater that swallowed the project-history timers' ticks.
//
// These contracts pin the fix in the sources, so they run without a build.
// tests/send_print_readiness_test.cpp executes the decisions themselves.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const gui = (name) => path.join(root, 'src', 'slic3r', 'GUI', name);
const stripComments = (text) => text
  .replace(/\r\n/g, '\n')
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^[ \t]*\/\/.*$/gm, '')
  .replace(/[ \t]+\/\/[^"\n]*$/gm, '');
const files = ['Plater.cpp', 'SelectMachine.cpp', 'SelectMachine.hpp', 'PrePrintChecker.hpp', 'PrePrintChecker.cpp'];
const sources = Object.fromEntries(files.map((name) => [name, stripComments(readFileSync(gui(name), 'utf8'))]));

// Brace-balanced body that starts at the first `{` after `signature`.
const block = (code, signature) => {
  const start = code.indexOf(signature);
  assert.notEqual(start, -1, `missing ${signature}`);
  const open = code.indexOf('{', start + signature.length);
  let depth = 0;
  for (let i = open; i < code.length; ++i) {
    if (code[i] === '{') ++depth;
    else if (code[i] === '}' && --depth === 0) return code.slice(open, i + 1);
  }
  assert.fail(`unterminated ${signature}`);
};

// The top-level arguments of the call whose `(` is at `open`.
const callArguments = (code, open) => {
  const args = [];
  let depth = 0, from = open + 1;
  for (let i = open; i < code.length; ++i) {
    const c = code[i];
    if ('([{'.includes(c)) ++depth;
    else if (')]}'.includes(c)) {
      if (--depth === 0) { args.push(code.slice(from, i).trim()); return args; }
    } else if (c === ',' && depth === 1) { args.push(code.slice(from, i).trim()); from = i + 1; }
  }
  assert.fail('unterminated call');
};

const between = (code, from, to) => {
  const a = code.indexOf(from);
  const b = code.indexOf(to, a);
  assert.ok(a >= 0 && b > a, `${from} ... ${to} must exist in that order`);
  return code.slice(a, b);
};

const order = (code, labels) => {
  let at = -1;
  for (const label of labels) {
    const next = code.indexOf(label, at + 1);
    assert.ok(next > at, `${label} must follow ${labels[labels.indexOf(label) - 1] ?? 'the start'}`);
    at = next;
  }
};

const checks = {
  pureValidation(src) {
    const body = block(src['Plater.cpp'], 'bool Plater::validate_print_setup_filament_maps(');
    const snapshot = body.match(/ConfigKeySnapshot<DynamicPrintConfig, ConfigOption>\s+(\w+)\(\s*plate_config,\s*\{([^}]*)\}\s*\)/);
    assert.ok(snapshot, 'the check snapshots the plate keys it writes');
    for (const key of ['filament_map_mode', 'filament_map', 'filament_nozzle_map', 'filament_volume_map'])
      assert.ok(snapshot[2].includes(`"${key}"`), `the snapshot covers ${key}`);
    assert.match(body, new RegExp(`auto restore = \\[&\\] \\{\\s*${snapshot[1]}\\.restore\\(plate_config\\);`), 'restore puts the snapshot back');
    assert.doesNotMatch(body, /set_filament_map_mode\s*\(/, 'the check never goes through the clearing mode setter');
    assert.doesNotMatch(body, /clear_filament_(?:map|map_mode|nozzle_map|volume_map)\s*\(/, 'the check never clears a map');
    assert.match(body, /plate_config\.option<ConfigOptionEnum<FilamentMapMode>>\("filament_map_mode", true\)->value = fmmManual;/,
      'the proposed mode is written directly');
    assert.match(body, /catch \(\.\.\.\) \{\s*restore\(\);/, 'a failed check restores the plate');
    assert.ok((body.match(/\brestore\(\);/g) || []).length >= 2, 'the plate is restored on both paths');
    assert.match(body, /quick_swap_volume_types\(/, 'nozzle capacity is checked for the types Swap stores');
  },

  swapStoresCheckedTypes(src) {
    const body = block(src['Plater.cpp'], 'bool Plater::apply_print_setup_filament_maps(');
    order(body, ['get_real_filament_volume_maps(', 'validate_print_setup_filament_maps(', 'quick_swap_volume_types(',
      'set_filament_map_mode(fmmManual)', 'set_filament_volume_maps(volumes)']);
  },

  backgroundTimerOwnId(src) {
    const code = src['Plater.cpp'];
    let count = 0;
    for (const match of code.matchAll(/\bq->Bind\(\s*wxEVT_TIMER\b/g)) {
      const open = code.indexOf('(', match.index);
      const args = callArguments(code, open);
      const isLambda = args[1].startsWith('[');
      assert.ok(args.length >= (isLambda ? 3 : 4), `a Plater timer handler must name its timer: ${args.slice(0, 2).join(', ')}`);
      if (args[1].includes('update_restart_background_process')) {
        assert.equal(args[2], 'this->background_process_timer.GetId()', 'the background-process handler takes only its own timer');
        ++count;
      }
    }
    assert.equal(count, 1, 'one background-process timer handler');
    order(code, ['this->background_process_timer.SetOwner(this->q, 0);', 'this->background_process_timer.GetId());']);
  },

  truthfulPlateStatus(src) {
    const dialog = src['SelectMachine.cpp'];
    const status = block(dialog, 'void SelectMachineDialog::update_show_status(');
    const gate = between(status, 'if (m_print_type == PrintFromType::FROM_NORMAL) {', 'if (!obj_->is_lan_mode_printer() && !agent->is_server_connected())');
    assert.doesNotMatch(gate, /get_curr_plate\(\)/, 'the plate check uses the plate being sent');
    assert.match(gate, /send_plate_readiness\(\)/);
    assert.match(gate, /SendReadiness::EmptyPlate\) \{\s*show_status\(PrintDialogStatus::PrintStatusBlankPlate\);/, 'only an empty plate is called empty');
    assert.match(gate, /SendReadiness::NeedsSlice\)[\s\S]*show_status\(PrintDialogStatus::PrintStatusPlateNeedsSlicing, params\);/,
      'a stale slice says it must be sliced again');
    assert.match(gate, /_L\("Plate %d needs to be sliced again before it can be sent\."\)/, 'Print all names the plate');

    const readiness = block(dialog, 'SendReadinessVerdict SelectMachineDialog::send_plate_readiness() const');
    assert.match(readiness, /if \(m_print_plate_idx == PLATE_ALL_IDX\) \{\s*for \(int index = 0; index < plates\.get_plate_count\(\); \+\+index\)/,
      'Print all checks every plate');
    assert.match(readiness, /plates\.get_plate\(m_print_plate_idx\)/, 'Print plate checks the plate it sends');
    assert.match(readiness, /has_printable_instances\(\), plate->is_valid_gcode_file\(\)/);
    assert.match(readiness, /return classify_send_plates\(facts\);/);

    const header = src['PrePrintChecker.hpp'];
    order(header, ['PrintStatusPrinterErrorBegin', 'PrintStatusBlankPlate,', 'PrintStatusPlateNeedsSlicing,', 'PrintStatusPrinterErrorEnd']);
    order(header, ['PrintStatusFilamentWarningBegin', 'PrintStatusQuickSwapPending,', 'PrintStatusFilamentWarningEnd']);
    const messages = src['PrePrintChecker.cpp'];
    assert.match(messages, /case PrintStatusPlateNeedsSlicing: return _L\("This plate needs to be sliced again before it can be sent\."\);/);
    assert.match(messages, /case PrintStatusQuickSwapPending: return _L\("Apply the nozzle moves with Swap, or undo them, before sending\."\);/);
    assert.match(messages, /case PrintStatusPlateNeedsSlicing: return "PrintStatusPlateNeedsSlicing";/);
    assert.match(messages, /case PrintStatusQuickSwapPending: return "PrintStatusQuickSwapPending";/);

    const show = block(dialog, 'void SelectMachineDialog::show_status(');
    assert.match(show, /status == PrintDialogStatus::PrintStatusPlateNeedsSlicing\) \{\s*Enable_Refresh_Button\(true\);\s*Enable_Send_Button\(false\);/);
    assert.match(show, /PrintStatusPlateNeedsSlicing && can_slice_again_from_dialog\(\)\) \{\s*m_pre_print_checker\.add_with_link\([\s\S]*?_L\("Slice again"\), \[this\]\(\) \{ slice_again_from_dialog\(\); \}/,
      'the stale-slice message offers Slice again');

    const can = block(dialog, 'bool SelectMachineDialog::can_slice_again_from_dialog() const');
    assert.match(can, /m_print_plate_idx == m_plater->get_partplate_list\(\)\.get_curr_plate_index\(\)/,
      'Slice again is offered only for the plate the Slice and print action slices');
    const slice = block(dialog, 'void SelectMachineDialog::slice_again_from_dialog()');
    order(slice, ['can_slice_again_from_dialog()', 'EndModal(wxID_CLOSE);', 'request_slice_and_print()', 'MessageDialog(']);
  },

  sendNeverSilent(src) {
    const dialog = src['SelectMachine.cpp'];
    const ok = block(dialog, 'void SelectMachineDialog::on_ok_btn(');
    assert.match(ok, /^\{\s*if \(quick_swap_pending\(\)\) \{\s*Enable_Send_Button\(false\);\s*add_quick_swap_pending_notice\(\);\s*update_print_status_msg\(\);\s*return;\s*\}/,
      'a click during a pending move is refused with its reason shown');
    assert.doesNotMatch(ok, /m_pending_filaments_map != m_filaments_map\)\s*return;/, 'no silent return');

    const enable = block(dialog, 'void SelectMachineDialog::Enable_Send_Button(bool en)');
    assert.match(enable, /const bool swap_pending = quick_swap_pending\(\);\s*if \(swap_pending\)\s*en = false;/);
    assert.match(enable, /get_pre_state_msg\(PrintDialogStatus::PrintStatusQuickSwapPending\)/, 'the disabled Send button carries the reason');
    assert.match(enable, /SetToolTip\(send_tip\)/);

    const notice = block(dialog, 'void SelectMachineDialog::add_quick_swap_pending_notice()');
    order(notice, ['if (!quick_swap_pending())', 'm_pre_print_checker.add(PrintDialogStatus::PrintStatusQuickSwapPending', 'Enable_Send_Button(false);']);
    const pending = block(dialog, 'bool SelectMachineDialog::quick_swap_pending() const');
    assert.match(pending, /m_print_type == FROM_NORMAL && Slic3r::GUI::quick_swap_pending\(m_pending_filaments_map, m_filaments_map\)/);

    const tick = block(dialog, 'void SelectMachineDialog::update_by_obj(');
    order(tick, ['update_show_status(obj_);', 'add_quick_swap_pending_notice();', 'update_print_status_msg();']);

    const refresh = block(dialog, 'void SelectMachineDialog::refresh_quick_swap_controls()');
    assert.match(refresh, /CallAfter\(\[this\] \{\s*refresh_quick_swap_controls\(\);\s*update_by_obj\(get_current_machine\(\)\);\s*\}\);/,
      'a move or its undo re-checks Send at once');
    assert.match(refresh, /const bool changed = quick_swap_pending\(\);/);
  },

  timelapseRow(src) {
    const dialog = src['SelectMachine.cpp'];
    const ctor = block(dialog, 'PrintOption::PrintOption(wxWindow *parent');
    assert.doesNotMatch(ctor, /FromDIP\(28\)/, 'the row is no longer a fixed 28 DIP');
    order(ctor, ['m_printoption_item = new PrintOptionItem(', 'update_row_height();', 'SetSizer(sizer);']);
    const height = block(dialog, 'void PrintOption::update_row_height()');
    assert.match(height, /std::max\(FromDIP\(28\), m_printoption_item \? m_printoption_item->GetMinSize\(\)\.GetHeight\(\) : 0\)/,
      'the row is never lower than its switch');
    assert.match(height, /SetMinSize\(wxSize\(-1, row_height\)\);\s*SetMaxSize\(wxSize\(-1, row_height\)\);/);
    order(block(dialog, 'void PrintOption::msw_rescale()'), ['m_printoption_item->msw_rescale();', 'update_row_height();', 'Layout();']);
    assert.match(block(dialog, 'void PrintOptionItem::msw_rescale()'), /update_size\(\);/, 'a rescale resizes the switch');

    const header = src['SelectMachine.hpp'];
    assert.match(block(header, 'void        update_options(std::vector<POItem> ops)'), /update_size\(\);/);
    assert.match(block(header, 'void update_size()'), /auto height = FromDIP\(22\) \+ FromDIP\(8\);\s*SetMinSize\(wxSize\(width, height\)\);/);

    assert.doesNotMatch(dialog, /m_timelapse_folder_btn->SetBackgroundColour\(ThemeColor::White\)/, 'no hard-coded white behind the folder button');
    assert.equal((dialog.match(/m_timelapse_folder_btn->SetBackgroundColour\(StateColor::semantic\(MD3::Role::SurfaceContainerLowest\)\)/g) || []).length, 2,
      'the folder button takes the row surface when built and when the theme changes');
    assert.match(block(dialog, 'void PrintOption::insert_extra_widget('), /widget->Reparent\(this\);\s*widget->SetBackgroundColour\(GetBackgroundColour\(\)\);/);
    assert.match(block(dialog, 'void SelectMachineDialog::on_dpi_changed('), /m_timelapse_folder_btn->msw_rescale\(\);\s*update_timelapse_folder_btn_icon\(\);/);
    assert.match(block(dialog, 'void SelectMachineDialog::sys_color_changed()'), /update_timelapse_folder_btn_icon\(\);/);
  },
};

for (const [name, check] of Object.entries(checks))
  test(name, () => check(sources));

// Each contract must reject the defect it pins.
test('each contract rejects the defect it pins', () => {
  const mutations = [
    ['pureValidation', 'Plater.cpp', 'plate_config.option<ConfigOptionEnum<FilamentMapMode>>("filament_map_mode", true)->value = fmmManual;', 'plate->set_filament_map_mode(fmmManual);'],
    ['pureValidation', 'Plater.cpp', '"filament_nozzle_map", "filament_volume_map"}', '"filament_nozzle_map"}'],
    ['swapStoresCheckedTypes', 'Plater.cpp', '    const auto real_volumes = plate->get_real_filament_volume_maps(project_config);\n', ''],
    ['backgroundTimerOwnId', 'Plater.cpp', '}, this->background_process_timer.GetId());', '});'],
    ['truthfulPlateStatus', 'SelectMachine.cpp', 'const SendReadinessVerdict verdict = send_plate_readiness();',
      'PartPlate* plate = m_plater->get_partplate_list().get_curr_plate(); const SendReadinessVerdict verdict = send_plate_readiness();'],
    ['truthfulPlateStatus', 'SelectMachine.cpp', 'for (int index = 0; index < plates.get_plate_count(); ++index)', 'for (int index = plates.get_curr_plate_index(); index <= plates.get_curr_plate_index(); ++index)'],
    ['sendNeverSilent', 'SelectMachine.cpp', '        add_quick_swap_pending_notice();\n        update_print_status_msg();\n        return;', '        return;'],
    ['sendNeverSilent', 'SelectMachine.cpp', '                update_by_obj(get_current_machine());\n', ''],
    ['timelapseRow', 'SelectMachine.cpp', '    update_row_height();\n    update_title_display();', '    update_title_display();'],
    ['timelapseRow', 'SelectMachine.cpp', 'm_timelapse_folder_btn->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));\n    m_timelapse_folder_btn->SetToolTip',
      'm_timelapse_folder_btn->SetBackgroundColour(ThemeColor::White);\n    m_timelapse_folder_btn->SetToolTip'],
  ];
  for (const [name, file, from, to] of mutations) {
    assert.ok(sources[file].includes(from), `mutation source for ${name} must exist: ${from}`);
    const mutated = { ...sources, [file]: sources[file].replace(from, to) };
    assert.throws(() => checks[name](mutated), `${name} must reject: ${from.trim().slice(0, 60)}`);
  }
});
