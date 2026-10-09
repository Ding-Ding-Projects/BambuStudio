#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <catch_main.hpp>

#include "slic3r/GUI/LocalConverter/LocalConverterCopy.hpp"

#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace Slic3r::GUI::LocalConverterCopy;

namespace {

std::vector<Line> every_line()
{
    std::vector<Line> lines;
    for (unsigned i = 0; i != static_cast<unsigned>(Line::Count); ++i)
        lines.push_back(static_cast<Line>(i));
    return lines;
}

// The printf placeholders of a message, in order. A voice variant may move
// words around a fact but never drops, adds or reorders one.
std::vector<std::string> placeholders(const char *text)
{
    std::vector<std::string> found;
    for (const char *p = text; *p; ++p) {
        if (*p != '%') continue;
        const char *start = p++;
        if (*p == '%') continue;
        while (*p && std::strchr("0123456789.-+ #lhzjt", *p)) ++p;
        if (!*p) break;
        found.emplace_back(start, p + 1);
    }
    return found;
}

} // namespace

TEST_CASE("every converter voice line has text at every funny level", "[local_converter_copy]")
{
    for (Line line : every_line())
        for (int level = FUNNY_LEVEL_MIN; level <= FUNNY_LEVEL_MAX; ++level) {
            const char *text = source(line, level);
            REQUIRE(text != nullptr);
            REQUIRE(std::strlen(text) > 0);
        }
}

TEST_CASE("levels follow the shared three-step ladder and clamp out-of-range values", "[local_converter_copy]")
{
    for (Line line : every_line()) {
        CHECK(std::string(source(line, 1)) == source(line, 2));
        CHECK(std::string(source(line, 4)) == source(line, 5));
        CHECK(std::string(source(line, 0)) == source(line, 1));
        CHECK(std::string(source(line, -7)) == source(line, 1));
        CHECK(std::string(source(line, 6)) == source(line, 5));
        CHECK(std::string(source(line, 99)) == source(line, 5));
        // The ladder is real: the playful voice is not the serious one.
        CHECK(std::string(source(line, 1)) != source(line, 5));
        CHECK(std::string(source(line, 3)) != source(line, 1));
        CHECK(std::string(source(line, 3)) != source(line, 5));
    }
}

TEST_CASE("facts carried by placeholders are identical at every level", "[local_converter_copy]")
{
    for (Line line : every_line()) {
        const auto serious = placeholders(source(line, 1));
        for (int level = FUNNY_LEVEL_MIN; level <= FUNNY_LEVEL_MAX; ++level)
            CHECK(placeholders(source(line, level)) == serious);
    }
    // The stop summary keeps both of its counts.
    CHECK(placeholders(source(Line::Stopped, 5)) == std::vector<std::string>{"%llu", "%llu"});
}

TEST_CASE("the serious voice keeps the established converter wording", "[local_converter_copy]")
{
    CHECK(std::string(source(Line::EmptyQueue, 1)) == "Empty queue. No source files have been selected.");
    CHECK(std::string(source(Line::NoMatches, 1)) == "No adapters match this search.");
    CHECK(std::string(source(Line::Converting, 1)) ==
          "Converting offline in the isolated worker. One file runs at a time; every result is saved.");
}

TEST_CASE("no two lines share a variant", "[local_converter_copy]")
{
    std::set<std::string> seen;
    for (Line line : every_line())
        for (int level : {1, 3, 5})
            CHECK(seen.insert(source(line, level)).second);
}
