#include <catch2/catch.hpp>

#include "slic3r/GUI/HomeAssistantSettingsSyncModel.hpp"

#include <map>
#include <string>

using namespace Slic3r::GUI::HomeAssistant::SettingsSync;

TEST_CASE("deny-list refuses secrets, credentials and paths by key", "[settings-sync][deny-list]")
{
    CHECK(is_denied_key("ha_token"));
    CHECK(is_denied_key("ha_url"));
    CHECK(is_denied_key("access_token"));
    CHECK(is_denied_key("user_token"));
    CHECK(is_denied_key("password"));
    CHECK(is_denied_key("api_key"));
    CHECK(is_denied_key("last_export_path"));
    CHECK(is_denied_key("last_output_dir"));
    CHECK(is_denied_key("recent_projects"));
    CHECK(is_denied_key("ollama_host"));
    CHECK(is_denied_key("printer_access_code"));
    CHECK(is_denied_key("dev_id"));
    CHECK(is_denied_key("ha_speakers"));
    CHECK(is_denied_key("ha_lights"));
    CHECK(is_denied_key("HA_TOKEN")); // case does not help
}

TEST_CASE("deny-list keeps ordinary settings", "[settings-sync][deny-list]")
{
    CHECK_FALSE(is_denied_key("language"));
    CHECK_FALSE(is_denied_key("dark_color_mode"));
    CHECK_FALSE(is_denied_key("ui_density"));
    CHECK_FALSE(is_denied_key("ui_accent_seed"));
    CHECK_FALSE(is_denied_key("ui_font_family"));
    CHECK_FALSE(is_denied_key("ui_font_scale"));
    CHECK_FALSE(is_denied_key("funny_level_en"));
    CHECK_FALSE(is_denied_key("funny_level_yue"));
    CHECK_FALSE(is_denied_key("app_display_name"));
    CHECK_FALSE(is_denied_key("narrator_enabled"));
    CHECK_FALSE(is_denied_key("backup_interval"));
    CHECK_FALSE(is_denied_key("filament_colors")); // "file" fragment must not catch filament
}

TEST_CASE("sensitive-looking values are refused under any key", "[settings-sync][deny-list]")
{
    CHECK(looks_sensitive_value("C:\\Users\\someone\\Documents"));
    CHECK(looks_sensitive_value("D:/models"));
    CHECK(looks_sensitive_value("\\\\nas\\share"));
    CHECK(looks_sensitive_value("/home/someone/models"));
    CHECK(looks_sensitive_value("https://homeassistant.local:8123"));
    CHECK(looks_sensitive_value("eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0In0.abc"));
    CHECK(looks_sensitive_value("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6")); // long mixed token
    CHECK_FALSE(looks_sensitive_value("1"));
    CHECK_FALSE(looks_sensitive_value("compact"));
    CHECK_FALSE(looks_sensitive_value("#146c2e"));
    CHECK_FALSE(looks_sensitive_value("Roboto"));
    CHECK_FALSE(looks_sensitive_value("My Slicer 2"));
    CHECK_FALSE(looks_sensitive_value("/")); // a lone slash is not a path
}

TEST_CASE("publishable settings filter the config root and sections", "[settings-sync]")
{
    std::map<std::string, std::string> root = {
        {"language", "en"},
        {"dark_color_mode", "1"},
        {"ha_token", "SECRETSECRET"},
        {"ha_url", "https://ha.local"},
        {"last_export_path", "C:\\out"},
        {"ui_font_family", "Roboto"},
        {"Weird Key!", "value"},
    };
    std::map<std::string, std::map<std::string, std::string>> sections = {
        {"printers", {{"serial", "01P00A"}}},
        {"app_settings", {{"narrator_enabled", "true"}, {"token", "no"}}},
    };
    auto out = publishable_settings(root, sections);
    CHECK(out.count("language") == 1);
    CHECK(out.count("dark_color_mode") == 1);
    CHECK(out.count("ui_font_family") == 1);
    CHECK(out.count("weird_key") == 1);
    CHECK(out.count("ha_token") == 0);
    CHECK(out.count("ha_url") == 0);
    CHECK(out.count("last_export_path") == 0);
    CHECK(out.count("app_settings_narrator_enabled") == 1);
    CHECK(out.count("app_settings_token") == 0);
    for (const auto &[key, value] : out) {
        CHECK(value.find("01P00A") == std::string::npos);
        CHECK(value.find("SECRET") == std::string::npos);
    }
}

TEST_CASE("attribute values are bounded", "[settings-sync]")
{
    std::map<std::string, std::string> root = {{"app_display_name", std::string(600, 'x')}};
    auto out = publishable_settings(root);
    REQUIRE(out.count("app_display_name") == 1);
    CHECK(out["app_display_name"].size() == kMaxAttributeValueBytes);
    std::map<std::string, std::string> many;
    for (int i = 0; i < 400; ++i) many["setting_" + std::to_string(i)] = "v";
    CHECK(publishable_settings(many).size() == kMaxAttributes);
}

TEST_CASE("state updates carry the aggregate sensor plus headline sensors", "[settings-sync]")
{
    std::map<std::string, std::string> root = {
        {"language", "yue_HK"}, {"dark_color_mode", "1"}, {"ui_density", "compact"}, {"ha_token", "SECRET"}};
    auto settings = publishable_settings(root);
    auto updates  = build_state_updates(settings, "2026-09-08T21:00:00-04:00", "2.3.0");
    REQUIRE(updates.size() == 1 + headline_keys().size());
    CHECK(updates[0].entity_id == kAggregateEntityId);
    nlohmann::json agg = nlohmann::json::parse(updates[0].body);
    CHECK(agg["state"] == "2026-09-08T21:00:00-04:00");
    CHECK(agg["attributes"]["language"] == "yue_HK");
    CHECK(agg["attributes"]["setting_count"] == 3);
    CHECK(agg["attributes"].count("ha_token") == 0);
    CHECK(agg["attributes"]["app_version"] == "2.3.0");
    bool found_language = false;
    for (const auto &u : updates) {
        CHECK(payload_is_clean(u.body));
        CHECK(u.body.find("SECRET") == std::string::npos);
        if (u.entity_id == "sensor.bambustudio_language") {
            found_language = true;
            nlohmann::json j = nlohmann::json::parse(u.body);
            CHECK(j["state"] == "yue_HK");
            CHECK(j["attributes"]["setting_key"] == "language");
        }
        if (u.entity_id == "sensor.bambustudio_ui_font_scale") {
            nlohmann::json j = nlohmann::json::parse(u.body);
            CHECK(j["state"] == "unknown"); // absent settings are reported as unknown, not invented
        }
    }
    CHECK(found_language);
}

TEST_CASE("payload cleanliness guard catches a denied key or a sensitive value", "[settings-sync][deny-list]")
{
    CHECK(payload_is_clean(R"({"state": "x", "attributes": {"friendly_name": "a", "language": "en"}})"));
    CHECK_FALSE(payload_is_clean(R"({"state": "x", "attributes": {"ha_token": "abc"}})"));
    CHECK_FALSE(payload_is_clean(R"({"state": "x", "attributes": {"note": "C:\\secret\\place"}})"));
    CHECK_FALSE(payload_is_clean(R"({"state": "x"})"));
    CHECK_FALSE(payload_is_clean("nope"));
}
