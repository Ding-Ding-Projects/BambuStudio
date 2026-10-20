#include "PersonalVocabularySchema.hpp"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <set>

namespace Slic3r::GUI::PersonalVocabulary {
namespace {
// Thrown from the parser callback to stop at the first structural problem.
struct Stop {
    Rejection reason;
};

// Calls visit(code_point) for each code point of valid UTF-8 text.
template<class Visit> void for_each_code_point(std::string_view text, Visit visit)
{
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        const std::size_t length = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
        char32_t code_point = length == 1 ? lead : length == 2 ? (lead & 0x1Fu) : length == 3 ? (lead & 0x0Fu) : (lead & 0x07u);
        for (std::size_t k = 1; k < length && i + k < text.size(); ++k)
            code_point = (code_point << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
        visit(code_point);
        i += length;
    }
}

bool unsafe_code_point(char32_t c)
{
    return c < 0x20 || c == 0x7F || (c >= 0x80 && c <= 0x9F) || // C0, DEL and C1 controls
        (c >= 0x202A && c <= 0x202E) ||                           // embeddings and overrides
        (c >= 0x2066 && c <= 0x2069);                             // isolates
}

// Unicode White_Space characters that are not already rejected as controls.
bool white_space(char32_t c)
{
    return c == 0x20 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 ||
        c == 0x202F || c == 0x205F || c == 0x3000;
}

bool safe_text(std::string_view text)
{
    if (text == "__proto__" || text == "constructor" || text == "prototype") return false;
    bool unsafe = false, content = false;
    for_each_code_point(text, [&](char32_t c) {
        if (unsafe_code_point(c)) unsafe = true;
        else if (!white_space(c)) content = true;
    });
    return content && !unsafe;
}
}

std::size_t code_points(std::string_view utf8)
{
    std::size_t count = 0;
    for (const char byte : utf8)
        if ((static_cast<unsigned char>(byte) & 0xC0u) != 0x80u) ++count;
    return count;
}

Rejection validate(std::string_view bytes, TextEntries &result)
{
    if (bytes.size() > max_file_bytes) return Rejection::TooLarge;
    if (bytes.empty()) return Rejection::Malformed;

    // Pass 1: the whole payload must be well-formed UTF-8 JSON within the depth
    // limit and without a repeated key in any object. The parser rejects
    // comments, trailing content and invalid UTF-8.
    nlohmann::json root;
    try {
        std::vector<std::set<std::string>> object_keys;
        auto callback = [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json &value) {
            using Event = nlohmann::json::parse_event_t;
            // depth counts the containers around this one, so it nests depth + 1 deep.
            if ((event == Event::object_start || event == Event::array_start) &&
                static_cast<std::size_t>(depth) + 1 > max_depth)
                throw Stop{Rejection::TooDeep};
            if (event == Event::object_start) object_keys.emplace_back();
            else if (event == Event::key) {
                if (object_keys.empty() || !object_keys.back().insert(value.get<std::string>()).second)
                    throw Stop{Rejection::DuplicateKey};
            } else if (event == Event::object_end && !object_keys.empty())
                object_keys.pop_back();
            return true;
        };
        root = nlohmann::json::parse(bytes.begin(), bytes.end(), callback);
    } catch (const Stop &stop) {
        return stop.reason;
    } catch (...) {
        return Rejection::Malformed;
    }

    // Pass 2: the version 1 schema. The version is checked first because a
    // future version may legitimately carry other fields.
    if (!root.is_object()) return Rejection::UnexpectedField;
    const auto version = root.find("schemaVersion");
    if (version == root.end() || !version->is_number_unsigned() ||
        version->get<std::uint64_t>() != static_cast<std::uint64_t>(schema_version))
        return Rejection::UnsupportedVersion;
    const auto entries = root.find("entries");
    if (root.size() != 2 || entries == root.end() || !entries->is_object()) return Rejection::UnexpectedField;

    TextEntries next;
    next.reserve(entries->size());
    for (const auto &item : entries->items()) {
        const std::string &key = item.key();
        if (code_points(key) > max_key_code_points) return Rejection::KeyTooLong;
        if (!safe_text(key)) return Rejection::UnsafeText;
        if (!item.value().is_string()) return Rejection::NonStringValue;
        const auto &value = item.value().get_ref<const std::string &>();
        if (code_points(value) > max_value_code_points) return Rejection::ValueTooLong;
        if (!safe_text(value)) return Rejection::UnsafeText;
        next.emplace_back(key, value);
    }
    result = std::move(next);
    return Rejection::None;
}

const char *rejection_name(Rejection reason)
{
    switch (reason) {
    case Rejection::None: return "none";
    case Rejection::Unreadable: return "unreadable";
    case Rejection::TooLarge: return "too-large";
    case Rejection::Malformed: return "malformed";
    case Rejection::TooDeep: return "too-deep";
    case Rejection::DuplicateKey: return "duplicate-key";
    case Rejection::UnsupportedVersion: return "unsupported-version";
    case Rejection::UnexpectedField: return "unexpected-field";
    case Rejection::NonStringValue: return "non-string-value";
    case Rejection::KeyTooLong: return "key-too-long";
    case Rejection::ValueTooLong: return "value-too-long";
    case Rejection::UnsafeText: return "unsafe-text";
    case Rejection::CacheUnavailable: return "cache-unavailable";
    }
    return "unknown";
}
}
