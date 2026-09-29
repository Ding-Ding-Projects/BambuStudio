import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Config profiles & backup gave its profile list only the height the text
// above it left over: one row in English, and half a row cut through its
// middle in bilingual mode. The list keeps room for its header and a few
// rows, and the dialog is sized from its content rather than a fixed guess.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'ConfigProfilesDialog.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const code = stripComments(source);

test('the profile list keeps room for several rows', () => {
  const min = code.match(/m_profile_list->SetMinSize\(wxSize\(-1, FromDIP\((\d+)\)\)\);/);
  assert.ok(min, 'the profile list sets a minimum height');
  assert.ok(Number(min[1]) >= 120, `a ${min[1]} DIP list shows too few rows`);
});

test('the dialog is never smaller than its content', () => {
  assert.match(code, /const wxSize need = GetSizer\(\)->CalcMin\(\) \+ \(GetSize\(\) - GetClientSize\(\)\);/);
  assert.match(code, /SetMinSize\(wxSize\(std::max\(FromDIP\(680\), need\.GetWidth\(\)\), std::max\(FromDIP\(640\), need\.GetHeight\(\)\)\)\);/);
});
