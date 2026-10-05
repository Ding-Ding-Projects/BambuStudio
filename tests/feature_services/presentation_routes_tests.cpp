#include "slic3r/GUI/FeatureServices/PresentationRoutes.hpp"
#include "slic3r/GUI/FeatureServices/PackageReceipts.hpp"
#include <iostream>

int main()
{
    using Slic3r::GUI::FeatureServices::presentation_setting_available;
    unsigned checks = 0;
    for (const auto* key : {"language", "funny_level_en", "funny_level_yue", "personal_vocabulary",
                           "dialog_emojis", "narrator_voice_yue", "narrator_rate_yue", "narrator_pitch_yue", "dim_sum_surprise"}) {
        if (presentation_setting_available(key, true)) return 1;
        ++checks;
        if (!presentation_setting_available(key, false)) return 2;
        ++checks;
    }
    for (const auto* key : {"narrator_enabled", "narrator_quiet", "narrator_voice_en", "narrator_rate_en", "narrator_pitch_en", "attention_focus"}) {
        if (!presentation_setting_available(key, true)) return 3;
        ++checks;
    }
    using Slic3r::GUI::FeatureServices::parse_worker_hash;
    const std::string valid(64, 'a');
    for (const auto& bytes : {valid, valid + "\n"}) {
        if (parse_worker_hash(bytes) != valid) return 4;
        ++checks;
    }
    for (const auto& bytes : {std::string(), valid + "\r\n", valid + "\n\n", std::string(64, 'A'), std::string(64, 'g'), std::string(63, 'a'), std::string(66, 'a')}) {
        if (!parse_worker_hash(bytes).empty()) return 5;
        ++checks;
    }
    std::cout << "PASS " << checks << " presentation route and package receipt assertions\n";
}
