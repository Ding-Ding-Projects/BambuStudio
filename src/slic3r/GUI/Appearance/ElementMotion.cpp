#include "ElementMotion.hpp"

#include "../Widgets/MD3EasingMath.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>

// L() only marks a literal for extraction (bbl/i18n/list.txt); the GUI
// translates the stored English text where it shows it.
#ifndef L
#define L(s) s
#endif

namespace Slic3r { namespace GUI { namespace ElementMotion {

// ---------------------------------------------------------------------------
// Triggers
// ---------------------------------------------------------------------------
const std::vector<Trigger> &triggers()
{
    static const std::vector<Trigger> all = {Trigger::Hover, Trigger::Focus,  Trigger::Press,
                                             Trigger::Selection, Trigger::Enable, Trigger::Entrance};
    return all;
}

const std::vector<Trigger> &state_triggers()
{
    static const std::vector<Trigger> states = {Trigger::Hover, Trigger::Focus, Trigger::Press,
                                                Trigger::Selection, Trigger::Enable};
    return states;
}

const char *trigger_key(Trigger trigger)
{
    switch (trigger) {
    case Trigger::Hover: return "hover";
    case Trigger::Focus: return "focus";
    case Trigger::Press: return "press";
    case Trigger::Selection: return "selection";
    case Trigger::Enable: return "enable";
    case Trigger::Entrance: return "entrance";
    }
    return "hover";
}

const char *trigger_label(Trigger trigger)
{
    switch (trigger) {
    case Trigger::Hover: return L("Hover");
    case Trigger::Focus: return L("Focus");
    case Trigger::Press: return L("Press");
    case Trigger::Selection: return L("Selection");
    case Trigger::Enable: return L("Enable and disable");
    case Trigger::Entrance: return L("Entrance");
    }
    return L("Hover");
}

Trigger trigger_for_change(int before, int after)
{
    const int changed = (before ^ after) & 0xFFFF;
    if (changed & kPressedBit) return Trigger::Press;
    if (changed & kCheckedBit) return Trigger::Selection;
    if (changed & kFocusedBit) return Trigger::Focus;
    if (changed & kHoveredBit) return Trigger::Hover;
    if (changed & kEnabledBit) return Trigger::Enable;
    // StateHandler only transitions on a change; hover is the neutral default.
    return Trigger::Hover;
}

// ---------------------------------------------------------------------------
// Fields and keys
// ---------------------------------------------------------------------------
namespace {
constexpr const char *kPrefix = "motion.";
}

const std::vector<Field> &fields()
{
    static const std::vector<Field> all = {Field::Duration, Field::Delay, Field::Easing};
    return all;
}

const char *field_key(Field field)
{
    switch (field) {
    case Field::Duration: return "duration";
    case Field::Delay: return "delay";
    case Field::Easing: return "easing";
    }
    return "duration";
}

std::string property_key(Trigger trigger, Field field)
{
    return std::string(kPrefix) + trigger_key(trigger) + "." + field_key(field);
}

std::optional<KeyParts> parse_key(const std::string &key)
{
    for (Trigger trigger : triggers())
        for (Field field : fields())
            if (key == property_key(trigger, field))
                return KeyParts {trigger, field};
    return std::nullopt;
}

bool is_motion_key(const std::string &key) { return parse_key(key).has_value(); }

bool is_motion_namespace(const std::string &key) { return key.rfind(kPrefix, 0) == 0; }

std::vector<std::string> all_keys()
{
    std::vector<std::string> keys;
    for (Trigger trigger : triggers())
        for (Field field : fields())
            keys.push_back(property_key(trigger, field));
    return keys;
}

// ---------------------------------------------------------------------------
// Easing
// ---------------------------------------------------------------------------
namespace {

struct CurveSpec
{
    const char *name;
    const char *label;
    double      x1, y1, x2, y2; // unused for emphasized
};

// Material motion tokens (md.sys.motion.easing.*) first, then CSS keywords.
const std::vector<CurveSpec> &curve_specs()
{
    static const std::vector<CurveSpec> specs = {
        {"standard", L("Standard"), 0.2, 0.0, 0.0, 1.0},
        {"standard-decelerate", L("Standard decelerate"), 0.0, 0.0, 0.0, 1.0},
        {"standard-accelerate", L("Standard accelerate"), 0.3, 0.0, 1.0, 1.0},
        {"emphasized", L("Emphasized"), 0.0, 0.0, 0.0, 0.0},
        {"emphasized-decelerate", L("Emphasized decelerate"), 0.05, 0.7, 0.1, 1.0},
        {"emphasized-accelerate", L("Emphasized accelerate"), 0.3, 0.0, 0.8, 0.15},
        {"linear", L("Linear"), 0.0, 0.0, 1.0, 1.0},
        {"ease", L("Ease"), 0.25, 0.1, 0.25, 1.0},
        {"ease-in", L("Ease in"), 0.42, 0.0, 1.0, 1.0},
        {"ease-out", L("Ease out"), 0.0, 0.0, 0.58, 1.0},
        {"ease-in-out", L("Ease in and out"), 0.42, 0.0, 0.58, 1.0},
    };
    return specs;
}

const CurveSpec *find_curve(const std::string &name)
{
    for (const CurveSpec &spec : curve_specs())
        if (name == spec.name)
            return &spec;
    return nullptr;
}

std::string format_number(double value)
{
    if (value == 0.0)
        value = 0.0; // no "-0"
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return result.ec == std::errc() ? std::string(buffer, result.ptr) : std::string("0");
}

std::string trim(const std::string &text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string lower(std::string text)
{
    for (char &c : text)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return text;
}

std::vector<std::string> split(const std::string &text, char separator)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == separator) {
            parts.push_back(trim(current));
            current.clear();
        } else
            current += c;
    }
    parts.push_back(trim(current));
    return parts;
}

std::vector<std::string> words(const std::string &text)
{
    std::vector<std::string> out;
    std::string current;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!current.empty())
                out.push_back(current);
            current.clear();
        } else
            current += c;
    }
    if (!current.empty())
        out.push_back(current);
    return out;
}

