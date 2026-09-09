#ifndef slic3r_GUI_NotificationHistory_hpp_
#define slic3r_GUI_NotificationHistory_hpp_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "Bulk/BulkSelection.hpp"

namespace Slic3r { namespace GUI {

// One recorded toast. Entries are append-only: a dismissal or an action only
// flips fields on the existing record, it never rewrites or reorders history.
struct NotificationHistoryEntry
{
    std::uint64_t id           = 0;
    std::int64_t  timestamp_ms = 0; // Unix epoch, milliseconds (UTC)
    int           level        = 0; // NotificationManager::NotificationLevel as int
    std::string   level_name;       // stable machine name, see NotificationHistory::level_name()
    std::string   type_name;        // NotificationType name (or "notification_<n>")
    std::string   title;            // first line of the toast text
    std::string   text;             // full toast text (title line included)
    bool          dismissed    = false;
    std::int64_t  dismissed_ms = 0;
    bool          seen         = false; // the user opened the centre after this arrived
    std::string   action;           // action taken on the toast ("" when none)
};

// Append-only, bounded record of every toast the NotificationManager pushed,
// with JSON persistence, filtering, selection semantics and export. This class
// has no wxWidgets or ImGui dependency so it is unit-testable in isolation;
// the manager owns one instance and the NotificationCenterPanel reads it.
class NotificationHistory
{
public:
    static constexpr std::size_t DEFAULT_MAX_ENTRIES = 500;
    static constexpr int         SCHEMA_VERSION      = 1;

    explicit NotificationHistory(std::size_t max_entries = DEFAULT_MAX_ENTRIES);

    // --- Recording -----------------------------------------------------------
    // Append a new record and return its id. When the bound is exceeded the
    // oldest records are dropped. `timestamp_ms` 0 means "now".
    std::uint64_t append(int level, const std::string &type_name, const std::string &text,
                         std::int64_t timestamp_ms = 0);
    // Flip the dismissed flag. Returns false when the id is unknown.
    bool mark_dismissed(std::uint64_t id, std::int64_t timestamp_ms = 0);
    // Record the action the user took on a toast (hypertext label, button).
    bool record_action(std::uint64_t id, const std::string &action);
    // Mark every entry as seen (the centre was opened). Returns true if any changed.
    bool mark_all_seen();
    // Permanently remove records (the bulk delete). Returns how many were removed.
    std::size_t erase(const std::set<std::uint64_t> &ids);

    // --- Read ----------------------------------------------------------------
    std::size_t size() const { return m_entries.size(); }
    std::size_t max_entries() const { return m_max_entries; }
    bool        empty() const { return m_entries.empty(); }
    // Oldest first (append order).
    const std::vector<NotificationHistoryEntry> &entries() const { return m_entries; }
    const NotificationHistoryEntry *find(std::uint64_t id) const;
    // Entries the user has not seen yet (drives the bell badge).
    std::size_t unread_count() const;
    // Entries whose toast is still on screen.
    std::size_t active_count() const;
    // Monotonic counter bumped on every mutation; a viewer compares it to
    // decide whether to repopulate.
    std::uint64_t revision() const { return m_revision; }

    // Fired after every mutation (append, dismiss, action, seen, erase, load).
    void set_on_change(std::function<void()> cb) { m_on_change = std::move(cb); }

    // --- Filtering -----------------------------------------------------------
    struct Filter
    {
        // Free-text query; matched against title + text + type + level name via
        // `matcher` when set, otherwise a case-insensitive substring test.
        std::string                                                    query;
        std::function<bool(const std::string &query, const std::string &haystack)> matcher;
        // Level allowlist; empty means every level.
        std::set<int> levels;
        bool          include_active    = true;
        bool          include_dismissed = true;
        // Human-readable description for export headers.
        std::string describe() const;
    };
    // Ids matching the filter, newest first.
    std::vector<std::uint64_t> filtered_ids(const Filter &filter) const;
    // Searchable haystack for one entry (what the query is matched against).
    static std::string haystack(const NotificationHistoryEntry &entry);

    // --- Selection -----------------------------------------------------------
    // Multi-select state expressed over entry ids so it survives a repopulate.
    // The model is the shared Bulk::BulkSelection (src/slic3r/GUI/Bulk/): "page"
    // is the slice of matches currently rendered; "all matches" is every id
    // the filter yields, rendered or not. The two select-all variants are
    // deliberately distinct so the UI can name which one it offers.
    using Selection = Bulk::BulkSelection<std::uint64_t>;

    // --- Export --------------------------------------------------------------
    enum class ExportFormat { Json, Csv, Markdown, PlainText };
    static const char *export_extension(ExportFormat format);
    static const char *export_format_name(ExportFormat format);
    // Serialize the given ids (in the given order) with a header that states the
    // exported range: how many of how many, the filter, and the time span.
    std::string export_entries(const std::vector<std::uint64_t> &ids, ExportFormat format,
                               const Filter &filter) const;

    // --- Persistence ---------------------------------------------------------
    std::string to_json() const;
    // Replace contents from JSON. Returns false (and fills `error`) on malformed
    // input; the existing entries are left untouched in that case.
    bool from_json(const std::string &json, std::string *error = nullptr);
    bool save(const std::filesystem::path &file, std::string *error = nullptr) const;
    // A missing file is not an error (returns true with an empty history).
    bool load(const std::filesystem::path &file, std::string *error = nullptr);

    // --- Helpers -------------------------------------------------------------
    // Stable machine names for NotificationManager::NotificationLevel values.
    // Order matches that enum (1 = ProgressBar ... 9 = Error).
    static std::string level_name(int level);
    // Human-friendly chip label for a level ("Error", "Warning", "Info", ...).
    static std::string level_display(int level);
    static std::int64_t now_ms();
    // "YYYY-MM-DDTHH:MM:SS.mmmZ"
    static std::string format_iso8601(std::int64_t timestamp_ms);
    static std::string first_line(const std::string &text);

private:
    void notify();
    void trim();

    std::size_t                           m_max_entries;
    std::uint64_t                         m_next_id  = 1;
    std::uint64_t                         m_revision = 0;
    std::vector<NotificationHistoryEntry> m_entries;
    std::function<void()>                 m_on_change;
};

} } // namespace Slic3r::GUI

#endif // slic3r_GUI_NotificationHistory_hpp_
