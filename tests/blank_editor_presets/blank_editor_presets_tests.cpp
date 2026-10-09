#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <catch_main.hpp>

#include "libslic3r/EffectivePreferences.hpp"
#include "libslic3r/PresentationDefaults.hpp"
#include "slic3r/GUI/DimSumSurpriseModel.hpp"
#include "slic3r/GUI/Presets/BlankEditorPresets.hpp"
#include "slic3r/GUI/Schedule/ScheduleRulePresets.hpp"
#include "slic3r/GUI/Schedule/ScheduledSettingsModel.hpp"

#include <algorithm>
#include <set>
#include <string>

using namespace Slic3r;
using namespace Slic3r::GUI::BlankEditorPresets;
namespace ScheduleRulePresets = Slic3r::GUI::ScheduleRulePresets;

namespace {

const Preset *find(const std::vector<Preset> &presets, const std::string &id)
{
    for (const Preset &preset : presets)
        if (preset.id == id)
            return &preset;
    return nullptr;
}

const Assignment *find_assignment(const Preset &preset, const std::string &key)
{
    for (const Assignment &assignment : preset.sets)
        if (assignment.key == key)
            return &assignment;
    return nullptr;
}

EditorSpec sample_spec()
{
    EditorSpec spec;
    spec.id     = "sample";
    spec.fields = {
        {"dark_color_mode", "Theme", {{"0", "Light"}, {"1", "Dark"}}, ""},
        {"ui_density", "Density", {{"comfortable", "Comfortable"}, {"compact", "Compact"}}, ""},
        {"ui_font_family", "Font", {}, "Default (Roboto)"},
        {"language", "Language mode", {}, ""},
    };
    spec.creates_defaults = "Creates a sample.";
    spec.creates_saved    = "Creates a sample from saved values.";
    spec.creates_empty    = "Creates an empty sample.";
    return spec;
}

} // namespace

TEST_CASE("Every presentation preference has a shipped value or a stated reason", "[blank-editors][defaults]")
{
    std::set<std::string> covered;
    for (const PresentationDefaults::Entry &entry : PresentationDefaults::entries()) {
        INFO(entry.key);
        CHECK(covered.insert(entry.key).second); // listed once
        CHECK_FALSE(PresentationDefaults::is_unfixed(entry.key));
    }
    for (const std::string &key : PresentationDefaults::unfixed_keys()) {
        INFO(key);
        CHECK(covered.insert(key).second);
        CHECK(unfixed_reason(key) != nullptr);
    }
    for (const EffectivePreferences::Descriptor &descriptor : EffectivePreferences::descriptors()) {
        INFO(descriptor.key);
        CHECK(covered.count(descriptor.key) == 1);
        const auto shipped = PresentationDefaults::value(descriptor.key);
        if (shipped) // the shipped value is one the preference itself accepts
            CHECK(EffectivePreferences::decode(descriptor, *shipped).has_value());
    }
    for (const std::string &key : GUI::Schedule::allowed_keys()) {
        INFO(key);
        CHECK(covered.count(key) == 1);
        const auto shipped = PresentationDefaults::value(key);
        if (shipped)
            CHECK(GUI::Schedule::validate_value(key, *shipped).empty());
    }
}

TEST_CASE("Shipped funny level and accent agree with their owners", "[blank-editors][defaults]")
{
    CHECK(PresentationDefaults::kFunnyLevel == GUI::DimSum::FUNNY_LEVEL_DEFAULT);
    CHECK(PresentationDefaults::value("funny_level_en") == std::to_string(PresentationDefaults::kFunnyLevel));
    CHECK(PresentationDefaults::value("funny_level_yue") == std::to_string(PresentationDefaults::kFunnyLevel));
    CHECK(PresentationDefaults::value("ui_accent_seed") == std::string(PresentationDefaults::kAccentSeed));
    CHECK_FALSE(PresentationDefaults::value("no_such_key").has_value());
    CHECK(PresentationDefaults::value_or_empty("no_such_key").empty());
}

