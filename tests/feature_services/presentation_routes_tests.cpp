#include "slic3r/GUI/FeatureServices/PresentationRoutes.hpp"
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
    std::cout << "PASS " << checks << " presentation route assertions\n";
}
