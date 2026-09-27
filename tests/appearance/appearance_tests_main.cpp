#include <catch_main.hpp>

#include <wx/filename.h>
#include <wx/init.h>
#include <wx/stdpaths.h>

#include "slic3r/GUI/Appearance/ElementStyle.hpp"

using Slic3r::GUI::StyleBag;
using Slic3r::GUI::StyleLoadReport;
using Slic3r::GUI::StylePreset;
using Slic3r::GUI::StyleRegistry;
namespace StyleProp = Slic3r::GUI::StyleProp;

// wxFont / wxColour / wxFile need the wx library initialised on MSW.
struct WxRuntimeListener : Catch::TestEventListenerBase
{
    using TestEventListenerBase::TestEventListenerBase;

    void testRunStarting(Catch::TestRunInfo const &) override { m_ok = wxInitialize(); }
    void testRunEnded(Catch::TestRunStats const &) override
    {
        if (m_ok)
            wxUninitialize();
    }

    bool m_ok = false;
};
CATCH_REGISTER_LISTENER(WxRuntimeListener)

namespace {

std::string temp_file(const char *stem)
{
    wxString dir = wxStandardPaths::Get().GetTempDir();
    wxString path = dir + wxFileName::GetPathSeparator() + "appearance_tests_" + stem + ".json";
    return std::string(path.ToUTF8().data());
}

} // namespace

TEST_CASE("Property catalogue is complete and stable", "[appearance]")
{
    const auto &keys = StyleProp::all();
    REQUIRE(keys.size() == 16);
    CHECK(StyleProp::is_known("fontFamily"));
    CHECK(StyleProp::is_known("borderColor"));
    CHECK_FALSE(StyleProp::is_known("glow"));
    // Element ids and property names are plain ASCII strings the file keeps
    // verbatim, so the same id resolves the same way in every session.
    CHECK(std::string(StyleProp::font_size) == "fontSize");
}

