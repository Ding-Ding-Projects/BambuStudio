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

double easeStandard(double t)
{
    // cubic-bezier(0.2, 0, 0, 1) flavour: fast start, long decelerate tail.
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - t, 3.0);
}

double easeEmphasized(double t)
{
    // A stronger settle for large moves: quintic decelerate.
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - t, 5.0);
}

namespace {
constexpr int kFrameMs = 16; // ~60fps
} // namespace

void Anim::Play(int duration_ms, std::function<void(double)> tick,
                std::function<void()> done, double (*curve)(double))
{
    Stop();
    m_tick     = std::move(tick);
    m_done     = std::move(done);
    m_curve    = curve != nullptr ? curve : &easeStandard;
    m_elapsed  = 0;
    m_duration = std::max(1, duration_ms);
    if (reduced() || m_duration <= kFrameMs) {
        if (m_tick) m_tick(1.0);
        if (m_done) m_done();
        return;
    }
    if (m_tick) m_tick(0.0);
    Start(kFrameMs);
}

void Anim::Notify()
{
    m_elapsed += kFrameMs;
    const double t = double(m_elapsed) / double(m_duration);
    if (t >= 1.0) {
        Stop();
        if (m_tick) m_tick(1.0);
        if (m_done) m_done();
        return;
    }
    if (m_tick) m_tick(m_curve(t));
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
    ::SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
    auto *anim = new Anim();
    wxWeakRef<wxWindow> ref(window);
    anim->Play(duration_ms,
        [ref](double t) {
            if (!ref)
                return;
            HWND h = (HWND) ref->GetHWND();
            if (h == nullptr)
                return;
            ::SetLayeredWindowAttributes(h, 0, (BYTE) std::lround(255.0 * t), LWA_ALPHA);
            if (t >= 1.0) // drop the layered style once opaque (avoids DWM cost)
                ::SetWindowLongW(h, GWL_EXSTYLE, ::GetWindowLongW(h, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        },
        // Deferred delete: done() can fire synchronously from inside Play()
        // (reduced motion), so the Anim must never delete itself re-entrantly.
        [anim]() { wxTheApp->CallAfter([anim]() { delete anim; }); });
#else
    (void) duration_ms;
#endif
}

} } // namespace MD3::Motion