TEST_CASE("Standard presets come in a fixed order and say what they create", "[blank-editors][presets]")
{
    const std::vector<Preset> presets = start_presets(sample_spec(), {});
    REQUIRE(presets.size() == 3);
    CHECK(presets[0].id == kShippedDefaultsId);
    CHECK(presets[0].origin == Origin::ShippedDefaults);
    CHECK(presets[1].id == kSavedSettingsId);
    CHECK(presets[1].origin == Origin::SavedSettings);
    CHECK(presets[2].id == kEmptyId);
    CHECK(presets[2].origin == Origin::Empty);
    CHECK(find(presets, kSavedSettingsId) == &presets[1]);
    CHECK(find(presets, "template:none") == nullptr);
    for (const Preset &preset : presets) {
        CHECK_FALSE(preset.title.empty());
        CHECK_FALSE(preset.creates.empty());
    }
    CHECK(presets[0].creates == "Creates a sample.");
    CHECK(presets[1].creates == "Creates a sample from saved values.");
    CHECK(presets[2].creates == "Creates an empty sample.");
    CHECK(presets[2].sets.empty());
    CHECK(presets[2].left_out.empty());
}

TEST_CASE("The shipped-defaults preset sets exactly the reset values", "[blank-editors][presets]")
{
    const std::vector<Preset> presets = start_presets(sample_spec(), {});
    const Preset &defaults = presets[0];
    REQUIRE(defaults.sets.size() == 3);
    for (const Assignment &assignment : defaults.sets) {
        INFO(assignment.key);
        CHECK(PresentationDefaults::value(assignment.key) == assignment.value);
        CHECK_FALSE(assignment.from_default);
        CHECK(assignment.scope.empty());
    }
    const Assignment *theme = find_assignment(defaults, "dark_color_mode");
    REQUIRE(theme != nullptr);
    CHECK(theme->label == "Theme");
    CHECK(theme->shown == "Light");
    CHECK(theme->shown_translatable);
    const Assignment *font = find_assignment(defaults, "ui_font_family");
    REQUIRE(font != nullptr);
    CHECK(font->value.empty());
    CHECK(font->shown == "Default (Roboto)");
    // The language has no single shipped value: left out, with the reason.
    REQUIRE(defaults.left_out.size() == 1);
    CHECK(defaults.left_out[0].key == "language");
    CHECK(defaults.left_out[0].label == "Language mode");
    CHECK(defaults.left_out[0].reason == unfixed_reason("language"));
    CHECK(defaults.values() == Values{{"dark_color_mode", "0"}, {"ui_density", "comfortable"}, {"ui_font_family", ""}});
}

TEST_CASE("The saved-settings preset copies saved values and falls back honestly", "[blank-editors][presets]")
{
    EditorSpec spec = sample_spec();
    spec.valid = [](const std::string &key, const std::string &value) {
        return key != "ui_density" || value == "comfortable" || value == "compact";
    };
    const Values saved = {{"dark_color_mode", "1"}, {"ui_density", "spacious"}, {"language", "yue_HK"}};
    const std::vector<Preset> presets = start_presets(spec, saved);
    const Preset &mine = presets[1];
    REQUIRE(mine.sets.size() == 4);
    const Assignment *theme = find_assignment(mine, "dark_color_mode");
    REQUIRE(theme != nullptr);
    CHECK(theme->value == "1");
    CHECK(theme->shown == "Dark");
    CHECK_FALSE(theme->from_default);
    // A rejected saved value is never carried over: the shipped default is used and flagged.
    const Assignment *density = find_assignment(mine, "ui_density");
    REQUIRE(density != nullptr);
    CHECK(density->value == "comfortable");
    CHECK(density->from_default);
    // Nothing saved: the shipped default, flagged.
    const Assignment *font = find_assignment(mine, "ui_font_family");
    REQUIRE(font != nullptr);
    CHECK(font->from_default);
    // Saved even without a shipped value.
    const Assignment *language = find_assignment(mine, "language");
    REQUIRE(language != nullptr);
    CHECK(language->value == "yue_HK");
    CHECK(language->shown == "yue_HK");
    CHECK_FALSE(language->shown_translatable);
    CHECK(mine.left_out.empty());

    // Neither saved nor shipped: left out with the reason, never guessed.
    const std::vector<Preset> bare_presets = start_presets(spec, {});
    const Preset &bare = bare_presets[1];
    REQUIRE(bare.left_out.size() == 1);
    CHECK(bare.left_out[0].key == "language");
    CHECK_FALSE(bare.left_out[0].reason.empty());
}

