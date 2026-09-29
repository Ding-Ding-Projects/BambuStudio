import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// In bilingual mode every message dialog body showed its English cut at the
// last word ("This is the newest versio") and never its Cantonese line (release
// md3-v148, clipping inventory CJ-020). The body sits in a scrolling page whose
// minimum and maximum size were fixed from the one-line English text; the
// bilingual decorator made the label two lines afterwards, the page could not
// grow, and a vertical scrollbar took the end of the line. The body now builds
// its bilingual text before it is measured, and keeps the decorator off it.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'MsgDialog.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

const body = () => {
  const start = source.indexOf('static void add_msg_content(');
  assert.ok(start >= 0, 'add_msg_content() must exist');
  const end = source.indexOf('\n}\n', start);
  return stripComments(source.slice(start, end));
};

test('the bilingual body text is built before the page is measured', () => {
  const code = body();
  const secondary = code.indexOf('I18N::bilingual_secondary(msg)');
  const measure = code.indexOf('GetMultiLineTextExtent(msg)');
  assert.ok(secondary > 0, 'the Cantonese for the message is looked up');
  assert.ok(measure > secondary, 'and appended before the text is measured');
  assert.match(code, /msg \+= "\\n" \+ secondary;/, 'stacked below the English');
});

test('the decorator leaves the body alone once it carries both languages', () => {
  const code = body();
  assert.match(code, /I18N::BilingualRegistry::instance\(\)\.set_managed\(wrapped_text, true\);/);
  assert.match(source, /#include "BilingualDecorator\.hpp"/);
  assert.match(source, /#include "BilingualRegistry\.hpp"/);
});
