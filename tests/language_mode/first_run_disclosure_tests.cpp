#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// Pure-model tests for the funny-level step of the first-run setup guide.
// The header under test is wxWidgets-free, so this target needs only Catch2
// and the bundled nlohmann json header from src/.

#include <catch_main.hpp>

#include "slic3r/GUI/FirstRunFunnyDisclosure.hpp"

#include <string>
#include <utility>

using namespace Slic3r::GUI::FirstRunFunnyDisclosure;
using nlohmann::json;

namespace {

State state_at(int english_level, int cantonese_level)
{
    State state;
    state.english.level   = english_level;
    state.english.stored  = true;
    state.english.sample  = "english sample";
    state.cantonese.level = cantonese_level;
    state.cantonese.stored = true;
    state.cantonese.sample = "cantonese sample";
    return state;
}

} // namespace

TEST_CASE("The disclosure ships the contract range and level 5 default for both languages", "[FirstRunDisclosure]")
{
    REQUIRE(LEVEL_MIN == 1);
    REQUIRE(LEVEL_MAX == 5);
    REQUIRE(LEVEL_DEFAULT == 5);

    // A fresh profile: nothing stored, both languages at the shipped default.
    const json fresh = payload(State{});
    REQUIRE(fresh.at("command") == RESPONSE_COMMAND);
    REQUIRE(fresh.at("available") == true);
    REQUIRE(fresh.at("min") == 1);
    REQUIRE(fresh.at("max") == 5);
    REQUIRE(fresh.at("default") == 5);
    for (const char *id : {ENGLISH_ID, CANTONESE_ID}) {
        INFO(id);
        REQUIRE(fresh.at(id).at("level") == 5);
        REQUIRE(fresh.at(id).at("stored") == false);
    }
}

TEST_CASE("The disclosure reports each language's own level at both extremes", "[FirstRunDisclosure]")
{
    for (const auto &[english, cantonese] : {std::pair{1, 1}, std::pair{5, 5}, std::pair{1, 5}, std::pair{5, 1}}) {
        INFO("english " << english << ", cantonese " << cantonese);
        const json out = payload(state_at(english, cantonese));
        REQUIRE(out.at("available") == true);
        REQUIRE(out.at(ENGLISH_ID).at("level") == english);
        REQUIRE(out.at(CANTONESE_ID).at("level") == cantonese);
        REQUIRE(out.at(ENGLISH_ID).at("stored") == true);
        REQUIRE(out.at(ENGLISH_ID).at("sample") == "english sample");
        REQUIRE(out.at(CANTONESE_ID).at("sample") == "cantonese sample");
        // The default is a fact and never follows the current level.
        REQUIRE(out.at("default") == LEVEL_DEFAULT);
    }

    // Values outside the range are reported clamped, like a stored value.
    const json clamped = payload(state_at(0, 9));
    REQUIRE(clamped.at(ENGLISH_ID).at("level") == LEVEL_MIN);
    REQUIRE(clamped.at(CANTONESE_ID).at("level") == LEVEL_MAX);
}

TEST_CASE("School mode leaves nothing about funny levels in the payload", "[FirstRunDisclosure]")
{
    State state     = state_at(3, 4);
    state.available = false;
    const json out  = payload(state);
    REQUIRE(out.at("command") == RESPONSE_COMMAND);
    REQUIRE(out.at("available") == false);
    REQUIRE(out.size() == 2);
    REQUIRE_FALSE(out.contains(ENGLISH_ID));
    REQUIRE_FALSE(out.contains(CANTONESE_ID));
    REQUIRE_FALSE(out.contains("default"));
}

TEST_CASE("Page requests save, reset and acknowledge exactly what they name", "[FirstRunDisclosure]")
{
    const auto describe = parse_request(json{{"command", REQUEST_COMMAND}, {"sequence_id", 1}});
    REQUIRE(describe);
    REQUIRE(describe->action == Action::Describe);

    const auto save_english = parse_request(json{{"command", SAVE_COMMAND}, {"language", "en"}, {"level", 3}});
    REQUIRE(save_english);
    REQUIRE(save_english->action == Action::Save);
    REQUIRE(save_english->language == Language::English);
    REQUIRE(save_english->level == 3);

    const auto save_cantonese = parse_request(json{{"command", SAVE_COMMAND}, {"language", "yue"}, {"level", 1}});
    REQUIRE(save_cantonese);
    REQUIRE(save_cantonese->language == Language::Cantonese);
    REQUIRE(save_cantonese->level == 1);

    // Each language resets independently, to the shipped default.
    const auto reset_cantonese = parse_request(json{{"command", RESET_COMMAND}, {"language", "yue"}});
    REQUIRE(reset_cantonese);
    REQUIRE(reset_cantonese->action == Action::Reset);
    REQUIRE(reset_cantonese->language == Language::Cantonese);
    REQUIRE(reset_cantonese->level == LEVEL_DEFAULT);
    const auto reset_english = parse_request(json{{"command", RESET_COMMAND}, {"language", "en"}, {"level", 2}});
    REQUIRE(reset_english);
    REQUIRE(reset_english->language == Language::English);
    REQUIRE(reset_english->level == LEVEL_DEFAULT);

    const auto acknowledge = parse_request(json{{"command", ACKNOWLEDGE_COMMAND}});
    REQUIRE(acknowledge);
    REQUIRE(acknowledge->action == Action::Acknowledge);

    REQUIRE(std::string(language_id(Language::English)) == "en");
    REQUIRE(std::string(language_id(Language::Cantonese)) == "yue");
}

TEST_CASE("Out-of-range levels clamp and malformed requests write nothing", "[FirstRunDisclosure]")
{
    REQUIRE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "en"}, {"level", 0}})->level == LEVEL_MIN);
    REQUIRE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "en"}, {"level", -40}})->level == LEVEL_MIN);
    REQUIRE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "yue"}, {"level", 6}})->level == LEVEL_MAX);
    REQUIRE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "yue"}, {"level", 18446744073709551615ull}})->level == LEVEL_MAX);

    // Not a whole number: rejected rather than guessed.
    REQUIRE_FALSE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "en"}, {"level", "3"}}));
    REQUIRE_FALSE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "en"}, {"level", 2.5}}));
    REQUIRE_FALSE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "en"}, {"level", true}}));
    REQUIRE_FALSE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "en"}}));

    // Unknown or missing language.
    REQUIRE_FALSE(parse_request(json{{"command", SAVE_COMMAND}, {"language", "fr"}, {"level", 3}}));
    REQUIRE_FALSE(parse_request(json{{"command", SAVE_COMMAND}, {"language", 1}, {"level", 3}}));
    REQUIRE_FALSE(parse_request(json{{"command", SAVE_COMMAND}, {"level", 3}}));
    REQUIRE_FALSE(parse_request(json{{"command", RESET_COMMAND}}));

    // Not a disclosure message at all.
    REQUIRE_FALSE(parse_request(json{{"command", "save_region"}, {"region", "Europe"}}));
    REQUIRE_FALSE(parse_request(json{{"command", 7}}));
    REQUIRE_FALSE(parse_request(json::array()));
    REQUIRE_FALSE(parse_request(json("save_funny_level")));
}
