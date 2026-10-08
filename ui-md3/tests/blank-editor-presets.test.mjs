import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Blank-slate editors offer start-from presets built only from the app's real
// defaults, the person's saved values and shipped templates. These contracts
// read the sources as text, so they run without a build:
//   * the shipped presentation defaults live in one table that the reset paths
//     (AppConfig::set_defaults and Preferences > Appearance > Reset) read too,
//     so a preset and a reset cannot disagree;
//   * the Start-from picker filters with the shared regex-capable search field,
//     names its list and its statement for assistive technology, and has
//     Enter / Escape keyboard paths.
// The model itself is covered by tests/blank_editor_presets (Catch2).

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFileSync(path.join(repoDir, ...parts), 'utf8').replace(/\r\n/g, '\n');
const strip = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const exists = (...parts) => existsSync(path.join(repoDir, ...parts));

test('the shipped presentation defaults live in one shared table', () => {
  assert.ok(exists('src', 'libslic3r', 'PresentationDefaults.hpp'), 'PresentationDefaults.hpp exists');
  const table = strip(read('src', 'libslic3r', 'PresentationDefaults.hpp'));
  for (const [key, value] of [['dark_color_mode', '0'], ['ui_density', 'comfortable'], ['ui_font_family', ''],
                              ['ui_font_scale', '1.0'], ['motion_preference', 'system'], ['narrator_language', 'en']]) {
    assert.match(table, new RegExp(`\\{"${key}", "${value.replace('.', '\\.')}"\\}`), `${key} ships as "${value}"`);
  }
  assert.match(table, /\{"ui_accent_seed", kAccentSeed\}/);
  assert.match(table, /kAccentSeed = "#146c2e"/);
  assert.match(table, /unfixed_keys\(\)[\s\S]*"language"/, 'the language is listed as having no single shipped value');
  assert.doesNotMatch(table, /#include <wx\//, 'the table stays free of wxWidgets');
});

test('AppConfig fills missing presentation keys from the shared table', () => {
  const config = strip(read('src', 'libslic3r', 'AppConfig.cpp'));
  assert.match(config, /#include "PresentationDefaults\.hpp"/);
  assert.match(config, /set\("dark_color_mode", PresentationDefaults::value_or_empty\("dark_color_mode"\)\)/);
  assert.match(config, /set\("motion_preference", PresentationDefaults::value_or_empty\("motion_preference"\)\)/);
  assert.match(config, /"narrator_enabled", "narrator_language", "narrator_rate_en"[^\n]*\n\s*if \(get\(key\)\.empty\(\)\) set\(key, PresentationDefaults::value_or_empty\(key\)\);/);
  assert.doesNotMatch(config, /set\("dark_color_mode", "0"\)/, 'no second copy of the shipped theme');
  assert.doesNotMatch(config, /set\("narrator_language", "en"\)/, 'no second copy of the shipped narrator language');
});

test('Reset appearance to defaults reads the same table', () => {
  const prefs = strip(read('src', 'slic3r', 'GUI', 'Preferences.cpp'));
  const start = prefs.indexOf('reset_btn->Bind(wxEVT_BUTTON');
  assert.ok(start > 0, 'the reset handler exists');
  const handler = prefs.slice(start, prefs.indexOf('apply_fonts();', start));
  for (const key of ['ui_density', 'ui_accent_seed', 'ui_font_family', 'ui_font_scale']) {
    assert.match(handler, new RegExp(`PresentationDefaults::value_or_empty\\("${key}"\\)`), `${key} comes from the table`);
  }
  assert.doesNotMatch(handler, /"comfortable"|"#146c2e"|"1\.0"/, 'no literal shipped value in the reset handler');
  assert.doesNotMatch(handler, /SetSelection\(0\);\s*\/\/ Comfortable/, 'selections follow the table, not fixed indices');
  assert.match(prefs, /#include "libslic3r\/PresentationDefaults\.hpp"/);
});

test('the preset model has no way to accept an invented starting value', () => {
  const header = strip(read('src', 'slic3r', 'GUI', 'Presets', 'BlankEditorPresets.hpp'));
  const model = strip(read('src', 'slic3r', 'GUI', 'Presets', 'BlankEditorPresets.cpp'));
  assert.match(header, /std::vector<Preset> start_presets\(const EditorSpec &spec, const Values &saved\);/);
  assert.match(header, /Preset template_preset\(/);
  assert.match(model, /PresentationDefaults::value\(field\.key\)/, 'shipped defaults come from the table');
  assert.doesNotMatch(model, /"dark_color_mode"|"ui_density"|"#146c2e"/, 'the model names no setting and no value of its own');
  assert.doesNotMatch(header + model, /#include <wx\//, 'the model stays free of wxWidgets');
});

test('the Start-from picker states what a preset does and is keyboard and screen reader ready', () => {
  const picker = strip(read('src', 'slic3r', 'GUI', 'Presets', 'StartFromPicker.cpp'));
  assert.match(picker, /SearchField::MatchPass pass\(m_search->GetValue\(\), m_search->IsRegexEnabled\(\)/, 'the search uses the shared regex builder matcher');
  assert.match(picker, /m_search->SetOnRegexToggle\(/);
  assert.match(picker, /m_list->SetName\(_L\("Presets to start from"\)\)/);
  assert.match(picker, /m_detail->SetName\(_L\("What the highlighted preset creates and sets"\)\)/);
  assert.match(picker, /m_detail->ChangeValue\(index >= 0 \? describe_preset\(/, 'the statement follows the highlighted preset before anything is applied');
  assert.match(picker, /key == WXK_ESCAPE/);
  assert.match(picker, /key == WXK_RETURN \|\| key == WXK_NUMPAD_ENTER/);
  assert.match(picker, /wxEVT_LISTBOX_DCLICK/);
  assert.match(picker, /_L\("Sets:"\)/);
  assert.match(picker, /_L\("Leaves out:"\)/);
  assert.match(picker, /_L\("No preset matches the search\. Clear the search to see every preset\."\)/, 'an honest no-match message');
  assert.match(picker, /static_assert\(Slic3r::PresentationDefaults::kFunnyLevel == Slic3r::GUI::I18N::FUNNY_LEVEL_DEFAULT/);
});

test('the preset sources are built, extracted for translation and tested', () => {
  const cmake = read('src', 'slic3r', 'CMakeLists.txt');
  assert.match(cmake, /GUI\/Appearance\/AppearanceEditorPopover\.hpp\n\s+GUI\/Presets\/BlankEditorPresets\.cpp\n\s+GUI\/Presets\/BlankEditorPresets\.hpp\n\s+GUI\/Presets\/StartFromPicker\.cpp\n\s+GUI\/Presets\/StartFromPicker\.hpp\n/);
  assert.match(read('tests', 'CMakeLists.txt'), /add_subdirectory\(blank_editor_presets\)/);
  assert.match(read('tests', 'blank_editor_presets', 'CMakeLists.txt'), /add_test\(NAME blank_editor_presets_tests/);
  const list = read('bbl', 'i18n', 'list.txt');
  assert.match(list, /^src\/slic3r\/GUI\/Presets\/BlankEditorPresets\.cpp$/m);
  assert.match(list, /^src\/slic3r\/GUI\/Presets\/StartFromPicker\.cpp$/m);
});
