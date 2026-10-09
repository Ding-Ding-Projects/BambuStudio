#include "CanvasDisclosures.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../PersonalVocabulary.hpp"

#include "libslic3r/AppConfig.hpp"

#include <map>
#include <utility>

#include <wx/window.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

namespace Slic3r { namespace GUI { namespace CanvasDisclosures {

namespace {

struct Registry
{
    std::vector<std::pair<std::string, Entry>> entries;
};

std::map<const wxWindow *, Registry> &registries()
{
    static std::map<const wxWindow *, Registry> instance;
    return instance;
}

Registry *find_registry(const wxWindow *canvas)
{
    auto it = registries().find(canvas);
    return it == registries().end() ? nullptr : &it->second;
}

// MSAA child ids are 1-based; 0 is the canvas itself.
int child_id_of(const Registry &registry, const std::string &id)
{
    for (size_t i = 0; i < registry.entries.size(); ++i)
        if (registry.entries[i].first == id)
            return static_cast<int>(i) + 1;
    return 0;
}

#if wxUSE_ACCESSIBILITY
class CanvasAccessible final : public wxWindowAccessible
{
public:
    explicit CanvasAccessible(wxWindow *canvas) : wxWindowAccessible(canvas), m_canvas(canvas) {}

    wxAccStatus GetChildCount(int *count) override
    {
        if (count == nullptr)
            return wxACC_FAIL;
        const Registry *registry = find_registry(m_canvas);
        *count = registry != nullptr ? static_cast<int>(registry->entries.size()) : 0;
        return wxACC_OK;
    }

    wxAccStatus GetChild(int child_id, wxAccessible **child) override
    {
        if (child == nullptr)
            return wxACC_FAIL;
        if (child_id == wxACC_SELF) {
            *child = this;
            return wxACC_OK;
        }
        if (entry(child_id) == nullptr)
            return wxACC_INVALID_ARG;
        *child = nullptr; // a simple element of this object
        return wxACC_OK;
    }

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (child_id == wxACC_SELF)
            return wxWindowAccessible::GetName(child_id, name);
        const Entry *e = entry(child_id);
        if (e == nullptr || name == nullptr)
            return wxACC_INVALID_ARG;
        *name = PersonalVocabulary::display(e->name);
        return wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (child_id == wxACC_SELF)
            return wxWindowAccessible::GetRole(child_id, role);
        if (entry(child_id) == nullptr || role == nullptr)
            return wxACC_INVALID_ARG;
        *role = wxROLE_SYSTEM_PUSHBUTTON;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (child_id == wxACC_SELF)
            return wxWindowAccessible::GetState(child_id, state);
        const Entry *e = entry(child_id);
        if (e == nullptr || state == nullptr)
            return wxACC_INVALID_ARG;
        *state = wxACC_STATE_SYSTEM_FOCUSABLE;
        if (e->keyboard_focus && m_canvas->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        *state |= e->expanded ? wxACC_STATE_SYSTEM_EXPANDED : wxACC_STATE_SYSTEM_COLLAPSED;
        return wxACC_OK;
    }

    wxAccStatus GetKeyboardShortcut(int child_id, wxString *shortcut) override
    {
        if (child_id == wxACC_SELF)
            return wxWindowAccessible::GetKeyboardShortcut(child_id, shortcut);
        if (entry(child_id) == nullptr || shortcut == nullptr)
            return wxACC_INVALID_ARG;
        *shortcut = "Shift+L";
        return wxACC_OK;
    }

    wxAccStatus GetLocation(wxRect &rect, int child_id) override
    {
        if (child_id == wxACC_SELF)
            return wxWindowAccessible::GetLocation(rect, child_id);
        const Entry *e = entry(child_id);
        if (e == nullptr)
            return wxACC_INVALID_ARG;
        rect = wxRect(m_canvas->ClientToScreen(e->client_rect.GetPosition()), e->client_rect.GetSize());
        return wxACC_OK;
    }

