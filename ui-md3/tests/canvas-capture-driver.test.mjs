import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// scripts/md3/capture-canvas.py photographs the 3D canvas (the plate and the
// gizmo panels) through the probe's canvas-png command, because PrintWindow
// leaves the OpenGL canvas blank on the real graphics driver. A capture that
// was not saved must never count as one: a blank canvas passed off as evidence
// would claim a language check that never happened.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const driver = (await readFile(path.join(repoDir, 'scripts', 'md3', 'capture-canvas.py'), 'utf8')).replace(/\r\n/g, '\n');

test('the driver asks the canvas for its own frame', () => {
  assert.match(driver, /send\(args\.desktop, main_hwnd, command=f'canvas-png \{staged\}'\)/);
  assert.doesNotMatch(driver, /cheap\('screenshot'/, 'a PrintWindow capture of the canvas is blank on the real driver');
});

test('the gizmos are opened from the probe\'s rail items, by name', () => {
  assert.match(driver, /send\(args\.desktop, main_hwnd, command=f'load \{CUBE\}'\)/, 'a gizmo needs an object');
  assert.match(driver, /rail = \[r for r in records if r\.get\('kind'\) == 'gl_item' and r\.get\('toolbar'\) == 'gizmo'\]/);
  assert.match(driver, /cheap\('mouse_click', hwnd=item\['host'\], x=rect\['x'\] \+ rect\['w'\] \/\/ 2, y=rect\['y'\] \+ rect\['h'\] \/\/ 2\)/,
    'the click lands in the item, in canvas pixels on the canvas window');
});

test('a capture that was not saved is reported, never counted', () => {
  assert.match(driver, /'png': target if saved else None/);
  assert.match(driver, /'result': 'no such rail item'/);
  assert.match(driver, /return 0 if saved and len\(saved\) == len\(results\) else 1/);
});
