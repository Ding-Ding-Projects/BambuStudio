#pragma once
#include "../TtsNarrator.hpp"
#include <wx/timer.h>
#include <functional>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r::GUI::FeatureServices {
class NarratorEnvironment final : public wxEvtHandler {
public:
    explicit NarratorEnvironment(std::function<bool()> quiet)
        : m_quiet(std::move(quiet)), m_timer(this)
    {
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { refresh(); });
        refresh();
        m_timer.Start(1000);
    }
    ~NarratorEnvironment() override { m_timer.Stop(); }
private:
    void refresh()
    {
        bool screen_reader = false;
#ifdef _WIN32
        BOOL active = FALSE;
        if (::SystemParametersInfoW(SPI_GETSCREENREADER, 0, &active, 0)) screen_reader = active != FALSE;
#endif
        TtsNarrator::set_quiet(m_quiet && m_quiet(), screen_reader);
    }
    std::function<bool()> m_quiet;
    wxTimer m_timer;
};
}