// A whole token as a finite number (no locale, no leading '+').
std::optional<double> number(const std::string &token)
{
    if (token.empty())
        return std::nullopt;
    double value = 0.0;
    const char *begin = token.data();
    const char *end   = token.data() + token.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end || !std::isfinite(value))
        return std::nullopt;
    return value;
}

std::optional<int> integer(const std::string &token)
{
    if (token.empty())
        return std::nullopt;
    int value = 0;
    const char *end = token.data() + token.size();
    const auto result = std::from_chars(token.data(), end, value);
    if (result.ec != std::errc() || result.ptr != end)
        return std::nullopt;
    return value;
}

// "name(args)" -> args; nullopt when `text` is not that call.
std::optional<std::string> call_args(const std::string &text, const std::string &name)
{
    const std::string open = name + "(";
    if (text.rfind(open, 0) != 0 || text.empty() || text.back() != ')')
        return std::nullopt;
    const std::string inner = text.substr(open.size(), text.size() - open.size() - 1);
    if (inner.find('(') != std::string::npos || inner.find(')') != std::string::npos)
        return std::nullopt;
    return inner;
}

const char *jump_name(Easing::Jump jump)
{
    switch (jump) {
    case Easing::Jump::Start: return "jump-start";
    case Easing::Jump::End: return "jump-end";
    case Easing::Jump::None: return "jump-none";
    case Easing::Jump::Both: return "jump-both";
    }
    return "jump-end";
}

std::optional<Easing::Jump> parse_jump(const std::string &word)
{
    if (word == "jump-start" || word == "start") return Easing::Jump::Start;
    if (word == "jump-end" || word == "end") return Easing::Jump::End;
    if (word == "jump-none") return Easing::Jump::None;
    if (word == "jump-both") return Easing::Jump::Both;
    return std::nullopt;
}

double steps_value(double t, int steps, Easing::Jump jump)
{
    // CSS Easing Functions: the step position of input progress t.
    double current = std::floor(t * steps);
    if (jump == Easing::Jump::Start || jump == Easing::Jump::Both)
        current += 1;
    if (t >= 0 && current < 0)
        current = 0;
    int jumps = steps;
    if (jump == Easing::Jump::Both) jumps = steps + 1;
    if (jump == Easing::Jump::None) jumps = steps - 1;
    if (t <= 1 && current > jumps)
        current = jumps;
    return jumps > 0 ? current / jumps : 1.0;
}

double linear_value(double t, const std::vector<LinearStop> &stops)
{
    if (stops.size() < 2)
        return t;
    const double input = t * 100.0; // stop inputs are percentages
    std::size_t a = 0;
    for (std::size_t i = 0; i < stops.size(); ++i)
        if (stops[i].input <= input)
            a = i;
    if (a == stops.size() - 1)
        a -= 1;
    const LinearStop &pa = stops[a];
    const LinearStop &pb = stops[a + 1];
    if (pa.input == pb.input)
        return pb.output;
    const double along = (input - pa.input) / (pb.input - pa.input);
    return pa.output + along * (pb.output - pa.output);
}

void set_issue(const char **slot, const char *issue)
{
    if (slot)
        *slot = issue;
}

} // namespace

