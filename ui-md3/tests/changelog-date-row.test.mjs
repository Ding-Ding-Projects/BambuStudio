import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v158's What's new showed its date fields' hint cut: "YYYY-MM-DD / DD/MM/YYYY"
// in a 132 DIP field read "YYYY-MM-DD / D", in English and in bilingual mode
// (clipping inventory CJ-027). A field as wide as its hint then needs a date row
// that can put the preset chips on a line of their own.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'ChangelogDialog.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const buildUi = stripComments(source.match(/void ChangelogDialog::build_ui\(\)[\s\S]*?\n\}/)[0]);

test('a date field is as wide as its whole hint', () => {
  assert.match(buildUi, /const wxString hint = locale_date_hint\(\);\s*field->GetTextCtrl\(\)->SetHint\(hint\);/);
  assert.match(buildUi, /const int width = std::max\(FromDIP\(132\), field->GetTextCtrl\(\)->GetTextExtent\(hint\)\.GetWidth\(\) \+ FromDIP\(28\)\);/,
    'never narrower than before, and never narrower than the hint plus the frame and entry margins');
  assert.match(buildUi, /field->SetMinSize\(wxSize\(width, FromDIP\(40\)\)\);/, 'the sizer keeps that width');
});

test('the date row puts its preset chips on a line of their own when they do not fit', () => {
  assert.match(source, /#include <wx\/wrapsizer\.h>/);
  assert.match(buildUi, /auto \*dates\s+= new wxWrapSizer\(wxHORIZONTAL\);/);
  // md3-v162 showed the chips at the start of their own line in every mode; the
  // group carries no spacer that would claim otherwise.
  assert.match(buildUi, /presets->Add\(m_preset_30,[^;]*\);\s*presets->Add\(m_preset_year,[^;]*\);\s*presets->Add\(m_preset_all,/);
  assert.doesNotMatch(buildUi, /presets->AddStretchSpacer\(/);
  assert.match(buildUi, /dates->Add\(range, 0,[^;]*\);\s*dates->Add\(presets, 0,[^;]*\);/, 'the chips wrap as one group');
  assert.doesNotMatch(buildUi, /dates->Add\(m_preset_/, 'no chip is laid out on its own in the wrapping row');
});
