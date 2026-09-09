#include <catch_main.hpp>

#include "slic3r/GUI/AppDisplayName.hpp"

#include <string>

using namespace Slic3r::GUI::AppDisplayName;

namespace {
const std::string kShipped = "Bambu Studio";

std::string repeat(const std::string &unit, size_t n)
{
    std::string out;
    for (size_t i = 0; i < n; ++i) out += unit;
    return out;
}
} // namespace

TEST_CASE("Config key and bounds are the documented contract", "[AppDisplayName]")
{
    REQUIRE(std::string(CONFIG_KEY) == "app_display_name");
    REQUIRE(MIN_LENGTH == 1);
    REQUIRE(MAX_LENGTH == 40);
}

TEST_CASE("Validation accepts 1 to 40 code points without control characters", "[AppDisplayName][validate]")
{
    REQUIRE(validate("A").ok());
    REQUIRE(validate(kShipped).ok());
    REQUIRE(validate(repeat("x", 40)).ok());
    // Code points, not bytes: 40 three-byte CJK characters are 120 bytes and still valid.
    const std::string cjk40 = repeat("\xE4\xB8\xAD", 40);
    REQUIRE(cjk40.size() == 120);
    REQUIRE(validate(cjk40).ok());
    REQUIRE(validate(cjk40).length == 40);
    // Emoji (4-byte sequences) count as one each.
    REQUIRE(validate("\xF0\x9F\x8D\xA1 Studio").length == 8);
    REQUIRE(validate("\xF0\x9F\x8D\xA1 Studio").ok());
}

TEST_CASE("Validation rejects empty, whitespace-only, over-long and control input", "[AppDisplayName][validate]")
{
    REQUIRE(validate("").problem == Problem::Empty);
    REQUIRE(validate("   ").problem == Problem::Empty);
    REQUIRE(validate(" \xC2\xA0").problem == Problem::Empty); // space, NBSP
    REQUIRE(validate("\t").problem == Problem::ControlCharacters); // tab is a C0 control as typed
    REQUIRE(validate(repeat("x", 41)).problem == Problem::TooLong);
    REQUIRE(validate(repeat("\xE4\xB8\xAD", 41)).problem == Problem::TooLong);
    REQUIRE(validate("Bambu\nStudio").problem == Problem::ControlCharacters);
    REQUIRE(validate("Bambu\rStudio").problem == Problem::ControlCharacters);
    REQUIRE(validate("Bambu\x7FStudio").problem == Problem::ControlCharacters);
    REQUIRE(validate("Bambu\x1BStudio").problem == Problem::ControlCharacters);
    // C1 control U+0085 (NEL) encoded as C2 85.
    REQUIRE(validate("Bambu\xC2\x85Studio").problem == Problem::ControlCharacters);
    // Control characters are reported before length so the fix-it text is right.
    REQUIRE(validate(repeat("x", 41) + "\n").problem == Problem::ControlCharacters);
}

TEST_CASE("Sanitization strips controls, trims and collapses whitespace, truncates to 40", "[AppDisplayName][sanitize]")
{
    REQUIRE(sanitize("  My   Slicer  ") == "My Slicer");
    REQUIRE(sanitize("My\tSlicer") == "My Slicer");
    REQUIRE(sanitize("My\nSlicer") == "MySlicer");        // line break removed, no separator invented
    REQUIRE(sanitize("\x01\x02") == "");
    REQUIRE(sanitize("\xE4\xB8\xAD\xE3\x80\x80\xE6\x96\x87") == "\xE4\xB8\xAD \xE6\x96\x87"); // ideographic space -> one space
    const std::string cut = sanitize(repeat("x", 45));
    REQUIRE(utf8_length(cut) == 40);
    REQUIRE(validate(cut).ok());
    // Truncation never leaves a dangling separator or splits a multi-byte sequence.
    const std::string words = sanitize(repeat("ab ", 20));
    REQUIRE(words.back() != ' ');
    REQUIRE(validate(words).ok());
    const std::string cjk = sanitize(repeat("\xE4\xB8\xAD", 45));
    REQUIRE(cjk.size() == 120);
    REQUIRE(validate(cjk).ok());
    // Malformed UTF-8 bytes do not crash and are counted as single code points.
    REQUIRE(utf8_length("\xFF\xFE") == 2);
    REQUIRE(validate(sanitize("ok\xFF")).ok());
}

TEST_CASE("Sanitized output is always valid unless it is empty", "[AppDisplayName][sanitize]")
{
    const std::string inputs[] = {"", " ", "\n\n", "x", repeat(" y", 100), repeat("\xF0\x9F\x8D\xA1", 50), "a\x7F" "b"};
    for (const std::string &in : inputs) {
        const std::string out = sanitize(in);
        const Validation  v   = validate(out);
        REQUIRE((v.ok() || (out.empty() && v.problem == Problem::Empty)));
    }
}

TEST_CASE("Resolve falls back to the shipped name for missing or invalid stored values", "[AppDisplayName][resolve]")
{
    REQUIRE(resolve("", kShipped) == kShipped);
    REQUIRE(resolve("   ", kShipped) == kShipped);
    REQUIRE(resolve("Bad\nName", kShipped) == kShipped);
    REQUIRE(resolve(repeat("x", 41), kShipped) == kShipped);
    REQUIRE(resolve("My Slicer", kShipped) == "My Slicer");
    // A stored value equal to the shipped name still resolves to it.
    REQUIRE(resolve(kShipped, kShipped) == kShipped);
}

TEST_CASE("Provenance distinguishes a stored name from the compiled-in default", "[AppDisplayName][provenance]")
{
    REQUIRE(provenance("", kShipped) == Provenance::Default);
    REQUIRE(provenance("Bad\nName", kShipped) == Provenance::Default);
    REQUIRE(provenance(kShipped, kShipped) == Provenance::Default); // same text: nothing user-visible differs
    REQUIRE(provenance("My Slicer", kShipped) == Provenance::Stored);
}

TEST_CASE("Stored value is empty when the user types the shipped name back", "[AppDisplayName][store]")
{
    REQUIRE(to_stored_value(kShipped, kShipped) == "");
    REQUIRE(to_stored_value("  Bambu   Studio ", kShipped) == "");
    REQUIRE(to_stored_value("", kShipped) == "");
    REQUIRE(to_stored_value(" My Slicer ", kShipped) == "My Slicer");
    REQUIRE(to_stored_value("bambu studio", kShipped) == "bambu studio"); // case differs: a real rename
}