const std::vector<NamedCurve> &named_curves()
{
    static const std::vector<NamedCurve> curves = [] {
        std::vector<NamedCurve> out;
        for (const CurveSpec &spec : curve_specs())
            out.push_back({spec.name, spec.label});
        return out;
    }();
    return curves;
}

double Easing::apply(double t) const
{
    if (!(t > 0.0)) t = 0.0; // NaN as well
    if (t > 1.0) t = 1.0;
    switch (kind) {
    case Kind::Linear: return t;
    case Kind::CubicBezier: return MD3::Motion::Math::cubic_bezier(t, x1, y1, x2, y2);
    case Kind::Steps: return steps_value(t, std::max(1, steps), jump);
    case Kind::LinearStops: return linear_value(t, stops);
    case Kind::Named: {
        if (name == "standard") return MD3::Motion::Math::standard(t);
        if (name == "emphasized") return MD3::Motion::Math::emphasized(t);
        if (name == "linear") return t;
        if (const CurveSpec *spec = find_curve(name))
            return MD3::Motion::Math::cubic_bezier(t, spec->x1, spec->y1, spec->x2, spec->y2);
        return MD3::Motion::Math::standard(t);
    }
    }
    return t;
}

std::string Easing::to_string() const
{
    switch (kind) {
    case Kind::Named: return name;
    case Kind::Linear: return "linear";
    case Kind::CubicBezier:
        return "cubic-bezier(" + format_number(x1) + ", " + format_number(y1) + ", " + format_number(x2) + ", " +
               format_number(y2) + ")";
    case Kind::Steps: return "steps(" + std::to_string(steps) + ", " + jump_name(jump) + ")";
    case Kind::LinearStops: {
        std::string out = "linear(";
        for (std::size_t i = 0; i < stops.size(); ++i) {
            if (i)
                out += ", ";
            out += format_number(stops[i].output) + " " + format_number(stops[i].input) + "%";
        }
        return out + ")";
    }
    }
    return "standard";
}

bool operator==(const Easing &a, const Easing &b)
{
    if (a.kind != b.kind)
        return false;
    switch (a.kind) {
    case Easing::Kind::Named: return a.name == b.name;
    case Easing::Kind::Linear: return true;
    case Easing::Kind::CubicBezier: return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2;
    case Easing::Kind::Steps: return a.steps == b.steps && a.jump == b.jump;
    case Easing::Kind::LinearStops:
        if (a.stops.size() != b.stops.size())
            return false;
        for (std::size_t i = 0; i < a.stops.size(); ++i)
            if (a.stops[i].output != b.stops[i].output || a.stops[i].input != b.stops[i].input)
                return false;
        return true;
    }
    return false;
}

