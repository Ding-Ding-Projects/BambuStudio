#include "PersonalVocabulary.hpp"
#include "PersonalModes/SchoolMode.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <wx/app.h>
#include <wx/stdpaths.h>
#include <wx/toplevel.h>
#include <wx/window.h>
#include <wx/weakref.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Slic3r::GUI::PersonalVocabulary {
namespace {
struct Observer {
    wxWeakRef<wxWindow> window;
    std::function<void()> refresh_display;
};

// Positions of the entries whose source starts with each code unit, in mapping
// order. Display looks only at entries that can match at a position, so the
// cost of a label does not grow with the size of the vocabulary.
using FirstUnitIndex = std::unordered_map<wchar_t, std::vector<std::size_t>>;

// remember() runs inside every translation, and other files translate text in
// their static initializers while BambuStudio.dll is still loading. A container
// at namespace scope may not be constructed yet at that point (its constructor
// is a dynamic initializer that can run later), and inserting into it there
// faulted during DLL initialization. All mutable state therefore lives in one
// object created on first use. It is never destroyed, so static destructors
// that run during unload cannot reach a destroyed container either.
struct State {
    std::mutex source_mutex;
    std::set<std::wstring> sources;
    Entries entries;
    std::map<wxWindow *, Observer> observers;
    bool initialized = false;
    bool cache_loaded = false;
    FirstUnitIndex index;
    Rejection cache_rejection = Rejection::None;
};

State &state()
{
    static State *const instance = new State();
    return *instance;
}

std::filesystem::path cache_path()
{
    return std::filesystem::path(wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()) /
        "private-display" / "vocabulary.json";
}

// Reads at most one byte more than the file limit, so an oversized file is
// rejected without reading the rest of it.
Rejection read_bounded(const std::filesystem::path &path, std::string &bytes)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return Rejection::Unreadable;
    bytes.resize(max_file_bytes + 1);
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad()) return Rejection::Unreadable;
    return bytes.size() > max_file_bytes ? Rejection::TooLarge : Rejection::None;
}

FirstUnitIndex index_entries(const Entries &mapping)
{
    FirstUnitIndex index;
    for (std::size_t i = 0; i < mapping.size(); ++i)
        if (!mapping[i].first.empty()) index[mapping[i].first.front()].push_back(i);
    return index;
}

wxString apply_indexed(const wxString &source, const Entries &mapping, const FirstUnitIndex &index)
{
    const auto value = source.ToStdWstring();
    if (value.size() > 32768) return source;
    if (value.find(L"://") != std::wstring::npos || value.find(L'\\') != std::wstring::npos ||
        value.find(L'/') != std::wstring::npos || value.find(L'%') != std::wstring::npos) return source;
    auto word = [](wchar_t c) { return std::iswalnum(c) || c == L'_'; };
    std::wstring output;
    for (std::size_t i = 0; i < value.size();) {
        bool matched = false;
        if (const auto candidates = index.find(value[i]); candidates != index.end()) {
            for (const std::size_t position : candidates->second) {
                const auto &entry = mapping[position];
                const auto end = i + entry.first.size();
                if (value.compare(i, entry.first.size(), entry.first) == 0 &&
                    (i == 0 || !word(value[i-1]) || !word(entry.first.front())) &&
                    (end == value.size() || !word(value[end]) || !word(entry.first.back()))) {
                    output += entry.second;
                    i = end;
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) output += value[i++];
        if (output.size() > 65536) return source;
    }
    return wxString(output);
}

bool write_exclusive(const std::filesystem::path &path, const std::string &bytes)
{
#ifdef _WIN32
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && ::FlushFileBuffers(file);
    ::CloseHandle(file);
#else
    const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return false;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto n = ::write(file, bytes.data() + offset, bytes.size() - offset);
        if (n <= 0) break;
        offset += static_cast<std::size_t>(n);
    }
    const bool ok = offset == bytes.size() && ::fsync(file) == 0;
    ::close(file);
#endif
    if (!ok) { std::error_code ignored; std::filesystem::remove(path, ignored); }
    return ok;
}

struct PendingFile {
    std::filesystem::path path;
    ~PendingFile() { if (!path.empty()) { std::error_code ignored; std::filesystem::remove(path, ignored); } }
};
}

void observe(wxWindow *window, std::function<void()> refresh_display)
{
    if (!window) return;
    auto &observers = state().observers;
    for (auto it = observers.begin(); it != observers.end();)
        if (!it->second.window) it = observers.erase(it); else ++it;
    observers[window] = Observer{window, std::move(refresh_display)};
}

