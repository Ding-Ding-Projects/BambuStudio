import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Every key and description in Keyboard Shortcuts sat in a grey box on the
// white page. A kit Label copies its parent's background when it is created,
// and the page and its scrolled panel had none of their own, so the labels
// took the system face colour while the dialog painted its surface colour.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'KBShortcutsDialog.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

test('the shortcut page and its scrolled panel carry the dialog surface before any label is made', () => {
  const page = source.match(/wxPanel\* KBShortcutsDialog::create_page\([\s\S]*?\n\}/);
  assert.ok(page, 'create_page() must exist');
  const code = stripComments(page[0]);
  const surface = 'SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest))';
  const pageColour = code.indexOf('main_page->' + surface);
  const panelColour = code.indexOf('scrollable_panel->' + surface);
  assert.ok(pageColour > 0, 'the page has the dialog surface colour');
  assert.ok(panelColour > 0, 'the scrolled panel has the dialog surface colour');
  const firstLabel = code.indexOf('new Label(');
  const firstPanelLabel = code.indexOf('new Label(scrollable_panel');
  assert.ok(pageColour < firstLabel, 'the page colour is set before its first label');
  assert.ok(panelColour < firstPanelLabel, 'the panel colour is set before the key and description labels');
  assert.ok(code.indexOf('wxGetApp().UpdateDarkUI(scrollable_panel)') < panelColour, 'set after UpdateDarkUI, which would otherwise override it');
});
