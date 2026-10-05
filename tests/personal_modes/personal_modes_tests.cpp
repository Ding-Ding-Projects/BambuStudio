#include "slic3r/GUI/PersonalModes/AttentionModes.hpp"
#include "slic3r/GUI/PersonalModes/SpeechQueue.hpp"
#include "slic3r/GUI/PersonalModes/SapiVoice.hpp"
#include "slic3r/GUI/PersonalModes/SchoolStore.hpp"
#include <iostream>
#include <stdexcept>

using namespace Slic3r::GUI::PersonalModes;
int checks = 0;
void check(bool ok, const char* name) { ++checks; if (!ok) throw std::runtime_error(name); }

void attention() {
    auto now = AttentionModes::Time{};
    bool focus = true, quiet = true, clock = true, action = true, prompt = true, persist = true;
    std::string next;
    std::chrono::seconds elapsed{}, idle{};
    int prompts = 0;
    AttentionModes modes({[&](bool x) { focus = x; }, [&](bool x) { quiet = x; },
        [&](bool x, auto e, auto i) { clock = x; elapsed = e; idle = i; },
        [&](bool x, const std::string& n) { action = x; next = n; },
        [&](bool x, auto) { prompt = x; if (x) ++prompts; },
        [&](const auto&) { return persist; }}, {}, now);
    check(!focus && !quiet && !clock && !action && !prompt, "all accommodations start off");
    auto settings = modes.settings(); settings.focus = true;
    check(modes.configure(settings, now) && focus && !quiet, "independent focus callback");
    settings.low_stimulation = true; settings.time_awareness = true;
    settings.one_thing = true; settings.next_action = "Review selected object"; settings.momentum = true;
    check(modes.configure(settings, now) && quiet && clock && action && next == settings.next_action, "all callbacks applied");
    modes.tick(now + std::chrono::minutes(20));
    check(prompt && prompts == 1 && elapsed.count() == 1200 && idle.count() == 1200, "elapsed and momentum deadline");
    modes.tick(now + std::chrono::minutes(21)); check(prompts == 1, "no repeated prompt");
    modes.dismiss_momentum(now + std::chrono::minutes(21)); check(!prompt, "dismiss removes prompt");
    modes.tick(now + std::chrono::minutes(50)); check(prompts == 1, "snooze respected");
    modes.tick(now + std::chrono::minutes(51)); check(prompts == 2, "prompt after stated snooze");
    modes.activity(now + std::chrono::minutes(52)); check(!prompt, "activity removes prompt");
    settings.low_stimulation = false; modes.configure(settings, now);
    modes.platform_reduced_motion(true, now); check(quiet && modes.reduce_motion(), "platform motion preference preserved");
    persist = false; settings.focus = false;
    check(!modes.configure(settings, now) && modes.settings().focus, "failed persistence leaves original state");
    AttentionModes restored({}, modes.settings(), now);
    check(restored.settings().next_action == "Review selected object", "saved next action restores");
}

