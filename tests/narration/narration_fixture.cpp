// Standard-library-only checks for the narration logic headers. No wxWidgets, no SAPI.
#include "src/slic3r/GUI/Narration/NarrationEvents.hpp"
#include "src/slic3r/GUI/Narration/NarrationTransitions.hpp"
#include "src/slic3r/GUI/Narration/NarrationPolicy.hpp"
#include "src/slic3r/GUI/Narration/VoiceCatalog.hpp"
#include "src/slic3r/GUI/PersonalModes/SpeechQueue.hpp"

#include <cstdio>
#include <cstring>

using namespace Slic3r::GUI::Narration;
namespace PM = Slic3r::GUI::PersonalModes;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)

static PrinterSnapshot snap(const char* status, int stage = -1, int error = 0, const char* machine = "m1")
{
    PrinterSnapshot s; s.machine = machine; s.status = status; s.stage = stage; s.error = error; return s;
}
static bool has(const std::vector<Event>& v, EventKind k) { for (auto& e : v) if (e.kind == k) return true; return false; }

static void events_and_tone()
{
    CHECK(tone_for_level(-3) == Tone::Serious && tone_for_level(1) == Tone::Serious && tone_for_level(2) == Tone::Serious);
    CHECK(tone_for_level(3) == Tone::Friendly);
    CHECK(tone_for_level(4) == Tone::Playful && tone_for_level(5) == Tone::Playful && tone_for_level(99) == Tone::Playful);
    // Every kind has three non-empty, distinct-or-equal rows, and no row mentions a path or a user.
    for (int k = 0; k < kEventKindCount; ++k)
        for (int t = 0; t < 3; ++t) {
            const char* m = utterance_msgid(static_cast<EventKind>(k), static_cast<Tone>(t));
            CHECK(m != nullptr && m[0] != 0);
            CHECK(std::strchr(m, '\\') == nullptr && std::strchr(m, '/') == nullptr && std::strstr(m, "@") == nullptr);
        }
    // Facts survive every tone: the failure variants always say what to do.
    CHECK(std::strstr(utterance_msgid(EventKind::FilamentRunout, Tone::Playful), "filament") != nullptr);
    CHECK(std::strstr(utterance_msgid(EventKind::PrinterError, Tone::Playful), "%06X") != nullptr);
    CHECK(std::strstr(utterance_msgid(EventKind::SliceFailed, Tone::Playful), "failed") != nullptr);
    CHECK(has_code(EventKind::PrinterError) && !has_code(EventKind::AmsError));
    CHECK(track_of(EventKind::PrintStarted) == Track::Printer && track_of(EventKind::SliceFinished) == Track::App &&
          track_of(EventKind::VoicePreview) == Track::Accessibility);
    CHECK(is_error(EventKind::SliceFailed) && !is_error(EventKind::PrintFinished));
}

static void transitions()
{
    PrinterTransitionDetector d;
    CHECK(d.observe(snap("FINISH", -1, 0)).empty());              // seed: old finished job stays silent
    CHECK(d.observe(snap("FINISH", -1, 0)).empty());              // no change, no event
    CHECK(has(d.observe(snap("RUNNING")), EventKind::PrintStarted));
    for (int i = 0; i < 5; ++i) CHECK(d.observe(snap("RUNNING")).empty()); // not every tick
    CHECK(has(d.observe(snap("PAUSE")), EventKind::PrintPaused));
    CHECK(has(d.observe(snap("RUNNING")), EventKind::PrintResumed));
    CHECK(has(d.observe(snap("FINISH")), EventKind::PrintFinished));
    CHECK(d.observe(snap("IDLE")).empty());

    // Failure with a fresh error code is spoken once, as the error, not twice.
    PrinterTransitionDetector f; f.observe(snap("RUNNING"));
    auto failed = f.observe(snap("FAILED", -1, 0x05000001));
    CHECK(failed.size() == 1 && failed[0].kind == EventKind::PrinterError && failed[0].code == 0x05000001);
    CHECK(f.observe(snap("FAILED", -1, 0x05000001)).empty());      // standing error is not repeated
    // Failure with no code.
    PrinterTransitionDetector g; g.observe(snap("RUNNING"));
    CHECK(has(g.observe(snap("FAILED")), EventKind::PrintFailed));
    // Cancellation is its own event and not an error.
    PrinterTransitionDetector c; c.observe(snap("RUNNING"));
    auto cancel = c.observe(snap("FAILED", -1, kErrorTaskCancelled));
    CHECK(cancel.size() == 1 && cancel[0].kind == EventKind::PrintCancelled);
    // AMS error uses module 0x07.
    PrinterTransitionDetector a; a.observe(snap("RUNNING"));
    CHECK(has(a.observe(snap("RUNNING", -1, 0x07008001)), EventKind::AmsError));
    // Runout: once, and the pause that follows is not announced a second time.
    PrinterTransitionDetector r; r.observe(snap("RUNNING", 0));
    auto runout = r.observe(snap("PAUSE", kStageFilamentRunout));
    CHECK(runout.size() == 1 && runout[0].kind == EventKind::FilamentRunout);
    CHECK(r.observe(snap("PAUSE", kStageFilamentRunout)).empty());
    CHECK(has(r.observe(snap("RUNNING", 0)), EventKind::PrintResumed));
    // Standing error at startup is seeded, not announced; a different printer reseeds.
    PrinterTransitionDetector s; CHECK(s.observe(snap("FAILED", -1, 0x05000001)).empty());
    CHECK(s.observe(snap("RUNNING", -1, 0, "other")).empty());
    CHECK(has(s.observe(snap("PAUSE", -1, 0, "other")), EventKind::PrintPaused));
}

