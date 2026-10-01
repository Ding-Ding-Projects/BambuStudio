import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Model Creator footer buttons captured as blank boxes (clipping inventory CJ-030). They were
// placed and sized right but stayed unpainted after the dialog's last layout: invalidated by hand on
// md3-v180, all four drew correctly. The dialog now gives each footer button its variant when it
// creates it (a kit Button without one restyles itself in its first paint) and repaints the footer
// once it is on screen and after every change of the buttons' state.

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

test('the footer is repainted once the dialog shows and after every state update', () => {
  const refresh = source.match(/void ModelCreatorDialog::refresh_footer\(\)\s*\{([\s\S]*?)\n\}/);
  assert.ok(refresh, 'refresh_footer() is missing');
  assert.match(refresh[1], /\{m_generate, m_cancel_button, m_preview, m_add\}/);
  assert.match(refresh[1], /->Refresh\(\)/);
  const update = source.match(/void ModelCreatorDialog::update_controls\(\)\s*\{([\s\S]*?)\n\}/);
  assert.ok(update, 'update_controls() is missing');
  assert.match(update[1], /refresh_footer\(\);/, 'update_controls() does not repaint the footer');
  assert.match(source, /Bind\(wxEVT_SHOW,[\s\S]{0,300}?refresh_footer\(\)/, 'nothing repaints the footer when the dialog shows');
});
