#include "ThemeFollowWatcher.hpp"
#include "ThemeFollow.hpp"

#include "../GLCanvas3D.hpp"
#include "../GUI_App.hpp"
#include "../MainFrame.hpp"
#include "../Plater.hpp"

#include "libslic3r/AppConfig.hpp"

#include <wx/timer.h>
#ifdef _WIN32
#include <wx/msw/registry.h>
#endif

#include <memory>

namespace Slic3r::GUI::ThemeFollow {

namespace {

class Watcher : public wxTimer
{
public:
    void Notify() override
    {
        AppConfig *cfg = wxGetApp().app_config;
        if (cfg == nullptr) return;
        const std::optional<bool> change = follow_change(cfg->get("dark_color_mode"), cfg->get(kFollowKey), windows_prefers_dark());
        if (!change) return;
        cfg->set("dark_color_mode", *change ? "1" : "0");
        cfg->save();
        apply_now(*change);
    }
};

std::unique_ptr<Watcher> &watcher()
{
    static std::unique_ptr<Watcher> w;
    return w;
}

} // namespace

std::optional<bool> windows_prefers_dark()
{
#ifdef _WIN32
    wxRegKey key(wxRegKey::HKCU, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
    if (!key.Exists() || !key.HasValue("AppsUseLightTheme")) return std::nullopt;
    long value = -1;
    if (!key.QueryValue("AppsUseLightTheme", &value)) return std::nullopt;
    return windows_dark_from(value);
#else
    return std::nullopt;
#endif
}

void apply_at_startup()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr) return;
    const std::optional<bool> change = follow_change(cfg->get("dark_color_mode"), cfg->get(kFollowKey), windows_prefers_dark());
    if (!change) return;
    cfg->set("dark_color_mode", *change ? "1" : "0");
    cfg->save();
}

void start_watching()
{
    if (watcher()) return;
    watcher() = std::make_unique<Watcher>();
    watcher()->Start(3000);
}

void stop_watching()
{
    if (watcher()) watcher()->Stop();
    watcher().reset();
}

void apply_now(bool dark)
{
    (void) dark; // dark_color_mode already holds it; the app reads it from there
    wxGetApp().Update_dark_mode_flag();
#ifdef _MSW_DARK_MODE
    wxGetApp().force_colors_update();
    wxGetApp().update_ui_from_settings();
#endif
    if (Plater *plater = wxGetApp().plater()) {
        SimpleEvent evt = SimpleEvent(EVT_GLCANVAS_COLOR_MODE_CHANGED);
        wxPostEvent(plater, evt);
    }
    if (wxGetApp().mainframe) wxGetApp().mainframe->Refresh();
}

} // namespace Slic3r::GUI::ThemeFollow
