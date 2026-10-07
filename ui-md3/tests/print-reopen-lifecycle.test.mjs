import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const header = path.join(root, 'src/slic3r/GUI/PrintSetupLayout.hpp');
const gui = readFileSync(path.join(root, 'src/slic3r/GUI/SelectMachine.cpp'), 'utf8');
const signature = 'bool SelectMachineDialog::Show(bool show)';
const start = gui.indexOf(signature);
const end = gui.indexOf('SelectMachineDialog::~SelectMachineDialog()', start);
assert.ok(start >= 0 && end > start, 'the actual Show definition must be present');
const showDefinition = gui.slice(start, end);

// Compile the unchanged production Show body. Only its wx/device collaborators
// are replaced by observable doubles; this is a lifecycle test, not a native UI.
const source = String.raw`
#include "PrintSetupLayout.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
static std::vector<std::string> events;
constexpr int LIST_REFRESH_INTERVAL = 200;
enum BedType { btDefault };
using t_config_enum_values = std::map<std::string, BedType>;
template<class T> struct ConfigOptionEnum {
    static const t_config_enum_values& get_enum_values() {
        static const t_config_enum_values values{{"default", btDefault}};
        return values;
    }
};
struct DeviceManager {
    int selected = 42;
    int get_selected_machine() { return selected; }
    void load_last_machine() { events.emplace_back("load-machine"); }
};
struct App {
    DeviceManager device;
    DeviceManager* getDeviceManager() { return &device; }
    void UpdateDlgDarkUI(void*) { events.emplace_back("theme"); }
    void reset_to_active() { events.emplace_back("active"); }
};
namespace Slic3r::GUI { inline App& wxGetApp() { static App app; return app; } }
using Slic3r::GUI::wxGetApp;
struct Panel { void Show() { events.emplace_back("options"); } };
struct Timer {
    void Start(int) { events.emplace_back("timer-start"); }
    void Stop() { events.emplace_back("timer-stop"); }
};
struct PartPlate { BedType get_bed_type(bool) { return btDefault; } };
struct PlateList { PartPlate plate; PartPlate* get_curr_plate() { return &plate; } };
struct Plater { PlateList plates; PlateList& get_partplate_list() { return plates; } };
struct PrinterBox { void UpdatePlate(const std::string&) { events.emplace_back("plate"); } };
enum class PrintDialogStatus { PrintStatusInit };
struct DPIDialog {
    bool shown = false;
    bool Show(bool value) { shown = value; events.emplace_back(value ? "base-show" : "base-hide"); return true; }
};
struct SelectMachineDialog : DPIDialog {
    Panel options;
    Timer timer;
    Plater plater;
    PrinterBox printer;
    Panel* m_options_other = &options;
    Timer* m_refresh_timer = &timer;
    Plater* m_plater = &plater;
    PrinterBox* m_printer_box = &printer;
    std::vector<int> m_ams_mapping_result;
    int parent_work_area = 800, dialog_work_area = 800;
    int body_height = 650, clamps = 0, submissions = 0;
    bool Show(bool show);
    void EnableEditing(bool) { events.emplace_back("editing"); }
    void show_status(PrintDialogStatus) { events.emplace_back("status-unchanged"); }
    void set_default() { events.emplace_back("defaults"); }
    void update_user_machine_list() { events.emplace_back("machines"); }
    int get_current_machine() { return 42; }
    void update_by_obj(int) { events.emplace_back("content"); }
    void Layout() { events.emplace_back("layout"); }
    void Fit() { events.emplace_back("fit"); }
    void CenterOnParent() { events.emplace_back("center"); }
    void update_scroll_area_size() {
        ++clamps;
        events.emplace_back("clamp");
        body_height = Slic3r::GUI::PrintSetupLayout::bounded_body_height(
            1000, 650, shown ? dialog_work_area : parent_work_area, 100, 12);
    }
};
` + showDefinition + String.raw`
int main() {
    int checks = 0;
    auto check = [&checks](bool value, const char* label) {
        ++checks;
        if (!value) { std::fprintf(stderr, "FAILED: %s\n", label); return false; }
        return true;
    };
    SelectMachineDialog dialog;
    if (!check(dialog.Show(true) && dialog.body_height == 650, "initial tall-display opening")) return 1;
    const int before_hide = dialog.clamps;
    events.clear();
    if (!check(dialog.Show(false), "hide result retained")) return 1;
    if (!check(events == std::vector<std::string>{"timer-stop", "base-hide"}, "hide returns before setup and geometry")) return 1;
    if (!check(dialog.clamps == before_hide, "hide never recalculates the body")) return 1;
    // Move only the parent to a shorter same-DPI display. No progress, status,
    // diagnostic, or DPI callback changes the retained dialog in this scenario.
    dialog.parent_work_area = 600;
    events.clear();
    if (!check(dialog.Show(true), "reopen result retained")) return 1;
    if (!check(dialog.body_height == 476, "cached reopen reserves the shorter parent display")) return 1;
    auto at = [](const char* event) { return std::find(events.begin(), events.end(), event); };
    if (!check(at("content") < at("layout") && at("layout") < at("fit") && at("fit") < at("clamp"), "clamp follows content and layout")) return 1;
    if (!check(at("clamp") < at("center") && at("center") < at("base-show"), "clamp precedes centering and native show")) return 1;
    if (!check(dialog.clamps == before_hide + 1, "one refresh on each reopen")) return 1;
    if (!check(wxGetApp().device.selected == 42 && dialog.submissions == 0, "display refresh retains device without submission")) return 1;
    std::printf("%d print reopen lifecycle assertions passed\n", checks);
}
`;

