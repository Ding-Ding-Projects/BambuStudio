import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The process, filament and printer settings pages scroll inside a composited
// (SetDoubleBuffered) MD3ScrolledWindow. Windows does not support the scroll
// blit on a composited window, so scrolling left rounded cards, separators and
// custom-drawn rows clipped or half painted. The scrolled window must repaint
// its whole viewport, children included, after every scroll.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const widgets = path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets');
const header = await readFile(path.join(widgets, 'MD3ScrolledWindow.hpp'), 'utf8');
const source = await readFile(path.join(widgets, 'MD3ScrolledWindow.cpp'), 'utf8');
const params = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'ParamsPanel.cpp'), 'utf8');
const strip = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

export function scrollRepaintsEverything(headerText, sourceText) {
  const declared = /void\s+ScrollWindow\(int dx, int dy, const wxRect \*rect = nullptr\) override;/.test(strip(headerText));
  const body = strip(sourceText).match(/void MD3ScrolledWindow::ScrollWindow\(int dx, int dy, const wxRect \*rect\)\s*\{([\s\S]*?)\n\}/);
  if (!declared || !body) return false;
  const code = body[1];
  const base = code.indexOf('wxScrolledWindow::ScrollWindow(dx, dy, rect);');
  const refresh = code.indexOf('Refresh(true);');
  return base >= 0 && refresh > base;
}

test('the settings pages scroll inside the composited MD3 scrolled window', () => {
  const code = strip(params);
  assert.match(code, /class PageScrolledWindow : public MD3ScrolledWindow/);
  assert.match(code, /m_page_view->SetDoubleBuffered\(true\);/);
});

test('every scroll repaints the whole viewport after the base scroll', () => {
  assert.ok(scrollRepaintsEverything(header, source));
});

test('negative: the guard turns red when the repaint is removed or moved before the scroll', () => {
  assert.equal(scrollRepaintsEverything(header, source.replace('Refresh(true);', '')), false);
  const reordered = source.replace(
    /wxScrolledWindow::ScrollWindow\(dx, dy, rect\);([\s\S]*?)Refresh\(true\);/,
    'Refresh(true);$1wxScrolledWindow::ScrollWindow(dx, dy, rect);');
  assert.equal(scrollRepaintsEverything(header, reordered), false);
  assert.equal(scrollRepaintsEverything(header.replace(/void\s+ScrollWindow[^;]*;/, ''), source), false);
});
