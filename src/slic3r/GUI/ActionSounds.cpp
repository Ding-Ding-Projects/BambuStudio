#include "ActionSounds.hpp"
#include "GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace Slic3r::GUI::ActionSounds {
namespace {
Preferences preferences()
{
    Preferences prefs;
    const auto* cfg = wxGetApp().app_config;
    if (!cfg) { prefs.enabled = false; return prefs; }
    prefs.enabled = Narration::parse_bool(cfg->get(enabled_key), true);
    // Low stimulation (an attention mode) keeps every action sound quiet.
    prefs.quiet = Narration::parse_bool(cfg->get(Narration::kKeyQuietToggle), false) || cfg->get("attention_low_stimulation") == "true";
    prefs.hours.enabled = Narration::parse_bool(cfg->get(Narration::kKeyQuietHours), false);
    if (auto n = Narration::parse_clock(cfg->get(Narration::kKeyQuietStart))) prefs.hours.start_minute = *n;
    if (auto n = Narration::parse_clock(cfg->get(Narration::kKeyQuietEnd))) prefs.hours.end_minute = *n;
    prefs.respect_focus = Narration::parse_bool(cfg->get(Narration::kKeyRespectFocus), true);
    return prefs;
}

int minute_now()
{
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return local.tm_hour * 60 + local.tm_min;
}
bool os_quiet()
{
#ifdef _WIN32
    QUERY_USER_NOTIFICATION_STATE state{};
    if (SUCCEEDED(SHQueryUserNotificationState(&state)))
        return state == QUNS_BUSY || state == QUNS_RUNNING_D3D_FULL_SCREEN ||
               state == QUNS_PRESENTATION_MODE || state == QUNS_QUIET_TIME || state == QUNS_NOT_PRESENT || state == QUNS_APP;
#endif
    return false;
}

#ifdef _WIN32
struct Audio {
    WAVEFORMATEX format{};
    std::vector<char> samples;
};
std::uint32_t little32(const char* p)
{
    return std::uint32_t(static_cast<unsigned char>(p[0])) |
        std::uint32_t(static_cast<unsigned char>(p[1])) << 8 |
        std::uint32_t(static_cast<unsigned char>(p[2])) << 16 |
        std::uint32_t(static_cast<unsigned char>(p[3])) << 24;
}
Audio read_cue(Cue cue)
{
    wchar_t directory[MAX_PATH]{};
    const UINT length = GetWindowsDirectoryW(directory, MAX_PATH);
    if (!length || length >= MAX_PATH) return {};
    const auto filename = cue == Cue::VocabularyApplied ? L"tada.wav" :
        cue == Cue::Rejected ? L"Windows Critical Stop.wav" : L"Windows Notify System Generic.wav";
    const auto path = std::filesystem::path(directory) / L"Media" / filename;
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    constexpr std::size_t limit = 2 * 1024 * 1024;
    std::vector<char> bytes(limit + 1);
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto count = input.gcount();
    if (input.bad() || count < 12 || static_cast<std::size_t>(count) > limit) return {};
    bytes.resize(static_cast<std::size_t>(count));
    if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4)) return {};
    Audio audio;
    for (std::size_t offset = 12; offset + 8 <= bytes.size();) {
        const std::size_t size = little32(bytes.data() + offset + 4);
        const auto start = offset + 8;
        if (size > bytes.size() - start) return {};
        if (!std::memcmp(bytes.data() + offset, "fmt ", 4)) {
            if (size < 16) return {};
            std::memcpy(&audio.format, bytes.data() + start, 16);
        } else if (!std::memcmp(bytes.data() + offset, "data", 4))
            audio.samples.assign(bytes.begin() + start, bytes.begin() + start + size);
        offset = start + size + (size & 1);
    }
    const auto& f = audio.format;
    if (f.wFormatTag != WAVE_FORMAT_PCM || f.nChannels < 1 || f.nChannels > 2 ||
        (f.wBitsPerSample != 8 && f.wBitsPerSample != 16) || f.nSamplesPerSec < 8000 || f.nSamplesPerSec > 96000 ||
        f.nBlockAlign != f.nChannels * f.wBitsPerSample / 8 ||
        f.nAvgBytesPerSec != f.nSamplesPerSec * f.nBlockAlign ||
        audio.samples.empty() || audio.samples.size() % f.nBlockAlign ||
        audio.samples.size() > std::size_t(f.nAvgBytesPerSec) * 5)
        return {};
    return audio;
}

