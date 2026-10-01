import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Model Creator footer buttons captured as blank boxes (clipping inventory CJ-030). A kit Button
// without a variant takes the Outlined style in its first paint, which changes its minimum while the
// footer is painting. The dialog now gives each footer button its variant when it creates it.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const source = readFileSync(
  path.resolve(testDir, '..', '..', 'src', 'slic3r', 'GUI', 'ModelCreator', 'ModelCreatorDialog.cpp'),
  'utf8',
).replace(/\r\n/g, '\n').replace(/\/\/.*$/gm, '');

test('every Model Creator footer button has its variant before the first paint', () => {
  const footer = source.indexOf('GetFooterSizer()');
  assert.notEqual(footer, -1);
  const before = source.slice(0, footer);
  assert.match(before, /m_generate->SetVariant\(Button::Variant::Filled\)/);
  assert.match(before, /for \(Button \*button : \{m_cancel_button, m_preview, m_add\}\)\s*button->SetVariant\(Button::Variant::Outlined\)/);
});
