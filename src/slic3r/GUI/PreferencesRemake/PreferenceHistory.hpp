#ifndef slic3r_GUI_PreferencesRemake_PreferenceHistory_hpp_
#define slic3r_GUI_PreferencesRemake_PreferenceHistory_hpp_

// Where every Preferences value came from, and the local record of who changed
// it and when. No wxWidgets here: tests/preference_history_test.cpp compiles it
// on its own. The dialog (Preferences.cpp) formats the results for display.
//
// The record is a local JSON-lines file in the data folder. It never leaves the
// computer. Secret-looking keys (tokens, passwords, access codes, station keys)
// are recorded as "changed" without either value.

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r::GUI::PreferenceHistory {

struct Change
{
    std::int64_t time = 0; // seconds since the Unix epoch
    std::string  key;
    std::string  from;     // empty when redacted
    std::string  to;       // empty when redacted
    bool         redacted = false;
};

// Keys whose values must never be written to the record or shown in it.
bool is_sensitive_key(std::string_view key);

// One Change per key whose value differs between the two snapshots. A key
// missing from `before` counts as "" there; a key missing from `after` is not
// reported (the dialog only snapshots keys it shows). Sorted by key.
std::vector<Change> diff(const std::map<std::string, std::string> &before,
                         const std::map<std::string, std::string> &after, std::int64_t time);

std::string           to_line(const Change &change); // one JSON object, no newline
std::optional<Change> from_line(std::string_view line);

constexpr std::size_t kKeep      = 1000; // entries kept after compaction
constexpr std::size_t kCompactAt = 1200; // the file is rewritten past this

class Store
{
public:
    explicit Store(std::filesystem::path file) : m_file(std::move(file)) {}

    // Reads the file; bad lines are skipped, never fatal. Missing file = empty.
    void load();
    // Appends to memory and to the file. Rewrites the file with the newest
    // kKeep entries once it holds more than kCompactAt. False when the file
    // could not be written (the in-memory record still has the changes).
    bool append(const std::vector<Change> &changes);

    const std::vector<Change> &changes() const { return m_changes; } // oldest first
    std::vector<Change>        newest(std::size_t limit) const;      // newest first
    std::vector<Change>        for_key(std::string_view key, std::size_t limit) const;
    std::optional<Change>      last_for_key(std::string_view key) const;
    const std::filesystem::path &file() const { return m_file; }

private:
    bool rewrite() const;

    std::filesystem::path m_file;
    std::vector<Change>   m_changes;
};

// Where a value comes from, for the provenance line under each row.
enum class Origin : std::uint8_t {
    Scheduled,       // Scheduled settings is overriding the stored value right now
    BuiltInDefault,  // nothing stored; the built-in default applies
    MatchesDefault,  // stored, equal to the built-in default, never changed here
    ChangedByYou,    // the record says you set the current value
    Stored,          // stored and different from the default, with no record
    NoDefault,       // nothing stored and no known default
};

struct Provenance
{
    Origin                     origin = Origin::NoDefault;
    std::string                value;          // the value in effect ("" when unknown)
    std::optional<std::string> default_value;  // the built-in default, when known
    std::int64_t               changed_at = 0; // for ChangedByYou
};

Provenance describe(const std::optional<std::string> &stored, const std::optional<std::string> &default_value,
                    const std::optional<std::string> &scheduled, const std::optional<Change> &last_change);

// True when the stored value differs from a known default (the "Changed by you"
// filter). Unknown defaults never count as changed.
bool differs_from_default(const std::optional<std::string> &stored, const std::optional<std::string> &default_value);

} // namespace Slic3r::GUI::PreferenceHistory

#endif // slic3r_GUI_PreferencesRemake_PreferenceHistory_hpp_