void speech() {
    SpeechQueue queue;
    auto now = SpeechQueue::Clock::time_point{};
    check(queue.enqueue({{{L"English", SpeechLanguage::English}, {L"Cantonese", SpeechLanguage::Cantonese}}, "state"}, now), "bilingual queued");
    auto track = queue.next(true, true, false, now);
    check(track && track->text == L"English", "English first");
    check(queue.enqueue({{{L"Error", SpeechLanguage::English}}, "error"}, now), "error queued");
    check(!queue.next(false, true, false, now), "long speech never interrupted");
    track = queue.next(true, true, false, now);
    check(track && track->text == L"Cantonese", "paired track remains consecutive");
    check(!queue.next(false, true, false, now), "Cantonese completion required");
    track = queue.next(true, true, false, now);
    check(track && track->text == L"Error", "error follows complete pair");
    check(!queue.enqueue({{{L"Too soon"}}, "state"}, now), "ordinary cooldown");
    check(queue.enqueue({{{L"New error"}}, "error"}, now), "error bypasses cooldown");
    check(!queue.next(true, true, true, now), "quiet prevents next start");
    queue.cancel();
    queue.enqueue({{{L"Old"}}, "scan"}, now); queue.enqueue({{{L"New"}}, "scan"}, now);
    track = queue.next(true, true, false, now);
    check(track && track->text == L"New", "superseded queued line replaced");
    queue.cancel(); queue.enqueue({{{L"Ordinary"}}, "state"}, now + std::chrono::minutes(1));
    check(!queue.next(true, false, false, now), "disabled narrator drops ordinary work");
    queue.enqueue({{{L"Preview"}}, "preview", true}, now);
    check(queue.next(true, false, false, now).has_value(), "explicit preview consent respected");
    check(speech_xml(L"<tag>&\"'", 99) == L"<pitch absmiddle=\"10\">&lt;tag&gt;&amp;&quot;&apos;</pitch>", "speech XML cannot inject markup");
    SapiVoice backend;
    std::vector<VoiceInfo> voices{{L"one", L"English A", true, false}, {L"two", L"Cantonese A", false, true}};
    auto chosen = backend.resolve(voices, true, L"two");
    check(chosen.available && !chosen.selected_missing && chosen.effective_id == L"two", "stable identity resolved");
    chosen = backend.resolve(voices, true, L"uninstalled");
    check(chosen.available && chosen.selected_missing && chosen.effective_id == L"two", "missing selection falls back without overwrite");
    check(!backend.resolve({}, true, L"two").available, "empty enumeration honest");
    check(backend.resolve(voices, true, L"").available, "late populated enumeration usable");
    check(!backend.resolve({voices[0]}, true, L"").available, "no wrong-language fallback");
}

void school() {
    SchoolRead current{RecordStatus::Missing, {}};
    int notifications = 0;
    SchoolMode mode({[&] { return current; },
        [&](const SchoolRecord& candidate, std::uint64_t rev, const auto& verify) {
            if (rev != current.record.revision) return RecordStatus::Conflict;
            if (current.record.enabled && !candidate.enabled && (!verify || !verify(current.record.credential_generation))) return RecordStatus::Unavailable;
            current = {RecordStatus::Ready, candidate}; ++current.record.revision; return RecordStatus::Ready;
        }, [&](const auto&, auto) { ++notifications; }});
    check(!mode.suppressed(), "missing shared record intentionally resets off");
    PresentationPreferences base{"bilingual_en_yue_HK", 5, 4, true, true};
    check(mode.enable("generation-one") == RecordStatus::Ready && mode.suppressed(), "enable shared record");
    auto effective = mode.effective(base);
    check(effective.language == "en" && effective.english_funny == 1 && !effective.personal_vocabulary && !effective.dim_sum, "complete effective suppression");
    check(base.language == "bilingual_en_yue_HK" && base.english_funny == 5, "base preference unchanged");
    check(!mode.available(PresentationCapability::Cantonese) && mode.available(PresentationCapability::Attention), "central capability filter");
    check(mode.rename("Quiet workspace") == RecordStatus::Ready && mode.record().display_name == "Quiet workspace", "exact chosen display name");
    check(mode.disable([](const auto&) { return false; }) == RecordStatus::Unavailable && mode.suppressed(), "invalid unlock cannot disable");
    check(mode.disable([](const auto& generation) { return generation == "generation-one"; }) == RecordStatus::Ready, "generation-bound unlock");
    check(mode.effective(base).language == base.language && mode.effective(base).english_funny == 5, "prior preference restoration");
    current.status = RecordStatus::Corrupt; mode.reload();
    check(mode.suppressed() && mode.record().display_name == "Quiet workspace", "corrupt record fails closed retaining chosen name");
    current = {RecordStatus::Missing, {}}; mode.reload(); check(!mode.suppressed(), "delete is intentional reset");
    check(notifications >= 6, "change observation includes rename state and corruption");
    const std::string valid = R"({"version":1,"enabled":true,"displayName":"Quiet workspace","revision":1,"credentialGeneration":"abc","updatedAt":"2026-10-05T12:00:00Z"})";
    check(parse_school_record(valid).status == RecordStatus::Ready, "bounded shared schema valid");
    check(parse_school_record("{}").status == RecordStatus::Corrupt, "missing fields rejected");
    check(parse_school_record(std::string(4097, ' ')).status == RecordStatus::Corrupt, "size bound");
    auto duplicate = valid; duplicate.insert(1, "\"enabled\":false,");
    check(parse_school_record(duplicate).status == RecordStatus::Corrupt, "duplicate keys rejected");
    auto nested = valid; nested.replace(nested.find("\"abc\""), 5, "{\"nested\":1}");
    check(parse_school_record(nested).status == RecordStatus::Corrupt, "nested value rejected");
    check(!SchoolMode::valid_name("\n") && !SchoolMode::valid_name("   "), "invalid display names rejected");
    check(!SchoolMode::valid_name("\xE2\x80\xAEhidden") && !SchoolMode::valid_name("\xC0\x80"), "bidi and invalid UTF8 rejected");
    check(SchoolMode::valid_name("\xE5\xAE\x89\xE9\x9D\x9C"), "valid Unicode chosen name preserved");
}

