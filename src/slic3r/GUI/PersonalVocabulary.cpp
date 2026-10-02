#include "PersonalVocabulary.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <wx/app.h>
#include <wx/stdpaths.h>
#include <wx/toplevel.h>
#include <wx/window.h>
#include <wx/weakref.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r::GUI::PersonalVocabulary {
namespace {
std::mutex source_mutex;
std::set<std::wstring> sources;
Entries entries;
bool initialized = false;
bool cache_loaded = false;

std::filesystem::path cache_path()
{
    return std::filesystem::path(wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()) /
        "private-display" / "vocabulary.json";
}

bool read_bounded(const std::filesystem::path &path, std::string &bytes)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    bytes.resize(max_bytes + 1);
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    return !input.bad() && bytes.size() <= max_bytes;
}

bool valid_text(const std::string &text)
{
    if (text.empty() || text.size() > max_text_bytes) return false;
    for (unsigned char ch : text) if (ch < 32 || ch == 127) return false;
    const auto unicode = wxString::FromUTF8(text).ToStdWstring();
    bool has_content = false;
    for (wchar_t ch : unicode) {
        if ((ch >= 0x80 && ch <= 0x9f) || (ch >= 0x202a && ch <= 0x202e) ||
            (ch >= 0x2066 && ch <= 0x2069)) return false;
        if (!std::iswspace(ch)) has_content = true;
    }
    return has_content && text != "__proto__" && text != "constructor" && text != "prototype";
}

struct Observer {
    wxWeakRef<wxWindow> window;
    std::function<void()> refresh_display;
};
std::map<wxWindow *, Observer> observers;
}

void observe(wxWindow *window, std::function<void()> refresh_display)
{
    if (!window) return;
    for (auto it = observers.begin(); it != observers.end();)
        if (!it->second.window) it = observers.erase(it); else ++it;
    observers[window] = Observer{window, std::move(refresh_display)};
}

bool parse(std::string_view bytes, Entries &result)
{
    if (bytes.empty() || bytes.size() > max_bytes) return false;
    try {
        bool valid = true;
        std::vector<std::set<std::string>> object_keys;
        auto callback = [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json &value) {
            if (depth > 3) throw std::invalid_argument("Unsupported document nesting");
            if (event == nlohmann::json::parse_event_t::object_start) object_keys.emplace_back();
            if (event == nlohmann::json::parse_event_t::key) {
                const auto key = value.get<std::string>();
                if (object_keys.empty() || !object_keys.back().insert(key).second || !valid_text(key)) valid = false;
            }
            if (event == nlohmann::json::parse_event_t::object_end && !object_keys.empty()) object_keys.pop_back();
            return true;
        };
        auto root = nlohmann::json::parse(bytes.begin(), bytes.end(), callback);
        if (!valid || !root.is_object() || root.size() != 2 || !root.contains("schemaVersion") ||
            !root["schemaVersion"].is_number_integer() || root["schemaVersion"] != 1 ||
            !root.contains("entries") || !root["entries"].is_object() || root["entries"].size() > max_entries) return false;
        Entries next;
        for (const auto &item : root["entries"].items()) {
            if (!valid_text(item.key()) || !item.value().is_string()) return false;
            const auto value = item.value().get<std::string>();
            if (!valid_text(value)) return false;
            next.emplace_back(wxString::FromUTF8(item.key()).ToStdWstring(), wxString::FromUTF8(value).ToStdWstring());
        }
        std::stable_sort(next.begin(), next.end(), [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
        result = std::move(next);
        return true;
    } catch (...) { return false; }
}

void initialize()
{
    if (initialized || !wxTheApp) return;
    initialized = true;
    std::string bytes;
    Entries cached;
    if (read_bounded(cache_path(), bytes) && parse(bytes, cached)) {
        entries = std::move(cached);
        cache_loaded = true;
    }
}

bool load(const std::filesystem::path &file)
{
    initialize();
    std::string bytes;
    Entries next;
    if (!read_bounded(file, bytes) || !parse(bytes, next)) return false;
    const auto cache = cache_path();
    std::error_code ec;
    std::filesystem::create_directories(cache.parent_path(), ec);
    if (ec) return false;
    // The private cache is outside the profile configuration and its history.
    // Persist no selected source path and expose no payload in diagnostics.
    const auto temporary = cache.parent_path() / "vocabulary.pending";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) return false;
    }
#ifdef _WIN32
    if (!::MoveFileExW(temporary.c_str(), cache.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
#else
    std::filesystem::rename(temporary, cache, ec);
    if (ec) return false;
#endif
    entries = std::move(next);
    cache_loaded = true;
    refresh();
    return true;
}

bool clear()
{
    initialize();
    std::error_code ec;
    std::filesystem::remove(cache_path(), ec);
    if (ec) return false;
    std::filesystem::remove(cache_path().parent_path() / "vocabulary.pending", ec);
    entries.clear();
    cache_loaded = false;
    refresh();
    return !ec;
}

bool loaded() { initialize(); return cache_loaded; }

bool is_cache_path(const std::filesystem::path &path)
{
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) return false;
    const auto private_dir = std::filesystem::weakly_canonical(cache_path().parent_path(), ec);
    if (ec) return false;
    const auto relative = canonical.lexically_relative(private_dir);
    return !relative.empty() && *relative.begin() != ".." && !relative.is_absolute();
}

wxString remember(const wxString &source)
{
    if (!source.empty()) {
        std::lock_guard<std::mutex> lock(source_mutex);
        if (sources.size() < 50000) sources.insert(source.ToStdWstring());
    }
    return source;
}

wxString display(const wxString &source)
{
    const auto value = source.ToStdWstring();
    { std::lock_guard<std::mutex> lock(source_mutex); if (!sources.count(value)) return source; }
    return apply(source, entries);
}

wxString apply(const wxString &source, const Entries &mapping)
{
    const auto value = source.ToStdWstring();
    if (value.size() > 32768) return source;
    if (value.find(L"://") != std::wstring::npos || value.find(L'\\') != std::wstring::npos ||
        value.find(L'/') != std::wstring::npos || value.find(L'%') != std::wstring::npos) return source;
    auto word = [](wchar_t c) { return std::iswalnum(c) || c == L'_'; };
    std::wstring output;
    for (std::size_t i = 0; i < value.size();) {
        bool matched = false;
        for (const auto &entry : mapping) {
            if (entry.first.empty()) continue;
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
        if (!matched) output += value[i++];
        if (output.size() > 65536) return source;
    }
    return wxString(output);
}

void refresh()
{
    if (!wxTheApp) return;
    // Snapshot callbacks because a display refresh may construct another widget.
    std::vector<Observer> live;
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
