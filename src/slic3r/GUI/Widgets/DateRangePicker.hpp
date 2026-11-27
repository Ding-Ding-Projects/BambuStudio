#pragma once

#include "libslic3r/Changelog.hpp"
#include <functional>

class wxWindow;

namespace Slic3r::GUI {
// The popup owns its lifetime and reports each change to the inclusive range.
void show_date_range_picker(wxWindow *anchor, const Changelog::DateRange &range,
                            std::function<void(const Changelog::DateRange &)> on_change);
}
