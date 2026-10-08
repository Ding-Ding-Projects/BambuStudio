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

test('Reset appearance to defaults restores exactly the shipped values in the table', () => {
  // The Preferences sources are fingerprinted by tests/native_preferences_atlas,
  // so the reset handler keeps its own literals; this contract fails the
  // moment either side changes without the other.
  const table = strip(read('src', 'libslic3r', 'PresentationDefaults.hpp'));
  const shipped = (key) => {
    if (key === 'ui_accent_seed') return table.match(/kAccentSeed = "([^"]*)"/)[1];
    return table.match(new RegExp(`\\{"${key}", "([^"]*)"\\}`))[1];
  };
  const prefs = read('src', 'slic3r', 'GUI', 'Preferences.cpp');
  const start = prefs.indexOf('reset_btn->Bind(wxEVT_BUTTON');
  assert.ok(start > 0, 'the reset handler exists');
  const handler = prefs.slice(start, prefs.indexOf('apply_fonts();', start));
  for (const key of ['ui_density', 'ui_accent_seed', 'ui_font_family', 'ui_font_scale']) {
    const written = handler.match(new RegExp(`app_config->set\\("${key}", "([^"]*)"\\);`));
    assert.ok(written, `the reset writes ${key}`);
    assert.equal(written[1], shipped(key), `${key}: reset and the shipped-defaults preset agree`);
  }
  assert.match(handler, new RegExp(`MD3::setAccentSeed\\(wxColour\\(wxString::FromUTF8\\("${shipped('ui_accent_seed')}"\\)\\)\\)`));
  // The controls the reset re-selects show the same shipped values.
  assert.match(prefs, /density->SetOptions\(\{_L\("Comfortable"\), _L\("Compact"\)\}\);/);
  assert.match(handler, /density->SetSelection\(0\);/);
  assert.equal(shipped('ui_density'), 'comfortable');
  const scales = prefs.match(/kScaleStrs = \{([^}]*)\};/)[1].split(',').map((v) => v.trim().replace(/"/g, ''));
  const scaleIndex = Number(handler.match(/text_size->SetSelection\((\d+)\);/)[1]);
  assert.equal(scales[scaleIndex], shipped('ui_font_scale'));
  const families = prefs.match(/std::vector<std::string> font_values = \{([^}]*)\};/)[1].split(',').map((v) => v.trim().replace(/"/g, ''));
  const familyIndex = Number(handler.match(/font_combo->SetSelection\((\d+)\);/)[1]);
  assert.equal(families[familyIndex], shipped('ui_font_family'));
  const seeds = [...prefs.matchAll(/\{"(#[0-9a-f]{6})", _L\("[A-Za-z]+"\)\}/g)].map((m) => m[1]);
  assert.equal(seeds[0], shipped('ui_accent_seed'), 'the reset selects the first swatch, which is the shipped seed');
  assert.match(handler, /\(\*swatches\)\[j\]->SetSelected\(j == 0\);/);
  // The Material token seed is the same colour.
  assert.match(read('src', 'slic3r', 'GUI', 'Widgets', 'MD3Tokens.hpp'), new RegExp(`inline const wxColour seed\\{"${shipped('ui_accent_seed')}"\\};`));
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

// Applying a preset states what it set and is a normal recorded action in
// local history, undoable from Version history like any other change.

test('an applied preset is restated right after it is applied', () => {
  const picker = strip(read('src', 'slic3r', 'GUI', 'Presets', 'StartFromPicker.cpp'));
  assert.match(picker, /wxString preset_applied_note\(const Preset &preset\)/);
  assert.match(picker, /_L\("Started from \\"%s\\"\. It set %s\."\)/);
  assert.match(picker, /_L\("Started from \\"%s\\"\. It sets nothing yet\."\)/);
});

test('the appearance Presets page states what the selected preset sets, before and after applying', () => {
  const editor = strip(read('src', 'slic3r', 'GUI', 'Appearance', 'AppearanceEditorPopover.cpp'));
  assert.match(editor, /m_preset_detail->SetName\(_L\("What the selected appearance preset sets"\)\)/);
  const detail = editor.slice(editor.indexOf('void AppearanceEditorPopover::show_preset_detail()'), editor.indexOf('void AppearanceEditorPopover::apply_preset('));
  assert.match(detail, /statement = AppearanceEditor::preset_statement\(name\);/);
  assert.match(detail, /text << describe_preset\(statement\);\s*m_preset_detail->SetLabel\(text\);/,
    'the statement follows the selection before anything is applied');
  assert.match(editor, /show_preset_detail\(\);\s*reflow_preset_page\(\);\s*\}\);/, 'selecting a preset restates it');
  assert.match(editor, /_L\("Select a preset to see exactly what it sets before you apply it\."\)/);
  assert.match(editor, /_L\("Applied \\"%s\\"\. It sets %s\."\)/, 'the active line restates what the applied preset set');
  assert.match(editor, /BlankEditorPresets::style_assignments\(bags\)/, 'the statement comes from the preset itself');
  // Both the Apply button and a double-click go through the recorded path.
  const applies = editor.match(/apply_preset\(m_preset_visible\[sel\]\)/g) ?? [];
  assert.equal(applies.length, 2);
  assert.doesNotMatch(editor, /set_active_preset\(m_preset_visible\[sel\]\)/, 'no unrecorded apply path is left');
});

test('every appearance preset action is a named local history revision', () => {
  const editor = strip(read('src', 'slic3r', 'GUI', 'Appearance', 'AppearanceEditorPopover.cpp'));
  for (const action of ['Apply appearance preset', 'Save appearance preset', 'Delete appearance preset', 'Reset element appearance']) {
    assert.match(editor, new RegExp(`PreferencesHistory::begin_appearance_action\\(BlankEditorPresets::history_label\\("${action}"`), action);
  }
  assert.match(editor, /PreferencesHistory::begin_appearance_action\("Reset all appearance"\)/);
  assert.match(editor, /PreferencesHistory::begin_appearance_action\("Import appearance theme"\)/);
  assert.match(editor, /if \(!r\.ok\) \{\s*PreferencesHistory::cancel_appearance_action\(\);/, 'a failed import does not lend its name to a later change');

  const style = strip(read('src', 'slic3r', 'GUI', 'Appearance', 'ElementStyle.cpp'));
  assert.match(style, /else if \(const SaveObserver &observer = save_observer_state\(\)\)\s*observer\(\);/, 'a successful save notifies history');

  const history = strip(read('src', 'slic3r', 'GUI', 'PreferencesHistory.cpp'));
  assert.match(history, /ElementStyle::set_save_observer\(\[\]\(\) \{ on_appearance_saved\(\); \}\);/);
  assert.match(history, /commit_appearance\("Appearance at startup"\);/, 'the state before the first change is kept');
  assert.match(history, /return profiles_root\(\) \/ "appearance\.history\.3mf";/);
  assert.match(history, /if \(!pending_appearance_label\.empty\(\)\) \{[\s\S]*?commit_appearance\(label\);/, 'a named action is recorded at once');
  assert.match(history, /if \(appearance_timer\(\)->IsRunning\(\)\) \{[\s\S]*?commit_appearance\(kAppearanceChange\);/, 'earlier unnamed changes keep their own revision');
  assert.match(history, /begin_appearance_action\("Restore appearance snapshot"\);/);
});

test('the command palette states and records an appearance preset the same way', () => {
  const palette = strip(read('src', 'slic3r', 'GUI', 'CommandPalette.cpp'));
  assert.match(palette, /preset_settings_line\(AppearanceEditor::preset_statement\(name\)\)/);
  assert.match(palette, /\[name\]\(\) \{ AppearanceEditor::apply_preset\(name\); \}/);
  const editor = strip(read('src', 'slic3r', 'GUI', 'Appearance', 'AppearanceEditorPopover.cpp'));
  const apply = editor.slice(editor.indexOf('bool apply_preset(const std::string &name)'));
  assert.match(apply, /PreferencesHistory::begin_appearance_action\(BlankEditorPresets::history_label\("Apply appearance preset", name\)\);\s*reg\.set_active_preset\(name\);\s*return ElementStyle::save\(\);/);
});

test('Version history lists, compares and restores appearance revisions', () => {
  const dialog = strip(read('src', 'slic3r', 'GUI', 'ProjectHistoryDialog.cpp'));
  assert.match(dialog, /append\(prefs, appearance_identity, "appearance", "Appearance"\);/);
  assert.match(dialog, /_L\("Printer"\), _L\("Appearance"\)[,}]/, 'an Appearance category filter, appended so saved searches keep their indices');
  assert.match(dialog, /categories\[\]=\{"","project","preferences","preset","draft","printer","appearance"[,}]/);
  assert.match(dialog, /PreferencesHistory::apply_appearance_snapshot\(result\.restored_path, error\)/);
  assert.match(dialog, /can_restore=[^;]*origin\.category=="appearance"/);
  assert.match(dialog, /left_document\.flatten\(\)\.items\(\)/, 'a comparison names each changed property');
  // Stored English messages are shown in the active language; the preset name stays as stored.
  assert.match(dialog, /L\("Apply appearance preset: %s"\), L\("Save appearance preset: %s"\), L\("Delete appearance preset: %s"\),/);
  assert.match(dialog, /return wxString::Format\(_\(format\), result\.Mid\(prefix\.size\(\)\)\);/);
  for (const message of ['Appearance change', 'Appearance at startup', 'Reset all appearance', 'Import appearance theme', 'Restore appearance snapshot']) {
    assert.ok(dialog.includes(`L("${message}")`), message);
  }
});

// Both scheduled-rule editors start a new rule from a preset: the shipped
// defaults, the person's own (base) settings or nothing, never an invented value.

test('the Scheduled settings window starts a rule from a preset, not an invented theme', () => {
  const panel = strip(read('src', 'slic3r', 'GUI', 'ScheduledSettings', 'Panel.cpp'));
  assert.doesNotMatch(panel, /\{"theme",std::string\("dark"\)\}/, 'the invented seed is gone');
  assert.match(panel, /button\(scroll,actions,"Add rule","新增規則",\[this\]\{if\(!retain_editor\(\)\)return;add_rule\(\);\}\);/);
  const add = panel.slice(panel.indexOf('void Panel::add_rule(){'), panel.indexOf('void Panel::load_editor(){'));
  assert.match(add, /RP::service_rule_spec\(m_service\.registry\(\),m_visible\)/, 'the registry and the visibility filter define the settings');
  assert.match(add, /BlankEditorPresets::start_presets\(spec,RP::encode_values\(m_service\.base\(\)\)\)/, 'saved settings are the base the service evaluates');
  assert.match(add, /StartFromPicker picker\(this,/);
  assert.match(add, /RP::decode_values\(m_service\.registry\(\),preset\.values\(\),r\.values\)/);
  assert.match(add, /show_status\(preset_applied_note\(preset\)/, 'what the preset set is stated right after');
  assert.match(panel, /m_service\.label_next_change\(label\);if\(!m_service\.replace\(draft,error\)\)\{m_service\.take_change_label\(\);/,
    'saving records the preset the rule started from');
  assert.match(panel, /found\?wxs\(std::get<std::string>\(value\)\):wxString\(\)/, 'no wxString / wxChar* conditional');
  const service = strip(read('src', 'libslic3r', 'ScheduledSettings', 'Service.hpp'));
  assert.match(service, /const Values& base\(\) const \{ return m_base; \}/);
  const owner = strip(read('src', 'slic3r', 'GUI', 'FeatureServices', 'ScheduledPreferences.cpp'));
  assert.match(owner, /options\.message = label\.empty\(\) \? std::string\("Scheduled settings change"\) : label;/);
  assert.match(owner, /bool ScheduledPreferences::restore\(/);
});

test('Preferences > Schedules starts a rule from a preset with the model default window', () => {
  const panel = strip(read('src', 'slic3r', 'GUI', 'Schedule', 'ScheduledSettingsPanel.cpp'));
  const add = panel.slice(panel.indexOf('void ScheduledSettingsPanel::add_rule()'), panel.indexOf('void ScheduledSettingsPanel::edit_selected()'));
  assert.doesNotMatch(add, /start_time\s*=\s*\{20, 0\}|end_time\s*=\s*\{7, 0\}/, 'no invented evening window');
  assert.match(add, /RP::schedule_rule_spec\(RP::language_mode_choices\(/);
  assert.match(add, /RP::saved_schedule_values\(live, Scheduler::instance\(\)\.override_state\(\)\)/, 'a rule-controlled key starts from the person\'s own value');
  assert.match(add, /StartFromPicker picker\(this, _L\(RP::picker_title\(\)\), _L\(RP::picker_subtitle\(\)\),/);
  assert.match(add, /RP::schedule_rule_from\(preset,/);
  assert.match(add, /ScheduleRuleDialog dlg\(this, rule, true, preset_applied_note\(preset\)\);/, 'the editor restates what the preset set');
  assert.match(add, /PreferencesHistory::label_next_snapshot\(BlankEditorPresets::history_label\(RP::history_action\(\), preset\.title\)\);/);
  assert.match(panel, /if \(!started_from\.empty\(\)\)\s*m_body->Add\(make_label\(started_from, true, true\)/);
  const history = strip(read('src', 'slic3r', 'GUI', 'PreferencesHistory.cpp'));
  assert.match(history, /Schedule::kDocumentConfigKey\n\s*\};/, 'the rule document is recorded with the preferences');
  assert.match(history, /Schedule::Scheduler::instance\(\)\.reload\(\);/, 'a restore reloads the rules');
  assert.match(history, /if \(key != Schedule::kDocumentConfigKey\) config->erase\("app", key\);/, 'older snapshots never erase the rules');
});

test('Version history lists, compares and restores Scheduled settings versions', () => {
  const dialog = strip(read('src', 'slic3r', 'GUI', 'ProjectHistoryDialog.cpp'));
  assert.match(dialog, /append\(prefs, schedules_identity, "schedules", "Scheduled settings"\);/);
  assert.match(dialog, /_L\("Appearance"\), _L\("Scheduled settings"\)\}/);
  assert.match(dialog, /"appearance","schedules"\}/);
  assert.match(dialog, /service->restore\(result\.restored_path, error\)/);
  assert.match(dialog, /L\("Add schedule rule from preset: %s"\)/);
  for (const message of ['Scheduled settings change', 'Restore scheduled settings']) assert.ok(dialog.includes(`L("${message}")`), message);
});
