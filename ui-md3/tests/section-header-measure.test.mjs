import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A section header ("SETTINGS") paints upper-case, one glyph at a time with
// letter-spacing, through GDI+ on Windows, but measured itself with plain GDI and
// one whole-string extent. The paired header "SETTINGS · 設定" came out short
// and was cut at its own edge on md3-v154 (Temperature calibration, bilingual).
// Its best size must be measured the way it paints.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets', 'Label.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const body = (signature) => {
  const match = source.match(new RegExp(signature + '[\\s\\S]*?\\n\\}'));
  assert.ok(match, signature + ' missing');
  return stripComments(match[0]);
};

test('the tracked width adds up each glyph, as drawing advances', () => {
  const width = body('double trackedTextWidth\\(wxDC &dc, const wxString &text, double tracking\\)');
  assert.match(width, /for \(size_t i = 0; i < text\.length\(\); \+\+i\)/);
  assert.match(width, /dc\.GetTextExtent\(text\.SubString\(i, i\), &w, &h\);/);
  assert.doesNotMatch(width, /dc\.GetTextExtent\(text, &w, &h\)/, 'not one whole-string extent');
  const draw = body('void drawTrackedText\\(wxDC &dc, const wxString &text, double x, int y, double tracking\\)');
  assert.match(draw, /text\.SubString\(i, i\)/);
});

test('the header measures with the font and the context it paints with', () => {
  const best = body('wxSize SectionHeader::DoGetBestClientSize\\(\\) const');
  assert.match(best, /wxGCDC dc\(client_dc\);/, 'GDI+ on Windows, like OnPaint');
  assert.match(best, /dc\.SetFont\(sectionHeaderFont\(\)\);/);
  const paint = body('void SectionHeader::OnPaint\\(wxPaintEvent &\\)');
  assert.match(paint, /wxGCDC dc\(pdc\);/);
  assert.match(paint, /dc\.SetFont\(sectionHeaderFont\(\)\);/);
  const font = body('wxFont sectionHeaderFont\\(\\)');
  assert.match(font, /wxFontEnumerator::IsValidFacename\(face\)/, 'GDI+ never gets a face missing from the session');
});
