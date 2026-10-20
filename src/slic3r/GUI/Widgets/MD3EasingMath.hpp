#ifndef slic3r_GUI_MD3EasingMath_hpp_
#define slic3r_GUI_MD3EasingMath_hpp_

// The Material easing curves as plain arithmetic, shared by the wx animator
// (MD3Motion.cpp) and the per-element motion model (Appearance/ElementMotion),
// so a curve the appearance editor names is exactly the curve the widgets play.
// No wxWidgets dependency: pure tests include it directly.

namespace MD3 { namespace Motion { namespace Math {

inline double bezier_coordinate(double t, double a, double b, double c, double d)
{
    const double s = 1.0 - t;
    return s * s * s * a + 3.0 * s * s * t * b + 3.0 * s * t * t * c + t * t * t * d;
}

// y of the cubic Bezier (x0,y0)-(x1,y1)-(x2,y2)-(x3,y3) at horizontal position
// x, found by bisection on the curve parameter. x1 and x2 inside [x0, x3] keep
// x monotonic, which every caller guarantees.
inline double bezier_at_x(double x, double x0, double y0, double x1, double y1,
                          double x2, double y2, double x3, double y3)
{
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 32; ++i) {
        const double mid = (lo + hi) * 0.5;
        if (bezier_coordinate(mid, x0, x1, x2, x3) < x) lo = mid;
        else hi = mid;
    }
    return bezier_coordinate((lo + hi) * 0.5, y0, y1, y2, y3);
}

// cubic-bezier(x1, y1, x2, y2) between (0,0) and (1,1), clamped outside [0, 1].
inline double cubic_bezier(double t, double x1, double y1, double x2, double y2)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return bezier_at_x(t, 0, 0, x1, y1, x2, y2, 1, 1);
}

// md.sys.motion.easing.standard: cubic-bezier(0.2, 0, 0, 1).
inline double standard(double t) { return cubic_bezier(t, 0.2, 0, 0, 1); }

// md.sys.motion.easing.emphasized: two connected cubic segments.
inline double emphasized(double t)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    if (t <= 0.166666)
        return bezier_at_x(t, 0, 0, 0.05, 0, 0.133333, 0.06, 0.166666, 0.4);
    return bezier_at_x(t, 0.166666, 0.4, 0.208333, 0.82, 0.25, 1, 1, 1);
}

} } } // namespace MD3::Motion::Math

#endif // slic3r_GUI_MD3EasingMath_hpp_
