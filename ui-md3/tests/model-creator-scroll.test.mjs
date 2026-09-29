import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v162's English Model Creator, at its fixed 720 x 780, gave the refinement
// note, Revisions and the status line no height at all and squeezed its footer
// buttons below their minimum, so they drew blank (clipping inventory CJ-028):
// the form had outgrown the dialog. Bilingual mode hid it by growing the dialog.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'ModelCreator', 'ModelCreatorDialog.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const ctor = stripComments(source.match(/ModelCreatorDialog::ModelCreatorDialog\(wxWindow \*parent, AddToPlate add_to_plate\)[\s\S]*?\n\}/)[0]);

test('the Model Creator form scrolls inside the dialog', () => {
  assert.match(source, /#include <wx\/scrolwin\.h>/);
  assert.match(ctor, /auto \*form = new wxScrolledWindow\(this,[^;]*wxVSCROLL[^;]*\);/);
  assert.match(ctor, /form->SetScrollRate\(0, FromDIP\(16\)\);/);
  assert.match(ctor, /form->SetMinSize\(wxSize\(-1, FromDIP\(240\)\)\);/, 'a small minimum, so the dialog does not grow to the whole form');
  assert.match(ctor, /form->SetSizer\(body\);\s*GetContentSizer\(\)->Add\(form, 1, wxEXPAND\);/);
  assert.match(ctor, /SetSize\(FromDIP\(wxSize\(720, 780\)\)\);\s*form->FitInside\(\);/, 'the form learns its virtual height once the dialog has its size');
});

test('every form control lives on the scrolled form, the footer buttons on the dialog', () => {
  const formPart = ctor.slice(ctor.indexOf('auto *form = new wxScrolledWindow('), ctor.indexOf('m_generate = new Button('));
  assert.ok(formPart.length > 0, 'the form section is found');
  assert.doesNotMatch(formPart.replace('new wxScrolledWindow(this,', ''), /\((this),/,
    'a control parented to the dialog would sit outside the scroll');
  assert.ok((formPart.match(/\(form, /g) || []).length >= 18, 'the labels, fields, lists and key buttons are on the form');
  for (const name of ['Generate', 'Cancel generation', 'Preview mesh', 'Add to plate']) {
    assert.ok(ctor.includes(`new Button(this, _L("${name}"))`), `${name} stays in the footer`);
  }
});
