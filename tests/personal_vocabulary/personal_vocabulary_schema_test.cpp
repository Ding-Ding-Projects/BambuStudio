// Pure tests for the version 1 personal vocabulary file contract. Uses only
// synthetic neutral text; never reads or prints a real vocabulary file.
//
//   g++ -std=c++17 -Wall -Wextra -Werror -Isrc
//       tests/personal_vocabulary/personal_vocabulary_schema_test.cpp
//       src/slic3r/GUI/PersonalVocabularySchema.cpp -o personal_vocabulary_schema_test
#include "../../src/slic3r/GUI/PersonalVocabularySchema.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace PV = Slic3r::GUI::PersonalVocabulary;
using PV::Rejection;

static int checks = 0;
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #expr); std::exit(1); } ++checks; } while (0)

static const PV::TextEntries sentinel{{"sentinel", "kept"}};

// Validates bytes; a rejection must leave the previous result untouched.
static Rejection run(const std::string &bytes, PV::TextEntries *accepted = nullptr)
{
    PV::TextEntries result = sentinel;
    const Rejection reason = PV::validate(bytes, result);
    if (reason != Rejection::None) CHECK(result == sentinel);
    if (accepted) *accepted = result;
    return reason;
}

static void expect(const std::string &bytes, Rejection wanted, int line)
{
    const Rejection got = run(bytes);
    if (got != wanted) {
        std::fprintf(stderr, "%s:%d: expected %s, got %s\n", __FILE__, line, PV::rejection_name(wanted), PV::rejection_name(got));
        std::exit(1);
    }
    ++checks;
}
#define EXPECT(bytes, reason) expect((bytes), (reason), __LINE__)

static std::string repeat(const std::string &unit, std::size_t count)
{
    std::string text;
    for (std::size_t i = 0; i < count; ++i) text += unit;
    return text;
}
static std::string document(const std::string &entries) { return R"({"schemaVersion":1,"entries":{)" + entries + "}}"; }
static std::string entry(const std::string &key, const std::string &value) { return '"' + key + R"(":")" + value + '"'; }
// JSON escape text for one UTF-16 code unit, built at run time so the source stays plain ASCII.
static std::string esc(const char *hex) { return std::string(1, '\\') + "u" + hex; }

static const std::string two_byte = "\xC3\xA9";          // U+00E9
static const std::string three_byte = "\xE4\xB8\xAD";    // U+4E2D
static const std::string astral = "\xF0\x9F\x98\x80";    // U+1F600, four UTF-8 bytes
static const std::string escaped_astral = esc("d83d") + esc("de00"); // the same code point as a JSON surrogate pair

static void limits_are_the_contract()
{
    CHECK(PV::schema_version == 1);
    CHECK(PV::max_file_bytes == 256u * 1024u);
    CHECK(PV::max_depth == 8u);
    CHECK(PV::max_key_code_points == 160u);
    CHECK(PV::max_value_code_points == 1000u);
}

static void code_points_are_counted_not_bytes()
{
    CHECK(PV::code_points("") == 0);
    CHECK(PV::code_points("Open") == 4);
    CHECK(PV::code_points(two_byte) == 1);
    CHECK(PV::code_points(three_byte) == 1);
    CHECK(PV::code_points(astral) == 1);
    CHECK(PV::code_points("a" + astral + three_byte + two_byte) == 4);
}

