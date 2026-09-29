import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// scripts/md3/check-context-menus.py asks real text fields of the built app for
// their menu and records which menu opens. On md3-v162 the Smart home URL field
// and the Preferences search field opened nothing on a right-click (the kit
// swallowed it) and the system's own edit menu (window class "#32768") on the
// keyboard request. The check must tell those apart and fail on either.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const checker = (await readFile(path.join(repoDir, 'scripts', 'md3', 'check-context-menus.py'), 'utf8')).replace(/\r\n/g, '\n');
const sweep = (await readFile(path.join(repoDir, 'scripts', 'md3', 'sweep-dialogs.py'), 'utf8')).replace(/\r\n/g, '\n');

test('both ways of asking for a menu are tried on every field', () => {
  assert.match(checker, /for how in \('right-click', 'keyboard'\):/);
  assert.match(checker, /cheap\('mouse_click', hwnd=field\['hwnd'\],[^;]*?button='right'\)/);
  assert.match(checker, /post\(args\.desktop, field\['hwnd'\], WM_CONTEXTMENU, field\['hwnd'\], 0xFFFFFFFF\)/,
    'the Menu key and Shift+F10 send the request without a position');
});

test('a native menu, or no menu at all, fails the run', () => {
  assert.match(checker, /NATIVE_MENU_CLASS = '#32768'/);
  assert.match(checker, /row\['result'\] = 'no menu opened'/);
  assert.match(checker, /'result': 'native menu' if native else 'material menu'/);
  assert.match(checker, /return 0 if tried and len\(material\) == len\(tried\) else 1/);
});

test('a surface is its dialog, not a popup that comes with it', () => {
  // The gear showed a 160 x 243 "panel" before Preferences, and the sweep measured
  // that popup instead of the dialog until md3-v162.
  assert.match(checker, /and w\['class'\] == '#32770'\), None\), 15\)/);
  assert.match(sweep, /found = wait_for\(lambda: next\(\(w for w in fresh\(\) if w\['class'\] == '#32770'\), None\), timeout\)/);
  assert.match(sweep, /cheap\('mouse_click', hwnd=main_hwnd, x=GEAR\[0\], y=GEAR\[1\]\)\s*tried\.append\('gear'\)\s*dialog = new_dialog\(15\)/);
});

test('the sweep captures every top-bar menu', () => {
  const entries = sweep.slice(sweep.indexOf('ENTRIES = ['), sweep.indexOf(']\n', sweep.indexOf('ENTRIES = [')));
  for (const menu of ['File', 'Edit', 'View', 'Objects', 'Calibration', 'Help']) {
    assert.ok(entries.includes(`'menu:${menu}'`), `menu:${menu}`);
  }
  assert.match(sweep, /command=f'"\{sys\.executable\}" "\{os\.path\.abspath\(__file__\)\}" --post-deactivate \{popup\["handle"\]\}'/,
    'a menu closes when it is deactivated, without restarting the app');
});
