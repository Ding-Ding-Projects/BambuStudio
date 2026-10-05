#include "libslic3r/EffectivePreferences.hpp"
#include <iostream>
#include <stdexcept>
using namespace Slic3r::EffectivePreferences;
int main()
{
    unsigned checks = 0;
    const auto check = [&](bool good) { ++checks; if (!good) throw std::runtime_error("Effective preference assertion failed"); };
    std::map<std::string, std::string> stored{{"language", "yue_HK"}, {"narrator_enabled", "false"}};
    const auto before = stored;
    Overlay overlay;
    auto read = [&](const std::string& key) { const auto value = overlay.read(key); return value ? *value : stored.at(key); };
    check(overlay.replace({{"language", std::string("en")}, {"narrator_enabled", true}}));
    check(read("language") == "en"); check(read("narrator_enabled") == "true"); check(stored == before);
    for (const char* key : {"password", "access_token", "download_path", "school_mode", "unknown_setting"}) {
        check(!overlay.replace({{key, std::string("value")}})); check(read("language") == "en");
    }
    check(!overlay.replace({{"narrator_enabled", std::string("true")}}));
    check(!overlay.replace({{"funny_level_en", std::int64_t(6)}}));
    check(!overlay.replace({{"narrator_rate_en", std::int64_t(-11)}}));
    check(!overlay.replace({{"narrator_pitch_yue", std::int64_t(11)}}));
    check(!overlay.replace({{"ui_accent_seed", std::string("#zzzzzz")}}));
    check(!overlay.replace({{"ui_font_family", std::string("bad\nname")}}));
    check(!overlay.replace({{"app_display_name", std::string(129, 'x')}}));
    check(overlay.replace({{"language", std::string("bilingual_en_yue_HK")}, {"funny_level_en", std::int64_t(5)}, {"dialog_emojis", true}}));
    overlay.suppress_presentation(true);
    check(read("language") == "en"); check(overlay.read("funny_level_en") == "1"); check(overlay.read("dialog_emojis") == "false");
    overlay.clear(); check(read("language") == "en");
    overlay.suppress_presentation(false); check(read("language") == "yue_HK"); check(stored == before);
    check(!decode(*descriptor("funny_level_en"), "2suffix"));
    check(!decode(*descriptor("narrator_enabled"), "yes"));
    check(decode(*descriptor("narrator_enabled"), "1").has_value());
    check(decode(*descriptor("narrator_rate_en"), "-10").has_value());
    std::cout << "PASS " << checks << " effective preference assertions\n";
}
