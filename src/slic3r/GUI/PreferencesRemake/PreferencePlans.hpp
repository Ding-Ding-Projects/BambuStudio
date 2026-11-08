#ifndef slic3r_GUI_PreferencesRemake_PreferencePlans_hpp_
#define slic3r_GUI_PreferencesRemake_PreferencePlans_hpp_

// The decisions behind the Preferences tools that change many settings at
// once, or point from one setting to another, without wxWidgets:
// tests/preference_plans_test.cpp compiles this on its own.
//
// - Reset this section: which settings go back to their built-in default,
//   which only their own control may change back, and which are private.
// - Import: what an exported preferences file would change.
// - Undo last change: whether the newest recorded change can be put back.
// - Related settings, and how many settings each section holds.
//
// The dialog (Preferences.cpp) applies the plans through each row's own
// control, so every side effect a change normally has still happens.

#include "PreferenceHistory.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r::GUI::PreferencePlans {

// A setting as the dialog shows it.
struct Setting
{
    std::string                key;
    std::optional<std::string> stored;        // the value written down, when there is one
    std::optional<std::string> default_value; // the built-in default, when the app declares one
    bool                       settable = false; // its row's own control can take a value from here
};

// Settings that restart the app, sign you out or answer a consent question
// when they change: a section reset or an import never changes them, only
// their own control does.
bool by_hand_only(std::string_view key);

// ---------------------------------------------------------------- reset this section
struct ResetPlan
{
    std::vector<std::string> reset;        // going back to the built-in default, in row order
    std::vector<std::string> by_hand;      // changed, but only their own control changes them back
    std::vector<std::string> unchanged;    // already the default, or no default is declared
    std::vector<std::string> private_keys; // tokens, codes and keys: never reset from here
};

// A key that appears twice is planned once, where it first appears.
ResetPlan plan_section_reset(const std::vector<Setting> &section);

// ---------------------------------------------------------------- import
struct ImportItem
{
    std::string key;
    std::string value;
};

struct ImportPlan
{
    // Stable ASCII when the file cannot be used: too-large, not-json,
    // not-preferences, newer-version, empty. Empty when it can.
    std::string              problem;
    std::vector<ImportItem>  apply;        // differs from what is stored, and its control can take it
    std::vector<ImportItem>  by_hand;      // differs, but only its own control may change it
    std::vector<std::string> same;         // already this value
    std::vector<std::string> not_here;     // not a setting Preferences shows
    std::vector<std::string> private_keys; // never imported
    std::vector<std::string> not_text;     // a value that is not text, true/false or a whole number
};

constexpr std::size_t kImportLimit = 1024 * 1024;

// Reads a preferences file written by Export preferences... as JSON: the
// envelope {"schema": "bambustudio.app-config", "schemaVersion": 1, ...,
// "data": {"app": {...}}}, or the bare {"app": {...}} written without the
// schema header. Only settings Preferences shows are considered.
ImportPlan plan_import(std::string_view text, const std::vector<Setting> &settings);

// ---------------------------------------------------------------- undo
struct UndoPlan
{
    bool        possible = false;
    std::string key;
    std::string value;  // what to put back
    // Why not, in stable ASCII: nothing, private, by-hand, not-settable, not-shown.
    std::string reason;
};

// The newest change only: undoing an older one past a newer one would surprise.
UndoPlan plan_undo(const std::vector<PreferenceHistory::Change> &newest_first, const std::vector<Setting> &settings);

// ---------------------------------------------------------------- related settings and counts
struct Row
{
    int                      page  = 0;
    bool                     title = false; // a section heading inside the page
    std::vector<std::string> keys;
};

// The other settings under the same heading as key, nearest first; when the
// heading holds fewer than two others, the rest of the page fills in.
std::vector<std::string> related(const std::vector<Row> &rows, std::string_view key, std::size_t limit = 6);

// Distinct settings on each page.
std::map<int, int> settings_per_page(const std::vector<Row> &rows);

} // namespace Slic3r::GUI::PreferencePlans

#endif // slic3r_GUI_PreferencesRemake_PreferencePlans_hpp_
