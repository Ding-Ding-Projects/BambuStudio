import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// wxSizerItem::CalcMin() returns GetMinSizeWithBorder(): the item's minimum with
// its borders already in. The probe added the borders a second time, so
// md3-v155's bilingual Preferences button row read "required 799, available 783,
// oversubscribed" while its three buttons filled the row to the pixel. A row
// verdict has to add up exactly what the sizer pays.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const probe = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'LayoutProbe.cpp'), 'utf8');
const stripComments = (source) => source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

test('the row judge counts each border once, inside CalcMin', () => {
  const judge = probe.match(/RowVerdict judge_sizer\(wxSizer \*sizer\)[\s\S]*?\n\}/);
  assert.ok(judge, 'judge_sizer must exist');
  const code = stripComments(judge[0]);
  assert.match(code, /const wxSize min = item->CalcMin\(\);/);
  assert.match(code, /v\.required \+= v\.orient == wxHORIZONTAL \? min\.x : min\.y;/);
  assert.doesNotMatch(code, /GetBorder\(\)/, 'CalcMin already carries the borders');
});

test('a starved item compares allocation and minimum on the same footing', () => {
  // wxSizerItem::GetSize() includes the borders too, so both sides of the
  // starvation check carry them once.
  const writer = stripComments(probe);
  assert.match(writer, /const wxSize min = item->CalcMin\(\);\s*const wxSize alloc = item->GetSize\(\);/);
});
