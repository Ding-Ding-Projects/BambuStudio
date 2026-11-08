#ifndef slic3r_GUI_FeatureFinder_LocalToolsHub_hpp_
#define slic3r_GUI_FeatureFinder_LocalToolsHub_hpp_

// The data half of the Local tools page (the first page of the Local tools
// tab): which tools it lists and under which chip, the pinned tools and their
// order, and the recent activity the tools write as they finish something.
// Nothing here needs wxWidgets, so tests/local_tools_hub_test.cpp compiles it
// on its own. The page is LocalToolsHubPanel; the tools write activity through
// LocalToolsActivity.
//
// Every tool here is an entry of the Every feature catalogue (its id, icon,
// title, where it lives and how it opens come from there); the hub adds a
// chip and the one-line description the tool's menu item already shows.

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r::GUI::LocalTools {

struct Group
{
    const char *id;
    const char *title; // untranslated
};

struct Tool
{
    const char *feature; // FeatureFinder::Feature::id
    const char *group;   // Group::id
    const char *summary; // untranslated; the same words as the tool's menu help
};

const std::vector<Group> &groups();
const std::vector<Tool>  &tools();
const Tool               *find_tool(std::string_view feature);
const Group              *find_group(std::string_view id);
std::size_t               count_in(std::string_view group);

// ---------------------------------------------------------------------------
// Pinned tools (AppConfig, comma-separated feature ids, in the order shown)
// ---------------------------------------------------------------------------

constexpr const char *kPinnedKey    = "local_tools_pinned";
constexpr const char *kFilterKey    = "local_tools_filter";
constexpr const char *kPinnedFilter = "pinned";
constexpr std::size_t kPinLimit     = 12;

// The pins of a new installation (the board's nine).
std::vector<std::string> default_pins();
// `present` is false while the key was never written: the defaults then. A
// written empty value means the person unpinned everything. Unknown ids and
// duplicates are dropped; at most kPinLimit are kept, in stored order.
std::vector<std::string> parse_pins(std::string_view stored, bool present);
std::string              serialize_pins(const std::vector<std::string> &pins);
bool                     is_pinned(const std::vector<std::string> &pins, std::string_view id);
// Pins at the end or unpins. An unknown id, or a pin past kPinLimit, changes nothing.
std::vector<std::string> toggle_pin(std::vector<std::string> pins, std::string_view id);
// Moves a pin `delta` places (negative is earlier), stopping at either end.
std::vector<std::string> move_pin(std::vector<std::string> pins, std::string_view id, int delta);

// The chip values: kPinnedFilter, "" for every tool, or a group id. An
// unknown stored value reads as "".
std::string parse_filter(std::string_view stored);

// Indices into tools(), in display order: the pinned order for kPinnedFilter,
// catalogue order otherwise.
std::vector<std::size_t> visible(std::string_view filter, const std::vector<std::string> &pins);

// ---------------------------------------------------------------------------
// Recent activity (one JSON object per line in the data folder)
// ---------------------------------------------------------------------------

struct Activity
{
    std::int64_t time = 0; // seconds since 1970-01-01 UTC
    std::string  tool;     // Tool::feature
    std::string  action;   // untranslated words the page translates, "Start", "Encoded"
    std::string  subject;  // a file, disk, server or extension name; never content
    std::string  result;   // "Done", "Failed", "Cancelled", "Incomplete", "Started"
};

constexpr std::size_t kActivityLimit     = 200;
constexpr std::size_t kActivityTextLimit = 120; // bytes, action and subject each

// Control characters become spaces, runs of spaces become one, the ends are
// trimmed and the text is cut at a whole UTF-8 character with "..." added.
std::string clean_text(std::string_view text, std::size_t limit = kActivityTextLimit);
// Only the five results above; anything else reads as "Done".
std::string clean_result(std::string_view result);

std::string to_line(const Activity &activity);
// False for a line that is not an object, names an unknown tool or has no action.
bool        from_line(std::string_view line, Activity &activity);

class ActivityLog
{
public:
    explicit ActivityLog(std::filesystem::path file = {}) : m_file(std::move(file)) {}
    void load();
    // Cleans, appends, trims to kActivityLimit (oldest go first) and saves.
    // False, and nothing kept, for an unknown tool or an empty action.
    bool append(Activity activity);
    void clear();
    const std::vector<Activity> &entries() const { return m_entries; } // oldest first
    // Newest first, at most `limit`, only `tool` when it is not empty.
    std::vector<Activity> newest(std::size_t limit, std::string_view tool = {}) const;
    const std::filesystem::path &file() const { return m_file; }

private:
    void                  save() const;
    std::filesystem::path m_file;
    std::vector<Activity> m_entries;
};

} // namespace Slic3r::GUI::LocalTools

#endif // slic3r_GUI_FeatureFinder_LocalToolsHub_hpp_