function compiler() {
  if (process.platform !== 'win32') return null;
  const vswhere = path.join(process.env['ProgramFiles(x86)'] ?? 'C:/Program Files (x86)',
    'Microsoft Visual Studio/Installer/vswhere.exe');
  assert.ok(existsSync(vswhere), 'Visual Studio discovery is required for this native test');
  const installation = execFileSync(vswhere, ['-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], { encoding: 'utf8' }).trim();
  const setup = path.join(installation, 'VC/Auxiliary/Build/vcvars64.bat');
  assert.ok(installation && existsSync(setup), 'A supported MSVC toolchain is required');
  return setup;
}

function compile(directory, setup, contents) {
  writeFileSync(path.join(directory, 'PrintSetupLayout.hpp'), contents);
  writeFileSync(path.join(directory, 'test.cpp'), '#include <initializer_list>\n' + source);
  if (setup) {
    writeFileSync(path.join(directory, 'compile.cmd'),
      `@echo off\r\ncall "${setup}" >nul\r\nif errorlevel 1 exit /b %errorlevel%\r\ncl /nologo /std:c++17 /EHsc /W4 test.cpp /Fe:test.exe /Fo:test.obj\r\nexit /b %errorlevel%\r\n`);
    execFileSync(process.env.ComSpec ?? 'cmd.exe', ['/d', '/c', 'compile.cmd'], { cwd: directory, stdio: 'pipe' });
  } else {
    execFileSync(process.env.CXX ?? 'c++', ['-std=c++17', '-Wall', '-Wextra', 'test.cpp', '-o', 'test'],
      { cwd: directory, stdio: 'pipe' });
  }
  return spawnSync(path.join(directory, setup ? 'test.exe' : 'test'), [], { cwd: directory, encoding: 'utf8' });
}

test('actual cached Show path refreshes geometry after a same-DPI display move', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'print-reopen-lifecycle-'));
  try {
    const result = compile(directory, compiler(), readFileSync(header, 'utf8'));
    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /10 print reopen lifecycle assertions passed/);
    console.log(result.stdout.trim());
  } finally {
    assert.equal(path.dirname(path.resolve(directory)), path.resolve(tmpdir()));
    assert.ok(path.basename(directory).startsWith('print-reopen-lifecycle-'));
    rmSync(directory, { recursive: true, force: true });
  }
});
