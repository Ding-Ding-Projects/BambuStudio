#include "ScheduleRulePresets.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

// L() only marks a literal for extraction (bbl/i18n/list.txt); the picker
// translates the stored English text where it shows it.
#ifndef L
#define L(s) s
#endif

namespace Slic3r { namespace GUI { namespace ScheduleRulePresets {

namespace {

using BlankEditorPresets::Choice;
using BlankEditorPresets::EditorSpec;
using BlankEditorPresets::Field;
using BlankEditorPresets::Values;

void set_creates(EditorSpec &spec)
{
    // Both editors give a new rule the model's own window: all day, every day.
    spec.creates_defaults = L("Creates a new schedule rule that applies all day, every day and sets each setting below to the value the app ships with.");
    spec.creates_saved    = L("Creates a new schedule rule that applies all day, every day and sets each setting below to your own saved value.");
    spec.creates_empty    = L("Creates a new schedule rule that applies all day, every day and sets nothing yet. Choose at least one setting before you save it.");
}

// Display names for the stored values of the presentation settings, matching
// the words the Preferences controls use.
const std::map<std::string, std::vector<Choice>> &known_choices()
{
    static const std::map<std::string, std::vector<Choice>> choices = {
        {"language", {{"en", L("English")}, {"yue_HK", L("Cantonese (Hong Kong)")}, {"bilingual_en_yue_HK", L("Bilingual: English and Cantonese")}}},
        {"dark_color_mode", {{"0", L("Light")}, {"1", L("Dark")}}},
        {"ui_density", {{"comfortable", L("Comfortable")}, {"compact", L("Compact")}}},
        {"ui_font_scale", {{"0.9", L("Small")}, {"1.0", L("Default")}, {"1.15", L("Large")}}},
        {"motion_preference", {{"system", L("System")}, {"reduced", L("Reduce motion")}}},
        {"narrator_language", {{"en", L("English")}, {"yue_HK", L("Cantonese")}, {"both", L("Both languages")}}},
    };
    return choices;
}

const char *empty_label(const std::string &key)
{
    if (key == "ui_accent_seed")
        return L("Default");
    if (key == "ui_font_family")
        return L("Default (Roboto)");
    if (key == "app_display_name")
        return L("Shipped name");
    if (key == "narrator_voice_en" || key == "narrator_voice_yue")
        return L("Automatic voice");
    return "";
}

} // namespace

EditorSpec schedule_rule_spec(const std::vector<Choice> &language_modes)
{
    // The rule dialog's own labels (Schedule/ScheduledSettingsPanel.cpp).
    static const std::map<std::string, const char *> labels = {
        {Schedule::kKeyLanguageMode, L("Language mode")},
        {Schedule::kKeyTheme, L("Theme")},
        {Schedule::kKeyDensity, L("Density")},
        {Schedule::kKeyAccentSeed, L("Accent color")},
        {Schedule::kKeyFontFamily, L("Font")},
        {Schedule::kKeyFontScale, L("Text size")},
        {Schedule::kKeyFunnyLevelEnglish, L("Funny level (English)")},
        {Schedule::kKeyFunnyLevelCantonese, L("Funny level (Cantonese)")},
        {Schedule::kKeyDisplayName, L("App name")},
    };
    EditorSpec spec;
    spec.id = "schedule-rule";
    for (const std::string &key : Schedule::allowed_keys()) {
        Field field;
        field.key = key;
        const auto label = labels.find(key);
        field.label = label == labels.end() ? key : label->second;
        if (key == Schedule::kKeyLanguageMode) {
            field.choices = language_modes;
        } else {
            const auto choices = known_choices().find(key);
            if (choices != known_choices().end())
                field.choices = choices->second;
        }
        field.empty_label = empty_label(key);
        spec.fields.push_back(std::move(field));
    }
    set_creates(spec);
    spec.valid = [](const std::string &key, const std::string &value) { return Schedule::validate_value(key, value).empty(); };
    return spec;
}

std::vector<Choice> language_mode_choices(const char *english, const char *cantonese, const char *bilingual)
{
    return {{english, L("English")}, {cantonese, L("Cantonese (Hong Kong)")}, {bilingual, L("Bilingual: English and Cantonese")}};
}

Values saved_schedule_values(const std::map<std::string, std::string> &live, const Schedule::OverrideState &state)
{
    Values saved;
    for (const std::string &key : Schedule::allowed_keys()) {
        // A key a rule controls shows the rule's value live; the person's own
        // value is the base captured when the rule took over.
        const auto base = state.base.find(key);
        if (base != state.base.end()) {
            saved[key] = base->second;
            continue;
        }
        const auto value = live.find(key);
        if (value != live.end())
            saved[key] = value->second;
    }
    return saved;
}

Schedule::Rule schedule_rule_from(const BlankEditorPresets::Preset &preset, const std::string &id, const std::string &label)
{
    Schedule::Rule rule; // all day, every day, no date bounds, local values
    rule.id     = id;
    rule.label  = label;
    rule.values = preset.values();
    return rule;
}

EditorSpec service_rule_spec(const ScheduledSettings::Registry &registry, const std::function<bool(const std::string &)> &visible)
{
    EditorSpec spec;
    spec.id = "scheduled-settings-rule";
    for (const auto &entry : registry.entries()) {
        if (visible && !visible(entry.first))
            continue;
        const ScheduledSettings::Descriptor &descriptor = entry.second;
        Field field;
        field.key   = entry.first;
        field.label = descriptor.label_en.empty() ? entry.first : descriptor.label_en;
        if (descriptor.kind == ScheduledSettings::Kind::Boolean) {
            field.choices = {{"true", L("On")}, {"false", L("Off")}};
        } else {
            const auto choices = known_choices().find(entry.first);
            if (choices != known_choices().end())
                field.choices = choices->second;
        }
        field.empty_label = empty_label(entry.first);
        spec.fields.push_back(std::move(field));
    }
    set_creates(spec);
    // A copy: the validator must not outlive a registry passed by reference.
    spec.valid = [registry](const std::string &key, const std::string &text) {
        const auto found = registry.entries().find(key);
        ScheduledSettings::Value value;
        std::string              error;
        return found != registry.entries().end() && decode(found->second, text, value) && registry.validate({{key, value}}, error);
    };
    return spec;
}

std::string encode(const ScheduledSettings::Value &value)
{
    if (const auto *flag = std::get_if<bool>(&value))
        return *flag ? "true" : "false";
    if (const auto *number = std::get_if<int64_t>(&value))
        return std::to_string(*number);
    if (const auto *real = std::get_if<double>(&value)) {
        std::ostringstream out;
        out.precision(15);
        out << *real;
        return out.str();
    }
    return std::get<std::string>(value);
}

bool decode(const ScheduledSettings::Descriptor &descriptor, const std::string &text, ScheduledSettings::Value &value)
{
    switch (descriptor.kind) {
    case ScheduledSettings::Kind::Boolean:
        if (text == "true" || text == "1") { value = true; return true; }
        if (text == "false" || text == "0") { value = false; return true; }
        return false;
    case ScheduledSettings::Kind::Integer: {
        int64_t number = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
        if (text.empty() || result.ec != std::errc() || result.ptr != text.data() + text.size())
            return false;
        value = number;
        return true;
    }
    case ScheduledSettings::Kind::Number: {
        if (text.empty())
            return false;
        char        *end  = nullptr;
        const double real = std::strtod(text.c_str(), &end);
        if (end != text.c_str() + text.size() || !std::isfinite(real))
            return false;
        value = real;
        return true;
    }
    default:
        value = text;
        return true;
    }
}

Values encode_values(const ScheduledSettings::Values &values)
{
    Values out;
    for (const auto &entry : values)
        out[entry.first] = encode(entry.second);
    return out;
}

bool decode_values(const ScheduledSettings::Registry &registry, const Values &values, ScheduledSettings::Values &out)
{
    ScheduledSettings::Values typed;
    for (const auto &entry : values) {
        const auto found = registry.entries().find(entry.first);
        ScheduledSettings::Value value;
        if (found == registry.entries().end() || !decode(found->second, entry.second, value))
            return false;
        typed[entry.first] = std::move(value);
    }
    std::string error;
    if (!registry.validate(typed, error))
        return false;
    out = std::move(typed);
    return true;
}

const char *picker_title() { return L("Start a new schedule rule"); }

const char *picker_subtitle() { return L("Choose what the new rule starts with. You can change every value before you save it."); }

const char *history_action() { return "Add schedule rule from preset"; }

} } } // namespace Slic3r::GUI::ScheduleRulePresets
