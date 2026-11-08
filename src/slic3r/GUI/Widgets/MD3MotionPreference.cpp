#include "MD3Motion.hpp"
#include "../GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"
#include <wx/app.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace MD3 { namespace Motion {

bool reduced()
{
    bool system_reduced = false;
#ifdef _WIN32
    BOOL animate = TRUE;
    if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0))
        system_reduced = animate == FALSE;
#endif
    const auto *app = dynamic_cast<Slic3r::GUI::GUI_App*>(wxTheApp);
    const std::string preference = app && app->app_config ? app->app_config->get("motion_preference") : "system";
    // Low stimulation (an attention mode) adds to the reduction; it never brings motion back.
    const bool low_stimulation = app && app->app_config && app->app_config->get("attention_low_stimulation") == "true";
    return low_stimulation || reduce_motion(preference, system_reduced);
}

} } // namespace MD3::Motion
