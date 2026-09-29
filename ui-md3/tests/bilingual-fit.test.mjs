import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// In bilingual mode a label reads "English · 廣東話" only when that fits. The
// fit used to be measured against the containing sizer alone, and a sizer in
// a scrolled panel can be wider than the panel: Keyboard Shortcuts drew
// "Objects list · 物件清" and a description running off the dialog. The fit
// must also respect the width that is actually visible.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'BilingualDecorator.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

test('the compact bilingual label must fit the visible width, not only the sizer', () => {
  const visible = source.match(/int visible_width\(wxWindow \*window\)[\s\S]*?\n\}/);
  assert.ok(visible, 'visible_width() must exist');
  const walk = stripComments(visible[0]);
  assert.match(walk, /GetParent\(\)/, 'it walks the ancestors');
  assert.match(walk, /GetClientSize\(\)/, 'it measures each ancestor\'s client area, not its virtual size');
  assert.match(walk, /IsTopLevel\(\)/, 'it stops at the dialog');

  const fits = source.match(/bool fits\(wxWindow \*window, Kind kind[\s\S]*?\n\}/);
  assert.ok(fits, 'fits() must exist');
  assert.match(stripComments(fits[0]), /std::min\(available_width\(window, growth\), visible_width\(window\) \+ growth\)/);
});
