#include "MD3Motion.hpp"
#include "MD3EasingMath.hpp"

#include <algorithm>
#include <cmath>

#include <wx/app.h>
#include <wx/window.h>
#include <wx/weakref.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace MD3 { namespace Motion {

double easeStandard(double t)
{
    return Math::standard(t);
}

double easeEmphasized(double t)
{
    // Material's emphasized path uses two connected cubic segments.
    return Math::emphasized(t);
}

double easeLinear(double t)
{
    return t <= 0.0 ? 0.0 : (t >= 1.0 ? 1.0 : t);
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
    m_run->owner_lost = nullptr;
}

void Anim::Finish(const std::shared_ptr<Run>& run, uint64_t generation)
{
    wxTimer::Stop();
    auto tick = run->tick;
    auto done = run->done;
    auto owner_lost = run->owner_lost;
    run->tick = nullptr;
    run->done = nullptr;
    run->owner_lost = nullptr;
    // The initial tick may destroy its owner before timer startup fails.
    // Never invoke even the final visual callback after that owner is gone.
    if (run->owner_bound && !run->owner) {
        if (owner_lost) owner_lost();
        return;
    }
    if (tick) tick(1.0);
    if (!run->alive || run->generation != generation) return;
    if (run->owner_bound && !run->owner) {
        if (owner_lost) owner_lost();
    } else if (done) done();
}

void Anim::Play(int duration_ms, std::function<void(double)> tick,
                std::function<void()> done, double (*curve)(double),
                wxWindow *owner, std::function<void()> owner_lost)
{
    Stop();
    auto run = m_run;
    const auto generation = run->generation;
    run->tick = std::move(tick);
    run->done = std::move(done);
    run->owner = owner;
    run->owner_bound = owner != nullptr;
    run->owner_lost = std::move(owner_lost);
    run->curve = curve != nullptr ? curve : &easeStandard;
    run->duration = std::max(1, duration_ms);
    run->started = std::chrono::steady_clock::now();
    if (owner_action(run->owner_bound, !!run->owner, owner && owner->IsShownOnScreen(), reduced()) == OwnerAction::Settle || run->duration <= kFrameMs) {
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
    const auto action = owner_action(run->owner_bound, !!run->owner,
        run->owner && run->owner->IsShownOnScreen(), reduced());
    if (action == OwnerAction::Cancel) {
        auto cleanup = run->owner_lost;
        Stop();
        // This callback may destroy the animator. No member access follows it.
        if (cleanup) cleanup();
        return;
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - run->started).count();
    const double t = std::clamp(elapsed / run->duration, 0.0, 1.0);
    if (t >= 1.0 || action == OwnerAction::Settle) {
        Finish(run, generation);
        return;
    }
    auto tick = run->tick;
    const double eased = run->curve(t);
    if (tick) tick(eased);
}

void FadeIn(wxWindow *window, int duration_ms)
{
    FadeIn(window, duration_ms, [](double t) { return easeStandard(t); });
}

void FadeIn(wxWindow *window, int total_ms, std::function<double(double)> progress)
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
    if (!progress)
        progress = [](double t) { return easeStandard(t); };
    ::SetWindowLongW(hwnd, GWL_EXSTYLE, ::GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    ::SetLayeredWindowAttributes(hwnd, 0, (BYTE) kEntranceFloorAlpha, LWA_ALPHA);
    auto *anim = new Anim();
    wxWeakRef<wxWindow> ref(window);
    anim->Play(total_ms,
        [ref, progress](double t) {
            if (!ref)
                return;
            HWND h = (HWND) ref->GetHWND();
            if (h == nullptr)
                return;
            // The final frame is always fully opaque, whatever the curve returns.
            const double shown = t >= 1.0 ? 1.0 : std::clamp(progress(t), 0.0, 1.0);
            const BYTE alpha = (BYTE) std::max<long>(kEntranceFloorAlpha, std::lround(255.0 * shown));
            ::SetLayeredWindowAttributes(h, 0, alpha, LWA_ALPHA);
            if (t >= 1.0) // drop the layered style once opaque (avoids DWM cost)
                ::SetWindowLongW(h, GWL_EXSTYLE, ::GetWindowLongW(h, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        },
        // Deferred delete: done() can fire synchronously from inside Play()
        // (reduced motion), so the Anim must never delete itself re-entrantly.
        [anim]() { wxTheApp->CallAfter([anim]() { delete anim; }); },
        &easeLinear, window,
        [anim]() { wxTheApp->CallAfter([anim]() { delete anim; }); });

    // wxTimer::Start can fail (no event loop yet, timer exhaustion). Without
    // this the window would keep the entrance alpha forever.
    if (!anim->IsRunning() && ::IsWindow(hwnd)) {
        ::SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        ::SetWindowLongW(hwnd, GWL_EXSTYLE, ::GetWindowLongW(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    }
#else
    (void) total_ms;
    (void) progress;
#endif
}

} } // namespace MD3::Motion
