// Standalone assertions for src/slic3r/GUI/ViewTools: snapping a drag to the
// grid (where the dragged centre lands, never the height, and the saved
// switch and step) and the per-object views (opacity kept visible, Wire, and
// a plain object forgotten). Compiled and run by tests/view_tools.test.mjs.
#include "src/slic3r/GUI/ViewTools/ObjectViews.cpp"
#include "src/slic3r/GUI/ViewTools/SnapGrid.cpp"

#include <cmath>
#include <cstdio>

using namespace Slic3r::GUI;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main()
{
    // Snap: the centre lands on the grid, whatever the move started from.
    SnapGrid::Move m = SnapGrid::snap(3, 7, {12.4, -1}, 10);
    CHECK(near(m.x, 17) && near(m.y, 3)); // lands on (20, 10)
    m = SnapGrid::snap(-4.5, 0, {-1, 0.2}, 10);
    CHECK(near(m.x, -5.5) && near(m.y, 0)); // lands on (-10, 0)
    m = SnapGrid::snap(-4.5, 0, {0.4, 0.2}, 10);
    CHECK(near(m.x, 4.5) && near(m.y, 0)); // lands on (0, 0)
    m = SnapGrid::snap(100, 100, {0, 0}, 10);
    CHECK(near(m.x, 0) && near(m.y, 0));  // already on the grid
    m = SnapGrid::snap(1.2, 2.6, {0.1, 0.1}, 0.5);
    CHECK(near(m.x, 1.5 - 1.2) && near(m.y, 2.5 - 2.6));
    m = SnapGrid::snap(3, 7, {12.4, -1}, 0); // no usable step: the move as it is
    CHECK(near(m.x, 12.4) && near(m.y, -1));
    m = SnapGrid::snap(3, 7, {12.4, -1}, -5);
    CHECK(near(m.x, 12.4) && near(m.y, -1));

    CHECK(SnapGrid::on_from("1") && !SnapGrid::on_from("") && !SnapGrid::on_from("0") && !SnapGrid::on_from("true"));
    CHECK(near(SnapGrid::step_from(""), SnapGrid::kDefaultStep));
    CHECK(near(SnapGrid::step_from("abc"), SnapGrid::kDefaultStep));
    CHECK(near(SnapGrid::step_from("-3"), SnapGrid::kDefaultStep));
    CHECK(near(SnapGrid::step_from("0.1"), SnapGrid::kMinStep));
    CHECK(near(SnapGrid::step_from("500"), SnapGrid::kMaxStep));
    CHECK(near(SnapGrid::step_from("2.5"), 2.5));
    CHECK(SnapGrid::step_text(5) == "5" && SnapGrid::step_text(2.5) == "2.5" && SnapGrid::step_text(1000) == "100");

    // Object views: an object never turns invisible, and a plain one is forgotten.
    ObjectViews::Views v;
    CHECK(v.look(7).plain() && v.empty());
    v.set_opacity(7, 0.8);
    CHECK(near(v.look(7).opacity, 0.8) && !v.look(7).wire && v.size() == 1);
    v.set_opacity(7, 0.0);
    CHECK(near(v.look(7).opacity, ObjectViews::kMinOpacity));
    v.set_opacity(7, 0.456);
    CHECK(near(v.look(7).opacity, 0.46));
    v.set_opacity(7, 7.0);
    CHECK(v.empty()); // fully opaque and solid: forgotten
    v.set_wire(3, true);
    CHECK(v.look(3).wire && near(v.look(3).opacity, 1.0) && v.size() == 1);
    v.set_opacity(3, 0.5);
    v.set_wire(3, false);
    CHECK(!v.look(3).wire && near(v.look(3).opacity, 0.5));
    v.set_opacity(3, 1.0);
    CHECK(v.empty());
    v.set_wire(9, true);
    v.reset(9);
    CHECK(v.empty());
    v.set_opacity(4, std::nan(""));
    CHECK(v.empty());

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("view tools: all checks passed\n");
    return 0;
}
