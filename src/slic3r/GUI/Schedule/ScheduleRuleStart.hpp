#ifndef slic3r_GUI_Schedule_ScheduleRuleStart_hpp_
#define slic3r_GUI_Schedule_ScheduleRuleStart_hpp_

// The "Start from" step of Preferences > Schedules > Add rule. It opens the
// Start from picker with the presets ScheduleRulePresets builds from the
// shipped defaults and the person's own settings (never from values invented
// here), turns the chosen preset into a new rule, and names the preferences
// history entry that saving the rule records.

#include "ScheduledSettingsModel.hpp"

#include <cstdint>
#include <string>

class wxWindow;

namespace Slic3r { namespace GUI { namespace Schedule {

struct RuleStart
{
    Rule        rule;         // fresh id, the model's default window, the preset's values
    std::string preset_title; // English source string of the chosen preset
};

// Opens the picker over `parent`; false when the person cancelled. `entropy`
// seeds the new rule's id (unique_rule_id).
bool start_rule(wxWindow *parent, std::uint32_t entropy, RuleStart &start);
// Name the preferences history entry of the save that follows
// ("Add schedule rule from preset: ..."), or drop the name when nothing was saved.
void record_rule_start(const RuleStart &start);
void forget_rule_start();

} } } // namespace Slic3r::GUI::Schedule

#endif // slic3r_GUI_Schedule_ScheduleRuleStart_hpp_
