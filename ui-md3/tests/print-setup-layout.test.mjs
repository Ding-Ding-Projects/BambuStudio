import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const header = path.join(root, 'src/slic3r/GUI/PrintSetupLayout.hpp');
const source = String.raw`
#include "PrintSetupLayout.hpp"
#include <climits>
#include <cstdio>
using Slic3r::GUI::PrintSetupLayout::bounded_body_height;
int main() {
    int checks = 0;
    auto check = [&checks](bool value, const char* message) {
        ++checks;
        if (!value) { std::fprintf(stderr, "FAILED: %s\n", message); return false; }
        return true;
    };
    if (!check(bounded_body_height(300, 650, 800, 100, 12) == 300, "short content stays natural")) return 1;
    if (!check(bounded_body_height(1000, 650, 800, 100, 12) == 650, "preferred body cap")) return 1;
    if (!check(bounded_body_height(1000, 650, 600, 100, 12) == 476, "measured footer remains reachable")) return 1;
    if (!check(bounded_body_height(1000, 650, 120, 100, 12) == 0, "no impossible positive minimum")) return 1;
    if (!check(bounded_body_height(1000, 650, -1, 100, 12) == 0, "invalid display stays bounded")) return 1;
    if (!check(bounded_body_height(-1, 650, 800, 100, 12) == 0, "nonpositive content")) return 1;
    if (!check(bounded_body_height(1000, -1, 800, 100, 12) == 0, "nonpositive preference")) return 1;
    if (!check(bounded_body_height(INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX) == 0, "reservation cannot overflow")) return 1;
    if (!check(bounded_body_height(INT_MAX, INT_MAX, INT_MAX, 0, 0) == INT_MAX, "large positive range retained")) return 1;
    if (!check(bounded_body_height(500, 650, 800, -1, -1) == 500, "negative reserves do not create space")) return 1;
    for (int percent : {100, 125, 150, 200}) {
        auto dip = [percent](int value) { return (value * percent + 50) / 100; };
        for (int work_dip : {480, 600, 800}) {
            for (int content_dip : {200, 1000}) {
                const int work = dip(work_dip), chrome = dip(140), margin = dip(12);
                const int height = bounded_body_height(dip(content_dip), dip(650), work, chrome, margin);
                if (!check(height >= 0 && height <= work - chrome - 2 * margin, "display boundary at scale")) return 1;
                if (!check(height <= dip(content_dip), "no empty expansion")) return 1;
                if (!check(height <= dip(650), "preferred cap at scale")) return 1;
                if (!check(bounded_body_height(dip(content_dip), dip(650), work, chrome + dip(80), margin) <= height,
                           "progress or diagnostics reserves more space")) return 1;
            }
        }
    }
    std::printf("%d print setup geometry assertions passed\n", checks);
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

test('print setup reserves actual chrome on short displays and at every supported scale', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'print-setup-layout-'));
  try {
    const current = readFileSync(header, 'utf8');
    const setup = compiler();
    const broken = current.replace('return available < wanted ? static_cast<int>(available) : wanted;', 'return wanted;');
    assert.notEqual(broken, current);
    const negative = compile(directory, setup, broken);
    assert.notEqual(negative.status, 0, 'ignoring the actual display must fail');
    assert.match(negative.stderr, /measured footer remains reachable/);
    const result = compile(directory, setup, current);
    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /106 print setup geometry assertions passed/);
    console.log(result.stdout.trim());
  } finally {
    assert.equal(path.dirname(path.resolve(directory)), path.resolve(tmpdir()));
    assert.ok(path.basename(directory).startsWith('print-setup-layout-'));
    rmSync(directory, { recursive: true, force: true });
  }
});
