import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const header = path.join(root, 'src/slic3r/GUI/PrepareInspectorLayout.hpp');
const gui = readFileSync(path.join(root, 'src/slic3r/GUI/SelectMachinePop.cpp'), 'utf8');
const start = gui.indexOf('void EditDevNameDialog::fit_validation_content()');
const end = gui.indexOf('void EditDevNameDialog::on_dpi_changed(', start);
assert.ok(start >= 0 && end > start);
const productionFit = gui.slice(start, end);
function sourceFor(fit) {
  return String.raw`
#include "PrepareInspectorLayout.hpp"
#include <algorithm>
#include <cstdio>
struct wxSize {
    int x, y;
    wxSize(int width, int height) : x(width), y(height) {}
    bool operator!=(const wxSize& other) const { return x != other.x || y != other.y; }
};
struct Sizer {
    wxSize measured{312, 180};
    wxSize CalcMin() { return measured; }
};
struct EditDevNameDialog {
    bool m_fitting_content = false;
    Sizer sizer;
    bool has_sizer = true;
    wxSize current{312, 180};
    int resizes = 0, layouts = 0;
    Sizer* GetSizer() { return has_sizer ? &sizer : nullptr; }
    wxSize GetClientSize() { return current; }
    void SetClientSize(wxSize value) { current = value; ++resizes; fit_validation_content(); }
    void Layout() { ++layouts; fit_validation_content(); }
    void fit_validation_content();
};
` + fit + String.raw`
int main() {
    int checks = 0;
    auto check = [&checks](bool value, const char* label) {
        ++checks;
        if (!value) { std::fprintf(stderr, "FAILED: %s\n", label); return false; }
        return true;
    };
    for (int percent : {100, 125, 150, 200}) {
        auto dip = [percent](int value) { return (value * percent + 50) / 100; };
        for (int floor : {32, 40}) {
            EditDevNameDialog dialog;
            dialog.current = wxSize(dip(312), dip(180));
            dialog.sizer.measured = dialog.current;
            dialog.fit_validation_content();
            if (!check(dialog.resizes == 0 && dialog.layouts == 1, "stable form does not resize")) return 1;
            // Actual wx sizers supply these extents in production. A longer
            // bilingual footer and wrapped validation require both dimensions.
            dialog.sizer.measured = wxSize(dip(400), dip(320));
            dialog.fit_validation_content();
            if (!check(dialog.current.x == dip(400) && dialog.current.y == dip(320), "measured validation and footer fit")) return 1;
            if (!check(dialog.resizes == 1 && dialog.layouts == 2 && !dialog.m_fitting_content, "size callback reentry remains bounded")) return 1;
            dialog.sizer.measured = wxSize(dip(200), dip(120));
            dialog.fit_validation_content();
            if (!check(dialog.current.x == dip(400) && dialog.current.y == dip(320), "cleared message does not shrink the active dialog")) return 1;
            dialog.has_sizer = false;
            dialog.fit_validation_content();
            if (!check(dialog.layouts == 3, "missing sizer does not reenter layout")) return 1;
            const int field = Slic3r::GUI::PrepareInspectorLayout::row_height(dip(floor), dip(31), 0, dip(6));
            if (!check(field >= dip(floor) && field >= dip(31) + 2 * dip(6), "larger field font retains padding")) return 1;
        }
    }
    std::printf("%d device name geometry assertions passed\n", checks);
}
`;
}
let source = sourceFor(productionFit);

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
  writeFileSync(path.join(directory, 'PrepareInspectorLayout.hpp'), contents);
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

test('actual device-name layout grows for measured validation and bilingual footer content', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'device-name-layout-'));
  try {
    const setup = compiler();
    const contents = readFileSync(header, 'utf8');
    const broken = productionFit.replace('if (target != current) SetClientSize(target);', '(void)target;');
    assert.notEqual(broken, productionFit);
    source = sourceFor(broken);
    const negative = compile(directory, setup, contents);
    assert.notEqual(negative.status, 0);
    assert.match(negative.stderr, /measured validation and footer fit/);
    source = sourceFor(productionFit);
    const result = compile(directory, setup, contents);
    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /48 device name geometry assertions passed/);
    console.log(result.stdout.trim());
  } finally {
    assert.equal(path.dirname(path.resolve(directory)), path.resolve(tmpdir()));
    assert.ok(path.basename(directory).startsWith('device-name-layout-'));
    rmSync(directory, { recursive: true, force: true });
  }
});
