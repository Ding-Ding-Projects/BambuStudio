// Runs only on a disposable hosted Windows runner, on a task-owned desktop.
// This links the production Anim implementation, not a timer model.
#include "slic3r/GUI/Widgets/MD3Motion.hpp"
#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <windows.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string preference = "system";
bool os_reduced = false;
using Clock = std::chrono::steady_clock;
int assertions = 0;
void require(bool condition) {
    ++assertions;
    if (!condition) throw std::runtime_error("motion assertion failed");
}
void pump(int milliseconds) {
    const auto until = Clock::now() + std::chrono::milliseconds(milliseconds);
    do {
        wxTheApp->Yield(true);
        wxTheApp->ProcessIdle();
        ::Sleep(1);
    } while (Clock::now() < until);
}
template<class Predicate> void until(Predicate predicate) {
    const auto limit = Clock::now() + std::chrono::seconds(2);
    while (!predicate() && Clock::now() < limit) pump(2);
    require(predicate());
}
struct Sample { double ms; double value; };
struct Trace {
    std::string name;
    Clock::time_point start = Clock::now();
    std::vector<Sample> samples;
    void tick(double value) {
        require(samples.size() < 256 && std::isfinite(value) && value >= 0 && value <= 1);
        samples.push_back({std::chrono::duration<double, std::milli>(Clock::now()-start).count(), value});
    }
};
double linear(double value) { return value; }
}

// The only injected boundary is preference acquisition. Production AppConfig
// and SystemParametersInfo integration are intentionally outside this fixture.
namespace MD3::Motion {
bool reduced() { return reduce_motion(preference, os_reduced); }
}

class MotionFixtureApp : public wxApp {
public:
    bool OnInit() override { SetExitOnFrameDelete(false); return true; }
};
wxIMPLEMENT_APP_NO_MAIN(MotionFixtureApp);

