#pragma once
#include "ServerModel.hpp"
#include "WorldRecoveryDialog.hpp"
#include <vector>
class wxWindow;
namespace Slic3r { namespace GUI { namespace GameServers {
struct ManagedSchedule
{
    WorldRecoveryTarget target;
    Schedule            schedule;
    std::time_t         next = 0, warned_for = 0;
    std::string         last;
};
enum class ScheduleIntent { None, Missed, Warn, Restart };
inline ScheduleIntent managed_schedule_due(ManagedSchedule &s, std::time_t now, bool resumed)
{
    if (!s.schedule.restart_on) return ScheduleIntent::None;
    if (!s.next) {
        s.next = next_restart(s.schedule, now);
        return ScheduleIntent::None;
    }
    if (s.next <= now) {
        const bool missed = resumed || now - s.next > 90;
        s.next            = next_restart(s.schedule, now);
        s.warned_for      = 0;
        return missed ? ScheduleIntent::Missed : ScheduleIntent::Restart;
    }
    if (s.schedule.warn_minutes > 0 && s.warned_for != s.next && now >= s.next - s.schedule.warn_minutes * 60) {
        if (resumed) return ScheduleIntent::None;
        s.warned_for = s.next;
        return ScheduleIntent::Warn;
    }
    return ScheduleIntent::None;
}
void poll_managed_schedules();
void show_managed_schedule(wxWindow *parent, WorldRecoveryTarget target);
}}} // namespace Slic3r::GUI::GameServers
