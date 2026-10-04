#include <catch2/catch.hpp>
#include "slic3r/GUI/HumanDate.hpp"

using namespace Slic3r::GUI;

TEST_CASE("Human dates use full English months and localized Cantonese dates", "[HumanDate]")
{
    const wxDateTime date = HumanDate::calendar("2026-09-29");
    REQUIRE(date.IsValid());
    REQUIRE(HumanDate::english(date) == "29 September 2026");
    REQUIRE(HumanDate::cantonese(date) == wxString::FromUTF8("2026年9月29日"));
    REQUIRE(HumanDate::english(HumanDate::calendar("2024-02-29")) == "29 February 2024");
}

TEST_CASE("Calendar dates reject invalid input without converting time zones", "[HumanDate]")
{
    REQUIRE_FALSE(HumanDate::calendar("2026-02-29").IsValid());
    REQUIRE_FALSE(HumanDate::calendar("not a date").IsValid());
    REQUIRE_FALSE(HumanDate::calendar("2026-09-29T00:00:00Z").IsValid());
    const auto date = HumanDate::calendar("2026-09-29");
    REQUIRE(date.GetYear() == 2026);
    REQUIRE(date.GetMonth() == wxDateTime::Sep);
    REQUIRE(date.GetDay() == 29);
}

TEST_CASE("UTC presentation parsing preserves the exact instant", "[HumanDate]")
{
    const auto date = HumanDate::utc_stamp("2026-09-29T00:30:00Z");
    REQUIRE(date.IsValid());
    REQUIRE(date.ToUTC().FormatISOCombined('T') == "2026-09-29T00:30:00");
    REQUIRE_FALSE(HumanDate::utc_stamp("2026-09-29T00:30:00").IsValid());
    REQUIRE_FALSE(HumanDate::utc_stamp("bad input").IsValid());
}
