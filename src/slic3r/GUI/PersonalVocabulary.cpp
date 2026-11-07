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
#include <wx/menu.h>
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
    std::filesystem::path cache_directory;
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
    return state().cache_directory / "vocabulary.json";
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

// ASCII case folding is locale-independent and leaves all non-ASCII code units
// exact. Replacement values retain the spelling supplied by the user.
wchar_t fold_ascii(wchar_t value)
{
    return value >= L'A' && value <= L'Z' ? value + (L'a' - L'A') : value;
}

FirstUnitIndex index_entries(const Entries &mapping)
{
    FirstUnitIndex index;
    for (std::size_t i = 0; i < mapping.size(); ++i)
        if (!mapping[i].first.empty()) index[fold_ascii(mapping[i].first.front())].push_back(i);
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
        const std::pair<std::wstring, std::wstring> *chosen = nullptr;
        bool preserved_ambiguous = false;
        if (const auto candidates = index.find(fold_ascii(value[i])); candidates != index.end()) {
            // Exact spelling wins, including a shorter exact source. Only when
            // none matches do we consider case-insensitive forms, longest first.
            for (int pass = 0; pass < 2 && !chosen; ++pass) {
                bool ambiguous = false;
                for (const std::size_t position : candidates->second) {
                    const auto &entry = mapping[position];
                    const auto end = i + entry.first.size();
                    if (end > value.size() ||
                        (i != 0 && word(value[i-1]) && word(entry.first.front())) ||
                        (end != value.size() && word(value[end]) && word(entry.first.back()))) continue;
                    const bool equal = pass == 0
                        ? value.compare(i, entry.first.size(), entry.first) == 0
                        : std::equal(entry.first.begin(), entry.first.end(), value.begin() + i,
                            [](wchar_t a, wchar_t b) { return fold_ascii(a) == fold_ascii(b); });
                    if (!equal) continue;
                    if (!chosen) chosen = &entry;
                    else if (chosen->first.size() != entry.first.size()) break;
                    else if (chosen->second != entry.second) ambiguous = true;
                    if (pass == 0) break;
                }
                // Conflicting variants have no inferred winner. Preserve the
                // complete source token, rather than matching a suffix inside it.
                if (ambiguous) {
                    output.append(value, i, chosen->first.size());
                    i += chosen->first.size();
                    chosen = nullptr;
                    preserved_ambiguous = true;
                    break;
                }
            }
        }
        if (chosen) {
            output += chosen->second;
            i += chosen->first.size();
        } else if (!preserved_ambiguous && i < value.size()) output += value[i++];
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

std::filesystem::path select_cache_directory(const std::filesystem::path &active,
    const std::filesystem::path &default_profile, const std::filesystem::path &legacy_local)
{
    std::error_code ec;
    bool same = std::filesystem::equivalent(active, default_profile, ec);
    if (!same) {
        ec.clear();
        const auto a = std::filesystem::weakly_canonical(active, ec);
        if (!ec) {
            const auto b = std::filesystem::weakly_canonical(default_profile, ec);
            if (!ec) {
#ifdef _WIN32
                same = ::CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
                same = a == b;
#endif
            }
        }
    }
    return (same ? legacy_local : active) / "private-display";
}

void configure_cache_directory(const std::filesystem::path &active,
    const std::filesystem::path &default_profile, const std::filesystem::path &legacy_local)
{
    auto &s = state();
    // Startup binds once. Later translations cannot switch a loaded profile.
    if (!s.cache_directory.empty() || active.empty() || default_profile.empty() || legacy_local.empty()) return;
    s.cache_directory = select_cache_directory(active, default_profile, legacy_local);
}

bool is_reserved_cache_relative(const std::filesystem::path &relative)
{
    // Exclude the complete reserved directory, including pending writes.
    if (relative.empty()) return false;
    auto first = relative.begin()->wstring();
    std::transform(first.begin(), first.end(), first.begin(), [](wchar_t c) { return std::towupper(c); });
    return first == L"PRIVATE-DISPLAY";
}

void initialize()
{
    auto &s = state();
    if (s.initialized || !wxTheApp || s.cache_directory.empty()) return;
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
    if (state().cache_directory.empty()) return reject(Rejection::CacheUnavailable);
    std::string bytes;
    Entries next;
    Rejection why = read_bounded(file, bytes);
    if (why == Rejection::None) parse(bytes, next, &why);
    if (why != Rejection::None) return reject(why);
    const auto cache = cache_path();
    std::error_code ec;
    std::filesystem::create_directories(cache.parent_path(), ec);
    if (ec) return reject(Rejection::CacheUnavailable);
    // The private cache is excluded from profile exports and history.
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
    if (state().cache_directory.empty()) return false;
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
    if (state().cache_directory.empty()) return true;
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

wxString remember_composed(const std::vector<wxString> &parts, const wxString &separator)
{
    wxString combined;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) combined += separator;
        combined += parts[i];
    }
    // This API propagates existing translation provenance, never grants it to
    // dynamic arguments or user content. Keep separators strictly structural.
    if (parts.empty() || parts.size() > 128 || combined.length() > 32768 || separator.length() > 8)
        return combined;
    for (const auto c : separator.ToStdWstring())
        if (c != L' ' && c != L'\n' && c != L'\t' && c != L':' && c != L',' && c != L';' && c != L'-')
            return combined;
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.source_mutex);
    for (const auto &part : parts)
        if (!part.empty() && !s.sources.count(part.ToStdWstring())) return combined;
    if (s.sources.size() < 50000) s.sources.insert(combined.ToStdWstring());
    return combined;
}

void remember_menu_label(const wxString &source, const wxString &normalized)
{
    const auto raw = source.ToStdWstring();
    if (raw.empty() || raw.size() > 32768) return;
    const auto prefix = raw.substr(0, raw.find(L'\t'));
    wxString expected = wxMenuItem::GetLabelText(wxString(prefix));
    expected.Trim(true).Trim(false);
    if (expected.ToStdWstring() != normalized.ToStdWstring()) return;
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.source_mutex);
    // A shortcut can be appended by the menu builder after translation. Only
    // the exact original label, or its exact pre-shortcut portion, grants trust.
    if (!s.sources.count(raw) && !s.sources.count(prefix)) return;
    if (s.sources.size() < 50000) s.sources.insert(normalized.ToStdWstring());
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
