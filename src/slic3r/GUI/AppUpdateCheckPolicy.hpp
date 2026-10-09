#pragma once
#include <string>
namespace Slic3r { namespace GUI {
// What the release check does (docs/features/windows/app-updates.md). Both times are UTC stamps
// and are compared with plain arithmetic, never with a clock or time-zone API, so every machine
// reaches the same verdict whatever its local zone.
struct AppUpdateCheckPolicy {
    enum class Action { Nothing, ShowNoNewVersion, RunSquirrelUpdate, OfferDownload };
    enum class Outcome { Nothing, ShowReady, ShowNoNewVersion, OfferDownload, NotifyFailure };
    // A release is newer only when published more than this long after the build, so the time
    // between compiling a release and publishing it never offers that release to itself.
    static constexpr long long kMarginSeconds = 3 * 60 * 60;

    // Days from 1970-01-01 to a proleptic Gregorian date (Howard Hinnant's days_from_civil).
    static constexpr long long days_from_civil(long long y, unsigned m, unsigned d) {
        y -= m <= 2 ? 1 : 0;
        const long long era = (y >= 0 ? y : y - 399) / 400;
        const unsigned  yoe = static_cast<unsigned>(y - era * 400);
        const unsigned  doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
        const unsigned  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + static_cast<long long>(doe) - 719468;
    }
    static constexpr int days_in_month(int year, int month) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return month == 2 ? (leap ? 29 : 28) : (month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31;
    }
    // Exactly "YYYY-MM-DDTHH:MM:SSZ" (GitHub's published_at, SLIC3R_BUILD_TIME_UTC) to seconds since
    // 1970-01-01T00:00:00Z. Anything else, including an impossible date or time, is rejected.
    static bool parse_utc(const std::string &stamp, long long &seconds) {
        if (stamp.size() != 20 || stamp[4] != '-' || stamp[7] != '-' || stamp[10] != 'T' || stamp[13] != ':' || stamp[16] != ':' || stamp[19] != 'Z')
            return false;
        const int start[6] = {0, 5, 8, 11, 14, 17};
        int       field[6] = {};
        for (int f = 0; f < 6; ++f)
            for (int i = start[f]; i < start[f] + (f == 0 ? 4 : 2); ++i) {
                if (stamp[i] < '0' || stamp[i] > '9')
                    return false;
                field[f] = field[f] * 10 + (stamp[i] - '0');
            }
        if (field[1] < 1 || field[1] > 12 || field[2] < 1 || field[2] > days_in_month(field[0], field[1]) || field[3] > 23 || field[4] > 59 || field[5] > 59)
            return false;
        seconds = days_from_civil(field[0], static_cast<unsigned>(field[1]), static_cast<unsigned>(field[2])) * 86400 + field[3] * 3600 + field[4] * 60 + field[5];
        return true;
    }
    // A stamp that cannot be read is never newer.
    static bool release_is_newer(const std::string &published_at_utc, const std::string &build_time_utc, long long margin_seconds) {
        long long published = 0, built = 0;
        return parse_utc(published_at_utc, published) && parse_utc(build_time_utc, built) && published - built > margin_seconds;
    }
    // An installed copy with automatic updates on always asks Update.exe, which compares package
    // versions, unless a background check meets the skipped tag. Every other copy opens the
    // download dialog only for a manual check, and only for a newer release.
    static Action decide(bool auto_update_enabled, bool squirrel_installed, bool newer_by_time, bool manual_check, bool tag_is_skipped) {
        if (auto_update_enabled && squirrel_installed)
            return manual_check || !tag_is_skipped ? Action::RunSquirrelUpdate : Action::Nothing;
        if (!manual_check)
            return Action::Nothing;
        return newer_by_time ? Action::OfferDownload : Action::ShowNoNewVersion;
    }
    // The release could not be read: the API refused the request (a shared address that has used
    // GitHub's anonymous hourly allowance gets 403 or 429), there was no network, or the answer
    // had no tag. Update.exe reads its own feed and compares package versions, so an installed copy
    // with the preference on still asks it, except on a background check while a version is
    // skipped, since the feed's tag is unknown. Its run counts as "not newer", so staging nothing
    // is not reported as a failure. Every other copy behaves as when nothing is newer.
    static Action decide_without_release(bool auto_update_enabled, bool squirrel_installed, bool manual_check, bool a_version_is_skipped) {
        return decide(auto_update_enabled, squirrel_installed, false, manual_check, a_version_is_skipped);
    }
    // After Update.exe. A staged version always shows the ready banner. Nothing staged is a real
    // failure only when the release was newer: a manual check then opens the download dialog, a
    // background check shows one failure notice per release. Otherwise there was nothing to install.
    static Outcome after_squirrel_update(bool updated, bool newer_by_time, bool manual_check, bool failure_already_reported) {
        if (updated)
            return Outcome::ShowReady;
        if (!newer_by_time)
            return manual_check ? Outcome::ShowNoNewVersion : Outcome::Nothing;
        if (manual_check)
            return Outcome::OfferDownload;
        return failure_already_reported ? Outcome::Nothing : Outcome::NotifyFailure;
    }
};
}}
