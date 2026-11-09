#ifndef slic3r_GUI_ViewTools_SnapGrid_hpp_
#define slic3r_GUI_ViewTools_SnapGrid_hpp_

// Snap to grid in the Prepare 3D view: while it is on, dragging objects with
// the mouse moves them so the centre of what is dragged lands on the grid, in
// steps of the chosen size (X and Y; the height never changes). Alt while
// dragging moves freely. The arrow keys keep their own steps. The rules live
// here without wxWidgets (tests/snap_grid_test.cpp); GLCanvas3D applies them.

#include <string>
#include <string_view>

namespace Slic3r::GUI::SnapGrid {

constexpr const char *kOnKey   = "snap_to_grid";
constexpr const char *kStepKey = "snap_grid_step";
constexpr double      kDefaultStep = 10.0; // mm, the bed's grid
constexpr double      kMinStep     = 0.5;
constexpr double      kMaxStep     = 100.0;

// "1" is on; anything else is off.
bool        on_from(std::string_view saved);
// A saved step in millimetres, kept between kMinStep and kMaxStep; the
// default when nothing usable is saved.
double      step_from(std::string_view saved);
std::string step_text(double step);

struct Move
{
    double x = 0, y = 0;
};
// The move that puts origin + move on the grid. Without a usable step the
// move is returned as it is.
Move snap(double origin_x, double origin_y, Move move, double step);

} // namespace Slic3r::GUI::SnapGrid

#endif // slic3r_GUI_ViewTools_SnapGrid_hpp_