std::optional<Easing> parse_easing(const std::string &raw, const char **issue)
{
    const std::string text = lower(trim(raw));
    if (text.empty()) {
        set_issue(issue, L("The easing is empty. Choose a curve or type one."));
        return std::nullopt;
    }
    Easing easing;
    if (text == "linear") {
        easing.kind = Easing::Kind::Linear;
        return easing;
    }
    if (text == "step-start" || text == "step-end") {
        easing.kind  = Easing::Kind::Steps;
        easing.steps = 1;
        easing.jump  = text == "step-start" ? Easing::Jump::Start : Easing::Jump::End;
        return easing;
    }
    if (find_curve(text)) {
        easing.kind = Easing::Kind::Named;
        easing.name = text;
        return easing;
    }
    if (auto args = call_args(text, "cubic-bezier")) {
        const std::vector<std::string> parts = split(*args, ',');
        std::vector<double> values;
        for (const std::string &part : parts)
            if (auto value = number(part))
                values.push_back(*value);
        if (parts.size() != 4 || values.size() != 4) {
            set_issue(issue, L("cubic-bezier() needs four numbers: x1, y1, x2, y2."));
            return std::nullopt;
        }
        if (values[0] < 0 || values[0] > 1 || values[2] < 0 || values[2] > 1) {
            set_issue(issue, L("In cubic-bezier(), x1 and x2 must be from 0 to 1."));
            return std::nullopt;
        }
        if (std::abs(values[1]) > kMaxBezierY || std::abs(values[3]) > kMaxBezierY) {
            set_issue(issue, L("In cubic-bezier(), y1 and y2 must be from -5 to 5."));
            return std::nullopt;
        }
        easing.kind = Easing::Kind::CubicBezier;
        easing.x1   = values[0];
        easing.y1   = values[1];
        easing.x2   = values[2];
        easing.y2   = values[3];
        return easing;
    }
    if (auto args = call_args(text, "steps")) {
        const std::vector<std::string> parts = split(*args, ',');
        const char *steps_issue = L("steps() needs a whole number of steps from 1 to 100, optionally followed by jump-start, jump-end, jump-none, jump-both, start or end.");
        if (parts.empty() || parts.size() > 2) {
            set_issue(issue, steps_issue);
            return std::nullopt;
        }
        const auto count = integer(parts[0]);
        std::optional<Easing::Jump> jump = Easing::Jump::End;
        if (parts.size() == 2)
            jump = parse_jump(parts[1]);
        if (!count || !jump || *count < 1 || *count > kMaxSteps) {
            set_issue(issue, steps_issue);
            return std::nullopt;
        }
        if (*jump == Easing::Jump::None && *count < 2) {
            set_issue(issue, L("steps() with jump-none needs at least 2 steps."));
            return std::nullopt;
        }
        easing.kind  = Easing::Kind::Steps;
        easing.steps = *count;
        easing.jump  = *jump;
        return easing;
    }
    if (auto args = call_args(text, "linear")) {
        const char *linear_issue = L("linear() needs 2 to 64 stops, each a number optionally followed by one or two percentages.");
        struct Point { double output; std::optional<double> input; };
        std::vector<Point> points;
        for (const std::string &entry : split(*args, ',')) {
            const std::vector<std::string> tokens = words(entry);
            if (tokens.empty() || tokens.size() > 3) {
                set_issue(issue, linear_issue);
                return std::nullopt;
            }
            const auto output = number(tokens[0]);
            if (!output) {
                set_issue(issue, linear_issue);
                return std::nullopt;
            }
            if (tokens.size() == 1)
                points.push_back({*output, std::nullopt});
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                const std::string &token = tokens[i];
                if (token.size() < 2 || token.back() != '%') {
                    set_issue(issue, linear_issue);
                    return std::nullopt;
                }
                const auto percent = number(token.substr(0, token.size() - 1));
                if (!percent) {
                    set_issue(issue, linear_issue);
                    return std::nullopt;
                }
                points.push_back({*output, *percent});
            }
        }
        if (points.size() < 2 || points.size() > kMaxLinearStops) {
            set_issue(issue, linear_issue);
            return std::nullopt;
        }
        // CSS linear(): the ends default to 0% and 100%, inputs never go
        // backwards, and runs without an input are spread evenly.
        if (!points.front().input) points.front().input = 0.0;
        if (!points.back().input) points.back().input = 100.0;
        double largest = -std::numeric_limits<double>::infinity();
        for (Point &point : points)
            if (point.input) {
                point.input = std::max(*point.input, largest);
                largest     = *point.input;
            }
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (points[i].input)
                continue;
            std::size_t end = i;
            while (!points[end].input)
                ++end;
            const double from = *points[i - 1].input;
            const double to   = *points[end].input;
            const double gaps = static_cast<double>(end - i + 1);
            for (std::size_t j = i; j < end; ++j)
                points[j].input = from + (to - from) * static_cast<double>(j - i + 1) / gaps;
        }
        easing.kind = Easing::Kind::LinearStops;
        for (const Point &point : points)
            easing.stops.push_back({point.output, *point.input});
        return easing;
    }
    set_issue(issue, L("Unknown easing. Use a Material curve, linear, ease, cubic-bezier(), steps() or linear()."));
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
namespace {
double total_exact(const Timing &timing)
{
    const double active = std::max(0.0, timing.duration_ms + std::min(0.0, timing.delay_ms));
    return std::max(0.0, timing.delay_ms) + active;
}
} // namespace

int Timing::total_ms() const
{
    const double total = total_exact(*this);
    return std::isfinite(total) ? static_cast<int>(std::lround(total)) : 0;
}

double Timing::progress(double fraction) const
{
    if (!(fraction < 1.0))
        return 1.0; // the end, and NaN: always settle on the target
    if (fraction < 0.0)
        fraction = 0.0;
    const double total = total_exact(*this);
    if (!(total > 0.0))
        return 1.0;
    const double local = fraction * total - delay_ms; // time since the transition's own start
    if (local < 0.0)
        return 0.0; // a positive delay holds the starting state
    if (!(duration_ms > 0.0))
        return 1.0; // a zero duration jumps when the delay ends
    return easing.apply(std::min(1.0, local / duration_ms));
}

bool operator==(const Timing &a, const Timing &b)
{
    return a.duration_ms == b.duration_ms && a.delay_ms == b.delay_ms && a.easing == b.easing;
}

Timing material_default(int default_duration_ms)
{
    Timing timing;
    timing.duration_ms = std::clamp(static_cast<double>(default_duration_ms), 0.0, kMaxDurationMs);
    return timing;
}

