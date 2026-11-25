#include "PrintCooldownService.hpp"

#include "../DeviceManager.hpp"
#include "../DeviceCore/DevBed.h"
#include "../DeviceCore/DevChamber.h"
#include "../DeviceCore/DevManager.h"
#include "../GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"

#include <wx/timer.h>
#include <wx/utils.h>

#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI { namespace PrintCooldown {

namespace {

// Temperatures arrive with the printer's status reports about once a second;
// polling at the same pace turns the fans off close to the moment it is cool.
constexpr int kPollMs = 1000;

AppConfig *config() { return wxGetApp().app_config; }

MachineObject *selected_printer()
{
    DeviceManager *manager = wxGetApp().getDeviceManager();
    MachineObject *obj     = manager != nullptr ? manager->get_selected_machine() : nullptr;
    return obj != nullptr && obj->is_connected() ? obj : nullptr;
}

float bed_temp(MachineObject *obj) { return obj->GetBed() != nullptr ? obj->GetBed()->GetBedTemp() : 0.0f; }
float chamber_temp(MachineObject *obj) { return obj->GetChamber() ? obj->GetChamber()->GetChamberTemp() : 0.0f; }

bool fan_control_blocked() { return wxGetApp().is_monitor_ctrl_blocked(MonitorCtrlId::Fan); }

class Watcher : public wxTimer
{
public:
    void Notify() override
    {
        MachineObject *obj = selected_printer();
        // While the printer is out of reach the session keeps its state; the
        // timeout is checked against the clock as soon as it is back.
        if (obj == nullptr) return;
        const Action action = m_session.observe(obj->get_dev_id(), obj->print_status, bed_temp(obj), chamber_temp(obj),
                                                wxGetLocalTimeMillis().GetValue(), enabled() && !fan_control_blocked());
        if (action == Action::None) return;
        const bool on = action == Action::FansOn;
        BOOST_LOG_TRIVIAL(info) << "Cool down after print: fans " << (on ? "on" : "off") << ", bed " << bed_temp(obj) << " C, chamber "
                                << chamber_temp(obj) << " C";
        obj->publish_gcode(fans_gcode(on));
    }

    const Session &session() const { return m_session; }

private:
    Session m_session;
};

Watcher *watcher()
{
    static Watcher *s_watcher = new Watcher(); // app-lifetime
    return s_watcher;
}

} // namespace

void install() { watcher()->Start(kPollMs); }

bool enabled() { return config() != nullptr && config()->get(ENABLED_KEY) == "true"; }

void set_enabled(bool on)
{
    if (config() != nullptr) config()->set(ENABLED_KEY, on ? "true" : "false");
}

State state()
{
    State          result;
    MachineObject *obj = selected_printer();
    result.connected   = obj != nullptr;
    result.blocked     = fan_control_blocked();
    result.phase       = watcher()->session().phase();
    result.remaining_s = int((watcher()->session().remaining_ms(wxGetLocalTimeMillis().GetValue()) + 999) / 1000);
    if (obj != nullptr) {
        result.bed_c     = bed_temp(obj);
        result.chamber_c = chamber_temp(obj);
    }
    return result;
}

}}} // namespace Slic3r::GUI::PrintCooldown
