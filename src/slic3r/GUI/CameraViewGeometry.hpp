#ifndef SLIC3R_CAMERA_VIEW_GEOMETRY_HPP
#define SLIC3R_CAMERA_VIEW_GEOMETRY_HPP
#include <algorithm>
#include <cmath>
namespace Slic3r { namespace GUI {
struct CameraViewState { double zoom = 1.0; double center_x = .5; double center_y = .5; };
struct CameraViewRect { double x = 0, y = 0, width = 0, height = 0; };
class CameraViewGeometry {
public:
    CameraViewState view;
    double width = 0, height = 0, image_width = 0, image_height = 0;
    bool valid() const { return width > 0 && height > 0 && image_width > 0 && image_height > 0; }
    double fit_scale() const { return valid() ? std::min(width / image_width, height / image_height) : 0; }
    static double finite_or(double value, double fallback) { return std::isfinite(value) ? value : fallback; }
    void clamp() {
        view.zoom = std::max(1.0, std::min(5.0, finite_or(view.zoom, 1.0)));
        view.center_x = finite_or(view.center_x, .5);
        view.center_y = finite_or(view.center_y, .5);
        if (!valid()) { view.center_x = std::max(0.0, std::min(1.0, view.center_x)); view.center_y = std::max(0.0, std::min(1.0, view.center_y)); return; }
        const double scale = fit_scale() * view.zoom;
        const double rx = std::min(.5, width / (2 * image_width * scale));
        const double ry = std::min(.5, height / (2 * image_height * scale));
        view.center_x = std::max(rx, std::min(1 - rx, view.center_x));
        view.center_y = std::max(ry, std::min(1 - ry, view.center_y));
    }
    CameraViewRect rect() const {
        if (!valid()) return {};
        const double w = image_width * fit_scale() * view.zoom, h = image_height * fit_scale() * view.zoom;
        return {width / 2 - view.center_x * w, height / 2 - view.center_y * h, w, h};
    }
    void zoom_at(double zoom, double x, double y) {
        clamp();
        if (!valid()) { view.zoom = zoom; clamp(); return; }
        const auto old = rect();
        // Letterbox anchors snap to the nearest image edge.
        const double u = std::max(0.0, std::min(1.0, (x - old.x) / old.width));
        const double v = std::max(0.0, std::min(1.0, (y - old.y) / old.height));
        x = old.x + u * old.width; y = old.y + v * old.height;
        view.zoom = zoom; clamp();
        const auto next = rect();
        view.center_x = u - (x - width / 2) / next.width;
        view.center_y = v - (y - height / 2) / next.height;
        clamp();
    }
    void pan(double dx, double dy) {
        if (!valid() || !std::isfinite(dx) || !std::isfinite(dy)) return;
        const auto r = rect(); view.center_x -= dx / r.width; view.center_y -= dy / r.height; clamp();
    }
};
}}
#endif
