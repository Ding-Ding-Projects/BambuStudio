import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// In bilingual mode a label reads "English · 廣東話" only when that fits. The
// fit used to be measured against the containing sizer alone, and a sizer in
// a scrolled panel can be wider than the panel: Keyboard Shortcuts drew
// "Objects list · 物件清" and a description running off the dialog. The fit
// must also respect the width that is actually visible.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'BilingualDecorator.cpp'), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

test('the compact bilingual label must fit the visible width, not only the sizer', () => {
  const visible = source.match(/int visible_width\(wxWindow \*window\)[\s\S]*?\n\}/);
  assert.ok(visible, 'visible_width() must exist');
  const walk = stripComments(visible[0]);
  assert.match(walk, /GetParent\(\)/, 'it walks the ancestors');
  assert.match(walk, /GetClientSize\(\)/, 'it measures each ancestor\'s client area, not its virtual size');
  assert.match(walk, /IsTopLevel\(\)/, 'it stops at the dialog');

  const fits = source.match(/bool fits\(wxWindow \*window, Kind kind[\s\S]*?\n\}/);
  assert.ok(fits, 'fits() must exist');
  assert.match(stripComments(fits[0]), /std::min\(available_width\(window, growth\), visible_width\(window\) \+ growth\)/);
});

test('a label whose width is pinned by an explicit minimum only goes compact when it fits that width', () => {
  // Temperature calibration's 120 DIP labels drew "Start temp: · 開" and cut the rest.
  const fits = source.match(/bool fits\(wxWindow \*window, Kind kind[\s\S]*?\n\}/);
  assert.ok(fits, 'fits() must exist');
  const code = stripComments(fits[0]);
  assert.match(code, /if \(window->GetMinSize\(\)\.GetWidth\(\) > 0\)\s*room = std::min\(room, window->GetSize\(\)\.GetWidth\(\)\);/);
});

test('section headers are paired with their Cantonese like any other label', () => {
  // The upper-case section header ("SETTINGS" in every calibration dialog) is a
  // custom-drawn window, so the decorator classed it as nothing and bilingual
  // mode left it English only. It is a single-line label that draws its text
  // as given (no mnemonics), and it re-measures itself when the label changes.
  const kindOf = stripComments(source.match(/Kind kind_of\(wxWindow \*window\)[\s\S]*?\n\}/)[0]);
  assert.match(source, /enum class Kind \{[^}]*\bHeader\b[^}]*\}/, 'a Kind for section headers');
  assert.match(kindOf, /dynamic_cast<::SectionHeader \*>\(window\) != nullptr\)\s*return Kind::Header;/);
  const asLabel = stripComments(source.match(/wxString as_label_text\([\s\S]*?\n\}/)[0]);
  assert.match(asLabel, /kind == Kind::KitButton \|\| kind == Kind::Header/, 'no mnemonic escaping for a header, it draws "&" as is');
});

test('list and table column titles go bilingual when the column has room', () => {
  // Version history ("Commit", "Message", "Time", "Size") and Config profiles
  // ("Profile", "Data folder") kept English-only column titles in bilingual
  // mode: a column title is not a window, so the per-window pass never saw it.
  const columns = source.match(/void decorate_columns\(wxWindow \*window\)[\s\S]*?\n    \}/);
  assert.ok(columns, 'decorate_columns() must exist');
  const code = stripComments(columns[0]);
  assert.match(code, /dynamic_cast<wxDataViewCtrl \*>\(window\)/, 'data view columns');
  assert.match(code, /dynamic_cast<wxListCtrl \*>\(window\)/, 'report list columns');
  assert.match(code, /InReportView\(\)/, 'only a report list has column titles');
  const title = stripComments(source.match(/bool bilingual_title\([\s\S]*?\n\}/)[0]);
  assert.match(title, /Contains\(inline_separator\(\)\)/, 'a title decorated once is never decorated again');
  assert.match(title, /text_width\(owner, decorated\) \+ owner->FromDIP\(24\) <= width/, 'the pair must fit the column width');
  const window = stripComments(source.match(/Change decorate_window\(wxWindow \*window, bool allow_compact, int growth\)[\s\S]*?\n    \}/)[0]);
  assert.match(window, /decorate_columns\(window\);/, 'every window the pass visits gets its columns checked');
});

test('placeholder hints go bilingual when the pair fits the field, typed text never changes', () => {
  // Search fields ("Search profiles", "Search versions") and the Model Creator
  // fields kept English-only placeholders in bilingual mode: text entry is
  // skipped as the user's own data, and the hint went with it. The hint is
  // catalogue text, so it takes its Cantonese; the value is never touched.
  const hint = source.match(/void decorate_hint\(wxWindow \*window\)[\s\S]*?\n    \}/);
  assert.ok(hint, 'decorate_hint() must exist');
  const code = stripComments(hint[0]);
  assert.match(code, /dynamic_cast<wxTextEntry \*>\(window\)/);
  assert.match(code, /GetHint\(\)/);
  assert.match(code, /Contains\(inline_separator\(\)\)/, 'a hint decorated once is never decorated again');
  assert.match(code, /SetHint\(/);
  assert.doesNotMatch(code, /SetValue\(|ChangeValue\(|GetValue\(\)/, 'the typed value is never read or written');
  assert.match(code, /GetClientSize\(\)\.GetWidth\(\)/, 'the pair must fit the field');
  const window = stripComments(source.match(/Change decorate_window\(wxWindow \*window, bool allow_compact, int growth\)[\s\S]*?\n    \}/)[0]);
  assert.match(window, /decorate_hint\(window\);/, 'every window the pass visits gets its hint checked');
});
