#pragma once
#include "libslic3r/ScheduledSettings/Service.hpp"
#include <wx/timer.h>
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
private:
    bool save(const std::string&);
    void apply(const ScheduledSettings::Effective&);
    AppConfig& m_config;
    ScheduledSettings::Service m_service;
    wxTimer m_timer;
    ScheduledSettings::Values m_last;
    bool m_applying = false;
};
}