TEST_CASE("A preset never carries a value from anywhere but the table or the saved values", "[blank-editors][presets]")
{
    EditorSpec spec;
    spec.id = "every-presentation-key";
    for (const EffectivePreferences::Descriptor &descriptor : EffectivePreferences::descriptors())
        spec.fields.push_back({descriptor.key, descriptor.key, {}, ""});
    spec.creates_defaults = spec.creates_saved = spec.creates_empty = "Creates.";
    spec.valid = [](const std::string &key, const std::string &value) {
        const auto *descriptor = EffectivePreferences::descriptor(key);
        return descriptor && EffectivePreferences::decode(*descriptor, value).has_value();
    };
    const Values saved = {{"dark_color_mode", "1"}, {"funny_level_en", "9"}, {"ui_accent_seed", "#123456"}};
    const std::vector<Preset> presets = start_presets(spec, saved);
    for (const Preset &preset : presets) {
        for (const Assignment &assignment : preset.sets) {
            INFO(preset.id << " " << assignment.key << "=" << assignment.value);
            const auto shipped = PresentationDefaults::value(assignment.key);
            const auto stored  = saved.find(assignment.key);
            const bool from_table = shipped && *shipped == assignment.value;
            const bool from_saved = preset.origin == Origin::SavedSettings && stored != saved.end() &&
                                    stored->second == assignment.value;
            CHECK((from_table || from_saved));
            CHECK(spec.valid(assignment.key, assignment.value));
        }
    }
    // The out-of-range saved funny level is not copied.
    const Preset &mine = presets[1];
    const Assignment *funny = find_assignment(mine, "funny_level_en");
    REQUIRE(funny != nullptr);
    CHECK(funny->value == "5");
    CHECK(funny->from_default);
}

TEST_CASE("A shipped default the editor rejects is left out, not altered", "[blank-editors][presets]")
{
    EditorSpec spec = sample_spec();
    spec.valid = [](const std::string &key, const std::string &) { return key != "ui_density"; };
    const std::vector<Preset> presets = start_presets(spec, {});
    const Preset &defaults = presets[0];
    CHECK(find_assignment(defaults, "ui_density") == nullptr);
    bool density_left_out = false;
    for (const Omission &omission : defaults.left_out)
        if (omission.key == "ui_density") {
            density_left_out = true;
            CHECK_FALSE(omission.reason.empty());
        }
    CHECK(density_left_out);
}

TEST_CASE("Templates pass through exactly as their owner resolved them", "[blank-editors][presets]")
{
    Assignment size;
    size.key   = "*/fontSize";
    size.scope = "Every element";
    size.label = "Font size (pt)";
    size.value = size.shown = "15.5";
    const Preset preset = template_preset("template:Large text", "Large text", "Makes Large text the active preset.", {size});
    CHECK(preset.origin == Origin::Template);
    CHECK(preset.id == "template:Large text");
    REQUIRE(preset.sets.size() == 1);
    CHECK(preset.sets[0].value == "15.5");
    CHECK(preset.values() == Values{{"*/fontSize", "15.5"}});
}

