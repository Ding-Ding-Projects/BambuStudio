#pragma once

#include "WorkspaceBundle.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Slic3r::Workspace {

struct PlanningWarning {
    std::string code;
    std::string item_id;
    std::string related_id;
};

struct Reminder {
    std::string item_id;
    std::string kind;
    std::int64_t due_utc = 0;
};

// Named IANA zones need a timezone-rule resolver before their 09:00 UTC
// instant can be trusted. The current planner verifies only UTC deadlines.
bool deadline_offset_verifiable(const Workspace &workspace, const ChecklistItem &item);

// Calendar instants are stored as UTC seconds. The selected IANA zone and
// per-event offset are retained for display and to disambiguate DST folds.
std::vector<PlanningWarning> validate_plan(const Workspace &workspace,
                                           const std::vector<std::string> &available_printer_ids);
std::vector<Reminder> due_reminders(const Workspace &workspace, std::int64_t last_checked_utc,
                                    std::int64_t now_utc);
std::vector<CalendarSlot> agenda(const Workspace &workspace, std::int64_t from_utc, std::int64_t until_utc);
std::vector<CalendarSlot> month_slots(const Workspace &workspace, int year, int month, int utc_offset_minutes);

bool add_checklist_item(Workspace &workspace, ChecklistItem item);
bool edit_checklist_item(Workspace &workspace, const ChecklistItem &item);
bool reorder_checklist_item(Workspace &workspace, const std::string &id, std::size_t index);
bool complete_checklist_item(Workspace &workspace, const std::string &id, bool completed);
bool snooze_slot(Workspace &workspace, const std::string &id, std::int64_t until_utc);
bool dismiss_slot_reminder(Workspace &workspace, const std::string &id);

std::string checklist_json(const Workspace &workspace);
std::string checklist_csv(const Workspace &workspace);
std::string calendar_ics(const Workspace &workspace);

} // namespace Slic3r::Workspace
