#include <catch_main.hpp>

#include "libslic3r/AppLogo/Logo.hpp"

#include <cmath>
#include <limits>
#include <set>
#include <string>

using namespace Slic3r::AppLogo;

namespace {
Settings preset_selection(unsigned index)
{
    Settings s;
    s.source = Source::Preset;
    s.preset = index;
    return s;
}
} // namespace

TEST_CASE("The shipped mark is the default and resets clear the stored key", "[AppLogo]")
{
    const Settings defaults;
    REQUIRE(defaults.valid());
    REQUIRE(defaults.shipped());
    REQUIRE(std::string(config_key) == "app_logo");

    std::string stored = "unchanged";
    REQUIRE(stored_value(defaults, stored));
    REQUIRE(stored.empty());

    const Resolved resolved = resolve("");
    REQUIRE(resolved.provenance == Provenance::Default);
    REQUIRE(resolved.settings.shipped());
    REQUIRE(same(resolved.settings, defaults));

    // The shipped mark has no generated source: every surface keeps its own
    // shipped asset, so the selected-logo renderer yields nothing for it.
    REQUIRE_FALSE(source_image(defaults).valid());
    REQUIRE_FALSE(render_selected(defaults, 32).valid());
}

TEST_CASE("Preset selections persist and resolve back exactly", "[AppLogo]")
{
    for (unsigned index = 0; index < preset_count; ++index) {
        Settings s = preset_selection(index);
        s.fit = Fit::Cover;
        s.focal_x = 10;
        s.focal_y = 90;
        s.inset = 12;
        s.transparent = false;
        s.background = {12, 34, 56, 255};

        std::string stored;
        REQUIRE(stored_value(s, stored));
        REQUIRE_FALSE(stored.empty());
        REQUIRE(stored.size() <= 64);

        const Resolved resolved = resolve(stored);
        REQUIRE(resolved.provenance == Provenance::Stored);
        REQUIRE(same(resolved.settings, s));
        REQUIRE_FALSE(resolved.settings.shipped());

        const Image image = render_selected(resolved.settings, 26);
        REQUIRE(image.valid());
        REQUIRE(image.width == 26);
        REQUIRE(image.pixels.front().a == 255); // opaque background chosen
    }
}

TEST_CASE("Edits made on the shipped source are kept, but only defaults clear the key", "[AppLogo]")
{
    Settings s;
    s.inset = 20;
    std::string stored;
    REQUIRE(stored_value(s, stored));
    REQUIRE_FALSE(stored.empty());
    const Resolved resolved = resolve(stored);
    REQUIRE(resolved.provenance == Provenance::Stored);
    REQUIRE(resolved.settings.shipped());
    REQUIRE(resolved.settings.inset == 20);
}

TEST_CASE("Invalid stored values fall back to the shipped mark without being trusted", "[AppLogo]")
{
    for (const char *bad : {"garbage", "2 2 0 0 50 50 8 1 255 255 255", "2 1 3 0 50 50 8 1 255 255 255",
                            "2 1 0 0 50 50 8 1 255 255", "3 1 0 0 50 50 8 1 255 255 255",
                            "2 1 0 0 50 50 8 1 255 255 255 ", " 2 1 0 0 50 50 8 1 255 255 255"}) {
        const Resolved resolved = resolve(bad);
        INFO(bad);
        REQUIRE(resolved.provenance == Provenance::Invalid);
        REQUIRE(resolved.settings.shipped());
        REQUIRE(same(resolved.settings, Settings{}));
    }
}

TEST_CASE("Version 1 values written before the shipped source existed select their preset", "[AppLogo]")
{
    const Resolved resolved = resolve("1 2 0 50 50 8 1 255 255 255");
    REQUIRE(resolved.provenance == Provenance::Stored);
    REQUIRE(resolved.settings.source == Source::Preset);
    REQUIRE(resolved.settings.preset == 2);
}

TEST_CASE("Invalid settings are never persisted", "[AppLogo]")
{
    Settings s = preset_selection(0);
    s.inset = 26;
    std::string stored = "previous";
    REQUIRE_FALSE(stored_value(s, stored));
    REQUIRE(stored == "previous");
    s = preset_selection(preset_count);
    REQUIRE_FALSE(stored_value(s, stored));
    REQUIRE(stored == "previous");
}

TEST_CASE("The picker catalogue lists the shipped mark and every preset once", "[AppLogo]")
{
    const auto &list = sources();
    REQUIRE(list.size() == 1 + preset_count);
    REQUIRE(list.front().source == Source::Shipped);
    std::set<std::string> ids;
    for (size_t i = 0; i < list.size(); ++i) {
        ids.insert(list[i].id);
        const Settings selected = with_source(Settings{}, i);
        REQUIRE(selected.valid());
        REQUIRE(source_index(selected) == int(i));
        REQUIRE(selected.shipped() == (i == 0));
        if (i > 0)
            REQUIRE(source_image(selected).valid());
    }
    REQUIRE(ids.size() == list.size());
    // Switching source keeps the presentation choices the user already made.
    Settings tuned = preset_selection(1);
    tuned.inset = 3;
    REQUIRE(with_source(tuned, 0).inset == 3);
    REQUIRE(with_source(tuned, 0).shipped());
    // Out-of-range picks leave the selection untouched.
    REQUIRE(same(with_source(tuned, list.size()), tuned));
}

TEST_CASE("Every chrome target and window icon size stays within the render bound", "[AppLogo]")
{
    std::set<std::string> ids;
    std::set<Target> targets;
    for (const TargetSpec &spec : chrome_targets()) {
        ids.insert(spec.id);
        targets.insert(spec.target);
        REQUIRE(spec.logical_px > 0);
        REQUIRE(spec.logical_px <= max_output_px);
    }
    REQUIRE(targets == std::set<Target>{Target::TitleBar, Target::WindowIcon, Target::About, Target::StartupScreen});
    REQUIRE(ids.size() == chrome_targets().size());

    const Settings s = preset_selection(0);
    for (const unsigned px : window_icon_sizes) {
        REQUIRE(px <= max_output_px);
        const Image image = render_selected(s, px);
        REQUIRE(image.valid());
        REQUIRE(image.width == px);
    }
}

TEST_CASE("Render plans scale with the display and cap the generated size", "[AppLogo]")
{
    RenderPlan p = plan(26, 1.0);
    REQUIRE(p.render_px == 26);
    REQUIRE(p.draw_px == 26);
    p = plan(26, 1.5);
    REQUIRE(p.render_px == 39);
    REQUIRE(p.draw_px == 39);
    p = plan(122, 2.5);
    REQUIRE(p.draw_px == 305);
    REQUIRE(p.render_px == max_output_px);
    p = plan(26, std::numeric_limits<double>::quiet_NaN());
    REQUIRE(p.draw_px == 26);
    p = plan(26, -2.0);
    REQUIRE(p.draw_px == 26);
    p = plan(64, 1000.0);
    REQUIRE(p.draw_px <= 64u * 8u);
    p = plan(0, 1.0);
    REQUIRE(p.render_px == 0);
    REQUIRE(p.draw_px == 0);
}
