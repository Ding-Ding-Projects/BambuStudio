#include "slic3r/GUI/Appearance/StyleMetrics.hpp"
#include <stdexcept>
static void require(bool condition) { if (!condition) throw std::runtime_error("style metric invariant failed"); }
#include <limits>
using Slic3r::GUI::StyleMetrics;
int main()
{
    StyleMetrics m {200, -2, 800, 12, 99, 8};
    auto s = m.sanitized();
    require(s.radius == 64 && s.border_width == 0 && s.padding == 64);
    require(s.margin == 12 && s.letter_spacing == 20 && s.line_height == 3);
    require(s.radius_for(12, 8) == 4 && s.inset_for(12, 8) == 4);
    require(s.radius_for(-1, 8) == 0 && s.inset_for(12, -1) == 0);
    require(s.line_advance(12) == 36);
    m.line_height = std::numeric_limits<double>::quiet_NaN();
    m.padding = std::numeric_limits<double>::infinity();
    require(m.sanitized().line_height == 1 && m.sanitized().padding == 0);
    require(m.line_advance(-5) == 0);
}