int main(int argc, char** argv)
{
    const char *hosted = std::getenv("RUNNER_ENVIRONMENT");
    if (!hosted || std::string(hosted) != "github-hosted" || argc != 3) return 2;
    const std::string receipt_path = argv[1], samples_path = argv[2];
    // Independent bound also covers a stalled wx dispatch. No child is launched.
    HANDLE finished = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!finished) return 2;
    HANDLE watchdog = ::CreateThread(nullptr, 0, [](void* event) -> DWORD {
        if (::WaitForSingleObject(static_cast<HANDLE>(event), 30000) != WAIT_OBJECT_0)
            ::TerminateProcess(::GetCurrentProcess(), 124);
        return 0;
    }, finished, 0, nullptr);
    if (!watchdog) { ::CloseHandle(finished); return 2; }

    HDESK original = ::GetThreadDesktop(::GetCurrentThreadId()); // borrowed
    const std::wstring name = L"BambuMotionFixture_" + std::to_wstring(::GetCurrentProcessId()) + L"_" + std::to_wstring(::GetTickCount64());
    HDESK desktop = ::CreateDesktopW(name.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    bool attached = desktop && ::SetThreadDesktop(desktop);
    bool wx_started = false, passed = false, restored = false, closed = false;
    int cases = 0;
    std::string phase = "initialization";
    std::vector<Trace> traces;
    try {
        require(attached);
        wx_started = wxEntryStart(argc, argv);
        require(wx_started && wxTheApp && wxTheApp->CallOnInit());
        wxGUIEventLoop loop;
        wxEventLoopActivator active(&loop);
        auto frame = std::make_unique<wxFrame>(nullptr, wxID_ANY, "Motion fixture", wxDefaultPosition, wxSize(320, 240));
        frame->Show();
        pump(20);
        require(frame->IsShownOnScreen());
        auto make_owner = [&]() { return std::make_unique<wxPanel>(frame.get()); };
        auto run_case = [&](const char* label, auto body) {
            phase = label;
            preference = "system"; os_reduced = false;
            traces.push_back(Trace{label});
            body(traces.back());
            ++cases;
            std::cout << "PASS " << label << '\n';
        };
        run_case("completion", [&](Trace& trace) {
            auto owner = make_owner(); MD3::Motion::Anim anim; int done = 0, lost = 0;
            anim.Play(120, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get(), [&] { ++lost; });
            until([&] { return done != 0; });
            require(!anim.IsRunning() && done == 1 && lost == 0 && trace.samples.size() >= 3);
            require(trace.samples.front().value == 0 && trace.samples.back().value == 1 && trace.samples.back().ms >= 100);
            for (size_t i=1; i<trace.samples.size(); ++i) require(trace.samples[i].value >= trace.samples[i-1].value);
            auto count = trace.samples.size(); pump(80); require(count == trace.samples.size() && done == 1);
        });
        run_case("explicit_stop", [&](Trace& trace) {
            auto owner = make_owner(); MD3::Motion::Anim anim; int done = 0, lost = 0;
            anim.Play(300, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get(), [&] { ++lost; });
            until([&] { return trace.samples.size() >= 2; });
            anim.Stop(); require(!anim.IsRunning());
            auto count = trace.samples.size(); pump(100); require(count == trace.samples.size() && done == 0 && lost == 0);
        });
        run_case("reversal", [&](Trace& trace) {
            auto owner = make_owner(); MD3::Motion::Anim anim; int old_done = 0, done = 0;
            double value = 0;
            anim.Play(300, [&](double t) { value = t; trace.tick(t); }, [&] { ++old_done; }, linear, owner.get());
            until([&] { return value > .1; });
            const double origin = value; const auto split = trace.samples.size();
            anim.Play(100, [&](double t) { value = origin * (1-t); trace.tick(value); }, [&] { ++done; }, linear, owner.get());
            require(value == origin);
            until([&] { return done != 0; });
            require(old_done == 0 && done == 1 && value == 0 && !anim.IsRunning());
            for (size_t i=split+1; i<trace.samples.size(); ++i) require(trace.samples[i].value <= trace.samples[i-1].value);
        });
        run_case("restart_in_initial_tick", [&](Trace& trace) {
            auto owner = make_owner(); MD3::Motion::Anim anim; int old_ticks = 0, old_done = 0, done = 0;
            anim.Play(200, [&](double) {
                ++old_ticks;
                anim.Play(100, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get());
            }, [&] { ++old_done; }, linear, owner.get());
            until([&] { return done != 0; });
            require(old_ticks == 1 && old_done == 0 && done == 1 && !anim.IsRunning());
        });
        run_case("owner_deleted_initial_tick", [&](Trace& trace) {
            auto owner = make_owner(); wxWeakRef<wxWindow> weak(owner.get()); MD3::Motion::Anim anim;
            int done = 0, lost = 0;
            anim.Play(100, [&](double t) { require(!!weak); trace.tick(t); owner.reset(); }, [&] { ++done; }, linear, owner.get(), [&] { ++lost; });
            until([&] { return lost != 0; });
            require(!weak && trace.samples.size() == 1 && done == 0 && lost == 1 && !anim.IsRunning());
            pump(80); require(trace.samples.size() == 1 && lost == 1);
        });
        run_case("owner_deleted_final_tick", [&](Trace& trace) {
            auto owner = make_owner(); wxWeakRef<wxWindow> weak(owner.get()); MD3::Motion::Anim anim;
            int done = 0, lost = 0;
            anim.Play(80, [&](double t) { require(!!weak); trace.tick(t); if (t == 1) owner.reset(); }, [&] { ++done; }, linear, owner.get(), [&] { ++lost; });
            until([&] { return lost != 0; });
            require(!weak && trace.samples.back().value == 1 && done == 0 && lost == 1 && !anim.IsRunning());
            auto count = trace.samples.size(); pump(80); require(count == trace.samples.size() && lost == 1);
        });
        run_case("owner_deleted_between_ticks", [&](Trace& trace) {
            auto owner = make_owner(); wxWeakRef<wxWindow> weak(owner.get()); MD3::Motion::Anim anim;
            int done = 0, lost = 0;
            anim.Play(200, [&](double t) { require(!!weak); trace.tick(t); }, [&] { ++done; }, linear, owner.get(), [&] { ++lost; });
            until([&] { return trace.samples.size() >= 2; });
            owner.reset(); const auto count = trace.samples.size();
            until([&] { return lost != 0; }); pump(80);
            require(count == trace.samples.size() && done == 0 && lost == 1 && !anim.IsRunning());
        });
        run_case("animator_deleted_initial_tick", [&](Trace& trace) {
            auto owner = make_owner(); auto anim = std::make_unique<MD3::Motion::Anim>(); int done = 0;
            anim->Play(100, [&](double t) { trace.tick(t); anim.reset(); }, [&] { ++done; }, linear, owner.get());
            pump(150); require(!anim && trace.samples.size() == 1 && done == 0);
        });
        run_case("animator_deleted_final_tick", [&](Trace& trace) {
            auto owner = make_owner(); auto anim = std::make_unique<MD3::Motion::Anim>(); int done = 0;
            anim->Play(80, [&](double t) { trace.tick(t); if (t == 1) anim.reset(); }, [&] { ++done; }, linear, owner.get());
            until([&] { return !anim; }); const auto count = trace.samples.size(); pump(80);
            require(count == trace.samples.size() && trace.samples.back().value == 1 && done == 0);
        });
        run_case("animator_deleted_between_ticks", [&](Trace& trace) {
            auto owner = make_owner(); auto anim = std::make_unique<MD3::Motion::Anim>(); int done = 0;
            anim->Play(200, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get());
            until([&] { return trace.samples.size() >= 2; }); anim.reset(); const auto count = trace.samples.size();
            pump(100); require(count == trace.samples.size() && done == 0);
        });
        run_case("owner_hidden", [&](Trace& trace) {
            auto owner = make_owner(); MD3::Motion::Anim anim; int done = 0, lost = 0;
            anim.Play(300, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get(), [&] { ++lost; });
            until([&] { return trace.samples.size() >= 2; }); owner->Hide();
            until([&] { return done != 0; }); const auto count = trace.samples.size(); pump(80);
            require(!anim.IsRunning() && done == 1 && lost == 0 && trace.samples.back().value == 1 && count == trace.samples.size());
        });
        run_case("reduced_preference_initial", [&](Trace& trace) {
            preference = "reduced"; auto owner = make_owner(); MD3::Motion::Anim anim; int done = 0;
            anim.Play(300, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get());
            require(!anim.IsRunning() && done == 1 && trace.samples.size() == 1 && trace.samples[0].value == 1);
            pump(80); require(done == 1 && trace.samples.size() == 1);
        });
        run_case("reduced_preference_during", [&](Trace& trace) {
            auto owner = make_owner(); MD3::Motion::Anim anim; int done = 0;
            anim.Play(300, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get());
            until([&] { return trace.samples.size() >= 2; }); const auto count = trace.samples.size(); preference = "reduced";
            until([&] { return done != 0; }); pump(80);
            require(!anim.IsRunning() && done == 1 && trace.samples.size() == count+1 && trace.samples.back().value == 1);
        });
        run_case("os_reduction_precedence", [&](Trace& trace) {
            preference = "system"; os_reduced = true; auto owner = make_owner(); MD3::Motion::Anim anim; int done = 0;
            anim.Play(300, [&](double t) { trace.tick(t); }, [&] { ++done; }, linear, owner.get());
            require(!anim.IsRunning() && done == 1 && trace.samples.size() == 1 && trace.samples[0].value == 1);
        });
        passed = cases == 14;
        phase = "teardown";
        frame.reset();
        pump(20);
    } catch (...) {
        // Only fixed phase labels leave this fixture, never exception text.
        passed = false;
    }
    if (wx_started) { wxTheApp->OnExit(); wxEntryCleanup(); }
    restored = attached && ::SetThreadDesktop(original);
    closed = desktop && ::CloseDesktop(desktop);
    std::ofstream samples(samples_path);
    samples << "case,elapsed_ms,value\n";
    for (const auto& trace : traces)
        for (const auto& sample : trace.samples) samples << trace.name << ',' << sample.ms << ',' << sample.value << '\n';
    samples.close();
    passed = passed && restored && closed && bool(samples);
    std::ofstream receipt(receipt_path);
    receipt << "{\"schema\":1,\"passed\":" << (passed ? "true" : "false")
            << ",\"cases\":" << cases << ",\"assertions\":" << assertions
            << ",\"phase\":\"" << phase << "\",\"desktopRestored\":" << (restored ? "true" : "false")
            << ",\"desktopClosed\":" << (closed ? "true" : "false") << "}\n";
    receipt.close();
    ::SetEvent(finished);
    const bool watchdog_joined = ::WaitForSingleObject(watchdog, 1000) == WAIT_OBJECT_0;
    ::CloseHandle(watchdog); ::CloseHandle(finished);
    return passed && bool(receipt) && watchdog_joined ? 0 : 1;
}
