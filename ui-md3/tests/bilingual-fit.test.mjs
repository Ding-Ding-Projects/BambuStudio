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
  assert.match(code, /if \(kind != Kind::KitButton && window->GetMinSize\(\)\.GetWidth\(\) > 0\)\s*room = std::min\(room, window->GetSize\(\)\.GetWidth\(\)\);/,
    'a kit Button grows its minimum with its label (md3-v150 showed every button English only), so it is never pinned');
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

test('a compact label that the settled layout does not fully show goes back to English', () => {
  // The fit counts on the dialog growing, but a fixed-width panel or a scrolling
  // page does not grow with it: on md3-v150 Keyboard Shortcuts still drew
  // "Objects list · 物件清" and an import description running off the dialog.
  const recheck = source.match(/bool recheck_compact\(wxWindow \*top\)[\s\S]*?\n    \}/);
  assert.ok(recheck, 'recheck_compact() must exist');
  const code = stripComments(recheck[0]);
  assert.match(code, /wxGetTopLevelParent\(window\) != top/, 'only the labels of this top-level window');
  assert.match(code, /visible_width\(window\)/, 'a label reaching past what its parents show is not shown');
  assert.match(code, /m_compact_refused\.insert\(window\)/, 'a label sent back stays English');
  assert.match(code, /SetLabel\(/, 'the English goes back on the label');
  assert.match(code, /tooltip_prefix\(\)/, 'and the Cantonese into its tooltip');
  const top = stripComments(source.match(/void decorate_top\(wxWindow \*top, bool allow_retry\)[\s\S]*?\n    \}/)[0]);
  assert.match(top, /recheck_compact\(top\)/, 'every pass checks after the layout has settled');
  const window = stripComments(source.match(/Change decorate_window\(wxWindow \*window, bool allow_compact, int growth\)[\s\S]*?\n    \}/)[0]);
  assert.match(window, /m_compact_refused\.count\(window\) == 0/, 'a refused label is not compacted again');
  const forget = stripComments(source.match(/void forget\(wxWindow \*window\)[\s\S]*?\n    \}/)[0]);
  assert.match(forget, /m_compact_refused\.erase\(window\);/);
});

test('a scrolling page never widens for a paired label and never scrolls sideways because of one', () => {
  // md3-v151's bilingual Preferences pushed its value controls off the right edge
  // and grew a horizontal scrollbar (clipping inventory CJ-021): the fit let a label
  // count on the dialog growing, but a settings page scrolls instead of growing, so
  // the row's control was pushed out of sight. The label itself stayed visible,
  // so a check of the label alone could not see it.
  const fits = stripComments(source.match(/bool fits\(wxWindow \*window, Kind kind[\s\S]*?\n\}/)[0]);
  assert.match(fits, /if \(scrolling_page_of\(window\) != nullptr\)\s*growth = 0;/, 'no growth allowance inside a scrolling page');
  const page = stripComments(source.match(/wxWindow \*scrolling_page_of\(wxWindow \*window\)[\s\S]*?\n\}/)[0]);
  assert.match(page, /dynamic_cast<wxScrollHelper \*>\(parent\)/);
  assert.match(page, /IsTopLevel\(\)/, 'only pages inside the dialog, not the dialog itself');
  const recheck = stripComments(source.match(/bool recheck_compact\(wxWindow \*top\)[\s\S]*?\n    \}/)[0]);
  assert.match(recheck, /page_too_wide\(page\)/, 'a page whose rows now need more width sends its paired labels back');
  assert.match(recheck, /squeezed \|\| widened \|\| width > visible_width\(window\)/);
  assert.match(recheck, /FitInside\(\)/, 'and the page gets back the width its rows need, so the sideways scrollbar goes');

  const wide = stripComments(source.match(/bool page_too_wide\(wxWindow \*page\) const[\s\S]*?\n    \}/)[0]);
  assert.match(wide, /GetSizer\(\)/);
  assert.match(wide, /GetClientSize\(\)\.GetWidth\(\)/, 'measured against what the page shows');
  assert.match(wide, /sizer->GetMinSize\(\)\.GetWidth\(\) > allowed/, 'from the rows themselves, not a virtual size that may not have caught up');
  assert.match(wide, /m_page_width\.find\(page\)/, 'a page that already scrolled sideways in English is only held to its English width');

  const window = stripComments(source.match(/Change decorate_window\(wxWindow \*window, bool allow_compact, int growth\)[\s\S]*?\n    \}/)[0]);
  assert.match(window, /remember_page_width\(window\);[\s\S]*?window->SetLabel\(next\.shown\)/, 'the English width is measured before the first label on the page changes');
  const forget = stripComments(source.match(/void forget\(wxWindow \*window\)[\s\S]*?\n    \}/)[0]);
  assert.match(forget, /m_page_width\.erase\(window\);/);
});

