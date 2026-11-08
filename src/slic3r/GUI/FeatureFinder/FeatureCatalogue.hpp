#ifndef slic3r_GUI_FeatureFinder_FeatureCatalogue_hpp_
#define slic3r_GUI_FeatureFinder_FeatureCatalogue_hpp_

// The data half of the Every feature page: one entry for every required
// feature, where it lives in this build, its shortcut, and whether it is built.
// Nothing here needs wxWidgets, so tests/feature_catalogue_test.cpp compiles it
// on its own. The page itself is FeatureFinderPanel.
//
// A feature in State::NotBuilt has no action, and the page shows "Not built
// yet" instead of an Open button. Flip it to State::Ready with a real action
// in the same commit that ships its native surface.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r::GUI::FeatureFinder {

enum class State : std::uint8_t { Ready, NotBuilt };
enum class Badge : std::uint8_t { None, New, Fixed };

struct Category
{
    const char *id;
    const char *title; // untranslated; translate with _() at display
};

struct Feature
{
    const char   *id;       // stable id, used for recently opened
    const char   *category; // Category::id
    std::uint32_t glyph;    // Material Symbols codepoint in the shipped font
    const char   *title;    // untranslated
    const char   *where;    // untranslated: where it lives in this build
    const char   *shortcut; // literal key chord, "" when none
    const char   *keywords; // extra search words, never shown
    Badge         badge;
    State         state;
    // "prefs:<AppConfig key>"  opens Preferences on that row
    // "command:<id>"           runs the command registered by the main frame
    // "palette"                opens the command palette
    // "docs:<path>"            opens the offline article (path under docs/features/)
    // ""                       nothing: only for State::NotBuilt
    const char *action;
};

const std::vector<Category> &categories();
const std::vector<Feature>  &features();

const Feature  *find(std::string_view id);
const Category *find_category(std::string_view id);
std::size_t     count_in(std::string_view category_id);
std::size_t     count_state(State state);

enum class ActionKind : std::uint8_t { None, Preference, Command, Palette, Documentation };

struct Action
{
    ActionKind  kind { ActionKind::None };
    std::string target; // key, command id or article path; empty for None and Palette
};

// Unknown prefixes and empty targets parse as ActionKind::None.
Action parse_action(std::string_view action);

// ---------------------------------------------------------------------------
// Filters and order
// ---------------------------------------------------------------------------

// The filter chip values: "" shows everything, a category id shows that
// category, kNotBuiltFilter shows only features that are not built yet.
constexpr const char *kNotBuiltFilter = "not-built";
bool passes_filter(const Feature &feature, std::string_view filter);

enum class SortOrder : std::uint8_t { Group, AToZ, Recent };
constexpr const char *kSortKey = "feature_finder_sort";
SortOrder   parse_sort(std::string_view stored); // unknown -> Group
const char *sort_value(SortOrder order);         // "group" / "a-z" / "recent"

// Indices into features(), in display order.
//  Group:  catalogue order (categories in order, features in order).
//  AToZ:   by `title_of(feature)` (the translated title), case-insensitive,
//          ties by id.
//  Recent: the recently opened ids first (most recent first), then the rest
//          in catalogue order.
std::vector<std::size_t> order(SortOrder order, const std::vector<std::string> &recent,
                               const std::function<std::string(const Feature &)> &title_of);

// ---------------------------------------------------------------------------
// Recently opened (AppConfig, comma-separated ids, most recent first)
// ---------------------------------------------------------------------------

constexpr const char *kRecentKey   = "feature_finder_recent";
constexpr std::size_t kRecentLimit = 8;

// Drops unknown ids, duplicates and features that are not built; keeps at most
// kRecentLimit, in stored order.
std::vector<std::string> parse_recent(std::string_view stored);
std::string              serialize_recent(const std::vector<std::string> &ids);
// Moves `id` to the front (adding it when absent) and trims to kRecentLimit.
// An unknown or not-built id leaves the list unchanged.
std::vector<std::string> touch_recent(std::vector<std::string> recent, std::string_view id);

} // namespace Slic3r::GUI::FeatureFinder

#endif // slic3r_GUI_FeatureFinder_FeatureCatalogue_hpp_
