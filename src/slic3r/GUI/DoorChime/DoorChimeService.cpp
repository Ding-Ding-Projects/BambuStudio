#include "DoorChimeService.hpp"

#include "../DeviceLiveReadings.hpp"
#include "../DeviceManager.hpp"
#include "../GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"

#include <wx/timer.h>

namespace Slic3r { namespace GUI { namespace DoorChime {

namespace {

// The door reading arrives with the printer's status reports; half a second
// keeps the chime close to the moment the door moves without busy polling.
constexpr int kPollMs = 500;

AppConfig *config() { return wxGetApp().app_config; }

MachineObject *selected_printer()
{
    DeviceManager *manager = wxGetApp().getDeviceManager();
    return manager != nullptr ? manager->get_selected_machine() : nullptr;
}

int door_reading(MachineObject *obj)
{
    const auto open = DeviceLive::door_open_reading(obj->door_open_bit, obj->support_door_open_check());
    return open ? (*open ? 1 : 0) : -1;
}

Readiness readiness_of(MachineObject *obj)
{
    if (obj == nullptr || !obj->is_connected()) return Readiness::NoPrinter;
    if (obj->is_in_printing() || !may_play(obj->print_status)) return Readiness::Printing;
    if (!obj->support_door_open_check()) return Readiness::NoDoorSensor;
    return Readiness::Ready;
}

void send(MachineObject *obj, bool open, const std::string &id)
{
    const std::string gcode = chime_gcode(find_chime(open ? open_chimes() : close_chimes(), id));
    if (!gcode.empty()) obj->publish_gcode(gcode);
}

class Watcher : public wxTimer
{
public:
    void Notify() override
    {
        MachineObject *obj = selected_printer();
        if (obj == nullptr) return;
        // The detector keeps running while the chime is off, so turning it on
        // never plays for a door that moved before.
        const Edge edge = m_detector.observe(obj->get_dev_id(), door_reading(obj));
        if (edge == Edge::None || !enabled() || readiness_of(obj) != Readiness::Ready) return;
        const bool open = edge == Edge::Opened;
        send(obj, open, chosen_id(open));
    }

private:
    EdgeDetector m_detector;
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

std::string chosen_id(bool open)
{
    const std::string stored = config() != nullptr ? config()->get(open ? OPEN_KEY : CLOSE_KEY) : std::string();
    return find_chime(open ? open_chimes() : close_chimes(), stored).id;
}

void set_chosen_id(bool open, const std::string &id)
{
    if (config() != nullptr) config()->set(open ? OPEN_KEY : CLOSE_KEY, id);
}

Readiness readiness() { return readiness_of(selected_printer()); }

Readiness preview(bool open, const std::string &id)
{
    MachineObject  *obj   = selected_printer();
    const Readiness ready = readiness_of(obj);
    // A preview only needs the buzzer, so a printer without a door sensor can
    // still play one.
    if (ready == Readiness::Ready || ready == Readiness::NoDoorSensor) {
        send(obj, open, id);
        return Readiness::Ready;
    }
    return ready;
}

}}} // namespace Slic3r::GUI::DoorChime
