#ifndef slic3r_GUI_MD3Motion_hpp_
#define slic3r_GUI_MD3Motion_hpp_

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

#include <wx/timer.h>
#include <wx/window.h>
#include <wx/weakref.h>
#include "MD3MotionPolicy.hpp"

class wxWindow;

// Material Design 3 motion system for the wx tree: duration tokens, the two
// canonical easing curves, and a small timer-driven animator.
//
// Every animation is interruptible (destroying or restarting an Anim stops
// the timer) and the whole system collapses to "jump to the end state" when
// the OS asks for reduced motion (SPI_GETCLIENTAREAANIMATION off), so motion
// never becomes an accessibility or latency problem.
namespace MD3 { namespace Motion {

// Duration tokens (ms) — md.sys.motion.duration.*
constexpr int short2  = 100; // small state changes (hover layers)
constexpr int medium1 = 250; // component-level moves (slider snap-back)
constexpr int medium2 = 300; // container transforms
constexpr int long2   = 500; // large surface transitions

// True when the saved preference or OS requests reduced motion.
bool reduced();

// Easing — md.sys.motion.easing.standard / .emphasized (decelerate flavour).
double easeStandard(double t);
double easeEmphasized(double t);

// One animation: drives `tick(eased01)` at ~60fps for `duration_ms`, then
// `tick(1.0)` and `done()`. Restarting an in-flight Anim cancels the old run
// (the previous done() is NOT fired — the new target supersedes it).
class Anim : public wxTimer
{
public:
    Anim() = default;
    ~Anim() override;
    void Stop(); // Cancels callbacks and timer, on the UI thread.

    // Optional owner binding settles hidden owners and cancels destroyed owners
    // at the next timer tick. owner_lost is resource cleanup only, never a user
    // action or completion event. Existing semantic state must change before Play.
    void Play(int duration_ms,
              std::function<void(double)> tick,
              std::function<void()> done = nullptr,
              double (*curve)(double) = &easeStandard,
              wxWindow *owner = nullptr,
              std::function<void()> owner_lost = nullptr);

    void Notify() override;

private:
    struct Run {
        std::function<void(double)> tick;
        std::function<void()> done;
        double (*curve)(double) = &easeStandard;
        std::chrono::steady_clock::time_point started;
        int duration = 1;
        uint64_t generation = 0;
        bool alive = true;
        wxWeakRef<wxWindow> owner;
        bool owner_bound = false;
        std::function<void()> owner_lost;
    };
    // Callback destruction leaves this detached state safe to inspect.
    std::shared_ptr<Run> m_run = std::make_shared<Run>();
    void Finish(const std::shared_ptr<Run>& run, uint64_t generation);
};

// Fade a window with its own HWND (dialogs, frames, transient popups) in
// from transparent over `duration_ms` via MSW layered-window alpha (no-op
// jump under reduced motion / non-MSW). The Anim is self-owned.
void FadeIn(wxWindow *window, int duration_ms = medium1);

} } // namespace MD3::Motion

#endif // slic3r_GUI_MD3Motion_hpp_