    wxAccStatus HitTest(const wxPoint &point, int *child_id, wxAccessible **child) override
    {
        const Registry *registry = find_registry(m_canvas);
        if (registry != nullptr && child_id != nullptr && child != nullptr) {
            const wxPoint client = m_canvas->ScreenToClient(point);
            for (size_t i = 0; i < registry->entries.size(); ++i)
                if (registry->entries[i].second.client_rect.Contains(client)) {
                    *child_id = static_cast<int>(i) + 1;
                    *child    = nullptr;
                    return wxACC_OK;
                }
        }
        return wxWindowAccessible::HitTest(point, child_id, child);
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action) override
    {
        if (child_id == wxACC_SELF)
            return wxWindowAccessible::GetDefaultAction(child_id, action);
        const Entry *e = entry(child_id);
        if (e == nullptr || action == nullptr)
            return wxACC_INVALID_ARG;
        *action = e->expanded ? _L("Collapse") : _L("Expand");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        if (child_id == wxACC_SELF)
            return wxWindowAccessible::DoDefaultAction(child_id);
        const Entry *e = entry(child_id);
        if (e == nullptr)
            return wxACC_INVALID_ARG;
        if (!e->toggle)
            return wxACC_NOT_SUPPORTED;
        // Copy first: the toggle republishes and may reallocate the entry.
        const std::function<void()> toggle = e->toggle;
        toggle();
        m_canvas->Refresh();
        return wxACC_OK;
    }

    wxAccStatus GetFocus(int *child_id, wxAccessible **child) override
    {
        if (child_id == nullptr || child == nullptr)
            return wxACC_FAIL;
        *child_id = 0;
        *child    = nullptr;
        if (!m_canvas->HasFocus())
            return wxACC_OK;
        if (const Registry *registry = find_registry(m_canvas))
            for (size_t i = 0; i < registry->entries.size(); ++i)
                if (registry->entries[i].second.keyboard_focus) {
                    *child_id = static_cast<int>(i) + 1;
                    return wxACC_OK;
                }
        *child = this;
        return wxACC_OK;
    }

private:
    const Entry *entry(int child_id) const
    {
        const Registry *registry = find_registry(m_canvas);
        if (registry == nullptr || child_id < 1 || child_id > static_cast<int>(registry->entries.size()))
            return nullptr;
        return &registry->entries[static_cast<size_t>(child_id - 1)].second;
    }

    wxWindow *m_canvas;
};
#endif

} // namespace

void publish(wxWindow *canvas, const std::string &id, Entry entry)
{
    if (canvas == nullptr)
        return;
    auto &all = registries();
    auto  it  = all.find(canvas);
    if (it == all.end()) {
        it = all.emplace(canvas, Registry{}).first;
#if wxUSE_ACCESSIBILITY
        // wxWindowAccessible installs itself on the canvas, which owns it.
        new CanvasAccessible(canvas);
#endif
        canvas->Bind(wxEVT_DESTROY, [canvas](wxWindowDestroyEvent &event) {
            if (event.GetWindow() == canvas)
                registries().erase(canvas);
            event.Skip();
        });
    }
    auto &entries = it->second.entries;
    for (auto &item : entries)
        if (item.first == id) {
            item.second = std::move(entry);
            return;
        }
    entries.emplace_back(id, std::move(entry));
}

void withdraw(wxWindow *canvas, const std::string &id, const void *owner)
{
    Registry *registry = find_registry(canvas);
    if (registry == nullptr)
        return;
    auto &entries = registry->entries;
    for (auto it = entries.begin(); it != entries.end(); ++it)
        if (it->first == id) {
            if (it->second.owner == owner)
                entries.erase(it);
            return;
        }
}

void announce(wxWindow *canvas, const std::string &id, bool expanded, bool keyboard_focus,
              const std::vector<CollapsibleFilters::AnnounceEvent> &events)
{
    Registry *registry = find_registry(canvas);
    if (registry == nullptr)
        return;
    const int child_id = child_id_of(*registry, id);
    if (child_id == 0)
        return;
    Entry &entry         = registry->entries[static_cast<size_t>(child_id - 1)].second;
    entry.expanded       = expanded;
    entry.keyboard_focus = keyboard_focus;
#if wxUSE_ACCESSIBILITY
    for (const CollapsibleFilters::AnnounceEvent event : events)
        wxAccessible::NotifyEvent(event == CollapsibleFilters::AnnounceEvent::Focus ? wxACC_EVENT_OBJECT_FOCUS
                                                                                    : wxACC_EVENT_OBJECT_STATECHANGE,
                                  canvas, wxOBJID_CLIENT, child_id);
#else
    (void) events;
#endif
}

CollapsibleFilters::Section::Read config_reader()
{
    return [](const std::string &section, const std::string &key) {
        AppConfig *config = wxGetApp().app_config;
        return config != nullptr ? config->get(section, key) : std::string();
    };
}

CollapsibleFilters::Section::Write config_writer()
{
    return [](const std::string &section, const std::string &key, const std::string &value) {
        if (AppConfig *config = wxGetApp().app_config)
            config->set(section, key, value);
    };
}

}}} // namespace Slic3r::GUI::CanvasDisclosures
