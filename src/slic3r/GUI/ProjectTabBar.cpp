#include "ProjectTabBar.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"

#include "libslic3r/AppConfig.hpp"

#include <algorithm>
#include <utility>

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_PROJECT_TAB_SWITCH, wxCommandEvent);
wxDEFINE_EVENT(EVT_PROJECT_TAB_CLOSE,  wxCommandEvent);
wxDEFINE_EVENT(EVT_PROJECT_TAB_NEW,    wxCommandEvent);

namespace {

// Display title from an on-disk project path: filename without directory or
// extension. Used when restoring; MainFrame later refreshes the live title.
wxString TitleFromPath(const std::string &path)
{
    if (path.empty())
        return _L("Untitled");
    const size_t s   = path.find_last_of("/\\");
    std::string base = s == std::string::npos ? path : path.substr(s + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0)
        base = base.substr(0, dot);
    return wxString::FromUTF8(base);
}

TabStrip::Options project_options()
{
    TabStrip::Options o;
    o.surface_key              = "projects";
    o.surface_name             = _L("Projects");
    o.strip_name               = _L("Project tabs");
    o.default_edge             = MD3::Tabs::DockEdge::Top;
    o.close_mode               = TabStrip::CloseMode::Close;
    o.show_new_button          = true;
    o.allow_close              = true;
    o.host_confirms_activation = true;
    return o;
}

} // namespace

ProjectTabBar::ProjectTabBar(wxWindow *parent) : TabStrip(parent, project_options())
{
    // Translate the strip's id-based events into the index-based project
    // events MainFrame orchestrates against; stop the strip events here.
    Bind(EVT_TABSTRIP_ACTIVATE, [this](wxCommandEvent &e) {
        const int index = GetModel().index_of(std::string(e.GetString().ToUTF8()));
        if (index < 0)
            return;
        wxCommandEvent evt(EVT_PROJECT_TAB_SWITCH);
        evt.SetEventObject(this);
        evt.SetInt(index);
        wxPostEvent(this, evt);
    });
    Bind(EVT_TABSTRIP_CLOSE_REQUEST, [this](wxCommandEvent &e) {
        const int index = GetModel().index_of(std::string(e.GetString().ToUTF8()));
        if (index < 0)
            return;
        wxCommandEvent evt(EVT_PROJECT_TAB_CLOSE);
        evt.SetEventObject(this);
        evt.SetInt(index);
        wxPostEvent(this, evt);
    });
    Bind(EVT_TABSTRIP_NEW, [this](wxCommandEvent &) {
        wxCommandEvent evt(EVT_PROJECT_TAB_NEW);
        evt.SetEventObject(this);
        wxPostEvent(this, evt);
    });
    Bind(EVT_TABSTRIP_CHANGED, [this](wxCommandEvent &) {
        // Drop project records whose tab is gone (bulk close / remove).
        for (auto it = m_projects.begin(); it != m_projects.end();) {
            if (GetModel().index_of(it->first) < 0)
                it = m_projects.erase(it);
            else
                ++it;
        }
    });
}

ProjectTabBar::~ProjectTabBar() = default;

std::string ProjectTabBar::IdAt(int i) const
{
    if (i < 0 || i >= GetModel().size())
        return std::string();
    return GetModel().at(i).id;
}

std::string ProjectTabBar::NewId(const std::string &file_path)
{
    if (!file_path.empty() && !GetModel().find(file_path))
        return file_path;
    std::string id;
    do {
        id = "untitled-" + std::to_string(m_next_untitled++);
    } while (GetModel().find(id));
    return id;
}

int ProjectTabBar::AddTab(const std::string &file_path, const wxString &title, bool activate)
{
    const std::string id = NewId(file_path);
    ProjectTab        tab;
    tab.file_path = file_path;
    tab.title     = title;
    m_projects[id] = tab;
    const int index = TabStrip::AddTab(id, title, file_path, /*activate*/ false);
    if (activate)
        Activate(id, /*emit*/ false);
    return index;
}

void ProjectTabBar::CloseTab(int i)
{
    const std::string id = IdAt(i);
    if (id.empty())
        return;
    m_projects.erase(id);
    RemoveTab(id);
}

void ProjectTabBar::SetActive(int i)
{
    const std::string id = IdAt(i);
    if (id.empty())
        return;
    Activate(id, /*emit*/ false);
}

int ProjectTabBar::GetActive() const { return ActiveIndex(); }