bool parse(std::string_view bytes, Entries &result, Rejection *reason)
{
    TextEntries text;
    const Rejection why = validate(bytes, text);
    if (reason) *reason = why;
    if (why != Rejection::None) return false;
    Entries next;
    next.reserve(text.size());
    for (const auto &[key, value] : text)
        next.emplace_back(wxString::FromUTF8(key).ToStdWstring(), wxString::FromUTF8(value).ToStdWstring());
    std::stable_sort(next.begin(), next.end(), [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
    result = std::move(next);
    return true;
}

void initialize()
{
    auto &s = state();
    if (s.initialized || !wxTheApp) return;
    s.initialized = true;
    // The private copy meets exactly the rules a selected file meets before it
    // is used. A copy that is unreadable, oversized, written under other rules
    // or otherwise invalid is ignored, original wording stays, and the settings
    // control reports why.
    const auto path = cache_path();
    std::error_code ec;
    const bool present = std::filesystem::exists(path, ec);
    if (!present && !ec) return;
    std::string bytes;
    Entries cached;
    Rejection reason = ec ? Rejection::Unreadable : read_bounded(path, bytes);
    if (reason == Rejection::None) parse(bytes, cached, &reason);
    if (reason != Rejection::None) {
        s.cache_rejection = reason;
        return;
    }
    s.entries = std::move(cached);
    s.index = index_entries(s.entries);
    s.cache_loaded = true;
}

bool load(const std::filesystem::path &file, Rejection *reason)
{
    initialize();
    auto reject = [reason](Rejection why) {
        if (reason) *reason = why;
        return false;
    };
    std::string bytes;
    Entries next;
    Rejection why = read_bounded(file, bytes);
    if (why == Rejection::None) parse(bytes, next, &why);
    if (why != Rejection::None) return reject(why);
    const auto cache = cache_path();
    std::error_code ec;
    std::filesystem::create_directories(cache.parent_path(), ec);
    if (ec) return reject(Rejection::CacheUnavailable);
    // The private cache is outside the profile configuration and its history.
    // Persist no selected source path and expose no payload in diagnostics.
    PendingFile pending;
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto process_id = ::GetCurrentProcessId();
#else
    const auto process_id = ::getpid();
#endif
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 32 && pending.path.empty(); ++attempt) {
        const auto candidate = cache.parent_path() / ("vocabulary-" + std::to_string(process_id) + "-" +
            std::to_string(stamp) + "-" + std::to_string(sequence.fetch_add(1)) + ".pending");
        if (write_exclusive(candidate, bytes)) pending.path = candidate;
    }
    if (pending.path.empty()) return reject(Rejection::CacheUnavailable);
#ifdef _WIN32
    if (!::MoveFileExW(pending.path.c_str(), cache.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return reject(Rejection::CacheUnavailable);
#else
    std::filesystem::rename(pending.path, cache, ec);
    if (ec) return reject(Rejection::CacheUnavailable);
#endif
    pending.path.clear();
    auto &s = state();
    s.entries = std::move(next);
    s.index = index_entries(s.entries);
    s.cache_loaded = true;
    s.cache_rejection = Rejection::None;
    if (reason) *reason = Rejection::None;
    refresh();
    return true;
}

bool clear()
{
    initialize();
    std::error_code ec;
    std::filesystem::remove(cache_path(), ec);
    if (ec) return false;
    auto &s = state();
    s.entries.clear();
    s.index.clear();
    s.cache_loaded = false;
    s.cache_rejection = Rejection::None;
    refresh();
    return true;
}

bool loaded() { initialize(); return state().cache_loaded; }

Rejection cache_rejection() { initialize(); return state().cache_rejection; }

bool is_cache_path(const std::filesystem::path &path)
{
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) return true;
    const auto private_dir = std::filesystem::weakly_canonical(cache_path().parent_path(), ec);
    if (ec) return true;
    const auto relative = canonical.lexically_relative(private_dir);
    return !relative.empty() && *relative.begin() != ".." && !relative.is_absolute();
}

wxString remember(const wxString &source)
{
    if (!source.empty()) {
        auto &s = state();
        std::lock_guard<std::mutex> lock(s.source_mutex);
        if (s.sources.size() < 50000) s.sources.insert(source.ToStdWstring());
    }
    return source;
}

wxString display(const wxString &source)
{
    if (PersonalModes::school_presentation_suppressed.load()) return source;
    const auto value = source.ToStdWstring();
    auto &s = state();
    { std::lock_guard<std::mutex> lock(s.source_mutex); if (!s.sources.count(value)) return source; }
    return apply_indexed(source, s.entries, s.index);
}

wxString apply(const wxString &source, const Entries &mapping)
{
    return apply_indexed(source, mapping, index_entries(mapping));
}

void refresh()
{
    if (!wxTheApp) return;
    // Snapshot callbacks because a display refresh may construct another widget.
    std::vector<Observer> live;
    auto &observers = state().observers;
    for (auto it = observers.begin(); it != observers.end();) {
        if (!it->second.window) it = observers.erase(it);
        else { live.push_back(it->second); ++it; }
    }
    for (auto &observer : live) if (observer.window) observer.refresh_display();
    for (wxWindow *window : wxTopLevelWindows) {
        window->InvalidateBestSize();
        window->Layout();
        window->Refresh();
    }
}
}
