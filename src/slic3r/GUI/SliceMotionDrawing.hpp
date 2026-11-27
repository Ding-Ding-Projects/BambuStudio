#pragma once
#include <algorithm>
#include <cstddef>
#include <array>
#include <vector>
#include <utility>
#include <cmath>


namespace Slic3r::GUI {
enum class SliceMotionKind { Ordinary, DialogRebuild, Required };
// Playback follows the retained G-code sequence. Never join separate movements.
// Oblique projection preserves a decline in Z when object-by-object printing starts the next part.
struct ProjectedSlicePath {
    std::vector<std::array<float,4>> edges;
};

template<class Snapshot>
ProjectedSlicePath project_slice_paths(const Snapshot &snapshot)
{
    ProjectedSlicePath path;
    if (snapshot.segments.empty()) return path;
    path.edges.reserve(snapshot.segments.size());
    const auto project = [](double x, double y, double z) {
        return std::pair<double,double>{x - y, .38 * (x + y) - z};
    };
    double minx=1e30, miny=1e30, maxx=-1e30, maxy=-1e30;
    for (const auto &s : snapshot.segments) {
        const auto a = project(s.x1,s.y1,s.z1), b = project(s.x2,s.y2,s.z2);
        for (const auto &p : {a,b}) {
            minx=std::min(minx,p.first); maxx=std::max(maxx,p.first);
            miny=std::min(miny,p.second); maxy=std::max(maxy,p.second);
        }
        path.edges.push_back({float(a.first),float(a.second),float(b.first),float(b.second)});
    }
    const double extent=std::max({maxx-minx,maxy-miny,.001});
    for (auto &edge : path.edges) {
        for (int i : {0,2}) {
            edge[i] = float(.5 + .9*(edge[i]-(minx+maxx)/2)/extent);
            edge[i+1] = float(.5 + .9*(edge[i+1]-(miny+maxy)/2)/extent);
        }
    }
    return path;
}

template<class Line>
void draw_slice_paths(const ProjectedSlicePath &path, double seconds, bool reduced, Line line)
{
    const size_t count=path.edges.size();
    const size_t visible=reduced ? count : std::min(count, size_t(1+std::fmod(seconds,7.)/5.*count));
    for (size_t i=0;i<visible;++i) {
        const auto &edge=path.edges[i];
        line(edge[0],edge[1],edge[2],edge[3],i+1==visible);
    }
}

// All coordinates are normalized to a decorative viewport. None denote tool movement.
template<class Line, class Ellipse>
void draw_slice_motion(SliceMotionKind kind, double seconds, bool reduced, Line line, Ellipse ellipse)
{
    constexpr double tau = 6.28318530718;
    if (kind == SliceMotionKind::Required) {
        const double beat = reduced ? .5 : .5 - .5 * std::cos(seconds * tau / 2.4);
        const double inset = .055 + .045 * beat;
        for (int x : {-1, 1}) for (int y : {-1, 1}) {
            const double cx = .5 + x * (.5 - inset), cy = .5 + y * (.5 - inset);
            line(cx, cy, cx - x * .17, cy, 3);
            line(cx, cy, cx, cy - y * .19, 3);
        }
    } else if (kind == SliceMotionKind::DialogRebuild) {
        const double phase = reduced ? .16 : std::fmod(seconds / 2.1, 1.);
        const double recoil = phase < .35 ? phase / .35 : (1. - phase) / .65;
        for (int direction : {-1, 1}) {
            const double x = .5 + direction * (.44 - .22 * recoil * recoil);
            line(x, .18, x - direction * .09, .5, 3);
            line(x - direction * .09, .5, x, .82, 3);
            line(x + direction * .05, .26, x - direction * .015, .5, 1);
        }
    } else {
        const double sweep = reduced ? .46 : std::fmod(seconds / 3.2, 1.);
        const double y = .81 - .6 * sweep;
        line(.17, y, .83, y, 2);
        ellipse(.5, .83, .20 + .23 * sweep, .045 + .07 * sweep);
    }
}
}
