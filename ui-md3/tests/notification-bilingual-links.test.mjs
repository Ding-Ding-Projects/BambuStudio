import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Notification links ("Restart to install update", "Release notes", "Retry")
// stayed English in bilingual mode while the text above them carried both
// languages. A link now reads "English · 廣東話" when that fits the text area on
// a line of its own. The decision is made once per layout, in count_lines(),
// and every measurement and draw uses that same text, so the hit box and the
// underline always match what is drawn and a link is never cut short.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const source = stripComments(await read('src', 'slic3r', 'GUI', 'NotificationManager.cpp'));
const header = stripComments(await read('src', 'slic3r', 'GUI', 'NotificationManager.hpp'));

const bodyOf = (signature) => {
  const start = source.indexOf(signature);
  assert.ok(start >= 0, `${signature} must exist`);
  return source.slice(start, source.indexOf('\n}', start) + 2);
};

test('a link is paired with its Cantonese only when the pair fits a line', () => {
  const helper = bodyOf('std::string bilingual_link_text(');
  assert.match(helper, /is_bilingual\(\)/, 'only in bilingual mode');
  assert.match(helper, /I18N::bilingual_secondary\(/, 'the Cantonese recorded for the English on screen');
  assert.match(helper, /ImGui::CalcTextSize\(compact\.c_str\(\)\)\.x <= available_width \? compact : english/, 'English alone when the pair does not fit');
});

test('the shown link text is decided in count_lines and used for every measure and draw', () => {
  const count = bodyOf('void NotificationManager::PopNotification::count_lines()');
  assert.match(count, /m_hypertext_shown\s*=\s*bilingual_link_text\(m_hypertext, available_width\);/);
  assert.match(count, /m_second_hypertext_shown\s*=\s*bilingual_link_text\(m_second_hypertext, available_width\);/);
  assert.doesNotMatch(count, /CalcTextSize\(m_hypertext\.c_str\(\)\)|CalcTextSize\(m_second_hypertext\.c_str\(\)\)/, 'layout measures what is drawn');
  const render = bodyOf('void NotificationManager::PopNotification::render_text(');
  assert.doesNotMatch(render, /CalcTextSize\(m_hypertext\.c_str\(\)\)|ellipsize_middle\(m_hypertext,|m_second_hypertext, false, true/, 'drawing uses the shown text');
  assert.match(render, /ellipsize_middle\(m_hypertext_shown,/);
  assert.match(header, /std::string\s+m_hypertext_shown;/);
  assert.match(header, /std::string\s+m_second_hypertext_shown;/);
});
