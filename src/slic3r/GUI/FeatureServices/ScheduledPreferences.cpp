#include "ScheduledPreferences.hpp"
#include "PresentationRoutes.hpp"
#include "SurfaceRegistry.hpp"
#include "../ScheduledSettings/Panel.hpp"
#include "../PersonalModes/SchoolMode.hpp"
#include "../GUI_App.hpp"
#include "../MainFrame.hpp"
#include "../I18N.hpp"
#include "../BilingualRegistry.hpp"
#include "../BilingualDecorator.hpp"
#include "../CommandPaletteIndex.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../PreferencesHistory.hpp"
#include "../Plater.hpp"
#include "../NotificationManager.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"
#include <fstream>
#include <wx/filename.h>
#include <wx/filefn.h>

namespace Slic3r::GUI::FeatureServices {
namespace {
ScheduledSettings::Registry registry()
{
    ScheduledSettings::Registry result;
    for (const auto& rule : EffectivePreferences::descriptors()) {
        ScheduledSettings::Kind kind = ScheduledSettings::Kind::Text;
        switch (rule.kind) {
        case EffectivePreferences::Kind::Boolean: kind = ScheduledSettings::Kind::Boolean; break;
        case EffectivePreferences::Kind::Integer: kind = ScheduledSettings::Kind::Integer; break;
        case EffectivePreferences::Kind::Choice: kind = ScheduledSettings::Kind::Choice; break;
        case EffectivePreferences::Kind::Color: kind = ScheduledSettings::Kind::Color; break;
        default: break;
        }
        const auto* entry = PaletteIndex::find_preference(rule.key);
        const std::string title = entry ? entry->title : "Presentation setting";
        result.add({rule.key, title, title, kind, double(rule.low), double(rule.high), rule.choices, rule.limit});
    }
    return result;
}
void notify(const char* source)
{
    if (auto* plater = wxGetApp().plater())
        plater->get_notification_manager()->push_notification(into_u8(_L(source)));
}
std::filesystem::path schedule_path() { return std::filesystem::u8path(data_dir()) / "scheduled-settings" / "schedule.json"; }
}
ScheduledPreferences::ScheduledPreferences(AppConfig& config)
    : m_config(config), m_service(registry(), {
        [this](const std::string& document) { return save(document); },
        [this](const ScheduledSettings::Effective& effective) { apply(effective); },
        [](ScheduledSettings::State state) {
            if (state == ScheduledSettings::State::Invalid || state == ScheduledSettings::State::Unavailable)
                notify("A scheduled settings source is unavailable. Open Scheduled settings for its current state.");
        }, ScheduledSettings::native_fetch}), m_timer(this)
{
    const auto file = schedule_path();
    std::error_code ec;
    if (std::filesystem::exists(file, ec)) {
        const auto length = std::filesystem::file_size(file, ec);
        if (!ec && length <= ScheduledSettings::max_payload) {
            std::ifstream input(file, std::ios::binary);
            std::string bytes(static_cast<std::size_t>(length), '\0');
            input.read(bytes.data(), bytes.size());
            std::string error;
            if (!input || !m_service.load(bytes, error)) notify("Stored schedules could not be loaded. Base settings remain active.");
        } else notify("Stored schedules exceed the supported limit. Base settings remain active.");
    }
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { pulse(); });
    m_timer.Start(1000);
    pulse();
}
ScheduledPreferences::~ScheduledPreferences() { m_timer.Stop(); m_config.clear_effective_preferences(); }
void ScheduledPreferences::pulse()
{
    if (m_applying) return;
    ScheduledSettings::Values base;
    for (const auto& rule : EffectivePreferences::descriptors())
        if (const auto value = EffectivePreferences::decode(rule, m_config.get_base(rule.key))) base.emplace(rule.key, *value);
    std::string error;
    if (!m_service.set_base(std::move(base), error)) { notify("Base settings could not be evaluated for schedules."); return; }
    ScheduledSettings::LocalTime now;
    if (!ScheduledSettings::local_time(m_service.schedule().timezone, now)) return;
    m_service.tick(now, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
void ScheduledPreferences::apply(const ScheduledSettings::Effective& effective)
{
    if (m_applying) return;
    EffectivePreferences::Values overrides;
    for (const auto& winner : effective.winners) {
        const auto value = effective.values.find(winner.first);
        if (value != effective.values.end()) overrides.emplace(value->first, value->second);
    }
    if (!m_config.replace_effective_preferences(overrides)) { notify("Scheduled values failed validation; previous settings remain active."); return; }
    if (effective.values == m_last) return;
    m_last = effective.values;
    m_applying = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{m_applying};
    m_config.suppress_presentation(PersonalModes::school_presentation_suppressed.load());
    auto& app = wxGetApp();
    I18N::configure_language_mode(m_config.get("language"), from_u8(localization_dir()));
    auto& language = I18N::language_mode_service();
    language.set_funny_level(I18N::FunnyLanguage::English, I18N::parse_funny_level(m_config.get("funny_level_en")));
    language.set_funny_level(I18N::FunnyLanguage::Cantonese, I18N::parse_funny_level(m_config.get("funny_level_yue")));
    language.set_dialog_emojis(I18N::parse_dialog_emojis(m_config.get("dialog_emojis")));
    const bool bilingual = !PersonalModes::school_presentation_suppressed.load() && language.profile().is_bilingual();
    I18N::BilingualRegistry::instance().reset(bilingual);
    I18N::enable_bilingual_decorator(bilingual);
    MD3::Metrics::setDensity(m_config.get("ui_density") == "compact" ? MD3::Metrics::Density::Compact : MD3::Metrics::Density::Comfortable);
    const auto accent = m_config.get("ui_accent_seed");
    if (!accent.empty()) MD3::setAccentSeed(wxColour(wxString::FromUTF8(accent)));
    app.Update_dark_mode_flag();
#ifdef _MSW_DARK_MODE
    app.force_colors_update();
#endif
    ::Label::rebuild_fonts(language.profile().font_language);
    app.update_ui_from_settings();
    SurfaceRegistry::instance().refresh_presentation();
    wxCommandEvent display_changed(EVT_APP_DISPLAY_NAME_CHANGED);
    app.ProcessEvent(display_changed);
}
bool ScheduledPreferences::save(const std::string& bytes)
{
    if (bytes.size() > ScheduledSettings::max_payload) return false;
    auto* history = PreferencesHistory::manager();
    if (!history) return false;
    const auto file = schedule_path();
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) return false;
    const auto stage = wxFileName::CreateTempFileName(wxString(file.parent_path().wstring()) + "/schedule-");
    if (stage.empty()) return false;
    const std::filesystem::path pending(stage.ToStdWstring());
    const auto snapshot = pending.wstring() + L".3mf";
    if (!wxRenameFile(stage, wxString(snapshot), false)) return false;
    { std::ofstream output(std::filesystem::path(snapshot), std::ios::binary | std::ios::trunc); output << bytes; output.flush(); if (!output) return false; }
    ProjectHistoryCommitOptions options;
    options.message = "Scheduled settings change";
    auto future = history->commit_snapshot(file.parent_path() / "schedules.history.3mf", std::filesystem::path(snapshot), options);
    if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready || !future.get().ok()) return false;
    // Publish only after the real local history sink confirms its snapshot.
    return wxRenameFile(wxString(snapshot), wxString(file.wstring()), true);
}
wxPanel* ScheduledPreferences::create_panel(wxWindow* parent)
{
    return new ScheduledSettingsUI::Panel(parent, m_service,
        [](const char* english, const char*) { return _L(english); },
        [](const std::string& key) { return presentation_setting_available(key, PersonalModes::school_presentation_suppressed.load()); });
}
}