test('a label its owner wrapped stays within the owner\'s width: compact when the pair fits it, stacked otherwise', async () => {
  // Every Preferences row title and description is a Label wrapped to 320 DIP.
  // md3-v151 paired it on one line ("No warnings when loading 3MF with modified
  // G-codes · ..." 629 px wide), which dropped the wrap: the label ran under the
  // row's switch and past the page edge. A short title ("Language · 語言") still
  // fits the owner's width on one line and stays compact.
  const labelHpp = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets', 'Label.hpp'), 'utf8');
  const labelCpp = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets', 'Label.cpp'), 'utf8');
  assert.match(labelHpp, /int GetWrapWidth\(\) const \{ return m_wrap_width; \}/);
  const wrap = stripComments(labelCpp.match(/void Label::Wrap\(int width\)[\s\S]*?\n\}/)[0]);
  assert.match(wrap, /if \(!GetHandle\(\)\) return;\s*m_wrap_width = std::max\(0, width\);/, 'recorded only when the wrap happens');
  const setLabel = stripComments(labelCpp.match(/void Label::SetLabel\(const wxString& label\)[\s\S]*?\n\}/)[0]);
  assert.match(setLabel, /\} else \{\s*m_wrap_width = 0;[^\n]*\s*wxStaticText::SetLabel\(label\);/, 'new text is not wrapped until its owner wraps it');

  const wraps = stripComments(source.match(/bool wraps\(wxWindow \*window, Kind kind, const wxString &english\)[\s\S]*?\n\}/)[0]);
  assert.match(wraps, /label->GetLabel\(\)\.Contains\('\\n'\)/, 'a label its owner wrapped onto several lines is stacked');
  const owner = stripComments(source.match(/int owner_wrap_width\(wxWindow \*window, Kind kind\)[\s\S]*?\n\}/)[0]);
  assert.match(owner, /label->GetWrapWidth\(\)/);
  assert.match(owner, /LB_AUTO_WRAP/, 'an auto-wrapping Label wraps itself');

  const window = stripComments(source.match(/Change decorate_window\(wxWindow \*window, bool allow_compact, int growth\)[\s\S]*?\n    \}/)[0]);
  assert.match(window, /next\.wrap_width = owner_wrap_width\(window, kind\);/);
  assert.match(window, /next\.wrap_width = previous->second\.wrap_width;/, 'our own one-line text clears the width on the Label, so it is kept');
  assert.match(window, /fits\(window, kind, label, compact, growth\) &&\s*\(next\.wrap_width == 0 \|\| text_width\(window, compact\) <= next\.wrap_width\)\)\s*next\.shown = compact;/,
    'compact only when the pair fits the owner\'s width on one line');
  assert.match(window, /else if \(next\.wrap_width > 0\)\s*next\.shown = next\.english \+ "\\n" \+ second;/, 'otherwise English over Cantonese, never English only');
  assert.match(window, /wrapped->Wrap\(next\.wrap_width > 0 \? next\.wrap_width : width\);/, 'wrapped again at the owner\'s width');
  assert.match(window, /static_cast<wxStaticText \*>\(window\)->Wrap\(width\);/, 'a plain static text still keeps its own width');

  const recheck = stripComments(source.match(/bool recheck_compact\(wxWindow \*top\)[\s\S]*?\n    \}/)[0]);
  assert.match(recheck, /const bool\s+stack\s+= applied\.wrap_width > 0 && !cantonese\.empty\(\);/, 'a compact label sent back from a page that is too wide stacks when its owner lets it wrap');
  assert.match(recheck, /static_cast<::Label \*>\(window\)->Wrap\(applied\.wrap_width\);/);
});

test('a label that stretches along its row counts the row\'s slack once', () => {
  // A stretching item already holds its share of the slack; adding the slack to
  // its stretched width let a paired label take the room of the control beside it.
  const available = stripComments(source.match(/int available_width\(wxWindow \*window, int growth\)[\s\S]*?\n\}/)[0]);
  assert.match(available, /item->GetProportion\(\) > 0/);
  assert.match(available, /std::min\(own, window->GetEffectiveMinSize\(\)\.GetWidth\(\)\)/);
  assert.match(available, /return from \+ std::max\(0, sizer->GetSize\(\)\.GetWidth\(\) - sizer->GetMinSize\(\)\.GetWidth\(\)\) \+ growth;/);
});

test('a label that grows taller moves the rows of its page', () => {
  // md3-v155's bilingual Preferences > 3D drew each stacked description's
  // Cantonese line under the next row's title: the dialog laid itself out again,
  // but a page keeps its size then, so the page's own sizer never ran.
  const relayout = stripComments(source.match(/void relayout\(const std::unordered_set<wxWindow \*> &parents\)[\s\S]*?\n\}/)[0]);
  assert.match(relayout, /parent->Layout\(\);/, 'every parent of a changed label lays out again');
  assert.match(relayout, /dynamic_cast<wxScrollHelper \*>\(parent\) != nullptr \? parent : scrolling_page_of\(parent\)/, 'and so does the scrolling page around it');
  assert.match(relayout, /page->FitInside\(\);\s*page->Layout\(\);/, 'the page takes the height its rows need and moves them');
  assert.match(relayout, /page != nullptr && page->GetSizer\(\) != nullptr/, 'a canvas that sets its own virtual size is left alone');

  const top = stripComments(source.match(/void decorate_top\(wxWindow \*top, bool allow_retry\)[\s\S]*?\n    \}/)[0]);
  assert.match(top, /relayout\(parents\);\s*if \(dialog\) \{\s*settle\(top, allow_retry\);/, 'a dialog lays out its pages before it settles');
  const recheck = stripComments(source.match(/bool recheck_compact\(wxWindow \*top\)[\s\S]*?\n    \}/)[0]);
  assert.match(recheck, /relayout\(parents\);/, 'labels sent back to a stacked pair move their rows too');
});
