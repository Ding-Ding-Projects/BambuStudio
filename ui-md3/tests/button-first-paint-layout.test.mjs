import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A kit Button that reaches its first paint unstyled adopts the Outlined style
// there, and that style's font and 18 DIP padding make its minimum wider than
// the 10 px construction padding did. md3-v162's Smart home footer had already
// placed "Close" by then and never laid it out again outside bilingual mode, so
// the button drew at 59 px against its 70 px minimum (clipping inventory CJ-029).

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const widgets = path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets');
const stripComments = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const source = stripComments(await readFile(path.join(widgets, 'Button.cpp'), 'utf8'));
const header = stripComments(await readFile(path.join(widgets, 'Button.hpp'), 'utf8'));
const body = (signature) => {
  const start = source.indexOf(signature);
  assert.ok(start >= 0, `${signature} is defined`);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

test('the first-paint style asks for a layout when it changes the minimum', () => {
  const paint = body('void Button::paintEvent(wxPaintEvent& evt)');
  assert.match(paint, /if \(!m_md3_variant && !m_caller_styled\)\s*\{\s*const wxSize before = GetMinSize\(\);\s*SetVariant\(Variant::Outlined\);\s*if \(GetMinSize\(\) != before\)\s*relayoutParentLater\(\);\s*\}/);
  assert.match(header, /void relayoutParentLater\(\);/);
});

test('the parent is laid out once, after the paint, and only when a sizer places the button', () => {
  const later = body('void Button::relayoutParentLater()');
  assert.match(later, /parent->GetSizer\(\) == nullptr/, 'a parent without a sizer would stretch its only child instead');
  assert.match(later, /GetContainingSizer\(\) == nullptr/, 'a button placed by hand gains nothing from a layout');
  assert.match(later, /static std::unordered_set<wxWindow \*> pending;/);
  assert.match(later, /if \(!pending\.insert\(parent\)\.second\)\s*return;/, 'buttons sharing a footer request one layout between them');
  assert.match(later, /wxWeakRef<wxWindow> alive\(parent\);/);
  assert.match(later, /wxTheApp->CallAfter\(\[parent, alive\]\(\) \{\s*pending\.erase\(parent\);\s*if \(alive\)\s*alive->Layout\(\);\s*\}\);/,
    'the request survives the button, and a parent destroyed meanwhile is left alone');
  for (const include of ['<wx/app.h>', '<wx/weakref.h>', '<unordered_set>']) {
    assert.ok(source.includes(`#include ${include}`), `Button.cpp includes ${include}`);
  }
});