ProjectTab &ProjectTabBar::TabAt(int i)
{
    const std::string      id = IdAt(i);
    ProjectTab &           t  = m_projects[id];
    const MD3::Tabs::Tab * mt = GetModel().find(id);
    if (mt) {
        t.title    = mt->title;
        t.group_id = mt->group_id;
        if (t.file_path.empty())
            t.file_path = mt->payload;
    }
    return t;
}

void ProjectTabBar::SetActiveDirty(bool dirty)
{
    const std::string id = ActiveId();
    if (id.empty())
        return;
    m_projects[id].dirty = dirty;
    SetDirty(id, dirty);
}

void ProjectTabBar::SetActiveTitle(const wxString &t)
{
    const std::string id = ActiveId();
    if (id.empty())
        return;
    m_projects[id].title = t;
    SetTitle(id, t);
}

void ProjectTabBar::Reorder(int from, int to) { MoveTab(from, to); }

int ProjectTabBar::CreateGroup(const wxString &name, const wxColour &color) { return TabStrip::CreateGroup(name, color); }

void ProjectTabBar::AssignGroup(int tab_i, int group_id)
{
    const std::string id = IdAt(tab_i);
    if (!id.empty())
        TabStrip::AssignGroup(id, group_id);
}

void ProjectTabBar::SaveToConfig()
{
    // Keep the payload (file path) current before the layout is written.
    for (const MD3::Tabs::Tab &t : GetModel().tabs()) {
        auto it = m_projects.find(t.id);
        if (it != m_projects.end() && it->second.file_path != t.payload)
            GetModel().find(t.id)->payload = it->second.file_path;
    }
    SaveLayout();
}

void ProjectTabBar::LoadFromConfig()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg)
        return;
    m_projects.clear();

    if (LoadTabsFromLayout() > 0) {
        for (const MD3::Tabs::Tab &t : GetModel().tabs()) {
            ProjectTab tab;
            tab.file_path = t.payload;
            tab.title     = t.title.IsEmpty() ? TitleFromPath(t.payload) : t.title;
            m_projects[t.id] = tab;
            if (t.id.rfind("untitled-", 0) == 0) {
                try {
                    m_next_untitled = std::max(m_next_untitled, std::stoi(t.id.substr(9)) + 1);
                } catch (...) {
                }
            }
        }
        if (GetModel().active().empty() && !GetModel().empty())
            Activate(GetModel().at(0).id, /*emit*/ false);
        return;
    }

    // Legacy import: [tab_groups] id -> "name|#RRGGBB", [project_tabs] index -> "path|group".
    std::map<int, int> legacy_group_ids; // legacy id -> new id
    if (cfg->has_section("tab_groups")) {
        for (const auto &kv : cfg->get_section("tab_groups")) {
            int legacy_id;
            try {
                legacy_id = std::stoi(kv.first);
            } catch (...) {
                continue;
            }
            const std::string &v    = kv.second;
            const size_t       bar  = v.rfind('|');
            const std::string  name = bar == std::string::npos ? v : v.substr(0, bar);
            const std::string  hex  = bar == std::string::npos ? std::string() : v.substr(bar + 1);
            legacy_group_ids[legacy_id] = GetModel().create_group(wxString::FromUTF8(name), wxColour(wxString::FromUTF8(hex)));
        }
    }
    if (cfg->has_section("project_tabs")) {
        std::vector<std::pair<int, std::string>> ordered;
        for (const auto &kv : cfg->get_section("project_tabs")) {
            try {
                ordered.emplace_back(std::stoi(kv.first), kv.second);
            } catch (...) {
            }
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) { return a.first < b.first; });
        for (const auto &e : ordered) {
            const std::string &v        = e.second;
            const size_t       bar      = v.rfind('|');
            const std::string  path     = bar == std::string::npos ? v : v.substr(0, bar);
            int                group_id = -1;
            if (bar != std::string::npos) {
                try {
                    group_id = std::stoi(v.substr(bar + 1));
                } catch (...) {
                }
            }
            const int i = AddTab(path, TitleFromPath(path), /*activate*/ false);
            if (group_id >= 0 && legacy_group_ids.count(group_id))
                AssignGroup(i, legacy_group_ids[group_id]);
        }
    }
    if (!GetModel().empty()) {
        Activate(GetModel().at(0).id, /*emit*/ false);
        SaveToConfig();
    }
}

}} // namespace Slic3r::GUI