static PrinterSnapshot door(int open, const char* machine = "m1")
{
    PrinterSnapshot s = snap("IDLE", -1, 0, machine); s.door_open = open; return s;
}

static void door_transitions()
{
    PrinterTransitionDetector d;
    CHECK(d.observe(door(1)).empty());                            // a door already open at startup stays silent
    CHECK(d.observe(door(1)).empty());                            // no change, no event
    auto closed = d.observe(door(0));
    CHECK(closed.size() == 1 && closed[0].kind == EventKind::DoorClosed);
    auto opened = d.observe(door(1));
    CHECK(opened.size() == 1 && opened[0].kind == EventKind::DoorOpened);
    CHECK(d.observe(door(-1)).empty());                           // the reading going away is not a door change
    CHECK(d.observe(door(0)).empty());                            // nor is it coming back
    CHECK(d.observe(door(1, "other")).empty());                   // another printer reseeds
    CHECK(track_of(EventKind::DoorOpened) == Track::Printer && !is_error(EventKind::DoorClosed) &&
          !has_code(EventKind::DoorOpened));
    CHECK(std::strstr(utterance_msgid(EventKind::DoorOpened, Tone::Playful), "door") != nullptr);
    CHECK(std::strstr(utterance_msgid(EventKind::DoorClosed, Tone::Serious), "closed") != nullptr);
}

static void dedupe()
{
    EventDeduper dd(std::chrono::seconds(30));
    const auto t0 = EventDeduper::Clock::now();
    CHECK(dd.admit({EventKind::PrintPaused, 0}, t0));
    CHECK(!dd.admit({EventKind::PrintPaused, 0}, t0 + std::chrono::seconds(10)));
    CHECK(dd.admit({EventKind::PrintResumed, 0}, t0 + std::chrono::seconds(10)));
    CHECK(dd.admit({EventKind::PrinterError, 1}, t0)); CHECK(dd.admit({EventKind::PrinterError, 2}, t0));
    CHECK(dd.admit({EventKind::PrintPaused, 0}, t0 + std::chrono::seconds(31)));
}

static void policy()
{
    CHECK(parse_clock("22:00") == 1320 && parse_clock("00:00") == 0 && parse_clock("23:59") == 1439);
    CHECK(!parse_clock("24:00") && !parse_clock("7:00") && !parse_clock("07:60") && !parse_clock("0700") && !parse_clock("ab:cd"));
    CHECK(parse_bool("true", false) && !parse_bool("false", true) && parse_bool("junk", true) && !parse_bool("", false));
    QuietHours night{true, 22 * 60, 7 * 60};
    CHECK(night.contains(23 * 60) && night.contains(0) && night.contains(6 * 60 + 59) && !night.contains(7 * 60) && !night.contains(12 * 60) && !night.contains(21 * 60 + 59));
    QuietHours day{true, 9 * 60, 17 * 60};
    CHECK(day.contains(9 * 60) && !day.contains(17 * 60) && !day.contains(8 * 60));
    QuietHours off{false, 0, 1439}, same{true, 600, 600};
    CHECK(!off.contains(100) && !same.contains(600));

    EnvironmentInput in; in.quiet_hours = night; in.minute_of_day = 12 * 60;
    CHECK(environment_state(in) == EnvironmentState::Open);
    in.minute_of_day = 23 * 60; CHECK(environment_state(in) == EnvironmentState::Held);
    in.minute_of_day = 12 * 60; in.screen_reader = true; CHECK(environment_state(in) == EnvironmentState::Held);
    in.screen_reader = false; in.manual_quiet = true; CHECK(environment_state(in) == EnvironmentState::Held);
    in.manual_quiet = false; in.notification = NotificationState::QuietTime; CHECK(environment_state(in) == EnvironmentState::ErrorsOnly);
    in.respect_focus_assist = false; CHECK(environment_state(in) == EnvironmentState::Open);
    in.respect_focus_assist = true; in.notification = NotificationState::Unknown; CHECK(environment_state(in) == EnvironmentState::Open);
    in.notification = NotificationState::AcceptsNotifications; CHECK(environment_state(in) == EnvironmentState::Open);

    AdmitInput a; a.master = true; a.track_enabled = true; a.kind = EventKind::PrintFinished;
    CHECK(admit(a));
    a.master = false; CHECK(!admit(a));
    a.master = true; a.track_enabled = false; CHECK(!admit(a));
    a.track_enabled = true; a.environment = EnvironmentState::Held; CHECK(!admit(a));
    a.kind = EventKind::PrinterError; CHECK(!admit(a));                       // errors do not break a Held environment
    a.environment = EnvironmentState::ErrorsOnly; CHECK(admit(a));            // but speak under Focus Assist
    a.kind = EventKind::PrintFinished; CHECK(!admit(a));
    AdmitInput p; p.kind = EventKind::VoicePreview; p.explicit_consent = true; p.master = false; p.track_enabled = false;
    CHECK(admit(p)); p.environment = EnvironmentState::ErrorsOnly; CHECK(admit(p));
    p.environment = EnvironmentState::Held; CHECK(!admit(p));                 // preview still yields to a screen reader
}

