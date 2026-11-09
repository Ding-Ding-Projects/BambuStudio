#include "SnapGrid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace Slic3r::GUI::SnapGrid {

bool on_from(std::string_view saved) { return saved == "1"; }

double step_from(std::string_view saved)
{
    if (saved.empty()) return kDefaultStep;
    const std::string text(saved);
    char             *end   = nullptr;
    const double      value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || !std::isfinite(value) || value <= 0) return kDefaultStep;
    return std::clamp(value, kMinStep, kMaxStep);
}

std::string step_text(double step)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", std::clamp(step, kMinStep, kMaxStep));
    return text;
}

Move snap(double origin_x, double origin_y, Move move, double step)
{
    if (!(step > 0) || !std::isfinite(step)) return move;
#ifdef SLIC3R_SNAP_GRID_NEGATIVE_FIXTURE
    // Test-only mutation (tests/snap_grid.test.mjs): rounds the move, not where it lands.
    (void) origin_x;
    (void) origin_y;
    return Move{std::round(move.x / step) * step, std::round(move.y / step) * step};
#else
    const double x = std::round((origin_x + move.x) / step) * step;
    const double y = std::round((origin_y + move.y) / step) * step;
    return Move{x - origin_x, y - origin_y};
#endif
}

} // namespace Slic3r::GUI::SnapGrid
