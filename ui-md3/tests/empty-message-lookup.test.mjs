import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A gettext catalogue answers the empty msgid with its header
// ("Project-Id-Version: Bambu Studio ..."). wx's own lookup refuses an empty
// string; the language service's direct lookups did not. On md3-v151, Cantonese
// mode gave every spin field without a unit that header as its label, and the
// number box was squeezed to 0 px. Every direct catalogue lookup must skip an
// empty message.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'LanguageMode.cpp'), 'utf8');
const cppTests = await readFile(path.join(repoDir, 'tests', 'language_mode', 'language_mode_tests_main.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

function body(signature) {
  const match = source.match(new RegExp(signature + '[\\s\\S]*?\\n\\}'));
  assert.ok(match, signature + ' missing');
  return stripComments(match[0]);
}

test('every direct catalogue lookup skips an empty message', () => {
  const english = body('wxString LanguageModeService::english\\(const wxString &message, const wxString &context\\) const');
  assert.match(english, /if \(m_english_catalog != nullptr && !message\.empty\(\)\)/);
  const plural = body('wxString LanguageModeService::english_plural\\(');
  assert.match(plural, /if \(m_english_catalog != nullptr && !singular\.empty\(\)\)/);
  const cantonese = body('const wxString \\*LanguageModeService::find_cantonese\\(');
  assert.match(cantonese, /if \(m_cantonese_catalog == nullptr \|\| message\.empty\(\)\)\s*return nullptr;/);
});

test('no other code reads a catalogue directly', () => {
  // Every GetString() on a catalogue is one of the three guarded lookups above.
  const calls = [...stripComments(source).matchAll(/(\w+)->GetString\(/g)].map((m) => m[1]);
  assert.ok(calls.length > 0);
  for (const owner of calls) assert.ok(['m_english_catalog', 'm_cantonese_catalog'].includes(owner), owner + '->GetString() is unguarded');
});

test('the language mode tests pin the empty message in all three modes', () => {
  assert.match(cppTests, /TEST_CASE\("An empty message never comes back as the catalogue header"/);
  assert.match(cppTests, /LANGUAGE_MODE_CANTONESE_HONG_KONG, LANGUAGE_MODE_ENGLISH_CANTONESE_HK, LANGUAGE_MODE_ENGLISH/);
  assert.match(cppTests, /REQUIRE\(service\.finish\(wxString\(\), wxString\(\)\)\.empty\(\)\);/);
});
