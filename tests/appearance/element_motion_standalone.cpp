// Pure tests for the per-element motion model behind the appearance editor's
// Animation and transition tab: keys, easing grammar and arithmetic, timing,
// validation, resolution through layered lookups, serialisation round trips,
// undo/redo and the capability table. No wxWidgets; plain g++ builds it:
//
//   g++ -std=c++17 -Wall -Wextra -Werror -Isrc
//       tests/appearance/element_motion_standalone.cpp
//       src/slic3r/GUI/Appearance/ElementMotion.cpp -o element_motion_tests

#include "slic3r/GUI/Appearance/ElementMotion.hpp"
#include "slic3r/GUI/Widgets/MD3EasingMath.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace EM = Slic3r::GUI::ElementMotion;
using nlohmann::json;

namespace {

int g_checks = 0;

void require(bool condition, const char *what)
{
    ++g_checks;
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
        throw std::runtime_error(what);
    }
}

bool near(double a, double b, double tolerance = 1e-9) { return std::abs(a - b) <= tolerance; }

EM::Easing parse(const std::string &text)
{
    const auto easing = EM::parse_easing(text);
    if (!easing)
        throw std::runtime_error("expected easing to parse: " + text);
    return *easing;
}

bool rejects(const std::string &text)
{
    const char *issue = nullptr;
    const auto easing = EM::parse_easing(text, &issue);
    return !easing && issue != nullptr && std::string(issue).size() > 10;
}

EM::Lookup bag_lookup(const json &bag)
{
    return [bag](const std::string &key) {
        const auto it = bag.find(key);
        return it == bag.end() ? json() : *it;
    };
}

void keys()
{
    const auto all = EM::all_keys();
    require(all.size() == 18, "six triggers by three fields");
    std::set<std::string> unique(all.begin(), all.end());
    require(unique.size() == 18, "keys are unique");
    for (EM::Trigger trigger : EM::triggers())
        for (EM::Field field : EM::fields()) {
            const std::string key = EM::property_key(trigger, field);
            const auto parts = EM::parse_key(key);
            require(parts && parts->trigger == trigger && parts->field == field, "key round trip");
            require(EM::is_motion_key(key) && EM::is_motion_namespace(key), "key is understood");
        }
    require(EM::property_key(EM::Trigger::Hover, EM::Field::Duration) == "motion.hover.duration", "documented key shape");
    require(EM::property_key(EM::Trigger::Entrance, EM::Field::Easing) == "motion.entrance.easing", "entrance key");
    require(!EM::is_motion_key("motion.hover.iterationCount"), "an unknown motion key is not understood");
    require(EM::is_motion_namespace("motion.hover.iterationCount"), "but it is in the motion namespace");
    require(!EM::is_motion_key("fontSize") && !EM::is_motion_namespace("fontSize"), "style keys are not motion keys");
    require(EM::state_triggers().size() == 5, "five state triggers");
    require(std::string(EM::trigger_key(EM::Trigger::Selection)) == "selection", "selection key");
}

void triggers()
{
    using EM::Trigger;
    require(EM::trigger_for_change(0, EM::kHoveredBit) == Trigger::Hover, "hover enter");
    require(EM::trigger_for_change(EM::kHoveredBit, 0) == Trigger::Hover, "hover leave");
    require(EM::trigger_for_change(0, EM::kFocusedBit) == Trigger::Focus, "focus");
    require(EM::trigger_for_change(EM::kHoveredBit, EM::kHoveredBit | EM::kPressedBit) == Trigger::Press, "press");
    require(EM::trigger_for_change(0, EM::kCheckedBit) == Trigger::Selection, "selection");
    require(EM::trigger_for_change(EM::kEnabledBit, 0) == Trigger::Enable, "disable");
    // Several bits in one event: the most specific wins.
    require(EM::trigger_for_change(0, EM::kPressedBit | EM::kFocusedBit | EM::kHoveredBit) == Trigger::Press, "press first");
    require(EM::trigger_for_change(0, EM::kCheckedBit | EM::kFocusedBit) == Trigger::Selection, "selection before focus");
    require(EM::trigger_for_change(0, EM::kFocusedBit | EM::kHoveredBit | EM::kEnabledBit) == Trigger::Focus, "focus before hover");
    require(EM::trigger_for_change(0, EM::kHoveredBit | EM::kEnabledBit) == Trigger::Hover, "hover before enable");
    // "Not" masks in the high half never select a trigger.
    require(EM::trigger_for_change(0, EM::kHoveredBit << 16) == Trigger::Hover, "neutral default");
}

