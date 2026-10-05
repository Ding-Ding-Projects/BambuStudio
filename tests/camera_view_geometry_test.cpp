#include "../src/slic3r/GUI/CameraViewGeometry.hpp"
#include <cassert>
#include <limits>
using Slic3r::GUI::CameraViewGeometry;
static bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
int main() {
    CameraViewGeometry g; g.width=1000; g.height=1000; g.image_width=1600; g.image_height=900;
    g.clamp(); auto r=g.rect(); assert(near(r.width,1000)); assert(near(r.height,562.5)); assert(near(r.y,218.75));
    g.zoom_at(2,700,500); r=g.rect(); assert(near((700-r.x)/r.width,.7));
    g.pan(1e6,1e6); r=g.rect(); assert(near(r.x,0)); assert(near(r.y,0));
    g.pan(-2e6,-2e6); r=g.rect(); assert(near(r.x+r.width,1000)); assert(near(r.y+r.height,1000));
    g.zoom_at(1,300,300); assert(near(g.view.center_x,.5)); assert(near(g.view.center_y,.5));
    g.zoom_at(99,500,500); assert(near(g.view.zoom,5));
    g.view.zoom=std::numeric_limits<double>::quiet_NaN();g.view.center_x=std::numeric_limits<double>::infinity();g.clamp();assert(near(g.view.zoom,1));assert(near(g.view.center_x,.5));
    g.width=0;g.zoom_at(2,0,0);assert(near(g.view.zoom,2));assert(near(g.rect().width,0));
    g.width=1000;g.height=200;g.view={2,.5,.9};g.clamp();assert(near(g.view.center_x,.5));
}