TEST_CASE("Values are shown with their choice or empty label", "[blank-editors][presets]")
{
    const Field theme{"dark_color_mode", "Theme", {{"0", "Light"}, {"1", "Dark"}}, ""};
    bool translatable = false;
    CHECK(shown_value(theme, "1", translatable) == "Dark");
    CHECK(translatable);
    CHECK(shown_value(theme, "7", translatable) == "7");
    CHECK_FALSE(translatable);
    CHECK(shown_value(theme, "", translatable) == "(empty)");
    CHECK(translatable);
    const Field name{"app_display_name", "App name", {}, "Shipped name"};
    CHECK(shown_value(name, "", translatable) == "Shipped name");
    CHECK(translatable);
}

TEST_CASE("An appearance preset is stated property by property", "[blank-editors][appearance]")
{
    // The shape of the shipped "Large text" preset: every element gets a size
    // and a line height; one element also gets a weight, a decoration and a
    // property this version does not know.
    std::map<std::string, nlohmann::json> preset;
    preset["*"]           = nlohmann::json{{"fontSize", 15.5}, {"lineHeight", 1.4}};
    preset["project-tab"] = nlohmann::json{{"fontWeight", 600}, {"underline", true}, {"glowRadius", 3}};
    const std::vector<Assignment> sets = style_assignments(preset);
    REQUIRE(sets.size() == 5);
    // "*" first, then the element ids in order; properties in the bag's order.
    CHECK(sets[0].scope == "every element");
    CHECK(sets[0].scope_translatable);
    CHECK(sets[0].key == "*/fontSize");
    CHECK(sets[0].label == "Font size (pt)");
    CHECK(sets[0].value == "15.5");
    CHECK(sets[0].shown == "15.5");
    CHECK_FALSE(sets[0].shown_translatable);
    CHECK(sets[1].label == "Line height");
    CHECK(sets[1].shown == "1.4");
    CHECK(sets[2].scope == "project-tab");
    CHECK_FALSE(sets[2].scope_translatable);
    CHECK(sets[2].key == "project-tab/fontWeight");
    CHECK(sets[2].label == "Font weight");
    CHECK(sets[2].shown == "600");
    // An unknown property is still stated, by its key.
    CHECK(sets[3].key == "project-tab/glowRadius");
    CHECK(sets[3].label == "glowRadius");
    CHECK(sets[3].shown == "3");
    CHECK(sets[4].label == "Underline");
    CHECK(sets[4].shown == "On");
    CHECK(sets[4].shown_translatable);
    // Strings are shown as written; the Material default preset sets nothing.
    CHECK(style_assignments({{"*", nlohmann::json{{"foreground", "#112233"}}}})[0].shown == "#112233");
    CHECK(style_assignments({}).empty());
    CHECK(style_property_label("radius") == std::string("Corner radius (px)"));
    CHECK(style_property_label("glowRadius") == nullptr);
}

namespace {

const std::vector<Choice> kLanguageModes = {
    {"en_US", "English"}, {"yue_HK", "Cantonese (Hong Kong)"}, {"bilingual_en_yue_HK", "Bilingual: English and Cantonese"}};

// The registry FeatureServices::ScheduledPreferences builds from the
// presentation descriptors (labels there come from the command palette).
ScheduledSettings::Registry service_registry()
{
    ScheduledSettings::Registry registry;
    for (const EffectivePreferences::Descriptor &d : EffectivePreferences::descriptors()) {
        ScheduledSettings::Kind kind = ScheduledSettings::Kind::Text;
        switch (d.kind) {
        case EffectivePreferences::Kind::Boolean: kind = ScheduledSettings::Kind::Boolean; break;
        case EffectivePreferences::Kind::Integer: kind = ScheduledSettings::Kind::Integer; break;
        case EffectivePreferences::Kind::Choice: kind = ScheduledSettings::Kind::Choice; break;
        case EffectivePreferences::Kind::Color: kind = ScheduledSettings::Kind::Color; break;
        default: break;
        }
        registry.add({d.key, d.key, d.key, kind, double(d.low), double(d.high), d.choices, d.limit});
    }
    return registry;
}

} // namespace