void easing_grammar()
{
    // Every named curve parses to itself and serialises back to its keyword.
    for (const EM::NamedCurve &curve : EM::named_curves()) {
        const EM::Easing easing = parse(curve.name);
        require(easing.to_string() == curve.name, "named curve round trip");
        require(near(easing.apply(0), 0) && near(easing.apply(1), 1), "named curve endpoints");
    }
    require(EM::named_curves().size() == 11, "eleven named curves");
    require(std::string(EM::named_curves().front().name) == "standard", "Material standard is first");

    // Case and surrounding space are not significant.
    require(parse("  Emphasized ").to_string() == "emphasized", "case insensitive keyword");

    const EM::Easing bezier = parse("cubic-bezier(0.2, 0, 0, 1)");
    require(bezier.kind == EM::Easing::Kind::CubicBezier, "cubic-bezier kind");
    require(bezier.to_string() == "cubic-bezier(0.2, 0, 0, 1)", "cubic-bezier canonical text");
    require(parse("cubic-bezier(.3,-0.5,0.7,1.5)").to_string() == "cubic-bezier(0.3, -0.5, 0.7, 1.5)", "short numbers and overshoot");
    require(parse("cubic-bezier(0.123456789, 0.5, 0.25, 0.987654321)") ==
                parse(parse("cubic-bezier(0.123456789, 0.5, 0.25, 0.987654321)").to_string()),
            "full precision survives a round trip");

    require(parse("steps(4)").to_string() == "steps(4, jump-end)", "steps default position");
    require(parse("steps(3, start)").to_string() == "steps(3, jump-start)", "legacy start");
    require(parse("steps(2,jump-none)").to_string() == "steps(2, jump-none)", "jump-none");
    require(parse("step-start").to_string() == "steps(1, jump-start)", "step-start");
    require(parse("step-end").to_string() == "steps(1, jump-end)", "step-end");

    const EM::Easing stops = parse("linear(0, 0.25 40%, 1)");
    require(stops.kind == EM::Easing::Kind::LinearStops && stops.stops.size() == 3, "three stops");
    require(stops.to_string() == "linear(0 0%, 0.25 40%, 1 100%)", "resolved inputs are written out");
    require(parse(stops.to_string()) == stops, "linear() round trip");
    // Runs without an input are spread evenly; inputs never go backwards.
    const EM::Easing spread = parse("linear(0, 0.1, 0.2, 1)");
    require(near(spread.stops[1].input, 100.0 / 3) && near(spread.stops[2].input, 200.0 / 3), "even spread");
    const EM::Easing backwards = parse("linear(0 50%, 1 20%)");
    require(near(backwards.stops[1].input, 50), "inputs are made non-decreasing");
    const EM::Easing twice = parse("linear(0, 0.5 25% 75%, 1)");
    require(twice.stops.size() == 4, "two percentages make two stops");

    // Refusals carry a reason.
    require(rejects(""), "empty");
    require(rejects("wobbly"), "unknown keyword");
    require(rejects("cubic-bezier(0.2, 0, 0)"), "three numbers");
    require(rejects("cubic-bezier(1.2, 0, 0, 1)"), "x1 above 1");
    require(rejects("cubic-bezier(0.2, 0, -0.1, 1)"), "x2 below 0");
    require(rejects("cubic-bezier(0.2, 6, 0, 1)"), "y above the bound");
    require(rejects("cubic-bezier(0.2, nan, 0, 1)"), "not a number");
    require(rejects("cubic-bezier(0.2, 0, 0, 1) x"), "trailing text");
    require(rejects("cubic-bezier(0.2, 0, 0, 1"), "unclosed");
    require(rejects("steps(0)"), "zero steps");
    require(rejects("steps(101)"), "too many steps");
    require(rejects("steps(2.5)"), "fractional steps");
    require(rejects("steps(1, jump-none)"), "jump-none needs two steps");
    require(rejects("steps(3, sideways)"), "unknown position");
    require(rejects("linear(1)"), "one stop");
    require(rejects("linear(0, 1 50)"), "percentage without a sign");
    require(rejects("linear(0, 0.5 10% 20% 30%, 1)"), "three percentages");
    std::string many = "linear(0";
    for (int i = 0; i < 64; ++i)
        many += ", 0.5";
    require(rejects(many + ")"), "more than 64 stops");
}