TEST_CASE("Defaults resolve to nothing so widgets keep their tokens", "[appearance]")
{
    StyleRegistry reg;
    CHECK(reg.active_preset() == StyleRegistry::kDefaultPreset);
    CHECK(reg.resolve("project-tab", StyleProp::font_size).is_null());
    CHECK(reg.resolved_bag("project-tab").empty());
    const wxFont base(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    CHECK(Slic3r::GUI::style_font_from_bag(reg.resolved_bag("project-tab"), base) == base);
}

TEST_CASE("Set, resolve and per-property reset", "[appearance]")
{
    StyleRegistry reg;
    int notified = 0;
    std::string last_id;
    reg.subscribe([&](const std::string &id) { ++notified; last_id = id; });

    reg.set("menu.item", StyleProp::font_size, 15.0);
    reg.set("menu.item", StyleProp::foreground, "#ff0000");
    CHECK(notified == 2);
    CHECK(last_id == "menu.item");
    CHECK(reg.resolve("menu.item", StyleProp::font_size).get<double>() == 15.0);
    CHECK(reg.has_override("menu.item", StyleProp::foreground));

    reg.reset_property("menu.item", StyleProp::font_size);
    CHECK(reg.resolve("menu.item", StyleProp::font_size).is_null());
    CHECK(reg.has_override("menu.item", StyleProp::foreground));

    reg.reset_element("menu.item");
    CHECK(reg.overridden_ids().empty());
    CHECK(reg.user_bag("menu.item") == nullptr);
}

TEST_CASE("Ids inherit from their parent through the slash", "[appearance]")
{
    StyleRegistry reg;
    reg.set("project-tab", StyleProp::font_weight, 600);
    reg.set("project-tab/model.3mf", StyleProp::foreground, "#123456");
    CHECK(reg.resolve("project-tab/model.3mf", StyleProp::font_weight).get<int>() == 600);
    CHECK(reg.resolve("project-tab/model.3mf", StyleProp::foreground).get<std::string>() == "#123456");
    CHECK(reg.resolve("project-tab/other.3mf", StyleProp::foreground).is_null());
    CHECK(reg.resolve("project-tab", StyleProp::foreground).is_null());
}

TEST_CASE("Preset inheritance and user override precedence", "[appearance]")
{
    StyleRegistry reg;
    REQUIRE(reg.set_active_preset("Large text"));
    // Shipped "*" entry reaches every element.
    CHECK(reg.resolve("anything", StyleProp::font_size).get<double>() == 15.5);
    // A user override wins over the preset.
    reg.set("anything", StyleProp::font_size, 11.0);
    CHECK(reg.resolve("anything", StyleProp::font_size).get<double>() == 11.0);
    // Per-property reset returns to the preset value, not to nothing.
    reg.reset_property("anything", StyleProp::font_size);
    CHECK(reg.resolve("anything", StyleProp::font_size).get<double>() == 15.5);
    // Unknown preset is refused and leaves the active one alone.
    CHECK_FALSE(reg.set_active_preset("No such preset"));
    CHECK(reg.active_preset() == "Large text");

    // A preset's element entry beats its "*" entry.
    StylePreset custom;
    custom["*"]        = StyleBag{{StyleProp::radius, 4}};
    custom["menu.item"] = StyleBag{{StyleProp::radius, 12}};
    REQUIRE(reg.save_preset("Custom", custom));
    REQUIRE(reg.set_active_preset("Custom"));
    CHECK(reg.resolve("menu.item", StyleProp::radius).get<int>() == 12);
    CHECK(reg.resolve("button", StyleProp::radius).get<int>() == 4);
    CHECK_FALSE(reg.is_shipped_preset("Custom"));
    CHECK(reg.is_shipped_preset("Rounded"));
    // Shipped names cannot be overwritten.
    CHECK_FALSE(reg.save_preset("Rounded", custom));
}

TEST_CASE("Save as preset snapshots the visible result", "[appearance]")
{
    StyleRegistry reg;
    REQUIRE(reg.set_active_preset("Bold labels"));
    reg.set("menu.item", StyleProp::foreground, "#00ff00");
    REQUIRE(reg.save_preset("Mine"));
    const StylePreset *mine = reg.preset("Mine");
    REQUIRE(mine != nullptr);
    CHECK(mine->at("*")[StyleProp::font_weight].get<int>() == 600);
    CHECK(mine->at("menu.item")[StyleProp::foreground].get<std::string>() == "#00ff00");

    // Deleting the active user preset falls back to the default.
    REQUIRE(reg.set_active_preset("Mine"));
    REQUIRE(reg.delete_preset("Mine"));
    CHECK(reg.active_preset() == StyleRegistry::kDefaultPreset);
    CHECK_FALSE(reg.delete_preset("Bold labels"));
}

TEST_CASE("Reset all clears overrides and the preset but keeps saved presets", "[appearance]")
{
    StyleRegistry reg;
    reg.set("a", StyleProp::padding, 3);
    StylePreset p;
    p["*"] = StyleBag{{StyleProp::margin, 9}};
    REQUIRE(reg.save_preset("Keep me", p));
    REQUIRE(reg.set_active_preset("Keep me"));
    std::string last;
    reg.subscribe([&](const std::string &id) { last = id; });
    reg.reset_all();
    CHECK(last == "*");
    CHECK(reg.overridden_ids().empty());
    CHECK(reg.active_preset() == StyleRegistry::kDefaultPreset);
    CHECK(reg.preset("Keep me") != nullptr);
}

TEST_CASE("JSON round trip preserves unknown keys and reports them", "[appearance]")
{
    const std::string text = R"({
        "schema": 1,
        "activePreset": "Rounded",
        "futureSetting": {"x": 1},
        "presets": {"Mine": {"*": {"fontSize": 14, "glow": "#ffffff"}}},
        "elements": {"menu.item": {"foreground": "#112233", "textShadow": "2px"}}
    })";
    StyleRegistry reg;
    StyleLoadReport r = reg.parse(text);
    REQUIRE(r.ok);
    CHECK(r.schema == 1);
    REQUIRE(r.unknown_top_level.size() == 1);
    CHECK(r.unknown_top_level[0] == "futureSetting");
    REQUIRE(r.unknown_properties.size() == 2);
    CHECK(std::find(r.unknown_properties.begin(), r.unknown_properties.end(), "Mine/*.glow") != r.unknown_properties.end());
    CHECK(std::find(r.unknown_properties.begin(), r.unknown_properties.end(), "menu.item.textShadow") != r.unknown_properties.end());
    CHECK(reg.active_preset() == "Rounded");

    nlohmann::json out = reg.to_json();
    CHECK(out["schema"].get<int>() == 1);
    CHECK(out["futureSetting"]["x"].get<int>() == 1);
    CHECK(out["presets"]["Mine"]["*"]["glow"].get<std::string>() == "#ffffff");
    CHECK(out["elements"]["menu.item"]["textShadow"].get<std::string>() == "2px");
    // The unknown property is also visible on the resolved bag.
    CHECK(reg.resolved_bag("menu.item")["textShadow"].get<std::string>() == "2px");

    // Re-parsing the dump is a fixed point.
    StyleRegistry again;
    REQUIRE(again.parse(reg.dump()).ok);
    CHECK(again.to_json() == out);
}

TEST_CASE("Load rejects garbage and newer schemas without touching state", "[appearance]")
{
    StyleRegistry reg;
    reg.set("keep", StyleProp::radius, 7);
    CHECK_FALSE(reg.parse("this is not json").ok);
    CHECK_FALSE(reg.parse("[1,2,3]").ok);
    CHECK_FALSE(reg.parse(R"({"schema": 99})").ok);
    CHECK_FALSE(reg.parse(R"({"elements": {}})").ok); // no schema
    CHECK(reg.resolve("keep", StyleProp::radius).get<int>() == 7);
    CHECK_FALSE(reg.last_report().ok);
    CHECK_FALSE(reg.last_report().error.empty());
}

