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
        REQUIRE_FALSE(PV::parse(bad, mapping));
        REQUIRE(mapping == original);
    }
}

TEST_CASE("Vocabulary bounds reject excess bytes entries and string sizes", "[PersonalVocabulary]")
{
    PV::Entries mapping;
    REQUIRE_FALSE(PV::parse(std::string(PV::max_bytes + 1, ' '), mapping));
    nlohmann::json document = {{"schemaVersion", 1}, {"entries", nlohmann::json::object()}};
    document["entries"]["Open"] = std::string(PV::max_text_bytes + 1, 'a');
    REQUIRE_FALSE(PV::parse(document.dump(), mapping));
    document["entries"] = nlohmann::json::object();
    for (size_t i = 0; i <= PV::max_entries; ++i) document["entries"][std::to_string(i)] = "value";
    REQUIRE_FALSE(PV::parse(document.dump(), mapping));
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
