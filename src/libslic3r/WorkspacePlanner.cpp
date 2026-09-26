#include "WorkspacePlanner.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace Slic3r::Workspace {
namespace {

// Civil-date arithmetic is independent of the host's local time zone.
std::int64_t days_from_civil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned yoe_days = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<int>(yoe_days) - 719468;
}

bool parse_date(const std::string &date, int &year, int &month, int &day)
{
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    for (std::size_t index = 0; index < date.size(); ++index)
        if (index != 4 && index != 7 && (date[index] < '0' || date[index] > '9')) return false;
    year = std::stoi(date.substr(0, 4));
    month = std::stoi(date.substr(5, 2));
    day = std::stoi(date.substr(8, 2));
    if (year < 1970 || year > 9999 || month < 1 || month > 12 || day < 1) return false;
    static const int lengths[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day <= lengths[month] + (month == 2 && leap ? 1 : 0);
}

std::string csv_field(const std::string &value)
{
    std::string result = "\"";
    for (char c : value) {
        if (c == '"') result += '"';
        result += c;
    }
    return result + '"';
}

std::string ics_escape(const std::string &value)
{
    std::string result;
    for (char c : value) {
        if (c == '\\' || c == ';' || c == ',') result += '\\';
        if (c == '\n') result += "\\n";
        else if (c != '\r') result += c;
    }
    return result;
}

std::string utc_stamp(std::int64_t seconds)
{
    std::time_t time = static_cast<std::time_t>(seconds);
    std::tm value{};
#ifdef _WIN32
    gmtime_s(&value, &time);
#else
    gmtime_r(&time, &value);
#endif
    char output[32]{};
    std::strftime(output, sizeof(output), "%Y%m%dT%H%M%SZ", &value);
    return output;
}

template<class T> T *by_id(std::vector<T> &items, const std::string &id)
{
    const auto found = std::find_if(items.begin(), items.end(), [&](const T &item) { return item.id == id; });
    return found == items.end() ? nullptr : &*found;
}

} // namespace

std::vector<PlanningWarning> validate_plan(const Workspace &workspace,
                                           const std::vector<std::string> &available_printer_ids)
{
    std::vector<PlanningWarning> warnings;
    std::set<std::string> members;
    std::set<std::string> slots;
    const std::set<std::string> printers(available_printer_ids.begin(), available_printer_ids.end());
    for (const auto &member : workspace.members) members.insert(member.id);
    for (const auto &slot : workspace.slots) slots.insert(slot.id);
    for (const auto &item : workspace.checklist) {
        if (!item.linked_member_id.empty() && !members.count(item.linked_member_id))
            warnings.push_back({"missing_member", item.id, item.linked_member_id});
        if (!item.linked_slot_id.empty() && !slots.count(item.linked_slot_id))
            warnings.push_back({"missing_slot", item.id, item.linked_slot_id});
        if (!item.due_date.empty()) {
            int year, month, day;
            if (!parse_date(item.due_date, year, month, day)) warnings.push_back({"invalid_date", item.id, {}});
        }
        if (item.due_utc_offset_minutes < -840 || item.due_utc_offset_minutes > 840)
            warnings.push_back({"invalid_offset", item.id, {}});
    }
    for (std::size_t index = 0; index < workspace.slots.size(); ++index) {
        const auto &slot = workspace.slots[index];
        if (!slot.member_id.empty() && !members.count(slot.member_id))
            warnings.push_back({"missing_member", slot.id, slot.member_id});
        if (!slot.printer_id.empty() && !printers.count(slot.printer_id))
            warnings.push_back({"missing_printer", slot.id, slot.printer_id});
        if (slot.end_utc <= slot.start_utc) warnings.push_back({"invalid_interval", slot.id, {}});
        if (slot.utc_offset_minutes < -840 || slot.utc_offset_minutes > 840)
            warnings.push_back({"invalid_offset", slot.id, {}});
        if (!slot.enabled || slot.printer_id.empty()) continue;
        for (std::size_t other = index + 1; other < workspace.slots.size(); ++other) {
            const auto &next = workspace.slots[other];
            if (next.enabled && next.printer_id == slot.printer_id &&
                slot.start_utc < next.end_utc && next.start_utc < slot.end_utc)
                warnings.push_back({"printer_overlap", slot.id, next.id});
        }
    }
    return warnings;
}

std::vector<Reminder> due_reminders(const Workspace &workspace, std::int64_t last_checked_utc,
                                    std::int64_t now_utc)
{
    std::vector<Reminder> reminders;
    if (!workspace.reminders_enabled || now_utc < last_checked_utc) return reminders;
    for (const auto &slot : workspace.slots) {
        if (!slot.enabled || slot.completed || slot.reminder_dismissed) continue;
        const std::int64_t due = slot.snoozed_until_utc > 0 ? slot.snoozed_until_utc :
            slot.start_utc - static_cast<std::int64_t>(slot.reminder_minutes) * 60;
        if (last_checked_utc < due && due <= now_utc) reminders.push_back({slot.id, "planned_print", due});
    }
    for (const auto &item : workspace.checklist) {
        if (item.completed || item.due_date.empty()) continue;
        int year, month, day;
        if (!parse_date(item.due_date, year, month, day)) continue;
        const std::int64_t due = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
            workspace.deadline_reminder_hour * 3600 - static_cast<std::int64_t>(item.due_utc_offset_minutes) * 60;
        if (last_checked_utc < due && due <= now_utc) reminders.push_back({item.id, "deadline", due});
    }
    std::sort(reminders.begin(), reminders.end(), [](const Reminder &a, const Reminder &b) {
        return a.due_utc < b.due_utc || (a.due_utc == b.due_utc && a.item_id < b.item_id);
    });
    return reminders;
}