static PM::SpeechEvent ev(const char* track, const char* category, const wchar_t* text, bool exempt = false)
{
    PM::SpeechEvent e; e.track = track; e.category = category; e.cooldown_exempt = exempt;
    e.tracks.push_back({text, PM::SpeechLanguage::English}); return e;
}

static void tracks()
{
    using Clock = PM::SpeechQueue::Clock;
    const auto now = Clock::now();
    PM::SpeechQueue q;
    // The same category on two tracks never replaces the other.
    CHECK(q.enqueue(ev("printer", "state", L"printer one"), now));
    CHECK(q.enqueue(ev("app", "state", L"app one"), now));
    CHECK(q.pending() == 2);
    // A newer event on the same track replaces its older sibling only.
    CHECK(q.enqueue(ev("printer", "state", L"printer two", true), now));
    CHECK(q.pending() == 2);
    auto first = q.next(true, true, false, now);
    CHECK(first && first->text == L"printer two");
    auto second = q.next(true, true, false, now);
    CHECK(second && second->text == L"app one");
    // Cooldown is per track: printer is cooling down, app of a different category is not.
    CHECK(!q.enqueue(ev("printer", "state", L"too soon"), now + std::chrono::seconds(1)));
    CHECK(q.enqueue(ev("printer", "state", L"exempt", true), now + std::chrono::seconds(1)));
    CHECK(q.enqueue(ev("accessibility", "state", L"other track"), now + std::chrono::seconds(1)));
    // Errors keep priority and survive drop_non_errors.
    PM::SpeechQueue e;
    e.enqueue(ev("app", "state", L"plain"), now);
    e.enqueue(ev("printer", "error", L"boom"), now);
    CHECK(e.pending() == 2);
    e.drop_non_errors();
    CHECK(e.pending() == 1);
    auto n = e.next(true, true, false, now);
    CHECK(n && n->text == L"boom");
}

static void voices()
{
    auto f = classify_language(L"409;9"); CHECK(f.english && !f.cantonese);
    f = classify_language(L"C04"); CHECK(!f.english && f.cantonese);
    f = classify_language(L"zh-HK"); CHECK(f.cantonese);
    f = classify_language(L"yue-HK"); CHECK(f.cantonese);
    f = classify_language(L"en-GB"); CHECK(f.english && !f.cantonese);
    f = classify_language(L"zh-CN;404"); CHECK(!f.english && !f.cantonese);
    f = classify_language(L""); CHECK(!f.english && !f.cantonese);
    f = classify_language(L"engine"); CHECK(!f.english);
    CHECK(classify_network(L"Microsoft HiuGaai Online (Natural) - Chinese (Hong Kong)") == NetworkCapability::NetworkBacked);
    CHECK(classify_network(L"Microsoft David Desktop") == NetworkCapability::Unknown);

    std::vector<CatalogVoice> none;
    CHECK(merge(none, none).empty());                                   // no speech synthesis at all
    CatalogVoice cant; cant.id = L"hk1"; cant.name = L"HK Voice"; cant.cantonese = true;
    CatalogVoice eng; eng.id = L"us1"; eng.name = L"US Voice"; eng.english = true;
    std::vector<CatalogVoice> oc{cant}, sp{eng, cant};
    auto only_onecore = merge(none, oc);
    CHECK(only_onecore.size() == 1 && !only_onecore[0].speakable && cantonese_installed_but_unusable(only_onecore));
    auto both = merge(sp, oc);                              // duplicate id is not listed twice
    CHECK(both.size() == 2 && both[1].speakable && !cantonese_installed_but_unusable(both));
    std::vector<CatalogVoice> en_only{eng};
    CHECK(!cantonese_installed_but_unusable(merge(en_only, none)));        // empty first, populated later
}

int main()
{
    events_and_tone(); transitions(); door_transitions(); dedupe(); policy(); tracks(); voices();
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("narration fixture: all checks passed\n");
    return 0;
}