TEST_CASE("Preferences schedule rules start from shipped defaults, saved settings or nothing", "[blank-editors][schedule]")
{
    const EditorSpec spec = ScheduleRulePresets::schedule_rule_spec(kLanguageModes);
    REQUIRE(spec.fields.size() == GUI::Schedule::allowed_keys().size());
    for (size_t i = 0; i < spec.fields.size(); ++i)
        CHECK(spec.fields[i].key == GUI::Schedule::allowed_keys()[i]);
    CHECK_FALSE(spec.creates_defaults.empty());
    CHECK_FALSE(spec.creates_saved.empty());
    CHECK_FALSE(spec.creates_empty.empty());

    // A rule currently controls the theme; its captured base value is the person's own.
    GUI::Schedule::OverrideState state;
    state.base["dark_color_mode"]    = "0";
    state.applied["dark_color_mode"] = "1";
    std::map<std::string, std::string> live;
    for (const std::string &key : GUI::Schedule::allowed_keys())
        live[key] = "";
    live["dark_color_mode"] = "1"; // the scheduled value, not the person's
    live["language"]        = "yue_HK";
    live["ui_density"]      = "compact";
    const Values saved = ScheduleRulePresets::saved_schedule_values(live, state);
    CHECK(saved.at("dark_color_mode") == "0");
    CHECK(saved.at("ui_density") == "compact");

    const std::vector<Preset> presets = start_presets(spec, saved);
    REQUIRE(presets.size() == 3);
    const Preset &defaults = presets[0];
    // Every shipped value is one the rule accepts, and only the language is left out.
    CHECK(defaults.sets.size() == 8);
    REQUIRE(defaults.left_out.size() == 1);
    CHECK(defaults.left_out[0].key == "language");
    for (const Assignment &assignment : defaults.sets) {
        INFO(assignment.key);
        CHECK(GUI::Schedule::validate_value(assignment.key, assignment.value).empty());
        CHECK(PresentationDefaults::value(assignment.key) == assignment.value);
    }
    const Assignment *theme = find_assignment(defaults, "dark_color_mode");
    REQUIRE(theme != nullptr);
    CHECK(theme->shown == "Light");

    const Preset &mine = presets[1];
    CHECK(mine.values().at("dark_color_mode") == "0");
    CHECK(mine.values().at("ui_density") == "compact");
    CHECK(mine.values().at("language") == "yue_HK");
    CHECK(find_assignment(mine, "language")->shown == "Cantonese (Hong Kong)");
    // Stored as empty (never set): the shipped default, flagged.
    CHECK(find_assignment(mine, "funny_level_en")->value == "5");
    CHECK(find_assignment(mine, "funny_level_en")->from_default);

    // The rule a preset creates is valid as it stands and uses the model's
    // own window: all day, every day, no dates, local values.
    const GUI::Schedule::Rule rule = ScheduleRulePresets::schedule_rule_from(defaults, "r-0001", "Shipped defaults");
    CHECK(rule.id == "r-0001");
    CHECK(rule.label == "Shipped defaults");
    CHECK(rule.values == defaults.values());
    CHECK(rule.all_day());
    CHECK(rule.weekdays == GUI::Schedule::kEveryDay);
    CHECK_FALSE(rule.start_date.has_value());
    CHECK(rule.source == GUI::Schedule::SourceKind::Local);
    CHECK(GUI::Schedule::validate_rule(rule).empty());
    CHECK(GUI::Schedule::validate_rule(ScheduleRulePresets::schedule_rule_from(mine, "r-0002", "Mine")).empty());
    // An empty start is honest about needing a setting before it can be saved.
    const GUI::Schedule::Rule empty_rule = ScheduleRulePresets::schedule_rule_from(presets[2], "r-0003", "Empty");
    CHECK(empty_rule.values.empty());
    CHECK_FALSE(GUI::Schedule::validate_rule(empty_rule).empty());
}