// ---------------------------------------------------------------------------
// Validation and resolution
// ---------------------------------------------------------------------------
Check validate(Field field, const nlohmann::json &value)
{
    switch (field) {
    case Field::Duration: {
        const bool ok = value.is_number() && std::isfinite(value.get<double>()) && value.get<double>() >= 0.0 &&
                        value.get<double>() <= kMaxDurationMs;
        return ok ? Check {} : Check {false, L("Duration must be a number of milliseconds from 0 to 10000.")};
    }
    case Field::Delay: {
        const bool ok = value.is_number() && std::isfinite(value.get<double>()) && value.get<double>() >= kMinDelayMs &&
                        value.get<double>() <= kMaxDelayMs;
        return ok ? Check {} : Check {false, L("Delay must be a number of milliseconds from -10000 to 10000.")};
    }
    case Field::Easing: {
        if (!value.is_string())
            return {false, L("Easing must be text, such as standard or cubic-bezier(0.2, 0, 0, 1).")};
        const char *issue = nullptr;
        if (!parse_easing(value.get<std::string>(), &issue))
            return {false, issue};
        return {};
    }
    }
    return {};
}

Check validate_key(const std::string &key, const nlohmann::json &value)
{
    const auto parts = parse_key(key);
    return parts ? validate(parts->field, value) : Check {};
}

Resolved resolve(const Lookup &lookup, Trigger trigger, const Timing &defaults)
{
    Resolved resolved;
    resolved.timing = defaults;
    for (Field field : fields()) {
        FieldState state;
        state.field = field;
        const nlohmann::json value = lookup ? lookup(property_key(trigger, field)) : nlohmann::json();
        if (!value.is_null()) {
            state.stored = true;
            state.value  = value;
            const Check check = validate(field, value);
            state.valid = check.ok;
            state.issue = check.issue;
            if (check.ok) {
                switch (field) {
                case Field::Duration: resolved.timing.duration_ms = value.get<double>(); break;
                case Field::Delay: resolved.timing.delay_ms = value.get<double>(); break;
                case Field::Easing: resolved.timing.easing = *parse_easing(value.get<std::string>()); break;
                }
            }
        }
        resolved.fields.push_back(std::move(state));
    }
    return resolved;
}

nlohmann::json to_json(Field field, const Timing &timing)
{
    auto number_json = [](double value) {
        if (std::floor(value) == value && std::abs(value) < 1e9)
            return nlohmann::json(static_cast<std::int64_t>(value));
        return nlohmann::json(value);
    };
    switch (field) {
    case Field::Duration: return number_json(timing.duration_ms);
    case Field::Delay: return number_json(timing.delay_ms);
    case Field::Easing: return timing.easing.to_string();
    }
    return nlohmann::json();
}

std::vector<std::string> unknown_motion_keys(const nlohmann::json &bag)
{
    std::vector<std::string> keys;
    if (!bag.is_object())
        return keys;
    for (auto it = bag.begin(); it != bag.end(); ++it)
        if (is_motion_namespace(it.key()) && !is_motion_key(it.key()))
            keys.push_back(it.key());
    return keys;
}

// ---------------------------------------------------------------------------
// Undo and redo
// ---------------------------------------------------------------------------
void EditHistory::record(Edit edit)
{
    edit.changes.erase(std::remove_if(edit.changes.begin(), edit.changes.end(),
                                      [](const Change &change) { return change.before == change.after; }),
                       edit.changes.end());
    if (edit.changes.empty())
        return;
    m_undo.push_back(std::move(edit));
    m_redo.clear();
    if (m_undo.size() > m_limit)
        m_undo.erase(m_undo.begin(), m_undo.begin() + static_cast<std::ptrdiff_t>(m_undo.size() - m_limit));
}

std::optional<Edit> EditHistory::undo()
{
    if (m_undo.empty())
        return std::nullopt;
    Edit edit = std::move(m_undo.back());
    m_undo.pop_back();
    m_redo.push_back(edit);
    return edit;
}

std::optional<Edit> EditHistory::redo()
{
    if (m_redo.empty())
        return std::nullopt;
    Edit edit = std::move(m_redo.back());
    m_redo.pop_back();
    m_undo.push_back(edit);
    return edit;
}

// ---------------------------------------------------------------------------
// Capability inventory (keep identical to design/native-appearance/
// motion-capability-inventory.json; ui-md3/tests/appearance-motion-tab.test.mjs)
// ---------------------------------------------------------------------------
namespace {
constexpr const char *kTracks   = "tracks";
constexpr const char *kTiming   = "timing";
constexpr const char *kTimeline = "timeline";
constexpr const char *kTriggers = "triggers";
constexpr const char *kAccess   = "access";
} // namespace

