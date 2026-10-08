#include "ScheduleRuleStart.hpp"
#include "ScheduleRulePresets.hpp"
#include "ScheduledSettings.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/LanguageMode.hpp"
#include "slic3r/GUI/PreferencesHistory.hpp"
#include "slic3r/GUI/Presets/StartFromPicker.hpp"

#include "libslic3r/AppConfig.hpp"

#include <map>

namespace Slic3r { namespace GUI { namespace Schedule {

bool start_rule(wxWindow *parent, std::uint32_t entropy, RuleStart &start)
{
    namespace RP = ScheduleRulePresets;
    std::map<std::string, std::string> live;
    if (AppConfig *cfg = wxGetApp().app_config)
        for (const std::string &key : allowed_keys())
            live[key] = cfg->get(key);
    const BlankEditorPresets::EditorSpec spec = RP::schedule_rule_spec(RP::language_mode_choices(
        I18N::LANGUAGE_MODE_ENGLISH_US, I18N::LANGUAGE_MODE_CANTONESE_HONG_KONG, I18N::LANGUAGE_MODE_ENGLISH_CANTONESE_HK));
    // A key a rule controls right now starts from the person's own value.
    StartFromPicker picker(parent, _L(RP::picker_title()), _L(RP::picker_subtitle()),
                           BlankEditorPresets::start_presets(spec, RP::saved_schedule_values(live, Scheduler::instance().override_state())));
    if (picker.ShowModal() != wxID_OK || picker.chosen() == nullptr)
        return false;
    const BlankEditorPresets::Preset &preset = *picker.chosen();
    start.rule = RP::schedule_rule_from(preset, unique_rule_id(Scheduler::instance().document(), entropy),
                                        into_u8(_L(preset.title.c_str())));
    start.preset_title = preset.title;
    return true;
}

void record_rule_start(const RuleStart &start)
{
    // Recorded in the preferences history like any other change.
    PreferencesHistory::label_next_snapshot(
        BlankEditorPresets::history_label(ScheduleRulePresets::history_action(), start.preset_title));
}

void forget_rule_start() { PreferencesHistory::label_next_snapshot(std::string()); }

} } } // namespace Slic3r::GUI::Schedule
