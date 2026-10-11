#include <catch_main.hpp>
#include "slic3r/GUI/PersonalVocabulary.hpp"
#include <nlohmann/json.hpp>

namespace PV = Slic3r::GUI::PersonalVocabulary;

TEST_CASE("Versioned vocabulary validates complete documents before replacement", "[PersonalVocabulary]")
{
    PV::Entries mapping;
    REQUIRE(PV::parse(R"({"schemaVersion":1,"entries":{"Open":"Inspect"}})", mapping));
    REQUIRE(mapping.size() == 1);
    const auto original = mapping;
    for (const std::string &bad : {
             std::string{}, std::string{"null"}, std::string{"{"},
             std::string{R"({"schemaVersion":2,"entries":{}})"},
             std::string{R"({"schemaVersion":1.0,"entries":{}})"},
             std::string{R"({"schemaVersion":1,"entries":[],"extra":true})"},
             std::string{R"({"schemaVersion":1,"schemaVersion":1,"entries":{}})"},
             std::string{R"({"schemaVersion":1,"entries":{"Open":"A","Open":"B"}})"},
             std::string{R"({"schemaVersion":1,"entries":{"Open":""}})"},
             std::string{R"({"schemaVersion":1,"entries":{"Open":"   "}})"},
             std::string{R"({"schemaVersion":1,"entries":{"Open":"a\u0000b"}})"},
             std::string{R"({"schemaVersion":1,"entries":{"Open":"a\u202eb"}})"},
             std::string{R"({"schemaVersion":1,"entries":{"__proto__":"x"}})"},
             std::string{R"({"schemaVersion":1,"entries":{"Open":{"nested":{"value":true}}}})"}}) {
        PV::Rejection reason = PV::Rejection::None;
        REQUIRE_FALSE(PV::parse(bad, mapping, &reason));
        REQUIRE(reason != PV::Rejection::None);
        REQUIRE(mapping == original);
    }
}

TEST_CASE("Vocabulary bounds count code points and accept every valid entry", "[PersonalVocabulary]")
{
    PV::Entries mapping;
    PV::Rejection reason = PV::Rejection::None;
    REQUIRE_FALSE(PV::parse(std::string(PV::max_file_bytes + 1, ' '), mapping, &reason));
    REQUIRE(reason == PV::Rejection::TooLarge);

    // Four UTF-8 bytes and two UTF-16 units each, but one code point.
    const std::string astral = "\xF0\x9F\x98\x80";
    std::string key;
    for (std::size_t i = 0; i < PV::max_key_code_points; ++i) key += astral;
    nlohmann::json document = {{"schemaVersion", 1}, {"entries", nlohmann::json::object()}};
    document["entries"][key] = "value";
    REQUIRE(PV::parse(document.dump(), mapping, &reason));
    REQUIRE(reason == PV::Rejection::None);
    REQUIRE(mapping.size() == 1);
    REQUIRE(wxString(mapping[0].first) == wxString::FromUTF8(key));
    document["entries"] = nlohmann::json::object();
    document["entries"][key + astral] = "value";
    REQUIRE_FALSE(PV::parse(document.dump(), mapping, &reason));
    REQUIRE(reason == PV::Rejection::KeyTooLong);

    document["entries"] = nlohmann::json::object();
    document["entries"]["Open"] = std::string(PV::max_value_code_points, 'a');
    REQUIRE(PV::parse(document.dump(), mapping, &reason));
    document["entries"]["Open"] = std::string(PV::max_value_code_points + 1, 'a');
    REQUIRE_FALSE(PV::parse(document.dump(), mapping, &reason));
    REQUIRE(reason == PV::Rejection::ValueTooLong);

    // There is no entry count limit: thousands of entries load and display.
    document["entries"] = nlohmann::json::object();
    for (std::size_t i = 0; i < 5000; ++i) document["entries"]["w" + std::to_string(i)] = "r" + std::to_string(i);
    REQUIRE(PV::parse(document.dump(), mapping, &reason));
    REQUIRE(mapping.size() == 5000);
    REQUIRE(PV::apply("w4999 and w12", mapping) == "r4999 and r12");

    document["entries"] = nlohmann::json::object();
    REQUIRE(PV::parse(document.dump(), mapping));
    REQUIRE(mapping.empty());
}

TEST_CASE("Display matching is longest first bounded and nonrecursive", "[PersonalVocabulary]")
{
    PV::Entries mapping;
    REQUIRE(PV::parse(R"({"schemaVersion":1,"entries":{"Open":"Inspect","Open item":"Review item","Inspect":"Again"}})", mapping));
    REQUIRE(PV::apply("Open item then Open", mapping) == "Review item then Inspect");
    REQUIRE(PV::apply("Opened item", mapping) == "Opened item");
    REQUIRE(PV::apply("Open %s", mapping) == "Open %s");
    REQUIRE(PV::apply("https://example.invalid/Open", mapping) == "https://example.invalid/Open");
    REQUIRE(PV::apply("C:\\Open\\item", mapping) == "C:\\Open\\item");
    REQUIRE(PV::apply("Open/item", mapping) == "Open/item");
    const wxString original = "Open item";
    REQUIRE(PV::remember(original) == original);
    REQUIRE(original == "Open item");
    REQUIRE(PV::display("Unregistered content") == "Unregistered content");
}