const char *area_label(const char *area)
{
    const std::string a = area ? area : "";
    if (a == kTracks) return L("Tracks, properties and keyframes");
    if (a == kTiming) return L("Timing and easing");
    if (a == kTimeline) return L("Timeline and playback");
    if (a == kTriggers) return L("Transitions and triggers");
    return L("Reversibility and access");
}

const std::vector<Capability> &capabilities()
{
    using S = Support;
    static const std::vector<Capability> rows = {
        // Tracks, properties and keyframes
        {"layers", kTracks, L("Layers"), S::Unavailable,
         L("The native renderer draws each element as one surface without layers, so motion cannot target a layer.")},
        {"tracks", kTracks, L("Animation tracks"), S::Unavailable,
         L("Each consumer runs one animator per element and trigger. There is no track to create, name, duplicate, reorder, mute, solo, group or remove.")},
        {"target-element-state", kTracks, L("Exact element and state"), S::Wired,
         L("Each trigger edits the transition of this exact element when that state changes. Kit buttons, fields, lists and cards read it.")},
        {"property-state-colours", kTracks, L("State colours"), S::Wired,
         L("A state transition animates every registered state colour of the element together: the background, border and text of kit buttons and boxes.")},
        {"property-window-opacity", kTracks, L("Window opacity"), S::Wired,
         L("An entrance fades a top-level window in from a quarter of full opacity: dialogs with the Material caption, the command palette, the regex builder, menus and this editor.")},
        {"property-list", kTracks, L("Per-property transition list"), S::Unavailable,
         L("The state animator does not know which registered colour is the background, border or text, so it cannot animate one and change another at once.")},
        {"property-geometry", kTracks, L("Transform, size and position"), S::Unavailable,
         L("Native windows have no transform, scale or rotation, and layout is whole-pixel sizer geometry, so these cannot be animated.")},
        {"property-control-opacity", kTracks, L("Opacity of a control"), S::Unavailable,
         L("Only top-level windows can be translucent here. A control inside a window cannot fade.")},
        {"property-discrete", kTracks, L("Shape, spacing and typography"), S::Unavailable,
         L("Radius, spacing and font values cannot be interpolated by this renderer, so they change at once.")},
        {"custom-properties", kTracks, L("Custom properties"), S::Unavailable,
         L("The native renderer has no custom properties to animate.")},
        {"computed-start", kTracks, L("Computed starting value"), S::Wired,
         L("A transition starts from the colour on screen, so an interrupted transition continues from where it was instead of jumping.")},
        {"explicit-values", kTracks, L("Explicit start and end values"), S::Unavailable,
         L("Start and end colours come from the element's state colours, set on the Colours tab and by the theme. A transition cannot name other values.")},
        {"interpolation", kTracks, L("Interpolation and composition"), S::Unavailable,
         L("Colours always interpolate per red, green, blue and alpha channel and replace each other. There is no other colour space and no add or accumulate mode.")},
        {"keyframes", kTracks, L("Keyframes, markers and segments"), S::Unavailable,
         L("The animator plays one segment from start to end. There are no keyframes or markers to add, select, copy, move or reorder, and no hold or per-segment easing; steps() gives an evenly stepped transition.")},
        {"motion-paths", kTracks, L("Motion paths"), S::Unavailable,
         L("Nothing moves along a path in the native renderer.")},
        // Timing and easing
        {"duration", kTiming, L("Duration"), S::Wired,
         L("From 0 to 10000 ms. 16 ms or less shows the final state at once, because the animator draws about every 16 ms.")},
        {"automatic-duration", kTiming, L("Automatic duration"), S::Wired,
         L("An unset duration uses the consumer's Material token: 100 ms for state changes and the window's own token for entrances.")},
        {"delay", kTiming, L("Delay"), S::Wired,
         L("From 0 to 10000 ms. The element keeps its starting colour or entrance opacity until the delay ends.")},
        {"negative-delay", kTiming, L("Negative delay"), S::Wired,
         L("Down to -10000 ms. The transition starts part of the way through.")},
        {"end-delay", kTiming, L("End delay"), S::Unavailable,
         L("A transition settles the moment it ends; the animator has no end-delay phase.")},
        {"repeat", kTiming, L("Repeat count, infinite repeat and repeat delay"), S::Unavailable,
         L("State transitions and entrances play once by definition, and an endless repeat would be continuous motion.")},
        {"direction-fill", kTiming, L("Direction, fill and iteration start"), S::Unavailable,
         L("Each transition plays once forward, holds its start during the delay and its target afterwards. Without repeats there is no alternate, reverse or iteration start; a negative delay starts part of the way through.")},
        {"playback-rate", kTiming, L("Playback rate"), S::Unavailable,
         L("There is no per-element playback rate, and no zero or negative rate. The Interface motion preference and Windows can only turn motion off.")},
        {"global-authority", kTiming, L("Global motion setting"), S::Wired,
         L("Preferences > Interface motion and the Windows animation setting decide first. When either reduces motion, these values are kept but not played.")},
        {"global-speed-level", kTiming, L("Global speed level"), S::Unavailable,
         L("This app has no animated rainbow colour and no global speed level yet, so there is nothing for a per-element value to compete with.")},
        {"easing-named", kTiming, L("Named curves"), S::Wired,
         L("Material standard and emphasized curves with their accelerate and decelerate forms, linear, ease, ease in, ease out and ease in and out.")},
        {"easing-bezier", kTiming, L("Cubic Bezier"), S::Wired,
         L("cubic-bezier(x1, y1, x2, y2) with x from 0 to 1 and y from -5 to 5, typed as four numbers in the easing field.")},
        {"easing-bezier-handles", kTiming, L("Dragging Bezier handles"), S::Unavailable,
         L("Not built yet. Type the four numbers in the easing field; the curve preview redraws at once.")},
        {"easing-linear-stops", kTiming, L("Linear stop lists"), S::Wired,
         L("linear() with 2 to 64 stops, each a number with up to two percentages.")},
        {"easing-steps", kTiming, L("Steps"), S::Wired,
         L("steps(count, position) with 1 to 100 steps and jump-start, jump-end, jump-none or jump-both.")},
        {"easing-spring", kTiming, L("Spring and physics"), S::Unavailable,
         L("The animator is time-based with a fixed duration; it has no spring or physics solver.")},
        {"timing-summary", kTiming, L("Computed timing"), S::Wired,
         L("The tab states the total active time of the selected transition. Nothing repeats, so nothing is infinite.")},
        {"validation", kTiming, L("Validation"), S::Wired,
         L("Out-of-range or malformed values are refused when typed. An invalid value from a file is listed on this tab and the Material default plays instead.")},
        {"precision", kTiming, L("Precision and units"), S::Wired,
         L("Times are whole milliseconds in the fields, and a fractional value from a file is kept until you change it. Curve numbers keep the precision you type.")},
        // Timeline and playback
        {"preview-curve", kTimeline, L("Curve and time ruler"), S::Wired,
         L("The preview draws the selected transition against a time ruler, with the delay shaded.")},
        {"preview-scrub", kTimeline, L("Seek and scrub"), S::Wired,
         L("Type a preview position in milliseconds to see the curve and colour at that moment.")},
        {"preview-playback", kTimeline, L("Play, stop and restart"), S::Wired,
         L("Plays the selected transition in the preview with the same arithmetic the element uses. Playing again restarts it.")},
        {"preview-reverse", kTimeline, L("Play reversed"), S::Wired,
         L("Plays the way back toward the starting colour, as leaving the state does.")},
        {"preview-speed", kTimeline, L("Preview speed"), S::Wired,
         L("Plays the preview at full, half or quarter speed. It changes the preview only, never the element.")},
        {"preview-compare", kTimeline, L("Before and after"), S::Wired,
         L("The preview draws the Material default as a dashed curve beside your timing.")},
        {"preview-zoom-loop", kTimeline, L("Zoom and loop preview"), S::Unavailable,
         L("The preview fits one transition to the card, so there is nothing to zoom. It does not loop, because looping would be continuous motion.")},
        {"document-timelines", kTimeline, L("Document, scroll and view timelines"), S::Unavailable,
         L("Native windows have no document clock, scroll timeline or view timeline. Every animation runs on the interface timer from the moment its trigger fires.")},
        {"sequencing", kTimeline, L("Sequencing, stagger and groups"), S::Unavailable,
         L("Without tracks, consumers start independently when their own trigger fires. Offsets, dependencies, stagger and parallel groups cannot be expressed.")},
        {"overlap", kTimeline, L("Overlapping properties"), S::Unavailable,
         L("Only one transition runs per element at a time, so there is no overlap to order, add or accumulate.")},
        {"interruption", kTimeline, L("Interruption"), S::Wired,
         L("A new state change interrupts the running transition and starts from the colour on screen. The interrupted one reports nothing.")},
        {"cancellation", kTimeline, L("Cancellation"), S::Wired,
         L("A hidden element settles at its final state and a closed one cancels its transition. Reduced motion settles every transition at once.")},
        {"completion", kTimeline, L("Completion"), S::Wired,
         L("Transitions never report completion. State, focus and input change at once and never wait for an animation.")},
        // Transitions and triggers
        {"trigger-hover", kTriggers, L("Hover"), S::Wired, L("The pointer enters or leaves the element.")},
        {"trigger-focus", kTriggers, L("Focus"), S::Wired, L("The element gains or loses keyboard focus.")},
        {"trigger-press", kTriggers, L("Press"), S::Wired, L("A pointer button goes down or up on the element.")},
        {"trigger-selection", kTriggers, L("Selection"), S::Wired, L("A toggle button or check is turned on or off.")},
        {"trigger-enable", kTriggers, L("Enable and disable"), S::Wired, L("The element becomes available or unavailable.")},
        {"trigger-entrance", kTriggers, L("Entrance"), S::Wired,
         L("A dialog, popup or this editor opens. Edit it in the Window entrance section for the window that contains this element.")},
        {"trigger-exit", kTriggers, L("Exit"), S::Unavailable,
         L("Windows and popups close at once; nothing waits for an exit animation.")},
        {"trigger-status-states", kTriggers, L("Validation, loading, success, warning and error states"), S::Unavailable,
         L("Kit widgets have no such states for the animator. Messages, progress bars and colours show them at once.")},
        {"trigger-other-events", kTriggers, L("Gestures, other events and scroll progress"), S::Unavailable,
         L("The animator listens only to hover, focus, press, selection and enable changes.")},
        {"trigger-thresholds", kTriggers, L("Thresholds, debounce and re-entry"), S::Unavailable,
         L("A trigger fires on the native event itself. A repeated trigger restarts from the colour on screen.")},
        {"trigger-priority", kTriggers, L("Priority"), S::Wired,
         L("When several states change in one event, press wins, then selection, focus, hover and enable.")},
        {"inheritance", kTriggers, L("Inheritance and overrides"), S::Wired,
         L("An unset value comes from the parent element (the id before a slash), then the active preset, then the Material default.")},
        {"discrete-transitions", kTriggers, L("Discrete transitions"), S::Unavailable,
         L("Values that cannot be interpolated change at once; there is no setting to delay or step them.")},
        {"visual-only", kTriggers, L("Visual only"), S::Wired,
         L("Triggers only change how a state change looks. They never run code, reach the network, sign in, or stand in for an operation finishing.")},
        // Reversibility and access
        {"reset-value", kAccess, L("Reset a value"), S::Wired,
         L("Each value has its own reset to the inherited or default value.")},
        {"reset-element-motion", kAccess, L("Reset element motion"), S::Wired,
         L("Drops every motion value of this element in one action. Reset element and Reset all at the bottom still reset everything.")},
        {"undo-redo", kAccess, L("Undo and redo"), S::Wired,
         L("Undo and Redo step through the motion edits made in this editor, across elements.")},
        {"local-history", kAccess, L("Local history"), S::Wired,
         L("Every motion change is saved and recorded in Version history with the rest of the appearance, and can be restored there.")},
        {"presets", kAccess, L("Named presets"), S::Wired,
         L("Save as preset keeps motion values with the other element styles, and a preset states every motion value it sets.")},
        {"import-export", kAccess, L("Import and export"), S::Wired,
         L("Exported themes carry motion values. Import checks them and lists invalid values.")},
        {"preserve-unknown", kAccess, L("Unknown settings kept"), S::Wired,
         L("A motion setting this version does not understand stays in the file and is listed on this tab, never silently dropped.")},
        {"copy-paste-style", kAccess, L("Copy and paste style"), S::Unavailable,
         L("The appearance editor has no copy and paste style yet, for motion or any other property.")},
        {"scheduled", kAccess, L("Scheduled settings"), S::Unavailable,
         L("Scheduled settings switch application settings by rule; element styles, motion included, cannot be scheduled yet.")},
        {"keyboard-screen-reader", kAccess, L("Keyboard and screen reader"), S::Wired,
         L("Every value is a named field reached with Tab, with numbers instead of dragging. Ctrl+Page Up and Ctrl+Page Down switch tabs.")},
        {"search", kAccess, L("Search with the regex builder"), S::Wired,
         L("Search this list in plain text or with the regular-expression builder beside the search field.")},
        {"command-palette", kAccess, L("Command palette teleport"), S::Unavailable,
         L("The command palette does not list individual appearance properties yet.")},
        {"reduced-motion", kAccess, L("Reduced motion"), S::Wired,
         L("Reduced motion settles the element and the preview at once. Your values are kept for when motion is allowed again.")},
        {"non-motion-equivalent", kAccess, L("Without motion"), S::Wired,
         L("The curve, the time ruler and the written timing describe the transition without playing it.")},
        {"editor-own-controls", kAccess, L("This editor's own controls"), S::Wired,
         L("The editor's tabs and buttons are kit buttons with their own element ids; right-click one to edit its motion. The editor's opening fade is a window entrance.")},
    };
    return rows;
}

} } } // namespace Slic3r::GUI::ElementMotion
