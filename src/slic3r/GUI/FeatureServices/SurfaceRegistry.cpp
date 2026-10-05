#include "SurfaceRegistry.hpp"
#include "../Appearance/ElementStyle.hpp"
#include "../I18N.hpp"
#include "../PersonalVocabulary.hpp"
#include "libslic3r/Utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <stdexcept>
#include <utility>
#include <set>
#include <vector>
#include <wx/app.h>
#include <wx/textctrl.h>
#include <wx/toplevel.h>
#include <wx/weakref.h>
#include <wx/window.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Slic3r::GUI::FeatureServices {
namespace {
bool valid_id(const std::string &id)
{
    if (id.empty() || id.size() > 256 || id.front() == '/' || id.back() == '/') return false;
    for (unsigned char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '/')) return false;
    return id.find("//") == std::string::npos;
}
bool within(wxWindow *window, wxWindow *ancestor)
{
    for (auto *p = window; p; p = p->GetParent()) if (p == ancestor) return true;
    return false;
}
struct IdentityLock {
#ifdef _WIN32
    HANDLE handle;
    explicit IdentityLock(const std::filesystem::path &path)
        : handle(::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
    ~IdentityLock() { if (valid()) ::CloseHandle(handle); }
    bool valid() const { return handle != INVALID_HANDLE_VALUE; }
#else
    int handle;
    bool locked;
    explicit IdentityLock(const std::filesystem::path &path)
        : handle(::open(path.c_str(), O_CREAT | O_RDWR, 0600)), locked(handle >= 0 && ::flock(handle, LOCK_EX | LOCK_NB) == 0) {}
    ~IdentityLock() { if (handle >= 0) ::close(handle); }
    bool valid() const { return locked; }
#endif
};
}

struct SurfaceRegistry::Impl : wxEvtHandler {
    struct Entry {
        wxWeakRef<wxWindow> window;
        std::string id;
        bool root = false;
        bool sensitive = false;
        bool has_label = false, has_tooltip = false, has_name = false;
        wxString label, tooltip, name;
    };
    struct Protection {
        wxWeakRef<wxWindow> window;
        CaptureProtection state = CaptureProtection::NotRequired;
#ifdef _WIN32
        DWORD previous = 0;
        bool changed = false;
#endif
    };
    std::map<wxWindow *, Entry> entries;
    std::map<wxWindow *, Protection> protections;
    ProtectionObserver observer;
    wxWeakRef<wxApp> app;
    bool busy = false;
    std::map<std::string, std::string> identities;
    bool identities_valid = true;

