// Source contract for how the native suite keeps and presents the official
// catalog. The snapshot model is covered by catalog_snapshot_tests.cpp.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const strip = (source) => source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
const dialog = strip(readFileSync('src/slic3r/GUI/OllamaSuite/OllamaSuiteDialog.cpp', 'utf8'));
const text = strip(readFileSync('src/slic3r/GUI/OllamaSuite/OllamaSuiteText.cpp', 'utf8'));

test('every verified traversal is saved with its revision, not only a certified one', () => {
  assert.doesNotMatch(dialog, /if\s*\(\s*snapshot\.complete\s*\)\s*atomic_json/, 'saving must not wait for a total the source never publishes');
  assert.match(dialog, /catalog_verified\s*\(\s*snapshot\s*\)/);
  assert.match(dialog, /catalog_revision\s*\(\s*snapshot\s*,\s*content_identity\s*\)/, 'the saved catalog carries its revision');
  assert.match(dialog, /atomic_json\s*\(\s*m_root\s*\/\s*"catalog\.json"/);
});

test('a failed refresh is recorded beside the last verified catalog and never replaces it', () => {
  assert.match(dialog, /"catalog-attempt\.json"/, 'the latest attempt is persisted separately');
  assert.match(dialog, /if\s*\(\s*verified\s*\)\s*m_state\.catalog\s*=\s*std::move\s*\(\s*snapshot\s*\)/, 'only a verified traversal replaces the shown catalog');
  assert.match(dialog, /load_catalog\s*\([^;]*content_identity\s*\)/, 'the saved catalog is checked against its revision when read back');
  assert.match(dialog, /load_attempt\s*\(/);
});

test('the catalog status is localized and shows verdict, revision, age and the failed attempt', () => {
  assert.match(dialog, /OllamaText::catalog_status\s*\(\s*m_state\.catalog\s*,\s*m_state\.catalog_attempt\s*,\s*now_seconds\s*\(\s*\)\s*\)/);
  assert.doesNotMatch(dialog, /"No verified catalog is cached/, 'no untranslated catalog status remains');
  assert.match(dialog, /show_catalog_age\s*\(\s*\)/, 'the age keeps updating while the dialog is open');
  for (const phrase of ['Fully traversed catalog', 'Certified complete catalog', 'Revision %s, verified at %s (UTC).', 'Stale: verified %s ago.', 'could not reach the official catalog'])
    assert.ok(text.includes(phrase), 'catalog status covers: ' + phrase);
});
