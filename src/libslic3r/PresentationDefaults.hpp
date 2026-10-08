#ifndef slic3r_PresentationDefaults_hpp_
#define slic3r_PresentationDefaults_hpp_

// Shipped defaults of the presentation preferences.
//
// One table, read by every path that needs "the value the app ships with":
// AppConfig::set_defaults() when a key is missing, Preferences > Appearance >
// "Reset appearance to defaults", and the start-from presets that blank
// editors offer (src/slic3r/GUI/Presets/BlankEditorPresets.hpp). Because they
// all read this table, a preset and a reset can never disagree about what the
// shipped value is.
//
// A key without one fixed shipped value (the interface language follows the
// system language the first time the app starts) is listed in unfixed_keys()
// instead, so an editor can say why it leaves the key out rather than invent
// a value for it.
//
// Header-only and free of wxWidgets so libslic3r, the GUI and the pure-model
// tests share it.

#include <optional>
#include <string>
#include <vector>

namespace Slic3r { namespace PresentationDefaults {

// Mirrors I18N::FUNNY_LEVEL_DEFAULT (LanguageMode.hpp); StartFromPicker.cpp
// static_asserts the two stay equal.
inline constexpr int         kFunnyLevel = 5;
// The Material brand seed (MD3::seed in MD3Tokens.hpp).
inline constexpr const char *kAccentSeed = "#146c2e";

struct Entry
{
    const char *key;
    const char *value;
};

inline const std::vector<Entry> &entries()
{
    static const std::vector<Entry> table = {
        {"dark_color_mode", "0"},             // light theme
        {"ui_density", "comfortable"},
        {"ui_accent_seed", kAccentSeed},
        {"ui_font_family", ""},               // the bundled default face
        {"ui_font_scale", "1.0"},
        {"motion_preference", "system"},
        {"app_display_name", ""},             // the shipped product name
        {"funny_level_en", "5"},              // kFunnyLevel
        {"funny_level_yue", "5"},             // kFunnyLevel
        {"dialog_emojis", "false"},
        {"narrator_enabled", "false"},
        {"narrator_quiet", "false"},
        {"narrator_language", "en"},
        {"narrator_voice_en", ""},            // the system's default voice
        {"narrator_voice_yue", ""},
        {"narrator_rate_en", "0"},
        {"narrator_rate_yue", "0"},
        {"narrator_pitch_en", "0"},
        {"narrator_pitch_yue", "0"},
    };
    return table;
}

// Presentation keys that deliberately have no single shipped value.
inline const std::vector<std::string> &unfixed_keys()
{
    static const std::vector<std::string> keys = {"language"};
    return keys;
}

inline std::optional<std::string> value(const std::string &key)
{
    for (const Entry &entry : entries())
        if (key == entry.key)
            return std::string(entry.value);
    return std::nullopt;
}

// The value for a key this table must cover; an unknown key is a programming
// error that returns "" so a caller never writes a guess.
inline std::string value_or_empty(const std::string &key)
{
    const std::optional<std::string> found = value(key);
    return found ? *found : std::string();
}

inline bool is_unfixed(const std::string &key)
{
    for (const std::string &unfixed : unfixed_keys())
        if (key == unfixed)
            return true;
    return false;
}

} } // namespace Slic3r::PresentationDefaults

#endif // slic3r_PresentationDefaults_hpp_
