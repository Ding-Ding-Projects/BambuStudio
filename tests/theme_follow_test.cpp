// Standalone assertions for src/slic3r/GUI/Appearance/ThemeFollow.cpp: Light,
// Dark and Follow Windows as stored, what each choice writes, Windows'
// AppsUseLightTheme value, and when following switches the look. Built and run
// by tests/theme_follow.test.mjs, which also builds it with
// SLIC3R_THEME_FOLLOW_NEGATIVE_FIXTURE: in that build the look follows Windows
// even when following is off, and the checks below must fail.
#include "src/slic3r/GUI/Appearance/ThemeFollow.cpp"

#include <cstdio>

using namespace Slic3r::GUI::ThemeFollow;

static int g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

int main()
{
    // As stored.
    CHECK(choice_from("0", "") == Choice::Light);
    CHECK(choice_from("1", "0") == Choice::Dark);
    CHECK(choice_from("1", "1") == Choice::FollowWindows);
    CHECK(choice_from("0", "1") == Choice::FollowWindows);

    // What each choice writes.
    CHECK(store(Choice::Light, true, "1").dark_color_mode == "0" && store(Choice::Light, true, "1").follow == "0");
    CHECK(store(Choice::Dark, false, "0").dark_color_mode == "1" && store(Choice::Dark, false, "0").follow == "0");
    CHECK(store(Choice::FollowWindows, true, "0").dark_color_mode == "1" && store(Choice::FollowWindows, true, "0").follow == "1");
    CHECK(store(Choice::FollowWindows, false, "1").dark_color_mode == "0");
    // Windows does not say: the look stays, following still starts.
    CHECK(store(Choice::FollowWindows, std::nullopt, "1").dark_color_mode == "1" && store(Choice::FollowWindows, std::nullopt, "1").follow == "1");

    // AppsUseLightTheme: 0 is dark, anything else light, missing says nothing.
    CHECK(windows_dark_from(0L) == std::optional<bool>(true));
    CHECK(windows_dark_from(1L) == std::optional<bool>(false));
    CHECK(!windows_dark_from(std::nullopt));

    // Following switches only when Windows says something different.
    CHECK(follow_change("0", "1", true) == std::optional<bool>(true));
    CHECK(follow_change("1", "1", false) == std::optional<bool>(false));
    CHECK(!follow_change("1", "1", true));
    CHECK(!follow_change("0", "1", std::nullopt));
    // Not following: Windows never changes the look.
    CHECK(!follow_change("0", "0", true));
    CHECK(!follow_change("1", "", false));

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("theme follow: all checks passed\n");
    return 0;
}
