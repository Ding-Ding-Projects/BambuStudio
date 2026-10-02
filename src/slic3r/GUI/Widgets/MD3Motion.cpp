#include "MD3Motion.hpp"

#include <algorithm>
#include <cmath>

#include <wx/app.h>
#include <wx/window.h>
#include <wx/weakref.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace MD3 { namespace Motion {

bool reduced()
{
#ifdef _WIN32
    BOOL animate = TRUE;
    if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0))
        return animate == FALSE;
#endif
    return false;
}

namespace {
double bezierCoordinate(double t, double a, double b, double c, double d)
{
    const double s = 1.0 - t;
    return s*s*s*a + 3.0*s*s*t*b + 3.0*s*t*t*c + t*t*t*d;
}

double bezierAtX(double x, double x0, double y0, double x1, double y1,
                 double x2, double y2, double x3, double y3)
{
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 32; ++i) {
        const double mid = (lo + hi) * 0.5;
        if (bezierCoordinate(mid, x0, x1, x2, x3) < x) lo = mid;
        else hi = mid;
    }
    return bezierCoordinate((lo + hi) * 0.5, y0, y1, y2, y3);
}
} // namespace

double easeStandard(double t)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return bezierAtX(t, 0, 0, 0.2, 0, 0, 1, 1, 1);
}

double easeEmphasized(double t)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    // Material's emphasized path uses two connected cubic segments.
    if (t <= 0.166666)
        return bezierAtX(t, 0, 0, 0.05, 0, 0.133333, 0.06, 0.166666, 0.4);
    return bezierAtX(t, 0.166666, 0.4, 0.208333, 0.82, 0.25, 1, 1, 1);
}

namespace {
constexpr int kFrameMs = 16; // ~60fps

// A fade must never be able to strand a window at alpha 0: an invisible window
// that still takes input reads to the user as "the palette will not close" or
// "the builder never opened". Entrances therefore start from a visible floor
// rather than fully transparent (imperceptible over a 100 ms fade).
constexpr int kEntranceFloorAlpha = 64; // 25%
} // namespace

Anim::~Anim()
{
    Stop();
    m_run->alive = false;
}

void Anim::Stop()
{
    wxTimer::Stop();
    ++m_run->generation;
    m_run->tick = nullptr;
    m_run->done = nullptr;
}

void Anim::Finish(const std::shared_ptr<Run>& run, uint64_t generation)
{
    wxTimer::Stop();
    auto tick = run->tick;
    auto done = run->done;
    run->tick = nullptr;
    run->done = nullptr;
    if (tick) tick(1.0);
    if (run->alive && run->generation == generation && done) done();
}

void Anim::Play(int duration_ms, std::function<void(double)> tick,
                std::function<void()> done, double (*curve)(double))
{
    Stop();
    auto run = m_run;
    const auto generation = run->generation;
    run->tick = std::move(tick);
    run->done = std::move(done);
    run->curve = curve != nullptr ? curve : &easeStandard;
    run->duration = std::max(1, duration_ms);
    run->started = std::chrono::steady_clock::now();
    if (reduced() || run->duration <= kFrameMs) {
        Finish(run, generation);
        return;
    }
    auto first_tick = run->tick;
    if (first_tick) first_tick(0.0);
    // A callback can restart or destroy the animator.
    if (!run->alive || run->generation != generation) return;
    if (!Start(kFrameMs)) Finish(run, generation);
}

void Anim::Notify()
{
    auto run = m_run;
    const auto generation = run->generation;
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - run->started).count();
    const double t = std::clamp(elapsed / run->duration, 0.0, 1.0);
    if (t >= 1.0 || reduced()) {
        Finish(run, generation);
        return;
    }
    auto tick = run->tick;
    const double eased = run->curve(t);
    if (tick) tick(eased);
}

void FadeIn(wxWindow *window, int duration_ms)
{
    if (window == nullptr)
        return;
#ifdef _WIN32
    HWND hwnd = (HWND) window->GetHWND();
    if (hwnd == nullptr)
        return;
    if (reduced()) {
        // Ensure fully opaque and untouched.
        ::SetWindowLongW(hwnd, GWL_EXSTYLE, ::GetWindowLongW(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        return;
    }
    ::SetWindowLongW(hwnd, GWL_EXSTYLE, ::GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    ::SetLayeredWindowAttributes(hwnd, 0, (BYTE) kEntranceFloorAlpha, LWA_ALPHA);
    auto *anim = new Anim();
    wxWeakRef<wxWindow> ref(window);
    anim->Play(duration_ms,
        [ref](double t) {
            if (!ref)
                return;
            HWND h = (HWND) ref->GetHWND();
            if (h == nullptr)
                return;
            const BYTE alpha = (BYTE) std::max<long>(kEntranceFloorAlpha, std::lround(255.0 * t));
            ::SetLayeredWindowAttributes(h, 0, alpha, LWA_ALPHA);
            if (t >= 1.0) // drop the layered style once opaque (avoids DWM cost)
                ::SetWindowLongW(h, GWL_EXSTYLE, ::GetWindowLongW(h, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        },
        // Deferred delete: done() can fire synchronously from inside Play()
        // (reduced motion), so the Anim must never delete itself re-entrantly.
        [anim]() { wxTheApp->CallAfter([anim]() { delete anim; }); });

    // wxTimer::Start can fail (no event loop yet, timer exhaustion). Without
    // this the window would keep the entrance alpha forever.
    if (!anim->IsRunning() && ::IsWindow(hwnd)) {
        ::SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        ::SetWindowLongW(hwnd, GWL_EXSTYLE, ::GetWindowLongW(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    }
#else
    (void) duration_ms;
#endif
}

} } // namespace MD3::Motion
