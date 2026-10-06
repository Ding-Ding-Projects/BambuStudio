#pragma once
#include <charconv>
#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Slic3r::EffectivePreferences {
enum class Kind { Boolean, Integer, Text, Choice, Color };
struct Descriptor { const char* key; Kind kind; int low = 0, high = 0; std::vector<std::string> choices; std::size_t limit = 256; };
using Value = std::variant<bool, std::int64_t, double, std::string>;
using Values = std::map<std::string, Value>;
inline const std::vector<Descriptor>& descriptors()
{
    static const std::vector<Descriptor> list{
        {"language", Kind::Choice, 0, 0, {"en", "yue_HK", "bilingual_en_yue_HK"}},
        {"dark_color_mode", Kind::Choice, 0, 0, {"0", "1"}},
        {"ui_density", Kind::Choice, 0, 0, {"comfortable", "compact"}},
        {"ui_accent_seed", Kind::Color}, {"ui_font_family", Kind::Text},
        {"ui_font_scale", Kind::Choice, 0, 0, {"0.9", "1.0", "1.15"}},
        {"motion_preference", Kind::Choice, 0, 0, {"system", "reduced"}},
        {"app_display_name", Kind::Text, 0, 0, {}, 128},
        {"funny_level_en", Kind::Integer, 1, 5}, {"funny_level_yue", Kind::Integer, 1, 5},
        {"dialog_emojis", Kind::Boolean}, {"narrator_enabled", Kind::Boolean}, {"narrator_quiet", Kind::Boolean},
        {"narrator_language", Kind::Choice, 0, 0, {"en", "yue_HK", "both"}},
        {"narrator_voice_en", Kind::Text}, {"narrator_voice_yue", Kind::Text},
        {"narrator_rate_en", Kind::Integer, -10, 10}, {"narrator_rate_yue", Kind::Integer, -10, 10},
        {"narrator_pitch_en", Kind::Integer, -10, 10}, {"narrator_pitch_yue", Kind::Integer, -10, 10}
    };
    return list;
}
inline const Descriptor* descriptor(const std::string& key) {
    for (const auto& item : descriptors()) if (key == item.key) return &item;
    return nullptr;
}
inline bool valid(const Descriptor& rule, const Value& value)
{
    if (rule.kind == Kind::Boolean) return std::holds_alternative<bool>(value);
    if (rule.kind == Kind::Integer) {
        const auto* n = std::get_if<std::int64_t>(&value);
        return n && *n >= rule.low && *n <= rule.high;
    }
    const auto* text = std::get_if<std::string>(&value);
    if (!text || text->size() > rule.limit) return false;
    for (unsigned char c : *text) if (c < 32 || c == 127) return false;
    if (rule.kind == Kind::Color) {
        if (text->size() != 7 || (*text)[0] != '#') return false;
        return text->find_first_not_of("0123456789abcdefABCDEF", 1) == std::string::npos;
    }
    if (rule.kind == Kind::Choice) {
        for (const auto& choice : rule.choices) if (*text == choice) return true;
        return false;
    }
    return true;
}
inline std::string encode(const Value& value) {
    if (const auto* b = std::get_if<bool>(&value)) return *b ? "true" : "false";
    if (const auto* n = std::get_if<std::int64_t>(&value)) return std::to_string(*n);
    if (const auto* s = std::get_if<std::string>(&value)) return *s;
    return {};
}
inline std::optional<Value> decode(const Descriptor& rule, const std::string& text) {
    Value value = text;
    if (rule.kind == Kind::Boolean) {
        if (text != "true" && text != "false" && text != "0" && text != "1") return {};
        value = text == "true" || text == "1";
    } else if (rule.kind == Kind::Integer) {
        std::int64_t n = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), n);
        if (result.ec != std::errc() || result.ptr != text.data() + text.size()) return {};
        value = n;
    }
    return valid(rule, value) ? std::optional<Value>(value) : std::nullopt;
}
class Overlay {
public:
    Overlay() = default;
    Overlay(const Overlay& other) { std::shared_lock<std::shared_mutex> lock(other.m_mutex); m_values = other.m_values; m_suppressed = other.m_suppressed; }
    Overlay& operator=(const Overlay& other) {
        if (this == &other) return *this;
        std::scoped_lock lock(m_mutex, other.m_mutex); m_values = other.m_values; m_suppressed = other.m_suppressed; return *this;
    }
    bool replace(const Values& values) {
        if (values.size() > descriptors().size()) return false;
        for (const auto& pair : values) {
            const auto* rule = descriptor(pair.first);
            if (!rule || !valid(*rule, pair.second)) return false;
        }
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_values = values;
        return true;
    }
    void clear() { std::unique_lock<std::shared_mutex> lock(m_mutex); m_values.clear(); }
    void suppress_presentation(bool value) { std::unique_lock<std::shared_mutex> lock(m_mutex); m_suppressed = value; }
    bool contains(const std::string& key) const { std::shared_lock<std::shared_mutex> lock(m_mutex); return m_values.count(key) != 0; }
    std::optional<std::string> read(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        if (m_suppressed) {
            if (key == "language" || key == "narrator_language") return "en";
            if (key == "funny_level_en" || key == "funny_level_yue") return "1";
            if (key == "dialog_emojis") return "false";
        }
        const auto found = m_values.find(key);
        if (found == m_values.end()) return {};
        return encode(found->second);
    }
private:
    mutable std::shared_mutex m_mutex;
    Values m_values;
    bool m_suppressed = false;
};
}