TEST_CASE("Scheduled settings rules start from the registry's real settings", "[blank-editors][schedule]")
{
    const ScheduledSettings::Registry registry = service_registry();
    const EditorSpec all = ScheduleRulePresets::service_rule_spec(registry, nullptr);
    CHECK(all.fields.size() == registry.entries().size());
    // The panel's visibility filter (school presentation) is honoured.
    const EditorSpec visible = ScheduleRulePresets::service_rule_spec(registry, [](const std::string &key) { return key != "language"; });
    CHECK(visible.fields.size() == registry.entries().size() - 1);
    for (const Field &field : visible.fields)
        CHECK(field.key != "language");

    // The base values the service evaluates are the "saved settings".
    ScheduledSettings::Values base = {{"dark_color_mode", std::string("1")}, {"funny_level_en", int64_t(2)},
                                      {"dialog_emojis", true}};
    const Values saved = ScheduleRulePresets::encode_values(base);
    CHECK(saved.at("dark_color_mode") == "1");
    CHECK(saved.at("funny_level_en") == "2");
    CHECK(saved.at("dialog_emojis") == "true");

    const std::vector<Preset> presets = start_presets(all, saved);
    for (const Preset &preset : presets) {
        ScheduledSettings::Values typed;
        INFO(preset.id);
        REQUIRE(ScheduleRulePresets::decode_values(registry, preset.values(), typed));
        std::string error;
        CHECK(registry.validate(typed, error));
        // The old seed was an invented key the real registry does not have.
        CHECK(typed.count("theme") == 0);
    }
    const Preset &defaults = presets[0];
    REQUIRE(defaults.left_out.size() == 1);
    CHECK(defaults.left_out[0].key == "language");
    ScheduledSettings::Values typed;
    REQUIRE(ScheduleRulePresets::decode_values(registry, defaults.values(), typed));
    CHECK(std::get<std::string>(typed.at("dark_color_mode")) == "0");
    CHECK(std::get<int64_t>(typed.at("funny_level_yue")) == 5);
    CHECK(std::get<bool>(typed.at("narrator_enabled")) == false);
    CHECK(find_assignment(defaults, "narrator_enabled")->shown == "Off");
    CHECK(find_assignment(defaults, "dark_color_mode")->shown == "Light");

    const Preset &mine = presets[1];
    REQUIRE(ScheduleRulePresets::decode_values(registry, mine.values(), typed));
    CHECK(std::get<int64_t>(typed.at("funny_level_en")) == 2);
    CHECK(std::get<bool>(typed.at("dialog_emojis")) == true);

    // Decoding refuses what the registry would refuse, and leaves the output alone.
    ScheduledSettings::Values untouched = {{"dark_color_mode", std::string("1")}};
    CHECK_FALSE(ScheduleRulePresets::decode_values(registry, {{"funny_level_en", "9"}}, untouched));
    CHECK_FALSE(ScheduleRulePresets::decode_values(registry, {{"theme", "dark"}}, untouched));
    CHECK(untouched.size() == 1);
    ScheduledSettings::Value value;
    const auto &emojis = registry.entries().at("dialog_emojis");
    CHECK(ScheduleRulePresets::decode(emojis, "1", value));
    CHECK(std::get<bool>(value));
    CHECK_FALSE(ScheduleRulePresets::decode(emojis, "yes", value));
    CHECK(ScheduleRulePresets::encode(ScheduledSettings::Value(2.5)) == "2.5");
    CHECK(std::string(ScheduleRulePresets::history_action()) == "Add schedule rule from preset");
}

TEST_CASE("Applying a preset is labelled in local history by what it applied", "[blank-editors][history]")
{
    CHECK(history_label("Apply appearance preset", "Large text") == "Apply appearance preset: Large text");
    CHECK(history_label("Add schedule rule from preset", "Shipped defaults") == "Add schedule rule from preset: Shipped defaults");
    // History messages stay one line however the title was written.
    CHECK(history_label("Apply appearance preset", "Two\nlines") == "Apply appearance preset: Two lines");
}
