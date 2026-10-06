import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const header = path.join(root, 'src/slic3r/GUI/PrepareInspectorLayout.hpp');
const source = String.raw`
#include "PrepareInspectorLayout.hpp"
#include <cstdio>
using Slic3r::GUI::PrepareInspectorLayout::row_height;
int main() {
    int checks = 0;
    auto check = [&checks](bool value, const char* message) {
        ++checks;
        if (!value) { std::fprintf(stderr, "FAILED: %s\n", message); return false; }
        return true;
    };
    // Device-pixel measurements are supplied by wx at the call sites. These
    // deliberately different text extents model a short label, a larger font,
    // and a two-line bilingual label without pretending to measure a native font.
    for (int percent : {100, 125, 150, 200}) {
        auto dip = [percent](int value) { return (value * percent + 50) / 100; };
        for (int floor_dip : {32, 40}) {
            const int floor = dip(floor_dip), pad = dip(4);
            if (!check(row_height(floor, 0, 0, 0) == floor, "density floor retained")) return 1;
            for (int text_dip : {17, 25, 48, 96}) {
                const int text = dip(text_dip), icon = dip(24);
                const int height = row_height(floor, text, icon, pad);
                if (!check(height >= floor, "density remains a minimum")) return 1;
                if (!check(height >= text + 2 * pad, "measured label has breathing room")) return 1;
                if (!check(height >= icon + 2 * pad, "existing icon fits")) return 1;
                if (!check(row_height(floor, text + dip(1), icon, pad) >= height,
                           "larger measured labels never shrink the row")) return 1;
            }
            // A scope switch can exceed the title height. Its measured best
            // height must win independently of the title and density floor.
            if (!check(row_height(floor, dip(20), dip(72), 0) == dip(72),
                       "existing control best height wins")) return 1;
            if (!check(row_height(floor, dip(72), dip(20), 0) == dip(72),
                       "multiline heading best height wins")) return 1;
        }
    }
    std::printf("%d inspector geometry assertions passed\n", checks);
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

test('inspector row sizing preserves measured content across density and DPI boundaries', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'prepare-inspector-layout-'));
  try {
    const current = readFileSync(header, 'utf8');
    const setup = compiler();
    const capped = current.replace('return density_minimum > padded ? density_minimum : padded;', 'return density_minimum;');
    assert.notEqual(capped, current);
    const cappedResult = compile(directory, setup, capped);
    assert.notEqual(cappedResult.status, 0, 'fixed row heights must fail with large measured labels');
    assert.match(cappedResult.stderr, /measured label has breathing room/);
    const noFloor = current.replace('return density_minimum > padded ? density_minimum : padded;', 'return padded;');
    assert.notEqual(noFloor, current);
    const noFloorResult = compile(directory, setup, noFloor);
    assert.notEqual(noFloorResult.status, 0, 'content-only heights must fail the density floor');
    assert.match(noFloorResult.stderr, /density floor retained/);
    const result = compile(directory, setup, current);
    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /152 inspector geometry assertions passed/);
    console.log(result.stdout.trim());
  } finally {
    assert.equal(path.dirname(path.resolve(directory)), path.resolve(tmpdir()));
    assert.ok(path.basename(directory).startsWith('prepare-inspector-layout-'));
    rmSync(directory, { recursive: true, force: true });
  }
});
