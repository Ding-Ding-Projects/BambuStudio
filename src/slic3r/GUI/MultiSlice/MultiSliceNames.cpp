#include "MultiSliceNames.hpp"

#include <cctype>

namespace Slic3r::GUI::MultiSlice {

std::string file_safe(const std::string &ink_name)
{
    std::string out;
    bool pending_gap = false;
    for (const unsigned char c : ink_name) {
        // Non-ASCII bytes are kept whole, so an ink named in Chinese stays readable.
        const bool keep = c >= 0x80 || std::isalnum(c) || c == '-' || c == '+' || c == '.';
        if (!keep) {
            pending_gap = !out.empty();
            continue;
        }
        if (pending_gap) out += '_';
        pending_gap = false;
        out += char(c);
    }
    while (!out.empty() && out.back() == '.') out.pop_back(); // Windows drops a trailing dot
    return out.empty() ? std::string("ink") : out;
}

std::string variant_file_name(const std::string &base, const std::string &ink_name, std::set<std::string> &taken)
{
    const std::string stem = base + "_" + file_safe(ink_name);
    std::string name = stem + ".gcode.3mf";
    for (int n = 2; taken.count(name) != 0; ++n)
        name = stem + " (" + std::to_string(n) + ").gcode.3mf";
    taken.insert(name);
    return name;
}

} // namespace Slic3r::GUI::MultiSlice
