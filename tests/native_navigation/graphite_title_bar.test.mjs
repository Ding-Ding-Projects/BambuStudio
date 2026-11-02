import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Graphite shell title bar: a language mode switch and an update chip sit
// between the appearance button and the window controls.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const bar = read('src/slic3r/GUI/BBLTopbar.cpp');
const barHeader = read('src/slic3r/GUI/BBLTopbar.hpp');
const app = read('src/slic3r/GUI/GUI_App.cpp');

const indexOf = (text, needle) => {
    const at = text.indexOf(needle);
    assert.notEqual(at, -1, needle);
    return at;
};

test('the language switch offers the three language modes and follows the Preferences flow', () => {
    assert.match(bar, /topbar_language_modes\[3\] = \{[^}]*LANGUAGE_MODE_ENGLISH,[^}]*LANGUAGE_MODE_CANTONESE_HONG_KONG,[^}]*LANGUAGE_MODE_ENGLISH_CANTONESE_HK,/);
    assert(bar.includes('wxGetApp().request_language_mode(mode)'));
    assert(app.includes('bool GUI_App::request_language_mode(const std::string &mode)'));
});

test('the update chip sits after the language switch and before the window controls', () => {
    const language = indexOf(bar, 'm_language_item = this->AddTool(ID_LANGUAGE');
    const update = indexOf(bar, 'm_update_item = this->AddTool(ID_UPDATE_READY');
    const iconize = indexOf(bar, 'this->AddTool(wxID_ICONIZE_FRAME');
    assert(language < update && update < iconize);
});

test('the update chip stays hidden until an update is ready and then restarts the app', () => {
    assert(barHeader.includes('bool              m_update_ready{false};'));
    assert(bar.includes('void BBLTopbar::apply_update_visibility()'));
    assert.match(bar, /void BBLTopbar::realize_with_hidden_items\(\)\n\{\n    Realize\(\);\n    apply_lan_drop_visibility\(\);\n    apply_update_visibility\(\);/);
    assert.match(bar, /void BBLTopbar::OnUpdateChip[\s\S]*?if \(!m_update_ready\)\n        return;[\s\S]*?restart_after_update\(\)/);
    assert(app.includes('frame->topbar()->SetUpdateReady(true);'));
});

test('the update chip uses the primary container roles and names the unsigned update', () => {
    assert.match(bar, /hover \? MD3::Role::Primary : MD3::Role::PrimaryContainer/);
    assert.match(bar, /hover \? MD3::Role::OnPrimary : MD3::Role::OnPrimaryContainer/);
    assert(bar.includes('Updates from this fork are not code-signed.'));
});

test('the search field sits after the drag spacer and opens the command palette', () => {
    const stretch = indexOf(bar, 'this->AddStretchSpacer(1);');
    const search = indexOf(bar, 'm_search_item = this->AddTool(ID_SEARCH');
    const history = indexOf(bar, 'm_history_item = this->AddTool(ID_HISTORY');
    assert(stretch < search && search < history);
    assert.match(bar, /void BBLTopbar::OnSearchField[\s\S]*?if \(!m_search_shown\)\n        return;[\s\S]*?GUI::CommandPalette::ShowPalette\(frame\)/);
    assert(bar.includes('this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnSearchField, this, ID_SEARCH);'));
});

test('the search field gives way on a narrow bar and starts hidden', () => {
    assert(barHeader.includes('bool              m_search_shown{false};'));
    assert.match(bar, /const bool search_fits = width >= FromDIP\(TOPBAR_SEARCH_MIN_BAR_W\);/);
    assert.match(bar, /apply_update_visibility\(\);\n    apply_search_visibility\(\);\n\}/);
    assert.match(bar, /item_id == ID_UPDATE_READY \|\| item_id == ID_SEARCH;/);
});

test('the search field names the palette shortcut and paints from container roles', () => {
    assert(bar.includes('PaletteIndex::palette_shortcut_label()'));
    assert.match(bar, /hover \? MD3::Role::SurfaceContainerHigh : MD3::Role::SurfaceContainer/);
    assert(bar.includes('_L("Search commands and settings")'));
});
