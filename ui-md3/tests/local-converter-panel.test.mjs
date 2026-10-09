import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The local file converter destination (File > Local file converter) was built from
// stock wx controls: a native notebook for the eight categories, native report lists
// for the adapter catalogues and the queue, bare edit boxes, stock static text and a
// stock scrolled window. Those render with the Windows look and none of the kit
// roles, focus rings or density metrics. These tests pin the rebuilt surface to the
// registered kit primitives and fail if a stock control comes back.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const panelDir = path.join(repoDir, 'src', 'slic3r', 'GUI', 'LocalConverter');
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const read = async (name) => code(await readFile(path.join(panelDir, name), 'utf8'));
const count = (text, pattern) => (text.match(pattern) || []).length;

test('the converter panel constructs no stock wx control', async () => {
  for (const name of ['LocalConverterPanel.cpp', 'LocalConverterPanel.hpp']) {
    const source = await read(name);
    const stock = source.match(/\bnew\s+(wxNotebook|wxListCtrl|wxTextCtrl|wxStaticText|wxScrolledWindow|wxCheckBox|wxChoice|wxListBox|wxButton|wxRadioButton|wxGauge|wxStaticBox)\s*\(/g) || [];
    assert.deepEqual(stock, [], `${name} must build every control from the kit`);
    assert.doesNotMatch(source, /\b(wxNotebook|wxListCtrl|wxTextCtrl|wxStaticText)\s*\*/, `${name} must not hold stock control pointers`);
    assert.doesNotMatch(source, /#include <wx\/(notebook|listctrl|textctrl|stattext|scrolwin)\.h>/, `${name} must not include stock control headers`);
  }
});

test('the eight categories ride a persisted kit TabStrip over a simple book', async () => {
  const source = await read('LocalConverterPanel.cpp');
  assert.match(source, /#include "slic3r\/GUI\/Widgets\/TabStrip\.hpp"/);
  assert.match(source, /options\.surface_key\s*=\s*"local_converter_categories"/, 'the strip persists its own layout');
  assert.match(source, /options\.default_edge\s*=\s*MD3::Tabs::DockEdge::Top/);
  assert.match(source, /options\.allow_close\s*=\s*false/, 'a category is never closed away');
  assert.match(source, /m_categories\s*=\s*new TabStrip\(body,\s*options\)/);
  assert.match(source, /m_category_pages\s*=\s*new wxSimplebook\(body/);
  // One stable id per required category, in contract order.
  const ids = source.match(/kCategoryIds\s*\{([^}]*)\}/);
  assert.ok(ids, 'the category ids are declared once');
  assert.deepEqual(ids[1].match(/"[^"]+"/g), ['"documents"', '"images"', '"audio"', '"video"', '"archives"', '"structured"', '"text"', '"binary"']);
  assert.match(source, /m_categories->AddTab\(kCategoryIds\[i\],\s*category_label\(i\)\)/);
  assert.match(source, /Bind\(EVT_TABSTRIP_ACTIVATE,/, 'activating a tab switches the page');
  assert.match(source, /Bind\(EVT_TABSTRIP_DOCK_CHANGED,/, 'moving the strip re-places it');
  assert.match(source, /m_categories->LoadLayout\(\)/, 'saved order, pins and dock edge are applied');
});

test('adapter catalogues and the queue are kit tables with the Material table style', async () => {
  const source = await read('LocalConverterPanel.cpp');
  assert.equal(count(source, /new MD3DataViewListCtrl\(/g), 2, 'one catalogue table per category (built in the loop) and one queue table');
  assert.equal(count(source, /md3_style_data_view\(/g), 2, 'both tables take the Material table style');
  assert.match(source, /m_jobs\s*=\s*new MD3DataViewListCtrl\([^;]*wxDV_MULTIPLE/, 'the queue keeps multi-selection for bulk actions');
  assert.match(source, /m_catalogs\[i\]\s*=\s*list/);
  assert.match(source, /wxEVT_DATAVIEW_SELECTION_CHANGED/);
  // Empty states are visible text, not a fake row that looks selectable.
  assert.match(source, /m_empty\[c\]->Show\(m_visible\[c\]\.empty\(\)\)/);
});

test('fields, labels and the scroller are kit primitives on Material roles', async () => {
  const source = await read('LocalConverterPanel.cpp');
  assert.match(source, /new MD3ScrolledWindow\(this/);
  assert.ok(count(source, /new TextInput\(/g) >= 3, 'page order, PDF title and output folder are kit fields');
  assert.match(source, /return new Label\(parent,value,LB_AUTO_WRAP\);/, 'body text is a wrapping kit label');
  assert.ok(count(source, /\btext\((?:body|page),/g) >= 6, 'introduction, details, captions, empty states, page and status lines are kit labels');
  assert.match(source, /SetBackgroundColour\(StateColor::semantic\(MD3::Role::Surface\)\)/);
  assert.match(source, /::Label::Head_20/, 'the destination title uses the kit headline face');
  assert.doesNotMatch(source, /ThemeColor::|\*wxWHITE|wxSYS_COLOUR/, 'no legacy palette');
  // Every action is a kit button with an explicit Material variant.
  const action = source.slice(source.indexOf('Button *action('), source.indexOf('\n}\n', source.indexOf('Button *action(')));
  assert.match(action, /SetVariant\(variant\)/);
  assert.match(action, /SetName\(name\)/);
  // Rotation is one exclusive choice: the chosen degree is the filled button.
  assert.match(source, /m_rotation\[r\]->SetVariant\(kRotations\[r\] == degrees \? Button::Variant::Filled : Button::Variant::Outlined\)/);
});

test('every interactive control carries an accessible name', async () => {
  const source = await read('LocalConverterPanel.cpp');
  for (const name of ['m_categories', 'm_jobs', 'm_destination', 'm_pdf_pages', 'm_pdf_title', 'm_queue_search', 'm_status'])
    assert.match(source, new RegExp(`${name}->SetName\\(`), `${name} must be named for assistive technology`);
  assert.match(source, /m_search\[i\]->SetName\(/);
  assert.match(source, /list->SetName\(/);
  assert.match(source, /pdf_search->SetName\(/);
});