    std::string identity(const std::string &id)
    {
        if (!wxTheApp || !valid_id(id) || Slic3r::data_dir().empty()) return {};
        const auto directory = std::filesystem::u8path(Slic3r::data_dir()) / "surface-identities";
        const auto path = directory / "identities.json";
        std::error_code create_error;
        std::filesystem::create_directories(directory, create_error);
        if (create_error) return {};
        IdentityLock lock(directory / "identities.lock");
        if (!lock.valid()) return {};
        // Re-read under the cross-process lock: another instance can add IDs.
        identities_valid = true;
        identities.clear();
        {
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                const auto size = std::filesystem::file_size(path, ec);
                if (ec || size > 1024 * 1024) identities_valid = false;
                else try {
                    std::ifstream input(path, std::ios::binary);
                    std::string bytes(1024 * 1024 + 1, '\0');
                    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                    bytes.resize(static_cast<std::size_t>(input.gcount()));
                    if (input.bad() || bytes.empty() || bytes.size() > 1024 * 1024)
                        throw std::runtime_error("Invalid identity storage");
                    std::vector<std::set<std::string>> keys;
                    const auto validate = [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json &value) {
                        if (depth > 2) throw std::runtime_error("Invalid identity storage");
                        if (event == nlohmann::json::parse_event_t::object_start) keys.emplace_back();
                        if (event == nlohmann::json::parse_event_t::key &&
                            (keys.empty() || !keys.back().insert(value.get<std::string>()).second))
                            throw std::runtime_error("Invalid identity storage");
                        if (event == nlohmann::json::parse_event_t::object_end && !keys.empty()) keys.pop_back();
                        return true;
                    };
                    const auto doc = nlohmann::json::parse(bytes.begin(), bytes.end(), validate);
                    if (!doc.is_object() || doc.size() != 2 || !doc.contains("schema") ||
                        !doc.at("schema").is_number_integer() || doc.at("schema") != 1 ||
                        !doc.contains("identities") || !doc.at("identities").is_object() ||
                        doc.at("identities").size() > 4096) throw std::runtime_error("Invalid identity storage");
                    std::set<std::string> used;
                    for (const auto &item : doc.at("identities").items()) {
                        const auto value = item.value().get<std::string>();
                        if (!valid_id(item.key()) || value.size() != 32 ||
                            value.find_first_not_of("0123456789abcdef") != std::string::npos || !used.insert(value).second)
                            throw std::runtime_error("Invalid identity storage");
                        identities[item.key()] = value;
                    }
                } catch (...) { identities_valid = false; identities.clear(); }
            } else if (ec) identities_valid = false;
        }
        if (!identities_valid) return {};
        const auto existing = identities.find(id);
        if (existing != identities.end()) return existing->second;
        if (identities.size() >= 4096) return {};
        try {
            std::random_device random;
            const char digits[] = "0123456789abcdef";
            std::string value;
            for (unsigned attempt = 0; attempt < 3; ++attempt) {
                value.clear();
                for (unsigned n = 0; n < 16; ++n) {
                    const auto byte = random() & 255u;
                    value += digits[byte >> 4]; value += digits[byte & 15];
                }
                if (std::none_of(identities.begin(), identities.end(), [&](const auto &entry) { return entry.second == value; })) break;
                value.clear();
            }
            if (value.empty()) return {};
            auto next = identities;
            next[id] = value;
            const auto bytes = nlohmann::json{{"schema", 1}, {"identities", next}}.dump(2);
            if (bytes.size() > 1024 * 1024) return {};
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            if (ec) return {};
            // The application owns this fixed directory; no user paths enter it.
            const auto pending = directory / "identities.pending";
            {
                std::ofstream output(pending, std::ios::binary | std::ios::trunc);
                if (!output) return {};
                output << bytes;
                output.flush();
                if (!output) return {};
            }
#ifdef _WIN32
            if (!::MoveFileExW(pending.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return {};
#else
            std::filesystem::rename(pending, path, ec);
            if (ec) return {};
#endif
            identities.swap(next);
            return value;
        } catch (...) { return {}; }
    }

    ~Impl() override
    {
        if (app) app->Unbind(wxEVT_IDLE, &Impl::idle, this);
    }
    void idle(wxIdleEvent &event) { sync(); event.Skip(); }
    void attach()
    {
        if (!app && wxTheApp) {
            app = wxTheApp;
            app->Bind(wxEVT_IDLE, &Impl::idle, this);
        }
    }
    void prune()
    {
        for (auto it = entries.begin(); it != entries.end();)
            if (!it->second.window) it = entries.erase(it); else ++it;
        for (auto it = protections.begin(); it != protections.end();)
            if (!it->second.window) it = protections.erase(it); else ++it;
    }
    bool sensitive(wxWindow *window) const
    {
        for (auto *p = window; p; p = p->GetParent()) {
            auto it = entries.find(p);
            if (it != entries.end() && it->second.window && it->second.sensitive) return true;
            if (wxDynamicCast(p, wxTextCtrl) && (p->GetWindowStyleFlag() & wxTE_PASSWORD)) return true;
        }
        return false;
    }
    bool intersects_sensitive(wxWindow *window) const
    {
        if (sensitive(window)) return true;
        for (auto *child : window->GetChildren()) if (intersects_sensitive(child)) return true;
        return false;
    }
    bool registered_tree(wxWindow *window) const
    {
        auto it = entries.find(window);
        if (it == entries.end() || !it->second.window || it->second.id.empty()) return false;
        for (auto *child : window->GetChildren()) if (!registered_tree(child)) return false;
        return true;
    }
    void present(wxWindow *window)
    {
        auto it = entries.find(window);
        if (it == entries.end() || !it->second.window) return;
        // Copy because setters and layout may synchronously construct controls.
        const auto entry = it->second;
        // An explicitly supplied neutral name describes an input, never its
        // entered value. It remains available on sensitive controls.
        if (entry.has_name)
            window->SetName(PersonalVocabulary::display(PersonalVocabulary::remember(I18N::translate(entry.name))));
        if (!entry.window || sensitive(window)) return;
        if (entry.has_label && !wxDynamicCast(window, wxTextCtrl))
            window->SetLabel(PersonalVocabulary::display(PersonalVocabulary::remember(I18N::translate(entry.label))));
        if (entry.window && entry.has_tooltip)
            window->SetToolTip(PersonalVocabulary::display(PersonalVocabulary::remember(I18N::translate(entry.tooltip))));
        if (entry.window) { window->InvalidateBestSize(); window->Layout(); window->Refresh(); }
    }
    void adopt(wxWindow *window, const std::string &id)
    {
        auto &entry = entries[window];
        entry.window = window;
        entry.id = id;
        ElementStyle::apply(window, id, wxString::FromUTF8(id.c_str()), false);
    }
    void discover(wxWindow *parent)
    {
        const auto found = entries.find(parent);
        if (found == entries.end() || found->second.id.empty()) return;
        const auto prefix = found->second.id;
        for (auto *child : parent->GetChildren()) {
            auto it = entries.find(child);
            if (it == entries.end() || it->second.id.empty()) {
                // IDs contain no labels, names, paths or other user content.
                std::string kind = wxString(child->GetClassInfo()->GetClassName()).ToStdString();
                kind.erase(std::remove_if(kind.begin(), kind.end(), [](unsigned char c) {
                    return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
                }), kind.end());
                if (kind.empty()) kind = "window";
                unsigned index = 0;
                std::string id;
                do {
                    id = prefix + "/" + kind + "." + std::to_string(index++);
                } while (std::any_of(entries.begin(), entries.end(), [&](const auto &pair) {
                    return pair.second.window && pair.second.id == id;
                }));
                adopt(child, id);
            }
            discover(child);
        }
    }
    void update_protection(wxWindow *top)
    {
        const bool required = intersects_sensitive(top);
        auto &p = protections[top];
        p.window = top;
        auto state = CaptureProtection::NotRequired;
#ifdef _WIN32
        auto handle = reinterpret_cast<HWND>(top->GetHandle());
        constexpr DWORD excluded = 0x00000011; // WDA_EXCLUDEFROMCAPTURE, Windows 10 2004+
        if (required) {
            DWORD before = 0, after = 0;
            if (!handle || !::GetWindowDisplayAffinity(handle, &before)) state = CaptureProtection::Failed;
            else {
                if (!p.changed) p.previous = before;
                const bool applied = ::SetWindowDisplayAffinity(handle, excluded) != FALSE;
                if (applied) p.changed = true;
                state = applied && ::GetWindowDisplayAffinity(handle, &after) && after == excluded
                    ? CaptureProtection::Applied : CaptureProtection::Failed;
            }
        } else if (p.changed) {
            DWORD after = 0;
            if (handle && ::SetWindowDisplayAffinity(handle, p.previous) &&
                ::GetWindowDisplayAffinity(handle, &after) && after == p.previous) p.changed = false;
            else state = CaptureProtection::Failed;
        }
#else
        if (required) state = CaptureProtection::Unsupported;
#endif
        const bool changed = p.state != state;
        p.state = state;
        if (changed && observer) {
            auto it = entries.find(top);
            observer(it == entries.end() ? std::string() : it->second.id, state);
        }
    }
    void sync()
    {
        if (busy) return;
        busy = true;
        struct Reset { bool &value; ~Reset() { value = false; } } reset{busy};
        attach();
        prune();
        std::vector<wxWeakRef<wxWindow>> roots;
        for (auto &pair : entries) if (pair.second.root) roots.push_back(pair.second.window);
        for (auto &root : roots) if (root) discover(root.get());
        std::set<wxWindow *> tops;
        for (auto &pair : entries) if (pair.second.window) {
            if (auto *top = wxGetTopLevelParent(pair.second.window.get())) tops.insert(top);
        }
        for (auto *top : tops) update_protection(top);
    }
    bool permitted(wxWindow *window)
    {
        sync();
        if (!window || window->IsBeingDeleted() || !registered_tree(window) || intersects_sensitive(window)) return false;
        // A discovered child detached from its registered root becomes unknown.
        for (const auto &pair : entries)
            if (pair.second.root && pair.second.window && within(window, pair.second.window.get())) return true;
        return false;
    }
};

SurfaceRegistry::SurfaceRegistry() : m(new Impl) {}
SurfaceRegistry::~SurfaceRegistry() = default;
SurfaceRegistry &SurfaceRegistry::instance()
{
    static SurfaceRegistry registry;
    return registry;
}
bool SurfaceRegistry::register_surface(wxWindow *window, const std::string &id)
{
    if (!window || window->IsBeingDeleted() || !valid_id(id)) return false;
    m->prune();
    for (const auto &pair : m->entries)
        if (pair.first != window && pair.second.id == id) return false;
    auto existing = m->entries.find(window);
    if (existing != m->entries.end() && !existing->second.id.empty() && existing->second.id != id) {
        if (existing->second.root) return false;
        // A newly discovered child may later receive its explicit semantic ID.
        // Its descendants are re-addressed, retaining sensitivity and sources.
        const auto old = existing->second.id;
        for (auto &pair : m->entries) {
            if (!pair.second.root && pair.second.id.compare(0, old.size() + 1, old + "/") == 0)
                pair.second.id.clear();
        }
    }
    m->adopt(window, id);
    m->entries[window].root = true;
    m->sync();
    return true;
}
void SurfaceRegistry::register_sensitive(wxWindow *window)
{
    if (!window || window->IsBeingDeleted()) return;
    auto &entry = m->entries[window];
    entry.window = window;
    entry.sensitive = true;
    entry.label.clear(); entry.tooltip.clear();
    entry.has_label = entry.has_tooltip = false;
    m->sync();
}
std::string SurfaceRegistry::surface_id(wxWindow *window)
{
    m->sync();
    auto it = m->entries.find(window);
    return it == m->entries.end() ? std::string() : it->second.id;
}
bool SurfaceRegistry::capture_permitted(wxWindow *w) { return m->permitted(w); }
bool SurfaceRegistry::history_permitted(wxWindow *w) { return m->permitted(w); }
bool SurfaceRegistry::export_permitted(wxWindow *w) { return m->permitted(w); }
std::string SurfaceRegistry::history_identity(wxWindow *window)
{
    if (!m->permitted(window)) return {};
    return m->identity(m->entries.at(window).id);
}
void SurfaceRegistry::record_label(wxWindow *window, const wxString &source)
{
    m->sync();
    auto it = m->entries.find(window);
    if (it == m->entries.end() || !it->second.window || m->sensitive(window) || wxDynamicCast(window, wxTextCtrl)) return;
    it->second.label = source; it->second.has_label = true;
    wxWeakRef<wxWindow> weak(window);
    PersonalVocabulary::observe(window, [this, weak] { if (weak) m->present(weak.get()); });
    m->present(window);
}
void SurfaceRegistry::record_tooltip(wxWindow *window, const wxString &source)
{
    m->sync();
    auto it = m->entries.find(window);
    if (it == m->entries.end() || !it->second.window || m->sensitive(window)) return;
    it->second.tooltip = source; it->second.has_tooltip = true;
    wxWeakRef<wxWindow> weak(window);
    PersonalVocabulary::observe(window, [this, weak] { if (weak) m->present(weak.get()); });
    m->present(window);
}
void SurfaceRegistry::record_name(wxWindow *window, const wxString &source)
{
    m->sync();
    auto it = m->entries.find(window);
    if (it == m->entries.end() || !it->second.window) return;
    it->second.name = source; it->second.has_name = true;
    wxWeakRef<wxWindow> weak(window);
    PersonalVocabulary::observe(window, [this, weak] { if (weak) m->present(weak.get()); });
    m->present(window);
}
void SurfaceRegistry::clear_label_source(wxWindow *window)
{
    m->prune();
    auto it = m->entries.find(window);
    if (it == m->entries.end()) return;
    it->second.label.clear();
    it->second.has_label = false;
}
void SurfaceRegistry::refresh_presentation()
{
    m->sync();
    std::vector<wxWeakRef<wxWindow>> live;
    for (auto &pair : m->entries) live.push_back(pair.second.window);
    for (auto &window : live) if (window) m->present(window.get());
    ElementStyle::restyle_all();
}
SurfaceRegistry::CaptureProtection SurfaceRegistry::capture_protection(wxWindow *window)
{
    m->sync();
    if (!window) return CaptureProtection::Failed;
    auto it = m->protections.find(wxGetTopLevelParent(window));
    return it == m->protections.end() ? CaptureProtection::Failed : it->second.state;
}
void SurfaceRegistry::synchronize() { m->sync(); }
void SurfaceRegistry::set_protection_observer(ProtectionObserver observer) { m->observer = std::move(observer); }
}
