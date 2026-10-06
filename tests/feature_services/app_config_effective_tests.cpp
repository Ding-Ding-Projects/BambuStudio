#include "libslic3r/AppConfig.hpp"
#include <iostream>
#include <stdexcept>
int main()
{
    Slic3r::AppConfig config;
    config.set("language", "yue_HK");
    config.set("narrator_enabled", "false");
    const auto stored = config.storage();
    const auto dirty = config.dirty();
    unsigned saves = 0;
    Slic3r::AppConfig::set_save_observer([&] { ++saves; });
    auto check = [](bool value) { if (!value) throw std::runtime_error("AppConfig effective preference assertion failed"); };
    check(config.replace_effective_preferences({{"language", std::string("en")}, {"narrator_enabled", true}}));
    check(config.get("language") == "en" && config.get_base("language") == "yue_HK");
    check(config.storage() == stored && config.dirty() == dirty && saves == 0);
    check(!config.replace_effective_preferences({{"access_token", std::string("test-only")}}));
    config.suppress_presentation(true);
    config.clear_effective_preferences();
    check(config.get("language") == "en");
    config.suppress_presentation(false);
    check(config.get("language") == "yue_HK" && config.get("narrator_enabled") == "false");
    check(config.storage() == stored && config.dirty() == dirty && saves == 0);
    Slic3r::AppConfig::set_save_observer({});
    std::cout << "PASS 7 AppConfig effective/base integration assertions\n";
}