TEST_CASE("A shipped preset name inside a file never shadows the shipped preset", "[appearance]")
{
    StyleRegistry reg;
    REQUIRE(reg.parse(R"({"schema":1,"presets":{"Rounded":{"*":{"radius":1}}}})").ok);
    REQUIRE(reg.set_active_preset("Rounded"));
    CHECK(reg.resolve("x", StyleProp::radius).get<int>() == 20);
}

TEST_CASE("Unknown active preset falls back to the default and is reported", "[appearance]")
{
    StyleRegistry reg;
    StyleLoadReport r = reg.parse(R"({"schema":1,"activePreset":"Gone"})");
    REQUIRE(r.ok);
    CHECK(reg.active_preset() == StyleRegistry::kDefaultPreset);
    REQUIRE(r.unknown_properties.size() == 1);
    CHECK(r.unknown_properties[0].rfind("activePreset=Gone", 0) == 0);
}

TEST_CASE("Export then import yields an equal registry", "[appearance]")
{
    StyleRegistry a;
    a.set("project-tab", StyleProp::font_family, "Roboto Mono");
    a.set("project-tab", StyleProp::underline, true);
    StylePreset p;
    p["*"] = StyleBag{{StyleProp::letter_spacing, 0.6}};
    REQUIRE(a.save_preset("Tracked", p));
    REQUIRE(a.set_active_preset("Tracked"));

    const std::string path = temp_file("export");
    std::string       err;
    REQUIRE(a.export_theme(path, &err));
    CHECK(err.empty());

    StyleRegistry b;
    b.set("other", StyleProp::padding, 1); // import replaces overrides
    StyleLoadReport r = b.import_theme(path);
    REQUIRE(r.ok);
    CHECK(b.to_json() == a.to_json());
    CHECK(b.resolve("other", StyleProp::padding).is_null());
    CHECK(b.resolve("project-tab", StyleProp::letter_spacing).get<double>() == 0.6);

    // save()/load() through the same file agree too.
    REQUIRE(a.save(path, &err));
    StyleRegistry c;
    REQUIRE(c.load(path).ok);
    CHECK(c.to_json() == a.to_json());
    wxRemoveFile(wxString::FromUTF8(path));

    StyleRegistry d;
    CHECK_FALSE(d.load(temp_file("does-not-exist")).ok);
}

TEST_CASE("Font and colour helpers apply the bag over a base", "[appearance]")
{
    const wxFont base(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    StyleBag bag = {{StyleProp::font_size, 14.0},
                    {StyleProp::font_weight, 700},
                    {StyleProp::font_style, "italic"},
                    {StyleProp::underline, true},
                    {StyleProp::strikethrough, true}};
    const wxFont f = Slic3r::GUI::style_font_from_bag(bag, base);
    CHECK(f.GetFractionalPointSize() == 14.0);
    CHECK(f.GetNumericWeight() == 700);
    CHECK(f.GetStyle() == wxFONTSTYLE_ITALIC);
    CHECK(f.GetUnderlined());
    CHECK(f.GetStrikethrough());
    // Out-of-range sizes and weights are clamped or ignored, never crash.
    StyleBag wild = {{StyleProp::font_size, 500.0}, {StyleProp::font_weight, 5000}};
    const wxFont g = Slic3r::GUI::style_font_from_bag(wild, base);
    CHECK(g.GetFractionalPointSize() == 10.0);
    CHECK(g.GetNumericWeight() == 900);

    const wxColour fallback(1, 2, 3);
    CHECK(Slic3r::GUI::style_colour_from_json("#ff8800", fallback) == wxColour(255, 136, 0));
    CHECK(Slic3r::GUI::style_colour_from_json("not a colour", fallback) == fallback);
    CHECK(Slic3r::GUI::style_colour_from_json(42, fallback) == fallback);
    CHECK(Slic3r::GUI::style_colour_to_string(wxColour(255, 136, 0)) == "#ff8800");
    CHECK(Slic3r::GUI::style_colour_to_string(wxColour(255, 136, 0, 128)) == "#ff880080");
}

TEST_CASE("Listeners are told which id changed and can unsubscribe", "[appearance]")
{
    StyleRegistry reg;
    std::vector<std::string> seen;
    const int token = reg.subscribe([&](const std::string &id) { seen.push_back(id); });
    reg.set("a", StyleProp::radius, 1);
    reg.set_active_preset("Rounded");
    reg.unsubscribe(token);
    reg.set("b", StyleProp::radius, 2);
    REQUIRE(seen.size() == 2);
    CHECK(seen[0] == "a");
    CHECK(seen[1] == "*");
}
