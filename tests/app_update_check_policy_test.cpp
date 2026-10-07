#undef NDEBUG
#include "../src/slic3r/GUI/AppUpdateCheckPolicy.hpp"
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
using Slic3r::GUI::AppUpdateCheckPolicy;
using Action  = AppUpdateCheckPolicy::Action;
using Outcome = AppUpdateCheckPolicy::Outcome;
static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

static long long utc(const char *stamp) {
    long long seconds = -12345;
    CHECK(AppUpdateCheckPolicy::parse_utc(stamp, seconds));
    return seconds;
}
static bool rejected(const std::string &stamp) {
    long long seconds = 42;
    return !AppUpdateCheckPolicy::parse_utc(stamp, seconds) && seconds == 42;
}
static bool newer(const char *published, const char *built) {
    return AppUpdateCheckPolicy::release_is_newer(published, built, AppUpdateCheckPolicy::kMarginSeconds);
}

static void time_assertions() {
    CHECK(utc("1970-01-01T00:00:00Z") == 0);
    CHECK(utc("1969-12-31T23:59:59Z") == -1);
    CHECK(utc("2000-01-01T00:00:00Z") == 946684800LL);
    CHECK(utc("2000-02-29T00:00:00Z") == 951782400LL);
    CHECK(utc("2026-10-03T21:05:29Z") == 1791061529LL);
    CHECK(utc("2028-02-29T12:00:00Z") == 1835438400LL);
    CHECK(utc("2028-03-01T00:00:00Z") - utc("2028-02-29T23:59:59Z") == 1);
    CHECK(utc("2027-12-31T23:59:59Z") == 1830297599LL);
    CHECK(utc("2028-01-01T00:00:00Z") - utc("2027-12-31T23:59:59Z") == 1);
    CHECK(utc("9999-12-31T23:59:59Z") == 253402300799LL);

    for (const char *bad : {"", "2026-10-03T21:05:29", "2026-10-03T21:05:29z", "2026-10-03 21:05:29Z", "2026-10-03T21:05:29.000Z",
                            "2026-10-03T21:05:29+00:00", "2026-10-03T21:05:29ZZ", "20261003-210529", "2026/10/03T21:05:29Z",
                            "2026-10-03T21-05-29Z", "2026-1O-03T21:05:29Z", " 026-10-03T21:05:29Z", "+026-10-03T21:05:29Z",
                            "2026-10-3T21:05:29Z ", "2026-00-03T21:05:29Z", "2026-13-03T21:05:29Z", "2026-10-00T21:05:29Z",
                            "2026-10-32T21:05:29Z", "2026-04-31T21:05:29Z", "2026-02-29T21:05:29Z", "2100-02-29T00:00:00Z",
                            "2026-10-03T24:00:00Z", "2026-10-03T21:60:00Z", "2026-10-03T21:05:60Z"})
        CHECK(rejected(bad));

    // Exactly the margin is not newer; one second more is.
    CHECK(!newer("2026-10-03T21:05:29Z", "2026-10-03T18:05:29Z"));
    CHECK(newer("2026-10-03T21:05:29Z", "2026-10-03T18:05:28Z"));
    CHECK(!AppUpdateCheckPolicy::release_is_newer("2026-10-03T21:05:29Z", "2026-10-03T21:05:29Z", 0));
    CHECK(AppUpdateCheckPolicy::release_is_newer("2026-10-03T21:05:30Z", "2026-10-03T21:05:29Z", 0));
    // md3-v225 was published at 2026-10-03T21:05:29Z.
    CHECK(newer("2026-10-03T21:05:29Z", "2026-10-03T17:00:00Z"));
    CHECK(!newer("2026-10-03T21:05:29Z", "2026-10-03T20:35:29Z"));
    CHECK(!newer("2026-10-03T21:05:29Z", "2026-10-04T02:00:00Z"));
    CHECK(newer("2028-03-01T02:00:00Z", "2028-02-28T23:00:00Z"));
    // A stamp that cannot be read is never newer, whatever the margin. The old local-time build
    // stamp is one of them.
    CHECK(!AppUpdateCheckPolicy::release_is_newer("2026-10-03T21:05:29Z", "20261003-170000", -1000000000LL));
    CHECK(!AppUpdateCheckPolicy::release_is_newer("not a date", "2026-10-03T17:00:00Z", -1000000000LL));
    CHECK(!AppUpdateCheckPolicy::release_is_newer("", "", -1000000000LL));
}

