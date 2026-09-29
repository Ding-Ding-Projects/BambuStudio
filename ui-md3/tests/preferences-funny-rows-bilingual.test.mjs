import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v155 still scrolled bilingual Preferences sideways after the decorator's
// scrolling-page rules (clipping inventory CJ-021): the funny-level and emoji
// rows are not decorated at all, because Preferences rendered them itself as one
// "English · 廣東話" line ("Not stored yet; using the compiled default 2. ·
// 未儲存；用編譯預設值 2。" is about 430 px in a 356 px column). The helpers must
// hand the pair to the bilingual registry and show the English, so the decorator
// decides the fit like it does for every other label.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Preferences.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const body = (signature) => {
  const match = source.match(new RegExp(signature + '[\\s\\S]*?\\n\\}'));
  assert.ok(match, signature + ' missing');
  return stripComments(match[0]);
};

test('the funny-level helpers register the pair and show English', () => {
  const registered = body('static wxString registered_english\\(const I18N::FormattedLocalizedText &text\\)');
  assert.match(registered, /if \(text\.has_secondary\(\)\)\s*I18N::BilingualRegistry::instance\(\)\.record\(text\.primary\(\), text\.secondary\(\)\);/);
  assert.match(registered, /return text\.primary\(\);/);
  assert.match(body('static wxString funny_row_label\\(const char \\*source\\)'), /return registered_english\(/);
  assert.match(body('static wxString funny_row_label_int\\(const char \\*source, int value\\)'), /return registered_english\(/);
  assert.match(source, /#include "BilingualRegistry\.hpp"/);
});

test('Preferences builds no compact pair for a label of its own', () => {
  // The one compact pair left is the title bar of the language-switch
  // confirmation: a native caption, not a label in a sizer, so there is no row
  // for it to run past. Any other one bypasses the decorator's fit rules.
  const code = stripComments(source);
  const calls = [...code.matchAll(/render_localized_text_compact\(/g)];
  for (const call of calls) {
    const before = code.slice(Math.max(0, call.index - 60), call.index);
    assert.match(before, /const auto caption_text = I18N::$/,
      'a compact pair built here bypasses the fit rules of the bilingual decorator');
  }
  assert.ok(calls.length <= 1, 'only the confirmation caption may be paired here');
});