void easing_values()
{
    // The model's named curves are the exact arithmetic the widgets play.
    for (double t : {0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0}) {
        require(parse("standard").apply(t) == MD3::Motion::Math::standard(t), "standard is the widget curve");
        require(parse("emphasized").apply(t) == MD3::Motion::Math::emphasized(t), "emphasized is the widget curve");
        require(near(parse("linear").apply(t), t), "linear is identity");
    }
    require(parse("standard").apply(0.5) > 0.5, "standard decelerates late in the curve");
    require(near(parse("cubic-bezier(0, 0, 1, 1)").apply(0.3), 0.3, 1e-6), "the straight Bezier is linear");
    require(near(parse("ease").apply(0.5), MD3::Motion::Math::cubic_bezier(0.5, 0.25, 0.1, 0.25, 1)), "ease");
    // Overshoot is kept by the curve; consumers clamp colours and opacity.
    require(parse("cubic-bezier(0.3, 0, 0.3, 1.8)").apply(0.6) > 1.0, "overshoot");

    const EM::Easing end4 = parse("steps(4, jump-end)");
    require(near(end4.apply(0), 0) && near(end4.apply(0.24), 0) && near(end4.apply(0.25), 0.25) && near(end4.apply(0.99), 0.75) &&
                near(end4.apply(1), 1),
            "jump-end");
    const EM::Easing start4 = parse("steps(4, jump-start)");
    require(near(start4.apply(0), 0.25) && near(start4.apply(0.5), 0.75) && near(start4.apply(1), 1), "jump-start");
    const EM::Easing none3 = parse("steps(3, jump-none)");
    require(near(none3.apply(0), 0) && near(none3.apply(0.5), 0.5) && near(none3.apply(1), 1), "jump-none");
    const EM::Easing both3 = parse("steps(3, jump-both)");
    require(near(both3.apply(0), 0.25) && near(both3.apply(0.5), 0.5) && near(both3.apply(1), 1), "jump-both");

    const EM::Easing stops = parse("linear(0, 0.25 40%, 1)");
    require(near(stops.apply(0.2), 0.125) && near(stops.apply(0.4), 0.25) && near(stops.apply(0.7), 0.625), "linear stops");
    require(near(parse("linear(0, 0.5 50% 70%, 1)").apply(0.6), 0.5), "a held segment");
    // Outside [0, 1] the input is clamped, NaN included.
    require(near(parse("linear").apply(-2), 0) && near(parse("linear").apply(7), 1), "clamped input");
    require(near(parse("linear").apply(std::numeric_limits<double>::quiet_NaN()), 0), "NaN input");
}

void timing()
{
    EM::Timing t = EM::material_default(100);
    require(t.duration_ms == 100 && t.delay_ms == 0 && t.easing.to_string() == "standard", "Material default");
    require(t.total_ms() == 100, "total without delay");
    for (double f : {0.0, 0.3, 0.6, 0.99})
        require(t.progress(f) == MD3::Motion::Math::standard(f), "the default plays the widget curve unchanged");
    require(t.progress(1.0) == 1.0 && t.progress(5.0) == 1.0, "settles at the end");
    require(t.progress(std::numeric_limits<double>::quiet_NaN()) == 1.0, "NaN settles");

    t.delay_ms = 100;
    require(t.total_ms() == 200, "a positive delay lengthens the active time");
    require(t.progress(0.25) == 0.0 && t.progress(0.5) == 0.0, "the start holds during the delay");
    require(near(t.progress(0.75), MD3::Motion::Math::standard(0.5)), "after the delay the curve runs");

    t.delay_ms = -50;
    require(t.total_ms() == 50, "a negative delay shortens it");
    require(near(t.progress(0.0), MD3::Motion::Math::standard(0.5)), "and starts part of the way through");

    t.delay_ms = -150;
    require(t.total_ms() == 0 && t.progress(0.0) == 1.0, "a delay beyond the duration is the end state");

    EM::Timing zero = EM::material_default(0);
    require(zero.total_ms() == 0 && zero.progress(0.0) == 1.0, "zero duration is instant");
    zero.delay_ms = 80;
    require(zero.total_ms() == 80 && zero.progress(0.5) == 0.0 && zero.progress(1.0) == 1.0, "a delayed jump");

    // A curve ending away from 1 still settles on the target state.
    EM::Timing stops = EM::material_default(100);
    stops.easing     = parse("linear(0, 0.4)");
    require(near(stops.progress(0.999), 0.4, 1e-3) && stops.progress(1.0) == 1.0, "final state is exact");

    require(EM::material_default(-5).duration_ms == 0 && EM::material_default(99999).duration_ms == EM::kMaxDurationMs,
            "defaults are clamped into range");
}

