import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const header = path.join(root, 'src/slic3r/GUI/WorkflowPrintState.hpp');
const source = String.raw`
#include "WorkflowPrintState.hpp"
#include <cstdio>
using namespace Slic3r::GUI::WorkflowPrint;
int main() {
    int checks = 0;
    auto check = [&checks](bool value, const char* message) {
        ++checks;
        if (!value) { std::fprintf(stderr, "FAILED: %s\n", message); return false; }
        return true;
    };
    Availability state;
    int calls = 0, reads = 0;
    Action received = Action::Monitor;
    auto observe = [&]() { ++reads; return state; };
    auto run = [&](Action action) { ++calls; received = action; };
    if (!check(state.status() == Status::Empty, "empty review")) return 1;
    state.has_content = true;
    if (!check(state.status() == Status::NeedsSlice, "unsliced content")) return 1;
    state.slicing = true;
    if (!check(state.status() == Status::Slicing, "active slicing")) return 1;
    state.slice_ready = true;
    state.output_enabled = true;
    state.slice_enabled = true;
    state.combined_enabled = true;
    for (Action action : {Action::Output, Action::Slice, Action::SliceAndPrint, Action::SliceAndSend}) {
        if (!check(!dispatch(action, observe, run), "busy action rejected")) return 1;
    }
    if (!check(calls == 0 && reads == 4, "busy actions read without dispatch")) return 1;
    state.slicing = false;
    if (!check(state.status() == Status::Ready, "ready review")) return 1;
    for (Action action : {Action::Output, Action::Slice, Action::SliceAndPrint, Action::SliceAndSend}) {
        const int before = calls;
        if (!check(dispatch(action, observe, run), "explicit enabled action")) return 1;
        if (!check(calls == before + 1 && received == action, "one exact action dispatch")) return 1;
    }
    const Availability displayed = state;
    state.output_enabled = false;
    state.slice_enabled = false;
    state.combined_enabled = false;
    if (!check(displayed.allows(Action::Output), "previous plate displayed enabled")) return 1;
    const int before_stale = calls;
    for (Action action : {Action::Output, Action::Slice, Action::SliceAndPrint, Action::SliceAndSend}) {
        if (!check(!dispatch(action, observe, run), "fresh disabled state beats stale display")) return 1;
    }
    if (!check(calls == before_stale, "stale display cannot submit")) return 1;
    if (!check(state.status() == Status::Unavailable, "ready slice with unavailable output")) return 1;
    for (Action action : {Action::Prepare, Action::Preview, Action::Monitor, Action::OutputOptions}) {
        if (!check(dispatch(action, observe, run) && received == action, "recovery navigation remains available")) return 1;
    }
    const int before_observe = calls;
    for (int i = 0; i < 10; ++i) { (void)observe().status(); (void)observe().allows(Action::Output); }
    if (!check(calls == before_observe, "review refresh never executes an action")) return 1;
    if (!check(!dispatch(static_cast<Action>(999), observe, run), "unknown action rejected")) return 1;
    std::printf("%d workflow behavior assertions passed\n", checks);
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
  writeFileSync(path.join(directory, 'WorkflowPrintState.hpp'), contents);
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

test('native review state preserves action boundaries and rejects stale readiness', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'workflow-print-state-'));
  try {
    const current = readFileSync(header, 'utf8');
    const setup = compiler();
    const result = compile(directory, setup, current);
    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /workflow behavior assertions passed/);
    console.log(result.stdout.trim());
    // Deliberately remove the production readiness check. The same executable
    // must catch the regression before it can dispatch a busy/stale output.
    const broken = current.replace('if (!read_current().allows(action)) return false;', 'read_current();');
    assert.notEqual(broken, current, 'negative control must alter the production check');
    const negative = compile(directory, setup, broken);
    assert.notEqual(negative.status, 0, 'removed readiness check must fail the behavioral assertions');
    assert.match(negative.stderr, /busy action rejected/);
  } finally {
    assert.equal(path.dirname(path.resolve(directory)), path.resolve(tmpdir()));
    assert.ok(path.basename(directory).startsWith('workflow-print-state-'));
    rmSync(directory, { recursive: true, force: true });
  }
});
