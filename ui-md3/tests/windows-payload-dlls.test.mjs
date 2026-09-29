import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Release md3-v143 could not start: the Ninja build skipped the DLL copy the
// install step ships, so BambuStudio.dll arrived without OpenCascade, FFmpeg,
// GMP and the Visual C++ runtime (LoadLibrary error 126, launcher exit -1).
// These contracts keep the copy on both generator kinds, the runtime beside
// the app, and the packaging check that refuses a payload missing a DLL.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');

const stripCmakeComments = (source) => source.replace(/^[ \t]*#.*$/gm, '');

const srcCmake = stripCmakeComments(await read('src', 'CMakeLists.txt'));
const workflow = await read('.github', 'workflows', 'build_bambu.yml');

test('the single-config (Ninja) build copies and installs the runtime DLLs', () => {
  const windows = srcCmake.match(/if \(CMAKE_CONFIGURATION_TYPES\)([\s\S]*?)\n    else \(\)([\s\S]*?)\n    endif \(\)/);
  assert.ok(windows, 'the Windows block branches on CMAKE_CONFIGURATION_TYPES');
  const [, multi, single] = windows;
  for (const [branch, name] of [[multi, 'multi-config'], [single, 'single-config']]) {
    assert.match(branch, /bambustudio_copy_dlls\(BambuStudioDllsCopy "Release" "" output_dlls_Release\)/,
      `the ${name} branch fills output_dlls_Release`);
  }
  assert.match(srcCmake, /install\(FILES \$\{output_dlls_\$\{build_type\}\} DESTINATION/);
});

test('the Visual C++ runtime ships beside the app', () => {
  assert.match(srcCmake, /set\(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "\."\)/);
  assert.match(srcCmake, /include\(InstallRequiredSystemLibraries\)/);
});

test('packaging refuses a payload that misses a DLL the app imports', () => {
  const check = workflow.indexOf('python .\\scripts\\ci\\check_payload_imports.py');
  assert.ok(check > 0, 'the workflow runs the payload import check');
  for (const later of ['Pack app', 'Create Windows Squirrel installer']) {
    const at = workflow.indexOf(`name: ${later}`);
    assert.ok(at > check, `the check runs before "${later}"`);
  }
});