void validation()
{
    using EM::Field;
    require(EM::validate(Field::Duration, 0).ok && EM::validate(Field::Duration, 10000).ok, "duration bounds");
    require(EM::validate(Field::Duration, 120.5).ok, "fractional milliseconds are valid");
    require(!EM::validate(Field::Duration, -1).ok && !EM::validate(Field::Duration, 10001).ok, "duration out of range");
    require(!EM::validate(Field::Duration, "100").ok && !EM::validate(Field::Duration, true).ok, "duration type");
    require(!EM::validate(Field::Duration, std::numeric_limits<double>::quiet_NaN()).ok, "NaN duration");
    require(!EM::validate(Field::Duration, std::numeric_limits<double>::infinity()).ok, "infinite duration");
    require(EM::validate(Field::Delay, -10000).ok && EM::validate(Field::Delay, 10000).ok, "delay bounds");
    require(!EM::validate(Field::Delay, -10001).ok, "delay below range");
    require(EM::validate(Field::Easing, "steps(2)").ok, "easing text");
    require(!EM::validate(Field::Easing, 3).ok && !EM::validate(Field::Easing, "nope").ok, "easing type and grammar");
    require(EM::validate(Field::Easing, "nope").issue != nullptr, "the grammar reason is reported");
    require(EM::validate_key("fontSize", "anything").ok, "non-motion keys are not judged here");
    require(!EM::validate_key("motion.press.delay", "soon").ok, "motion keys are");
}

void resolution()
{
    const EM::Timing defaults = EM::material_default(100);
    // Nothing stored: the default, every field unstored.
    EM::Resolved none = EM::resolve(bag_lookup(json::object()), EM::Trigger::Hover, defaults);
    require(none.timing == defaults && none.fields.size() == 3, "unset resolves to the default");
    for (const EM::FieldState &field : none.fields)
        require(!field.stored && field.valid, "nothing stored");

    // Valid and invalid values side by side.
    const json bag = {{"motion.hover.duration", 250},
                      {"motion.hover.delay", "later"},
                      {"motion.hover.easing", "emphasized-decelerate"},
                      {"motion.focus.duration", 40}};
    EM::Resolved hover = EM::resolve(bag_lookup(bag), EM::Trigger::Hover, defaults);
    require(hover.timing.duration_ms == 250, "stored duration applies");
    require(hover.timing.delay_ms == 0, "an invalid delay keeps the default");
    require(hover.fields[1].stored && !hover.fields[1].valid && hover.fields[1].issue, "and says why");
    require(hover.fields[1].value == "later", "the stored value is kept verbatim");
    require(hover.timing.easing.to_string() == "emphasized-decelerate", "stored easing applies");
    EM::Resolved focus = EM::resolve(bag_lookup(bag), EM::Trigger::Focus, defaults);
    require(focus.timing.duration_ms == 40 && focus.timing.easing.to_string() == "standard", "triggers are independent");
    EM::Resolved press = EM::resolve(bag_lookup(bag), EM::Trigger::Press, defaults);
    require(press.timing == defaults, "an unset trigger is the default");
    require(EM::resolve(nullptr, EM::Trigger::Hover, defaults).timing == defaults, "no lookup at all");
}

