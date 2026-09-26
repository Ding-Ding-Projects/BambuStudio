#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Slic3r::Workspace {

struct SourceFile {
    std::string relative_path;
    std::filesystem::path local_path;
};

struct Member {
    std::string id;
    std::string name;
    std::filesystem::path project_path;
    std::vector<SourceFile> editable_sources;
};

struct ChecklistItem {
    std::string id;
    std::string text;
    bool completed = false;
    int order = 0;
    std::string due_date;             // YYYY-MM-DD in the workspace's time zone.
    int due_utc_offset_minutes = 0;  // Offset for 09:00 on that date, including DST.
    std::string linked_member_id;
    std::string linked_slot_id;
};

struct CalendarSlot {
    std::string id;
    std::string title;
    std::string printer_id;
    std::string member_id;
    std::string time_zone;            // IANA time-zone name retained for display.
    std::int64_t start_utc = 0;      // Unix seconds; conversion happens at the UI boundary.
    std::int64_t end_utc = 0;
    int utc_offset_minutes = 0;      // Offset chosen for the local wall time, including DST.
    bool enabled = true;
    bool completed = false;
    int reminder_minutes = 15;
    std::int64_t snoozed_until_utc = 0;
    bool reminder_dismissed = false;
};

struct Workspace {
    std::string id;
    std::string title;
    std::string notes;
    std::string time_zone = "UTC";
    std::vector<Member> members;
    std::vector<ChecklistItem> checklist;
    std::vector<CalendarSlot> slots;
    int deadline_reminder_hour = 9;
    bool reminders_enabled = true;
};

struct Result {
    std::string error;
    Workspace workspace;
    std::filesystem::path staging_directory;
    bool ok() const { return error.empty(); }
};

std::string new_id();

// Save never changes the source files. A failed save retains the old archive
// and the staging archive in the destination directory for recovery.
Result save_bundle(const Workspace &workspace, const std::filesystem::path &destination);

// Files are extracted only below staging_root. The caller owns successful
// staging_directory cleanup. An invalid bundle never extracts an entry.
Result load_bundle(const std::filesystem::path &archive, const std::filesystem::path &staging_root);

// Validates without extracting files. This also verifies each entry's SHA-256.
Result inspect_bundle(const std::filesystem::path &archive);

} // namespace Slic3r::Workspace
