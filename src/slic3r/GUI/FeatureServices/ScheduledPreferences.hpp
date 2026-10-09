#pragma once
#include "libslic3r/ScheduledSettings/Service.hpp"
#include <wx/timer.h>
#include <filesystem>
#include <string>
class wxPanel;
class wxWindow;
namespace Slic3r { class AppConfig; }
namespace Slic3r::GUI::FeatureServices {
class ScheduledPreferences final : public wxEvtHandler {
public:
    explicit ScheduledPreferences(AppConfig&);
    ~ScheduledPreferences() override;
    wxPanel* create_panel(wxWindow*);
    void pulse();
    // Local history: where every saved schedule is recorded, and putting a
    // recorded version back (itself recorded as a restore).
    static std::filesystem::path history_identity();
    bool restore(const std::filesystem::path& snapshot, std::string& error);
private:
    bool save(const std::string&);
    void apply(const ScheduledSettings::Effective&);
    AppConfig& m_config;
    ScheduledSettings::Service m_service;
    wxTimer m_timer;
    ScheduledSettings::Values m_last;
    bool m_applying = false;
    // The schedule as it was before this session's first change is recorded
    // once, so that change can be undone from Version history.
    bool m_recorded_start = false;
};
}