void serialisation()
{
    // Values written by the editor survive a save and load (JSON text) and
    // resolve to the same timing: the theme export/import path.
    EM::Timing authored;
    authored.duration_ms = 320;
    authored.delay_ms    = -40;
    authored.easing      = parse("linear(0, 0.3 25%, 0.9 75%, 1)");
    json bag = json::object();
    for (EM::Field field : EM::fields())
        bag[EM::property_key(EM::Trigger::Press, field)] = EM::to_json(field, authored);
    bag["motion.press.iterationCount"] = 3; // a portable value this version does not know
    bag["fontSize"]                    = 14;
    require(bag["motion.press.duration"].is_number_integer(), "whole milliseconds stay integers");

    const json reloaded = json::parse(bag.dump(2));
    require(reloaded == bag, "the text round trip is lossless, unknown keys included");
    const EM::Resolved back = EM::resolve(bag_lookup(reloaded), EM::Trigger::Press, EM::material_default(100));
    require(back.timing == authored, "the authored timing comes back exactly");
    const auto unknown = EM::unknown_motion_keys(reloaded);
    require(unknown.size() == 1 && unknown[0] == "motion.press.iterationCount", "unknown motion keys are reported, not dropped");

    EM::Timing fractional = authored;
    fractional.duration_ms = 120.5;
    require(EM::to_json(EM::Field::Duration, fractional).is_number_float(), "fractional milliseconds keep their precision");
}

void undo_redo()
{
    EM::EditHistory history(3);
    require(!history.can_undo() && !history.can_redo(), "empty");
    history.record({"no-op", {{"a", "motion.hover.duration", 100, 100}}});
    require(!history.can_undo(), "an edit that changes nothing is ignored");

    history.record({"Hover duration", {{"a", "motion.hover.duration", json(), 200}}});
    history.record({"Hover delay", {{"a", "motion.hover.delay", json(), 50}}});
    require(history.undo_size() == 2 && history.next_undo()->label == "Hover delay", "two edits");

    auto undone = history.undo();
    require(undone && undone->changes[0].key == "motion.hover.delay" && undone->changes[0].before.is_null(), "undo the delay");
    require(history.can_redo() && history.next_redo()->label == "Hover delay", "redo is available");
    auto redone = history.redo();
    require(redone && redone->changes[0].after == 50, "redo the delay");
    history.undo();

    // A new edit after undo discards the redo branch.
    history.record({"Hover easing", {{"a", "motion.hover.easing", json(), "linear"}}});
    require(!history.can_redo() && history.undo_size() == 2, "redo branch dropped");

    // Bounded: the oldest edits fall off.
    history.record({"one", {{"b", "motion.focus.duration", json(), 1}}});
    history.record({"two", {{"b", "motion.focus.duration", 1, 2}}});
    require(history.undo_size() == 3, "bounded to the limit");
    int undos = 0;
    while (history.undo())
        ++undos;
    require(undos == 3 && history.redo_size() == 3, "every remaining edit undoes");

    // A grouped edit (reset element motion) undoes as one step.
    EM::EditHistory grouped;
    grouped.record({"Reset element motion", {{"a", "motion.hover.duration", 200, json()}, {"a", "motion.hover.delay", 10, json()}}});
    const auto reset = grouped.undo();
    require(reset && reset->changes.size() == 2, "one step restores both values");
}

void inventory()
{
    std::set<std::string> ids;
    std::map<std::string, int> per_area;
    int wired = 0, unavailable = 0;
    for (const EM::Capability &row : EM::capabilities()) {
        require(row.id && *row.id && ids.insert(row.id).second, "capability ids are unique");
        require(row.title && std::string(row.title).size() > 2, "every capability has a title");
        require(row.detail && std::string(row.detail).size() >= 30, "every capability explains itself");
        ++per_area[row.area];
        (row.support == EM::Support::Wired ? wired : unavailable) += 1;
        require(std::string(EM::area_label(row.area)).size() > 5, "every area has a heading");
    }
    require(per_area.size() == 5, "five contract areas");
    require(wired > 0 && unavailable > 0, "both supported and unavailable capabilities are listed");
    for (const char *id : {"duration", "delay", "negative-delay", "easing-bezier", "easing-steps", "easing-linear-stops",
                           "easing-spring", "tracks", "keyframes", "trigger-hover", "trigger-entrance", "trigger-exit",
                           "trigger-status-states", "reduced-motion", "undo-redo", "import-export", "presets"})
        require(ids.count(id) == 1, "a required contract capability is present");
}

} // namespace

int main()
{
    try {
        keys();
        triggers();
        easing_grammar();
        easing_values();
        timing();
        validation();
        resolution();
        serialisation();
        undo_redo();
        inventory();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "element motion tests failed: %s\n", e.what());
        return 1;
    }
    std::printf("element motion tests passed (%d checks)\n", g_checks);
    return 0;
}