void native_store() {
#ifdef _WIN32
    const auto directory = std::filesystem::temp_directory_path() / ("personal-modes-test-" + std::to_string(::GetCurrentProcessId()));
    std::filesystem::create_directory(directory);
    struct Cleanup {
        std::filesystem::path directory;
        ~Cleanup() { std::error_code ec; std::filesystem::remove(directory / "record.json", ec);
            std::filesystem::remove(directory / "school-mode-v1.lock", ec); std::filesystem::remove(directory, ec); }
    } cleanup{directory};
    SchoolStore first(directory / "record.json"), second(directory / "record.json");
    check(first.read().status == RecordStatus::Missing, "native missing record");
    SchoolRecord record; record.enabled = true; record.credential_generation = "first";
    check(first.compare_exchange(record, 0) == RecordStatus::Ready, "atomic native write");
    auto read = second.read();
    check(read.status == RecordStatus::Ready && read.record.enabled && read.record.revision == 1, "second instance sees shared write");
    check(second.compare_exchange(record, 0) == RecordStatus::Conflict, "stale writer rejected");
    record = read.record; record.enabled = false;
    check(first.compare_exchange(record, 1) == RecordStatus::Unavailable, "direct unauthenticated disable rejected");
    check(first.compare_exchange(record, 1, [](const auto&) { return false; }) == RecordStatus::Unavailable, "wrong answer rejected");
    check(first.compare_exchange(record, 1, [](const auto& generation) { return generation == "first"; }) == RecordStatus::Ready,
          "native generation-bound disable");
    HANDLE competing = ::CreateFileW((directory / "school-mode-v1.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, 0, nullptr);
    check(competing != INVALID_HANDLE_VALUE, "fixture owns shared lock");
    const auto blocked = second.compare_exchange(record, 2);
    ::CloseHandle(competing);
    check(blocked == RecordStatus::Conflict, "cross-process exclusive lock honored");
    { std::ofstream corrupt(directory / "record.json"); corrupt << "{"; }
    check(first.read().status == RecordStatus::Corrupt, "corruption reported");
    check(first.compare_exchange(record, 2) == RecordStatus::Corrupt, "corruption never overwritten");
#endif
}
int main() {
    try { attention(); speech(); school(); native_store(); std::cout << "PASS " << checks << " personal-mode assertions\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "FAIL after " << checks << ": " << e.what() << '\n'; return 1; }
}