static void accepts_valid_documents()
{
    PV::TextEntries accepted;
    CHECK(run(document(entry("Open", "Inspect")), &accepted) == Rejection::None);
    CHECK(accepted == PV::TextEntries({{"Open", "Inspect"}}));
    CHECK(run(document(""), &accepted) == Rejection::None);
    CHECK(accepted.empty());
    // Whitespace, field order and inner spaces are free.
    CHECK(run("\n { \"entries\" : { \"Open item\" : \"Inspect item\" } , \"schemaVersion\" : 1 } \n", &accepted) == Rejection::None);
    CHECK(accepted == PV::TextEntries({{"Open item", "Inspect item"}}));
    // Escapes decode before every rule.
    CHECK(run(document('"' + esc("004f") + R"(pen":"Insp)" + esc("0065") + R"(ct")"), &accepted) == Rejection::None);
    CHECK(accepted == PV::TextEntries({{"Open", "Inspect"}}));
    // A UTF-8 byte order mark is accepted only as the first bytes of the file.
    EXPECT("\xEF\xBB\xBF" + document(entry("Open", "Inspect")), Rejection::None);
    EXPECT(document(entry("Open", "Inspect")) + "\xEF\xBB\xBF", Rejection::Malformed);
    // Only the exact reserved names are unsafe.
    EXPECT(document(entry("__proto__x", "prototype value")), Rejection::None);
}

static void file_size_boundary()
{
    // A large valid file filled to exactly the limit, with the padding inside
    // the entries object so the last byte is the closing brace.
    std::string entries;
    std::size_t count = 0;
    while (entries.size() < PV::max_file_bytes - 2048) {
        if (count) entries += ',';
        entries += entry("k" + std::to_string(count), "v");
        ++count;
    }
    std::string exact = document(entries);
    CHECK(exact.size() < PV::max_file_bytes);
    exact.insert(exact.size() - 2, PV::max_file_bytes - exact.size(), ' ');
    CHECK(exact.size() == PV::max_file_bytes);
    CHECK(exact.back() == '}');
    PV::TextEntries accepted;
    CHECK(run(exact, &accepted) == Rejection::None);
    CHECK(accepted.size() == count);

    std::string over = exact;
    over.insert(over.size() - 2, 1, ' ');
    CHECK(over.size() == PV::max_file_bytes + 1);
    EXPECT(over, Rejection::TooLarge);
    EXPECT(std::string(PV::max_file_bytes + 1, ' '), Rejection::TooLarge);
    // Multi-byte text counts in UTF-8 bytes toward the file limit.
    std::string wide_entries;
    for (std::size_t i = 0; wide_entries.size() < PV::max_file_bytes - 4096; ++i)
        wide_entries += (i ? "," : "") + entry("k" + std::to_string(i), repeat(three_byte, 900));
    std::string wide = document(wide_entries);
    wide.insert(wide.size() - 2, PV::max_file_bytes - wide.size(), ' ');
    CHECK(wide.size() == PV::max_file_bytes);
    CHECK(run(wide) == Rejection::None);
    wide.insert(wide.size() - 2, three_byte);
    EXPECT(wide, Rejection::TooLarge);
}

static std::string nested_under_entry(std::size_t depth)
{
    // The root object is level 1 and the entries object level 2.
    return R"({"schemaVersion":1,"entries":{"Open":)" + repeat("[", depth - 2) + repeat("]", depth - 2) + "}}";
}
static std::string nested_extra_field(std::size_t depth)
{
    return R"({"schemaVersion":1,"entries":{},"extra":)" + repeat(R"({"a":)", depth - 2) + "{}" + repeat("}", depth - 2) + "}";
}

static void depth_boundary()
{
    // The version 1 schema itself uses two levels (see accepts_valid_documents).
    // At exactly eight levels the structural check passes and the schema rule decides.
    EXPECT(nested_under_entry(PV::max_depth), Rejection::NonStringValue);
    EXPECT(nested_under_entry(PV::max_depth + 1), Rejection::TooDeep);
    EXPECT(nested_extra_field(PV::max_depth), Rejection::UnexpectedField);
    EXPECT(nested_extra_field(PV::max_depth + 1), Rejection::TooDeep);
    EXPECT(repeat("[", PV::max_depth) + repeat("]", PV::max_depth), Rejection::UnexpectedField);
    EXPECT(repeat("[", PV::max_depth + 1) + repeat("]", PV::max_depth + 1), Rejection::TooDeep);
    // Far deeper input stops at the limit instead of recursing.
    EXPECT(repeat("[", 100000) + repeat("]", 100000), Rejection::TooDeep);
    EXPECT(repeat("[", 200000), Rejection::TooDeep);
}

static void key_boundary(const std::string &unit, const char *label)
{
    PV::TextEntries accepted;
    const std::string at_limit = repeat(unit, PV::max_key_code_points);
    if (run(document('"' + at_limit + R"(":"v")"), &accepted) != Rejection::None) {
        std::fprintf(stderr, "key at the limit rejected: %s\n", label);
        std::exit(1);
    }
    CHECK(accepted.size() == 1 && PV::code_points(accepted[0].first) == PV::max_key_code_points);
    EXPECT(document('"' + repeat(unit, PV::max_key_code_points + 1) + R"(":"v")"), Rejection::KeyTooLong);
}

static void value_boundary(const std::string &unit, const char *label)
{
    PV::TextEntries accepted;
    if (run(document(R"("k":")" + repeat(unit, PV::max_value_code_points) + '"'), &accepted) != Rejection::None) {
        std::fprintf(stderr, "value at the limit rejected: %s\n", label);
        std::exit(1);
    }
    CHECK(accepted.size() == 1 && PV::code_points(accepted[0].second) == PV::max_value_code_points);
    EXPECT(document(R"("k":")" + repeat(unit, PV::max_value_code_points + 1) + '"'), Rejection::ValueTooLong);
}

static void text_length_boundaries()
{
    for (auto [unit, label] : {std::pair<std::string, const char *>{"k", "ascii"}, {two_byte, "two-byte"},
                               {three_byte, "three-byte"}, {astral, "astral"}, {escaped_astral, "escaped astral"}}) {
        key_boundary(unit, label);
        value_boundary(unit, label);
    }
    // 160 astral code points are 640 UTF-8 bytes and still fit; the limit is code points.
    CHECK(repeat(astral, PV::max_key_code_points).size() == 640);
    // Mixed widths count each code point once.
    EXPECT(document(entry(repeat("k", PV::max_key_code_points - 1) + astral, "v")), Rejection::None);
    EXPECT(document(entry(repeat("k", PV::max_key_code_points) + astral, "v")), Rejection::KeyTooLong);
    EXPECT(document(entry("k", repeat(three_byte, PV::max_value_code_points - 1) + astral)), Rejection::None);
    EXPECT(document(entry("k", repeat(three_byte, PV::max_value_code_points) + astral)), Rejection::ValueTooLong);
}

static void many_entries_are_accepted()
{
    // Far more than any former fixed count: the file size is the only bound.
    for (std::size_t count : {2049u, 5000u, 20000u}) {
        std::string entries;
        for (std::size_t i = 0; i < count; ++i) {
            if (i) entries += ',';
            entries += entry("k" + std::to_string(i), "v");
        }
        PV::TextEntries accepted;
        CHECK(run(document(entries), &accepted) == Rejection::None);
        CHECK(accepted.size() == count);
    }
}

static void rejects_whole_file_and_never_partially()
{
    // Thousands of valid entries followed by one bad entry apply nothing.
    std::string entries;
    for (std::size_t i = 0; i < 3000; ++i) entries += entry("k" + std::to_string(i), "v") + ',';
    EXPECT(document(entries + R"("zz":7)"), Rejection::NonStringValue);
    EXPECT(document(entries + entry("zz", repeat("v", PV::max_value_code_points + 1))), Rejection::ValueTooLong);
    EXPECT(document(entries + entry("k1", "again")), Rejection::DuplicateKey);
}

static void existing_rejections_remain()
{
    // Malformed JSON, invalid UTF-8, comments and trailing content.
    for (const std::string &bad : {std::string{}, std::string{" "}, std::string{"{"}, std::string{"nul"},
             document(entry("Open", "Inspect")) + "x", document(entry("Open", "Inspect")) + "{}",
             std::string{R"({"schemaVersion":1,"entries":{"Open":"Inspect",}})"},
             std::string{R"({"schemaVersion":1,"entries":{'Open':'Inspect'}})"},
             std::string{"{\"schemaVersion\":1,/* note */\"entries\":{}}"},
             document(entry("Open", "Inspect")) + "// note",
             document(entry("Open\xFF", "Inspect")), document(entry("Open", "\xC0\xAF")),
             document(entry("Open", "\xED\xA0\x80")), document(entry("Open", "\xF0\x9F\x98")),
             document(entry("Open", esc("d800"))), document(entry("Open", esc("de00") + "x")),
             std::string{R"({"schemaVersion":NaN,"entries":{}})"}})
        EXPECT(bad, Rejection::Malformed);
    // Duplicate keys anywhere, compared after escapes decode.
    EXPECT(R"({"schemaVersion":1,"schemaVersion":1,"entries":{}})", Rejection::DuplicateKey);
    EXPECT(document(entry("Open", "A") + "," + entry("Open", "B")), Rejection::DuplicateKey);
    EXPECT(document(entry("Open", "A") + "," + entry(esc("004f") + "pen", "B")), Rejection::DuplicateKey);
    EXPECT(R"({"schemaVersion":1,"entries":{},"extra":{"a":1,"a":2}})", Rejection::DuplicateKey);
    // Unknown, missing or non-integer versions.
    for (const char *version : {"2", "0", "-1", "1.0", "1e0", "\"1\"", "true", "null", "[1]"})
        EXPECT(std::string(R"({"schemaVersion":)") + version + R"(,"entries":{}})", Rejection::UnsupportedVersion);
    EXPECT(R"({"entries":{}})", Rejection::UnsupportedVersion);
    EXPECT(R"({"schemaVersion":2,"entries":[],"extra":true})", Rejection::UnsupportedVersion);
    // Anything but exactly the two root fields with an entries object.
    for (const char *bad : {"null", "[]", "1", "\"text\"", R"({"schemaVersion":1})",
                            R"({"schemaVersion":1,"entries":[]})", R"({"schemaVersion":1,"entries":"Open"})",
                            R"({"schemaVersion":1,"entries":null})", R"({"schemaVersion":1,"entries":{},"extra":true})"})
        EXPECT(bad, Rejection::UnexpectedField);
    // String-only replacement values.
    for (const char *value : {"1", "true", "null", "{}", "[]", R"({"nested":{"value":true}})", R"(["Inspect"])"})
        EXPECT(document(std::string(R"("Open":)") + value), Rejection::NonStringValue);
    // Blank text, controls, direction controls and reserved object names.
    for (const std::string &text : {std::string{}, std::string{"   "}, std::string{"\xC2\xA0"}, std::string{"\xE3\x80\x80"},
             "a" + esc("0000") + "b", "a" + esc("001f") + "b", "a" + esc("007f") + "b", "a" + esc("0085") + "b",
             "a" + esc("202e") + "b", "a" + esc("2066") + "b", std::string{"__proto__"}, std::string{"constructor"},
             std::string{"prototype"}}) {
        EXPECT(document(entry(text, "Inspect")), Rejection::UnsafeText);
        EXPECT(document(entry("Open", text)), Rejection::UnsafeText);
    }
}

static void structural_problems_take_precedence()
{
    // The structural pass covers the whole payload before any schema rule.
    EXPECT(R"({"schemaVersion":2,"entries":{"Open":7}} trailing)", Rejection::Malformed);
    EXPECT(R"({"schemaVersion":2,"entries":{"Open":7,"Open":8}})", Rejection::DuplicateKey);
    EXPECT(R"({"schemaVersion":2,"entries":{"Open":[[[[[[[[]]]]]]]]}})", Rejection::TooDeep);
}

int main()
{
    limits_are_the_contract();
    code_points_are_counted_not_bytes();
    accepts_valid_documents();
    file_size_boundary();
    depth_boundary();
    text_length_boundaries();
    many_entries_are_accepted();
    rejects_whole_file_and_never_partially();
    existing_rejections_remain();
    structural_problems_take_precedence();
    std::printf("PASS personal vocabulary schema (%d checks)\n", checks);
    return 0;
}
