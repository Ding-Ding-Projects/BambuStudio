#ifndef slic3r_GUI_Presets_BlankEditorPresets_hpp_
#define slic3r_GUI_Presets_BlankEditorPresets_hpp_

// Start-from presets for editors that would otherwise open empty.
//
// Every preset is built here from exactly three kinds of source:
//   * the shipped defaults table (libslic3r/PresentationDefaults.hpp), which
//     is the same table the reset-to-defaults paths read;
//   * the person's own saved values, passed in by the editor;
//   * a template the application itself ships (for example the shipped
//     appearance presets), passed in already resolved by its owner.
// There is no way to hand this module a literal starting value, so a preset
// can never be an invented value dressed up as a default. Each preset carries
// what it creates and every setting it sets (and every setting it leaves out,
// with the reason), so the picker can state both before the preset is applied
// and the editor can repeat them afterwards.
//
// Text fields hold English source strings; the GUI translates them with _L().
// The module is free of wxWidgets: tests/blank_editor_presets builds it with
// g++ alone.

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace BlankEditorPresets {

using Values = std::map<std::string, std::string>;

// A stored value and the words shown for it (English source string).
struct Choice
{
    std::string value;
    std::string label;
};

// One setting an editor can start from.
struct Field
{
    std::string         key;
    std::string         label;       // English source string
    std::vector<Choice> choices;     // display names for stored values
    std::string         empty_label; // shown for an empty value; "" shows "(empty)"
};

enum class Origin
{
    ShippedDefaults, // the value the app ships with (the reset-to-defaults value)
    SavedSettings,   // the person's own saved value
    Empty,           // creates the item and sets nothing
    Template,        // a template the application ships
};

struct Assignment
{
    std::string key;
    std::string scope;                      // what the value applies to; "" = the new item itself
    std::string label;                      // the setting, English source string
    std::string value;                      // the stored value written
    std::string shown;                      // how the value is shown
    bool        shown_translatable = false; // `shown` is an English source string
    bool        from_default       = false; // SavedSettings: nothing saved, the shipped default is used
};

struct Omission
{
    std::string key;
    std::string label;  // English source string
    std::string reason; // English source string
};

struct Preset
{
    std::string             id;
    Origin                  origin = Origin::Empty;
    std::string             title;   // English source string
    std::string             creates; // English source string
    std::vector<Assignment> sets;
    std::vector<Omission>   left_out;

    // key -> value of every assignment, ready to apply.
    Values values() const;
};

// Returns true when the editor accepts `value` for `key`.
using Validator = std::function<bool(const std::string &key, const std::string &value)>;

struct EditorSpec
{
    std::string        id;               // stable editor id, e.g. "schedule-rule"
    std::vector<Field> fields;           // the settings the editor sets, in display order
    std::string        creates_defaults; // English source sentences, one per standard preset
    std::string        creates_saved;
    std::string        creates_empty;
    Validator          valid;            // null accepts every value
};

// Stable ids of the standard presets.
inline constexpr const char *kShippedDefaultsId = "shipped-defaults";
inline constexpr const char *kSavedSettingsId   = "saved-settings";
inline constexpr const char *kEmptyId           = "empty";

// The standard start-from presets, in this order: shipped defaults, your saved
// settings, empty. `saved` holds the person's own stored values; a key that is
// missing, or whose saved value the editor rejects, starts from its shipped
// default instead (flagged with from_default), and a key with neither is left
// out with the reason.
std::vector<Preset> start_presets(const EditorSpec &spec, const Values &saved);

// A preset for a template the application ships. The owner passes the
// template's own values already resolved.
Preset template_preset(std::string id, std::string title, std::string creates, std::vector<Assignment> sets);

// How a stored value of `field` is shown; `translatable` reports whether the
// result is an English source string (a choice or empty label).
std::string shown_value(const Field &field, const std::string &value, bool &translatable);

// Why a field is left out of the shipped-defaults preset, as an English source
// string, or nullptr when the key has a shipped value.
const char *unfixed_reason(const std::string &key);

} } } // namespace Slic3r::GUI::BlankEditorPresets

#endif // slic3r_GUI_Presets_BlankEditorPresets_hpp_