std::vector<CalendarSlot> agenda(const Workspace &workspace, std::int64_t from_utc, std::int64_t until_utc)
{
    std::vector<CalendarSlot> result;
    for (const auto &slot : workspace.slots)
        if (slot.enabled && slot.start_utc < until_utc && slot.end_utc > from_utc) result.push_back(slot);
    std::sort(result.begin(), result.end(), [](const CalendarSlot &a, const CalendarSlot &b) {
        return a.start_utc < b.start_utc || (a.start_utc == b.start_utc && a.id < b.id);
    });
    return result;
}

std::vector<CalendarSlot> month_slots(const Workspace &workspace, int year, int month, int utc_offset_minutes)
{
    if (year < 1970 || year > 9999 || month < 1 || month > 12 || utc_offset_minutes < -840 || utc_offset_minutes > 840)
        return {};
    const auto first = days_from_civil(year, static_cast<unsigned>(month), 1) * 86400 - utc_offset_minutes * 60;
    const auto next = month == 12 ? days_from_civil(year + 1, 1, 1) * 86400 - utc_offset_minutes * 60 :
        days_from_civil(year, static_cast<unsigned>(month + 1), 1) * 86400 - utc_offset_minutes * 60;
    return agenda(workspace, first, next);
}

bool add_checklist_item(Workspace &workspace, ChecklistItem item)
{
    if (workspace.checklist.size() >= 512 || item.id.empty() || by_id(workspace.checklist, item.id)) return false;
    item.order = static_cast<int>(workspace.checklist.size());
    workspace.checklist.push_back(std::move(item));
    return true;
}

bool edit_checklist_item(Workspace &workspace, const ChecklistItem &item)
{
    auto *existing = by_id(workspace.checklist, item.id);
    if (!existing) return false;
    const int order = existing->order;
    *existing = item;
    existing->order = order;
    return true;
}

bool reorder_checklist_item(Workspace &workspace, const std::string &id, std::size_t index)
{
    auto &items = workspace.checklist;
    const auto found = std::find_if(items.begin(), items.end(), [&](const ChecklistItem &item) { return item.id == id; });
    if (found == items.end() || index >= items.size()) return false;
    ChecklistItem moved = std::move(*found);
    items.erase(found);
    items.insert(items.begin() + index, std::move(moved));
    for (std::size_t i = 0; i < items.size(); ++i) items[i].order = static_cast<int>(i);
    return true;
}

bool complete_checklist_item(Workspace &workspace, const std::string &id, bool completed)
{
    auto *item = by_id(workspace.checklist, id);
    if (!item) return false;
    item->completed = completed;
    return true;
}

bool snooze_slot(Workspace &workspace, const std::string &id, std::int64_t until_utc)
{
    auto *slot = by_id(workspace.slots, id);
    if (!slot || until_utc <= 0) return false;
    slot->snoozed_until_utc = until_utc;
    slot->reminder_dismissed = false;
    return true;
}

bool dismiss_slot_reminder(Workspace &workspace, const std::string &id)
{
    auto *slot = by_id(workspace.slots, id);
    if (!slot) return false;
    slot->reminder_dismissed = true;
    return true;
}

std::string checklist_json(const Workspace &workspace)
{
    nlohmann::json items = nlohmann::json::array();
    for (const auto &item : workspace.checklist)
        items.push_back({{"id", item.id}, {"text", item.text}, {"completed", item.completed}, {"order", item.order},
                         {"due_date", item.due_date}, {"linked_member_id", item.linked_member_id},
                         {"linked_slot_id", item.linked_slot_id}});
    return items.dump(2);
}

std::string checklist_csv(const Workspace &workspace)
{
    std::string result = "id,text,completed,order,due_date,linked_member_id,linked_slot_id\r\n";
    for (const auto &item : workspace.checklist)
        result += csv_field(item.id) + "," + csv_field(item.text) + "," + (item.completed ? "true" : "false") + "," +
                  std::to_string(item.order) + "," + csv_field(item.due_date) + "," +
                  csv_field(item.linked_member_id) + "," + csv_field(item.linked_slot_id) + "\r\n";
    return result;
}

std::string calendar_ics(const Workspace &workspace)
{
    std::string result = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Bambu Studio//Workspace Calendar//EN\r\n";
    for (const auto &slot : workspace.slots) {
        if (!slot.enabled) continue;
        result += "BEGIN:VEVENT\r\nUID:" + ics_escape(slot.id) + "@bambu-workspace\r\nDTSTART:" + utc_stamp(slot.start_utc) +
                  "\r\nDTEND:" + utc_stamp(slot.end_utc) + "\r\nSUMMARY:" + ics_escape(slot.title) +
                  "\r\nDESCRIPTION:" + ics_escape("Planned print; printer " + slot.printer_id) + "\r\nEND:VEVENT\r\n";
    }
    return result + "END:VCALENDAR\r\n";
}

} // namespace Slic3r::Workspace
