#include "slic3r/GUI/SliceMotionDrawing.hpp"
#include <array>
#include <stdexcept>
#include <iostream>
#include <vector>

using namespace Slic3r::GUI;
void require(bool condition)
{
    if (!condition) throw std::runtime_error("Slice motion frame invariant failed");
}

using Stroke = std::array<double,5>;
struct Segment { double x1,y1,z1,x2,y2,z2; };
struct Snapshot { std::vector<Segment> segments; };

std::vector<Stroke> frame(SliceMotionKind state,double seconds,bool reduced)
{
    std::vector<Stroke> result;
    draw_slice_motion(state,seconds,reduced,
        [&](double x,double y,double xx,double yy,int weight){result.push_back({x,y,xx,yy,double(weight)});},
        [&](double x,double y,double rx,double ry){result.push_back({x,y,rx,ry,-1});});
    return result;
}

std::vector<Stroke> projection(const Snapshot &fixture,double time,bool reduced)
{
    std::vector<Stroke> result;
    draw_slice_paths(project_slice_paths(fixture),time,reduced,[&](double x,double y,double xx,double yy,bool head){
        require(std::isfinite(x) && std::isfinite(y) && std::isfinite(xx) && std::isfinite(yy));
        result.push_back({x,y,xx,yy,head?1.:0.});
    });
    return result;
}

int main()
{
    const auto cases = {SliceMotionKind::Required,SliceMotionKind::DialogRebuild,SliceMotionKind::Ordinary};
    for (auto state:cases) {
        require(frame(state,.1,true)==frame(state,1.2,true));
        require(frame(state,.1,false)!=frame(state,1.2,false));
        for (auto other:cases)
            if (state!=other) require(frame(state,.1,true)!=frame(other,.1,true));
    }
    // Per-object order resets Z and jumps across a travel gap. Every stroke has its own endpoints.
    Snapshot sequential{{{0,0,.2,10,0,.2},{10,0,.7,10,10,.7},{40,20,.2,40,26,.2}}};
    Snapshot translated_rotated{{{80,60,.2,80,65,.2},{80,65,.8,70,65,.8}}};
    require(projection(sequential,1,true)!=projection(translated_rotated,1,true));
    for (auto &fixture:{sequential,translated_rotated}) {
        require(projection(fixture,0,false).size()==1);
        require(projection(fixture,6,false)==projection(fixture,0,true));
        require(projection(fixture,3,true)==projection(fixture,6,true));
        for (auto s:cases) require(!frame(s,.5,false).empty());
    }
    require(projection({},1,true).empty());
    std::cout<<"Three moving and three static compositions passed; two distinct ordered geometry fixtures and empty-path handling passed.\n";
}