void play_native(Cue cue, const Preferences& prefs, const std::atomic<unsigned>& generation, unsigned expected, std::mutex& submission_mutex)
{
    Audio audio = read_cue(cue);
    if (audio.samples.empty() || generation.load() != expected) return;
    HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!done) return;
    HWAVEOUT output{};
    if (waveOutOpen(&output, WAVE_MAPPER, &audio.format, reinterpret_cast<DWORD_PTR>(done), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        CloseHandle(done); return;
    }
    WAVEHDR header{};
    header.lpData = audio.samples.data();
    header.dwBufferLength = static_cast<DWORD>(audio.samples.size());
    if (waveOutPrepareHeader(output, &header, sizeof(header)) == MMSYSERR_NOERROR) {
        bool submitted = false;
        {
            // Serialize the final admission check and submission against a
            // settings change, including one arriving while the device opens.
            std::lock_guard<std::mutex> lock(submission_mutex);
            if (generation.load() == expected && admitted(prefs, minute_now(), os_quiet()))
                submitted = waveOutWrite(output, &header, sizeof(header)) == MMSYSERR_NOERROR;
        }
        if (submitted) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
            while (!(header.dwFlags & WHDR_DONE) && generation.load() == expected &&
                   admitted(prefs, minute_now(), os_quiet()) && std::chrono::steady_clock::now() < deadline)
                WaitForSingleObject(done, 25);
        }
        waveOutReset(output);
        waveOutUnprepareHeader(output, &header, sizeof(header));
    }
    waveOutClose(output);
    CloseHandle(done);
}
#else
void play_native(Cue, const Preferences&, const std::atomic<unsigned>&, unsigned, std::mutex&) {}
#endif

class Player {
public:
    ~Player() {
        { std::lock_guard<std::mutex> lock(m_mutex); m_stopping = true; ++m_generation; }
        m_ready.notify_one();
        if (m_thread.joinable()) m_thread.join();
    }
    void enqueue(Cue cue, Preferences prefs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_prefs = prefs;
        if (!admitted(prefs, minute_now(), os_quiet())) { cancel_locked(); return; }
        if (!m_thread.joinable()) m_thread = std::thread([this] { run(); });
        m_queue.append(cue);
        m_ready.notify_one();
    }
    void changed(Preferences prefs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_prefs = prefs;
        // Cancel pending/playing cues even when two enabled configurations
        // differ: no stale request may outlive an explicit settings change.
        cancel_locked();
    }
    void cancel() { std::lock_guard<std::mutex> lock(m_mutex); cancel_locked(); }
private:
    void cancel_locked() { m_queue.cancel(); ++m_generation; }
    void run() {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_stopping) {
            const auto cue = m_queue.take();
            if (!cue) { m_ready.wait(lock); continue; }
            const auto prefs = m_prefs;
            const auto generation = m_generation.load();
            lock.unlock();
            try {
                if (admitted(prefs, minute_now(), os_quiet())) play_native(*cue, prefs, m_generation, generation, m_mutex);
            } catch (...) { /* Audio is optional; never change the action result. */ }
            lock.lock();
        }
    }
    std::mutex m_mutex;
    std::condition_variable m_ready;
    CueQueue m_queue;
    Preferences m_prefs;
    std::atomic<unsigned> m_generation{0};
    bool m_stopping = false;
    std::thread m_thread;
};
Player& player() { static Player value; return value; }
} // namespace

void play(Cue cue) noexcept { try { player().enqueue(cue, preferences()); } catch (...) {} }
void settings_changed() noexcept { try { player().changed(preferences()); } catch (...) {} }
void cancel() noexcept { try { player().cancel(); } catch (...) {} }
} // namespace Slic3r::GUI::ActionSounds
