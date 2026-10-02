#ifndef slic3r_GUI_CanvasMenuSearchModel_hpp_
#define slic3r_GUI_CanvasMenuSearchModel_hpp_

#include "Widgets/RegexBuilderBridgeState.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace Slic3r::GUI {

inline std::string canvas_menu_search_key(const RegexBuilderValues& values,
                                          const std::vector<std::string>& items)
{
    std::string key = std::to_string(values.pattern.size()) + ":" + values.pattern;
    key += values.regex_enabled ? "R" : "L";
    key += values.case_sensitive ? "C" : "c";
    key += values.whole_word ? "W" : "w";
    key += values.multiline ? "M" : "m";
    for (const auto& item : items) key += std::to_string(item.size()) + ":" + item;
    return key;
}

// UI-independent state shared by the canvas renderer and hosted regression tests.
// A result is usable only for the exact query, flags and ordered candidate list.
struct CanvasMenuSearchModel
{
    std::vector<bool> visible;
    std::string signature;
    std::string requested_signature;
    std::string evaluated_signature;
    bool unavailable = false;

    bool change_input(const std::string& key, size_t count)
    {
        if (key == signature) return false;
        signature = key;
        evaluated_signature.clear();
        visible.assign(count, true);
        unavailable = false;
        return true;
    }

    bool accept_result(const std::string& key, std::vector<bool> mask, bool incomplete)
    {
        if (key != signature) return false;
        unavailable = incomplete || mask.size() != visible.size();
        if (unavailable) std::fill(visible.begin(), visible.end(), true);
        else visible = std::move(mask);
        evaluated_signature = signature;
        return true;
    }

    bool needs_result() const { return evaluated_signature != signature; }
};

} // namespace Slic3r::GUI
#endif
