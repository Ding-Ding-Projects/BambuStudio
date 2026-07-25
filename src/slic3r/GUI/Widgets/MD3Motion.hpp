#ifndef slic3r_GUI_MD3Motion_hpp_
#define slic3r_GUI_MD3Motion_hpp_

#include <functional>
#include <memory>

#include <wx/timer.h>

class wxWindow;
class wxTopLevelWindow;

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

// True when the OS requests reduced motion; animations then jump to 1.0.
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
    ~Anim() override { Stop(); }

    void Play(int duration_ms,
              std::function<void(double)> tick,
              std::function<void()> done = nullptr,
              double (*curve)(double) = &easeStandard);

    void Notify() override;

private:
    std::function<void(double)> m_tick;
    std::function<void()>       m_done;
    double (*m_curve)(double) { &easeStandard };
    int m_elapsed { 0 };
    int m_duration { 0 };
};

// Fade a top-level window in from transparent over `duration_ms` (no-op jump
// under reduced motion). The Anim is self-owned and destroys itself when done.
void FadeIn(wxTopLevelWindow *window, int duration_ms = medium1);

} } // namespace MD3::Motion

#endif // slic3r_GUI_MD3Motion_hpp_
