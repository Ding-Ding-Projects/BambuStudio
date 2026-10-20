#ifndef slic3r_GUI_Appearance_ElementMotion_hpp_
#define slic3r_GUI_Appearance_ElementMotion_hpp_

// Per-element motion: the timing the real renderer consumers play for one
// element, edited on the appearance editor's Animation and transition tab.
//
// Values live in the element's ordinary style bag (ElementStyle) as flat
// scalar properties, so they resolve, reset, save, form presets, export and
// import exactly like every other appearance property:
//
//   motion.<trigger>.duration   number, milliseconds, 0..10000
//   motion.<trigger>.delay      number, milliseconds, -10000..10000
//   motion.<trigger>.easing     string, a named curve, cubic-bezier(),
//                               steps() or linear() stop list
//
// <trigger> is hover, focus, press, selection, enable (the state changes the
// kit StateHandler animates) or entrance (the opening fade of a top-level
// window). Anything unset or invalid keeps the consumer's Material default.
//
// The global Interface motion preference and the operating system's
// animation setting stay authoritative: when either asks for reduced motion,
// every consumer shows its final state at once and these values are kept
// but not played. Nothing here can turn motion back on.
//
// Pure C++17 plus nlohmann::json, so tests build it without wxWidgets.

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace Slic3r { namespace GUI { namespace ElementMotion {

// ---------------------------------------------------------------------------
// Triggers
// ---------------------------------------------------------------------------
enum class Trigger { Hover, Focus, Press, Selection, Enable, Entrance };

// Every trigger, in display order.
const std::vector<Trigger> &triggers();
// The state-change triggers StateHandler plays (all but Entrance).
const std::vector<Trigger> &state_triggers();
const char *trigger_key(Trigger trigger);   // "hover", "focus", ...
const char *trigger_label(Trigger trigger); // English; the GUI translates it

// StateHandler's state bits (Widgets/StateHandler.hpp). StateHandler.cpp
// asserts at compile time that they still match.
constexpr int kEnabledBit  = 1;
constexpr int kCheckedBit  = 2;
constexpr int kFocusedBit  = 4;
constexpr int kHoveredBit  = 8;
constexpr int kPressedBit  = 16;

// The trigger a state change plays. Several bits can change in one event; the
// most specific wins: press, then selection, focus, hover and enable.
Trigger trigger_for_change(int before, int after);

// ---------------------------------------------------------------------------
// Fields and property keys
// ---------------------------------------------------------------------------
enum class Field { Duration, Delay, Easing };

const std::vector<Field> &fields();
const char *field_key(Field field); // "duration" | "delay" | "easing"
std::string property_key(Trigger trigger, Field field);

struct KeyParts
{
    Trigger trigger;
    Field   field;
};
// "motion.hover.duration" -> {Hover, Duration}; nullopt for anything else.
std::optional<KeyParts> parse_key(const std::string &key);
// One of the eighteen keys above.
bool is_motion_key(const std::string &key);
// Any key under "motion." (understood or not).
bool is_motion_namespace(const std::string &key);
// The eighteen keys, trigger-major.
std::vector<std::string> all_keys();

// ---------------------------------------------------------------------------
// Ranges
// ---------------------------------------------------------------------------
constexpr double      kMaxDurationMs  = 10000.0;
constexpr double      kMinDelayMs     = -10000.0;
constexpr double      kMaxDelayMs     = 10000.0;
constexpr int         kMaxSteps       = 100;
constexpr std::size_t kMaxLinearStops = 64;
constexpr double      kMaxBezierY     = 5.0; // |y1|, |y2| bound for cubic-bezier()

// ---------------------------------------------------------------------------
// Easing
// ---------------------------------------------------------------------------
struct LinearStop
{
    double output { 0.0 };
    double input { 0.0 }; // resolved, 0..1 and non-decreasing
};

struct Easing
{
    enum class Kind { Named, Linear, CubicBezier, Steps, LinearStops };
    enum class Jump { Start, End, None, Both };

    Kind        kind { Kind::Named };
    std::string name { "standard" };     // Named: a key of named_curves()
    double      x1 { 0.2 }, y1 { 0.0 }; // CubicBezier
    double      x2 { 0.0 }, y2 { 1.0 };
    int         steps { 1 };            // Steps
    Jump        jump { Jump::End };
    std::vector<LinearStop> stops;      // LinearStops

    // Eased output for input progress t; t is clamped to [0, 1].
    double apply(double t) const;
    // Canonical text, accepted again by parse_easing().
    std::string to_string() const;
};
bool operator==(const Easing &a, const Easing &b);
inline bool operator!=(const Easing &a, const Easing &b) { return !(a == b); }

struct NamedCurve
{
    const char *name;  // canonical keyword
    const char *label; // English; the GUI translates it
};
// Material curves first, then linear and the CSS keywords.
const std::vector<NamedCurve> &named_curves();

// Parse "standard", "cubic-bezier(0.2, 0, 0, 1)", "steps(4, jump-end)",
// "linear(0, 0.25 40%, 1)" and the other named curves. On failure returns
// nullopt and, when `issue` is given, an English reason the GUI translates.
std::optional<Easing> parse_easing(const std::string &text, const char **issue = nullptr);

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
struct Timing
{
    double duration_ms { 100.0 };
    double delay_ms { 0.0 };
    Easing easing;

    // Whole active time in milliseconds: a positive delay holds the start, a
    // negative delay starts part of the way through.
    int total_ms() const;
    // Eased progress at `fraction` (0..1) of total_ms(). 0 during a positive
    // delay; exactly 1 at and after the end, so every transition settles on
    // its target state whatever the curve.
    double progress(double fraction) const;
};
bool operator==(const Timing &a, const Timing &b);

// The default a consumer plays when nothing is set: its own Material
// duration token with the standard curve and no delay.
Timing material_default(int default_duration_ms);

// ---------------------------------------------------------------------------
// Validation and resolution
// ---------------------------------------------------------------------------
struct Check
{
    bool        ok { true };
    const char *issue { nullptr }; // English; the GUI translates it
};
Check validate(Field field, const nlohmann::json &value);
// Valid for every key outside the eighteen motion keys.
Check validate_key(const std::string &key, const nlohmann::json &value);

// Reads one property through the registry's layers (or a test bag).
using Lookup = std::function<nlohmann::json(const std::string &key)>;

struct FieldState
{
    Field          field { Field::Duration };
    bool           stored { false }; // a value was found for the key
    bool           valid { true };   // false: stored but invalid, default used
    const char *   issue { nullptr };
    nlohmann::json value;            // the stored value, verbatim
};

struct Resolved
{
    Timing                  timing;
    std::vector<FieldState> fields; // Duration, Delay, Easing
};
Resolved resolve(const Lookup &lookup, Trigger trigger, const Timing &defaults);

// The JSON value stored for `field` of `timing`.
nlohmann::json to_json(Field field, const Timing &timing);
// Keys under "motion." in `bag` that this version does not understand.
std::vector<std::string> unknown_motion_keys(const nlohmann::json &bag);

// ---------------------------------------------------------------------------
// Undo and redo for motion edits
// ---------------------------------------------------------------------------
struct Change
{
    std::string    id;
    std::string    key;
    nlohmann::json before; // null: no user override
    nlohmann::json after;
};

struct Edit
{
    std::string         label; // English summary, e.g. "Hover duration"
    std::vector<Change> changes;
};

// A bounded undo/redo stack. A new edit discards the redo branch; an edit
// whose every change leaves the value as it was is ignored.
class EditHistory
{
public:
    explicit EditHistory(std::size_t limit = 100) : m_limit(limit ? limit : 1) {}

    void record(Edit edit);
    bool can_undo() const { return !m_undo.empty(); }
    bool can_redo() const { return !m_redo.empty(); }
    const Edit *next_undo() const { return m_undo.empty() ? nullptr : &m_undo.back(); }
    const Edit *next_redo() const { return m_redo.empty() ? nullptr : &m_redo.back(); }
    // The edit to revert (apply each change's `before`), moved to redo.
    std::optional<Edit> undo();
    // The edit to apply again (each change's `after`), moved to undo.
    std::optional<Edit> redo();
    std::size_t undo_size() const { return m_undo.size(); }
    std::size_t redo_size() const { return m_redo.size(); }

private:
    std::size_t       m_limit;
    std::vector<Edit> m_undo;
    std::vector<Edit> m_redo;
};

// ---------------------------------------------------------------------------
// Capability inventory
// ---------------------------------------------------------------------------
// Every motion capability the appearance contract names, with what the
// native renderer does with it. design/native-appearance/motion-capability-
// inventory.json is the hand-written matrix behind this table; a source
// contract keeps the two identical.
enum class Support { Wired, Unavailable };

struct Capability
{
    const char *id;
    const char *area;   // English group heading
    const char *title;  // English
    Support     support;
    const char *detail; // English: the consumer, or the exact reason
};
const std::vector<Capability> &capabilities();
const char *area_label(const char *area);

} } } // namespace Slic3r::GUI::ElementMotion

#endif // slic3r_GUI_Appearance_ElementMotion_hpp_
