#ifndef slic3r_GUI_Schedule_ScheduleRulePresets_hpp_
#define slic3r_GUI_Schedule_ScheduleRulePresets_hpp_

// Start-from presets for the two scheduled-settings rule editors:
//   * Preferences > Schedules (Schedule/ScheduledSettingsPanel), whose rules
//     set the AppConfig keys in ScheduledSettingsModel::allowed_keys();
//   * File > Scheduled settings (ScheduledSettings/Panel), whose rules set
//     the presentation settings its ScheduledSettings::Registry accepts.
// A new rule starts from the shipped defaults, from the person's own saved
// (base) settings, or empty, through BlankEditorPresets and the Start from
// picker. It never starts from a value invented here. The labels and choice
// names match the editors' own controls, so the picker and the editor read
// the same. wx-free: tests/blank_editor_presets builds it with g++.

#include "ScheduledSettingsModel.hpp"

#include "libslic3r/ScheduledSettings/Schedule.hpp"
#include "slic3r/GUI/Presets/BlankEditorPresets.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace ScheduleRulePresets {

// --- Preferences > Schedules -------------------------------------------------

// The nine settings a rule may set, in the rule dialog's order, with its
// labels and choices. `language_modes` are the language mode values and their
// English labels, owned by LanguageMode.hpp in the GUI.
BlankEditorPresets::EditorSpec schedule_rule_spec(const std::vector<BlankEditorPresets::Choice> &language_modes);
// The three baseline language modes with the rule dialog's labels, for the
// mode ids the GUI passes in (I18N::LANGUAGE_MODE_*).
std::vector<BlankEditorPresets::Choice> language_mode_choices(const char *english, const char *cantonese, const char *bilingual);

// The person's own values for those settings: the base value captured when a
// rule took control of a key, otherwise the live value.
BlankEditorPresets::Values saved_schedule_values(const std::map<std::string, std::string> &live,
                                                 const Schedule::OverrideState &state);

// A new rule from a preset: the rule model's own default window (all day,
// every day, no date bounds, local values) and exactly the preset's values.
Schedule::Rule schedule_rule_from(const BlankEditorPresets::Preset &preset, const std::string &id, const std::string &label);

// --- File > Scheduled settings -----------------------------------------------

// Every setting the registry accepts that `visible` lets the panel show.
BlankEditorPresets::EditorSpec service_rule_spec(const ScheduledSettings::Registry &registry,
                                                 const std::function<bool(const std::string &)> &visible);

// Typed values to and from the strings presets carry.
std::string encode(const ScheduledSettings::Value &value);
bool        decode(const ScheduledSettings::Descriptor &descriptor, const std::string &text, ScheduledSettings::Value &value);
BlankEditorPresets::Values encode_values(const ScheduledSettings::Values &values);
// False when any value is unknown to the registry or fails its validation;
// `out` is left unchanged then.
bool decode_values(const ScheduledSettings::Registry &registry, const BlankEditorPresets::Values &values,
                   ScheduledSettings::Values &out);

// --- Shared copy (English source strings) -------------------------------------

const char *picker_title();    // the Start from picker's title for a new rule
const char *picker_subtitle(); // what the picker offers
const char *history_action();  // "Add schedule rule from preset"

} } } // namespace Slic3r::GUI::ScheduleRulePresets

#endif // slic3r_GUI_Schedule_ScheduleRulePresets_hpp_
