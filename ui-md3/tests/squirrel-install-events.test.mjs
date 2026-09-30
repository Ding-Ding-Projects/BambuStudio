import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The installer (Squirrel.Windows) makes a shortcut for, and starts, every executable in the
// package unless one of them is marked aware of it. Unmarked, the regex helper got the shortcut
// named after the application, and the application's own shortcut was named "BambuStudio" in a
// Start Menu folder "Bambu Research" (issue #52). The launcher is now the one marked executable:
// it handles the install events itself and makes its own shortcuts.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (...parts) => (await readFile(path.join(repoDir, ...parts), 'utf8')).replace(/\r\n/g, '\n');
const code = (text) => text.replace(/\/\*[\s\S]*?\*\//g, ' ').replace(/^[ \t]*\/\/.*$/gm, ' ');
const block = (rc, id) => {
  const start = rc.indexOf(`BLOCK "${id}"`);
  assert.notEqual(start, -1, `version block ${id} exists`);
  return rc.slice(start, rc.indexOf('}', start));
};

test('the launcher is marked aware of the installer, in the block the installer reads', async () => {
  const rc = code(await read('src', 'platform', 'msw', 'BambuStudio.rc.in'));
  // Squirrel.Windows queries \StringFileInfo\040904B0\SquirrelAwareVersion and no other block.
  assert.match(block(rc, '040904B0'), /VALUE "SquirrelAwareVersion", "1"/);
  assert.match(rc, /VALUE "Translation", 0x409, 1252, 0x409, 1200/);
  // The installer names a shortcut after ProductName and files it under CompanyName.
  for (const id of ['040904E4', '040904B0']) {
    assert.match(block(rc, id), /VALUE "ProductName", "Bambu Studio MD3"/, `${id} product name`);
    assert.match(block(rc, id), /VALUE "CompanyName", "codingmachineedge"/, `${id} company name`);
  }
  // No other executable may be marked, or it would get install events and a shortcut of its own.
  const msw = path.join(repoDir, 'src', 'platform', 'msw');
  for (const name of await readdir(msw)) {
    if (name === 'BambuStudio.rc.in' || !/\.rc(\.in)?$/.test(name)) continue;
    assert.doesNotMatch(await read('src', 'platform', 'msw', name), /SquirrelAwareVersion/, name);
  }
});

test('the launcher handles every install event before it loads anything', async () => {
  const launcher = code(await read('src', 'BambuStudio_app_msvc.cpp'));
  const handler = launcher.slice(launcher.indexOf('static int handle_squirrel_event('), launcher.indexOf('extern "C" {'));
  assert.match(handler, /wcscmp\(event, L"--squirrel-install"\) == 0 \|\| wcscmp\(event, L"--squirrel-updated"\) == 0/);
  assert.match(handler, /wcscmp\(event, L"--squirrel-uninstall"\) == 0/);
  assert.match(handler, /install \? L"--createShortcut=" : L"--removeShortcut="\) \+ exe_name \+\s*L" --shortcut-locations=Desktop,StartMenu"/,
    'the shortcuts are made and removed by Update.exe, for this executable only');
  assert.match(handler, /if \(wcscmp\(event, L"--squirrel-firstrun"\) == 0\)\s*return -1;/, 'the first run after an install is a normal start');
  assert.match(handler, /remove_legacy_squirrel_shortcut\(CSIDL_DESKTOPDIRECTORY, L"BambuStudio\.lnk", root\);/);
  assert.match(handler, /remove_legacy_squirrel_shortcut\(CSIDL_PROGRAMS, L"Bambu Research\\\\BambuStudio\.lnk", root\);/);
  // Legacy shortcuts are removed only when they point into this installation.
  assert.match(launcher, /ours = _wcsnicmp\(target, root\.c_str\(\), root\.size\(\)\) == 0;[\s\S]*?if \(!ours\)\s*return;\s*::DeleteFileW\(link\.c_str\(\)\);/);

  const main = launcher.slice(launcher.indexOf('_set_error_mode(_OUT_TO_MSGBOX);'));
  const event = main.indexOf('handle_squirrel_event(argc, argv)');
  assert.ok(event !== -1 && event < main.indexOf('OpenGLVersionCheck opengl_version_check;') && event < main.indexOf('BambuStudio.dll'),
    'an install event returns before the OpenGL check opens a window and before BambuStudio.dll loads');
  assert.match(main, /if \(squirrel_exit >= 0\)\s*return squirrel_exit;/);
  assert.match(main, /if \(wcscmp\(argv\[i\], L"--squirrel-firstrun"\) == 0\)\s*continue;/, 'the first-run argument never reaches the app');
});

test('the hosted install check reads the shortcuts back', async () => {
  const verify = await read('scripts', 'ci', 'Verify-HostedSquirrelInstall.ps1');
  assert.match(verify, /\$desktopLink = Join-Path \(\[Environment\]::GetFolderPath\('Desktop'\)\) 'Bambu Studio MD3\.lnk'/);
  assert.match(verify, /\$startLink = Join-Path \(\[Environment\]::GetFolderPath\('Programs'\)\) 'codingmachineedge\\Bambu Studio MD3\.lnk'/);
  assert.match(verify, /Assert-True \(\$target -ieq \$expectedTarget\)/, 'each shortcut starts the application');
  assert.match(verify, /Assert-True \(\$strayLinks\.Count -eq 0\)/, 'no shortcut starts another executable of the installation');
});
