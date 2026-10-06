#pragma once
#include <string_view>

namespace Slic3r::GUI::FeatureServices {
// Check both discovery and activation. Retained results must not activate a
// capability which became unavailable after the result list was constructed.
inline bool presentation_setting_available(std::string_view key, bool suppressed)
{
    if (!suppressed) return true;
    return key != "language" && key != "funny_level_en" && key != "funny_level_yue" &&
        key != "personal_vocabulary" && key != "dialog_emojis" &&
        key != "narrator_voice_yue" && key != "narrator_rate_yue" && key != "narrator_pitch_yue" &&
        key.find("dim_sum") == std::string_view::npos;
}
}