static void decision_assertions() {
    // decide(auto_update_enabled, squirrel_installed, newer_by_time, manual_check, tag_is_skipped)
    CHECK(AppUpdateCheckPolicy::decide(true, true, true, false, false) == Action::RunSquirrelUpdate);
    CHECK(AppUpdateCheckPolicy::decide(true, true, false, false, false) == Action::RunSquirrelUpdate);
    CHECK(AppUpdateCheckPolicy::decide(true, true, true, false, true) == Action::Nothing);
    CHECK(AppUpdateCheckPolicy::decide(true, true, false, false, true) == Action::Nothing);
    CHECK(AppUpdateCheckPolicy::decide(true, true, false, true, true) == Action::RunSquirrelUpdate);
    CHECK(AppUpdateCheckPolicy::decide(true, true, false, true, false) == Action::RunSquirrelUpdate);
    CHECK(AppUpdateCheckPolicy::decide(true, true, true, true, false) == Action::RunSquirrelUpdate);
    CHECK(AppUpdateCheckPolicy::decide(false, true, true, true, false) == Action::OfferDownload);
    CHECK(AppUpdateCheckPolicy::decide(false, true, false, true, false) == Action::ShowNoNewVersion);
    CHECK(AppUpdateCheckPolicy::decide(false, true, true, false, false) == Action::Nothing);
    CHECK(AppUpdateCheckPolicy::decide(true, false, true, true, true) == Action::OfferDownload);
    CHECK(AppUpdateCheckPolicy::decide(true, false, true, true, false) == Action::OfferDownload);
    CHECK(AppUpdateCheckPolicy::decide(true, false, false, true, false) == Action::ShowNoNewVersion);
    CHECK(AppUpdateCheckPolicy::decide(true, false, true, false, false) == Action::Nothing);
    CHECK(AppUpdateCheckPolicy::decide(true, false, false, false, false) == Action::Nothing);
    CHECK(AppUpdateCheckPolicy::decide(false, false, false, false, false) == Action::Nothing);
    // A background check never opens a dialog or a toast, and only an installed copy with
    // automatic updates on runs Update.exe.
    for (int bits = 0; bits < 32; ++bits) {
        const bool auto_update = bits & 1, installed = bits & 2, is_newer = bits & 4, manual = bits & 8, skipped = bits & 16;
        const Action action = AppUpdateCheckPolicy::decide(auto_update, installed, is_newer, manual, skipped);
        CHECK(manual || (action != Action::OfferDownload && action != Action::ShowNoNewVersion));
        CHECK((action == Action::RunSquirrelUpdate) == (auto_update && installed && (manual || !skipped)));
    }

    // after_squirrel_update(updated, newer_by_time, manual_check, failure_already_reported)
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(true, false, false, false) == Outcome::ShowReady);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(true, true, false, true) == Outcome::ShowReady);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(true, true, true, false) == Outcome::ShowReady);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(false, false, true, false) == Outcome::ShowNoNewVersion);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(false, false, false, false) == Outcome::Nothing);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(false, false, false, true) == Outcome::Nothing);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(false, true, true, false) == Outcome::OfferDownload);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(false, true, true, true) == Outcome::OfferDownload);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(false, true, false, false) == Outcome::NotifyFailure);
    CHECK(AppUpdateCheckPolicy::after_squirrel_update(false, true, false, true) == Outcome::Nothing);
    for (int bits = 0; bits < 16; ++bits) {
        const bool updated = bits & 1, is_newer = bits & 2, manual = bits & 4, reported = bits & 8;
        const Outcome outcome = AppUpdateCheckPolicy::after_squirrel_update(updated, is_newer, manual, reported);
        CHECK(!updated || outcome == Outcome::ShowReady);
        CHECK(manual || (outcome != Outcome::OfferDownload && outcome != Outcome::ShowNoNewVersion));
        CHECK(!manual || outcome != Outcome::NotifyFailure);
    }
}

int main() {
    time_assertions();
    decision_assertions();
#ifndef _WIN32
    // The verdict must not move with the local zone. mktime() of one wall-clock time shows that
    // each zone really is in effect (EDT is UTC-4 and Hong Kong UTC+8 on 3 October 2026).
    const struct { const char *zone; long long offset; } zones[] = {{"UTC", 0}, {"America/Toronto", 4 * 3600}, {"Asia/Hong_Kong", -8 * 3600}};
    for (const auto &z : zones) {
        setenv("TZ", z.zone, 1);
        tzset();
        std::tm wall{};
        wall.tm_year  = 2026 - 1900;
        wall.tm_mon   = 9;
        wall.tm_mday  = 3;
        wall.tm_hour  = 12;
        wall.tm_isdst = -1;
        CHECK(static_cast<long long>(std::mktime(&wall)) - utc("2026-10-03T12:00:00Z") == z.offset);
        time_assertions();
    }
#endif
    std::cout << assertions << " app update check policy assertions passed\n";
}
