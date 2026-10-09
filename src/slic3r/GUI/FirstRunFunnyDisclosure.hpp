#ifndef slic3r_GUI_FirstRunFunnyDisclosure_hpp_
#define slic3r_GUI_FirstRunFunnyDisclosure_hpp_

// Pure, wxWidgets-free model for the funny-level step of the first-run setup
// guide (resources/web/guide/12). The page owns the wording, which lives in the
// web text catalog (resources/web/data/text.js) beside every other guide page;
// this model owns the state the page renders and the messages the page may send
// back, so the Catch2 target under tests/language_mode can check both without a
// window. GuideFrame (WebGuideDialog.cpp) applies the requests to AppConfig and
// to the language mode service.

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

namespace Slic3r { namespace GUI { namespace FirstRunFunnyDisclosure {

// The same values as I18N::FUNNY_LEVEL_MIN, _MAX and _DEFAULT. WebGuideDialog.cpp
// static_asserts the match so the guide can never disclose a different default
// from the one the application ships.
inline constexpr int LEVEL_MIN     = 1;
inline constexpr int LEVEL_MAX     = 5;
inline constexpr int LEVEL_DEFAULT = 5;

// Script-message commands exchanged with the guide page.
inline constexpr const char *REQUEST_COMMAND     = "request_funny_disclosure";
inline constexpr const char *RESPONSE_COMMAND    = "response_funny_disclosure";
inline constexpr const char *SAVE_COMMAND        = "save_funny_level";
inline constexpr const char *RESET_COMMAND       = "reset_funny_level";
inline constexpr const char *ACKNOWLEDGE_COMMAND = "acknowledge_funny_disclosure";

// Language ids used by the page and in its messages.
inline constexpr const char *ENGLISH_ID   = "en";
inline constexpr const char *CANTONESE_ID = "yue";

// The real message the page shows at the current level of each language, so
// the disclosure demonstrates what the level does instead of only describing it.
inline constexpr const char *SAMPLE_SOURCE = "Slicing complete";

enum class Language { English, Cantonese };

struct LanguageState {
    int         level { LEVEL_DEFAULT };
    // True when a value is saved in the preferences; false means the shipped
    // default is in effect.
    bool        stored { false };
    // SAMPLE_SOURCE as it reads at this level in this language (UTF-8).
    std::string sample;
};

struct State {
    // False while School mode treats funny levels as not installed: the page
    // then shows nothing about them and moves straight on.
    bool          available { true };
    LanguageState english;
    LanguageState cantonese;
};

inline int clamp_level(int level)
{
    return std::max(LEVEL_MIN, std::min(LEVEL_MAX, level));
}

inline const char *language_id(Language language)
{
    return language == Language::English ? ENGLISH_ID : CANTONESE_ID;
}

// "en" or "yue"; anything else, including a non-string, is rejected.
inline std::optional<Language> parse_language(const nlohmann::json &value)
{
    if (!value.is_string())
        return std::nullopt;
    const std::string id = value.get<std::string>();
    if (id == ENGLISH_ID)
        return Language::English;
    if (id == CANTONESE_ID)
        return Language::Cantonese;
    return std::nullopt;
}

// The "response_funny_disclosure" payload the page renders. When the step is
// not available the payload carries no level, default or sample at all, so
// nothing about funny levels reaches the page.
inline nlohmann::json payload(const State &state)
{
    nlohmann::json out = nlohmann::json::object();
    out["command"]   = RESPONSE_COMMAND;
    out["available"] = state.available;
    if (!state.available)
        return out;

    out["min"]     = LEVEL_MIN;
    out["max"]     = LEVEL_MAX;
    out["default"] = LEVEL_DEFAULT;
    const auto describe = [](const LanguageState &language) {
        nlohmann::json item = nlohmann::json::object();
        item["level"]       = clamp_level(language.level);
        item["stored"]      = language.stored;
        item["sample"]      = language.sample;
        return item;
    };
    out[ENGLISH_ID]   = describe(state.english);
    out[CANTONESE_ID] = describe(state.cantonese);
    return out;
}

enum class Action { Describe, Save, Reset, Acknowledge };

struct Request {
    Action   action { Action::Describe };
    Language language { Language::English };
    // The level to store (Save) or the shipped default to return to (Reset).
    int      level { LEVEL_DEFAULT };
};

// Interprets one script message from the page. Unknown commands, missing or
// unknown languages and levels that are not whole numbers yield nullopt, so
// nothing is written. Whole numbers outside 1..5 are clamped, exactly as a
// stored value is when it is read back.
inline std::optional<Request> parse_request(const nlohmann::json &message)
{
    if (!message.is_object())
        return std::nullopt;
    const auto command_it = message.find("command");
    if (command_it == message.end() || !command_it->is_string())
        return std::nullopt;
    const std::string command = command_it->get<std::string>();

    Request request;
    if (command == REQUEST_COMMAND) {
        request.action = Action::Describe;
        return request;
    }
    if (command == ACKNOWLEDGE_COMMAND) {
        request.action = Action::Acknowledge;
        return request;
    }
    if (command != SAVE_COMMAND && command != RESET_COMMAND)
        return std::nullopt;

    const auto language_it = message.find("language");
    if (language_it == message.end())
        return std::nullopt;
    const std::optional<Language> language = parse_language(*language_it);
    if (!language)
        return std::nullopt;
    request.language = *language;

    if (command == RESET_COMMAND) {
        request.action = Action::Reset;
        request.level  = LEVEL_DEFAULT;
        return request;
    }

    const auto level_it = message.find("level");
    if (level_it == message.end() || !level_it->is_number_integer())
        return std::nullopt;
    std::int64_t level = 0;
    if (level_it->is_number_unsigned()) {
        const std::uint64_t unsigned_level = level_it->get<std::uint64_t>();
        level = unsigned_level > static_cast<std::uint64_t>(LEVEL_MAX) ? LEVEL_MAX : static_cast<std::int64_t>(unsigned_level);
    } else {
        level = level_it->get<std::int64_t>();
    }
    request.action = Action::Save;
    request.level  = static_cast<int>(std::max<std::int64_t>(LEVEL_MIN, std::min<std::int64_t>(LEVEL_MAX, level)));
    return request;
}

}}} // namespace Slic3r::GUI::FirstRunFunnyDisclosure

#endif // slic3r_GUI_FirstRunFunnyDisclosure_hpp_
