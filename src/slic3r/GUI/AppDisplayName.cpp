#include "AppDisplayName.hpp"

#include <cstdint>

namespace Slic3r { namespace GUI { namespace AppDisplayName {

namespace {

// Decode one UTF-8 sequence starting at `i`; returns the code point and advances
// `i`. Malformed bytes decode as U+FFFD and advance by one byte.
uint32_t decode_utf8(const std::string &s, size_t &i)
{
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t   extra = 0;
    uint32_t cp    = 0;
    if (c < 0x80) { cp = c; extra = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { ++i; return 0xFFFD; }
    for (size_t k = 1; k <= extra; ++k) {
        if (i + k >= s.size()) { ++i; return 0xFFFD; }
        const unsigned char cc = static_cast<unsigned char>(s[i + k]);
        if ((cc & 0xC0) != 0x80) { ++i; return 0xFFFD; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    i += extra + 1;
    return cp;
}

void append_utf8(std::string &out, uint32_t cp)
{
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool is_control(uint32_t cp) { return cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F); }

// Whitespace we normalise: ASCII space/tab plus NBSP and the ideographic space.
bool is_space(uint32_t cp) { return cp == 0x20 || cp == 0x09 || cp == 0xA0 || cp == 0x3000; }

} // namespace

size_t utf8_length(const std::string &utf8)
{
    size_t n = 0;
    for (size_t i = 0; i < utf8.size();) { decode_utf8(utf8, i); ++n; }
    return n;
}

bool contains_control_characters(const std::string &utf8)
{
    for (size_t i = 0; i < utf8.size();)
        if (is_control(decode_utf8(utf8, i))) return true;
    return false;
}

Validation validate(const std::string &candidate)
{
    Validation v;
    v.length = utf8_length(candidate);
    if (contains_control_characters(candidate)) { v.problem = Problem::ControlCharacters; return v; }
    // Whitespace-only counts as empty: it would render as a blank wordmark.
    bool all_space = true;
    for (size_t i = 0; i < candidate.size() && all_space;) all_space = is_space(decode_utf8(candidate, i));
    if (candidate.empty() || all_space) { v.problem = Problem::Empty; return v; }
    if (v.length > MAX_LENGTH) { v.problem = Problem::TooLong; return v; }
    return v;
}

std::string sanitize(const std::string &text)
{
    std::string out;
    bool   pending_space = false;
    size_t count         = 0;
    for (size_t i = 0; i < text.size();) {
        const uint32_t cp = decode_utf8(text, i);
        // Tab is both whitespace and a C0 control: treat it as a separator here
        // (validate() still refuses it as typed), then drop the other controls.
        if (is_space(cp)) { if (!out.empty()) pending_space = true; continue; }
        if (is_control(cp)) continue;
        if (pending_space) {
            if (count + 1 > MAX_LENGTH) break;
            out.push_back(' ');
            ++count;
            pending_space = false;
        }
        if (count + 1 > MAX_LENGTH) break;
        append_utf8(out, cp);
        ++count;
    }
    // The truncation branch above can leave a separator as the last character.
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string to_stored_value(const std::string &candidate, const std::string &shipped_name)
{
    const std::string clean = sanitize(candidate);
    return clean == shipped_name ? std::string() : clean;
}

std::string resolve(const std::string &stored, const std::string &shipped_name)
{
    return validate(stored).ok() ? stored : shipped_name;
}

Provenance provenance(const std::string &stored, const std::string &shipped_name)
{
    return (validate(stored).ok() && stored != shipped_name) ? Provenance::Stored : Provenance::Default;
}

}}} // namespace Slic3r::GUI::AppDisplayName
