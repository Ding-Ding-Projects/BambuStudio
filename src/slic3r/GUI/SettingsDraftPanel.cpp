#include "Widgets/MD3ScrolledWindow.hpp"
#include "SettingsDraftPanel.hpp"
#include "LocalConfigHistory.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "ProjectTabBar.hpp"
#include "Plater.hpp"
#include "Tab.hpp"
#include "OptionsGroup.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"
#include "Widgets/TabStrip.hpp"
#include "Widgets/TabStripDialogs.hpp"
#include "Widgets/MD3Dialog.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/TextInput.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/ListBox.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/PresetBundle.hpp"
#include <wx/scrolwin.h>
#include <wx/textdlg.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <stdexcept>
#include <algorithm>

namespace Slic3r { namespace GUI {
namespace {
std::vector<SettingsDraftPanel*> draft_panels;
Tab *live_tab(Preset::Type type) { return wxGetApp().get_tab(type); }
wxString draft_title(Preset::Type type) {
    return type == Preset::TYPE_PRINT ? _L("Process draft") :
           type == Preset::TYPE_FILAMENT ? _L("Ink preset draft") : _L("Printer preset draft");
}
}
SettingsDraftStore::Target SettingsDraftPanel::Target(Preset::Type type) const {
    SettingsDraftStore::Target target;
    if (wxGetApp().mainframe && wxGetApp().mainframe->project_tabbar())
        target.project_id = wxGetApp().mainframe->project_tabbar()->ActiveId();
    if (auto *tab = live_tab(type)) target.target_id = tab->get_presets()->get_selected_preset().name;
    return target;
}
SettingsDraftPanel::SettingsDraftPanel(wxWindow *parent, TabStrip *strip, std::function<void(bool)> show_page)
    : wxPanel(parent), m_strip(strip), m_show_page(std::move(show_page)),
      m_storage_key(strip->GetOptions().surface_key) {
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    SetSizer(new wxBoxSizer(wxVERTICAL));
    if (wxGetApp().app_config) m_store.restore(wxGetApp().app_config->get("settings_drafts", m_storage_key));
    for (const auto &id : m_store.ids()) {
        const auto *draft = m_store.find(id);
        m_strip->AddTab(id, draft_title(draft->type) + ": " + wxString::FromUTF8(draft->source_preset_name) + " [" + wxString::FromUTF8(id.substr(id.size() - 4)) + "]");
        m_strip->SetDirty(id, m_store.dirty(id));
    }
    m_strip->Bind(EVT_TABSTRIP_NEW, [this](wxCommandEvent &) { OpenPicker(); });
    m_strip->Bind(EVT_TABSTRIP_CLOSE_REQUEST, [this](wxCommandEvent &e) {
        const auto id = std::string(e.GetString().ToUTF8());
        if (!m_store.find(id)) { m_strip->SetHidden(id, true); return; }
        if (m_store.dirty(id) && md3_message_box(_L("Discard this draft's unsaved changes?"), _L("Close draft"),
                                               wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;
        if (m_active == id) { ++m_view_generation; m_groups.clear(); GetSizer()->Clear(true); }
        m_store.erase(id); m_strip->RemoveTab(id); Persist();
        if (m_active == id) { m_active.clear(); m_show_page(false); }
    });
    draft_panels.push_back(this);
    Hide();
}
SettingsDraftPanel::~SettingsDraftPanel() {
    draft_panels.erase(std::remove(draft_panels.begin(), draft_panels.end(), this), draft_panels.end());
}
bool SettingsDraftPanel::RestoreHistory(const std::string &id, const DynamicPrintConfig &config) {
    for (auto *panel : draft_panels) {
        auto *draft = panel->m_store.find(id);
        if (!draft || !(draft->target == panel->Target(draft->type))) continue;
        // Restore only the detached editor. Applying it remains a separate action.
        draft->config = LocalConfigHistory::merge_restored_config(draft->config, config);
        panel->m_active = id;
        panel->m_strip->Activate(id);
        panel->Rebuild();
        panel->m_show_page(true);
        panel->Persist();
        return true;
    }
    return false;
}
void SettingsDraftPanel::Persist() {
    if (wxGetApp().app_config) {
        wxGetApp().app_config->set("settings_drafts", m_storage_key, m_store.serialize());
        wxGetApp().app_config->save();
    }
    if (!m_active.empty()) {
        m_strip->SetDirty(m_active, m_store.dirty(m_active));
        if (const auto *draft = m_store.find(m_active))
            LocalConfigHistory::record_config("draft", draft->id, "Edit settings draft", draft->config);
    }
}
bool SettingsDraftPanel::Activate(const std::string &id) {
    if (!m_store.find(id)) {
        // Hand the body back only when a draft page was up: switching between
        // ordinary sections must not resize the sidebar.
        if (IsShown()) m_show_page(false);
        return false;
    }
    if (m_active != id) { m_active = id; Rebuild(); }
    m_show_page(true); return true;
}
bool SettingsDraftPanel::Restore(const std::string &id) {
    if (!m_store.find(id)) return false;
    m_active = id; Rebuild(); return true;
}
void SettingsDraftPanel::Rebuild() {
    ++m_view_generation;
    m_groups.clear();
    GetSizer()->Clear(true);
    auto *draft = m_store.find(m_active);
    if (!draft) return;
    auto *caption = new Label(this, draft_title(draft->type));
    GetSizer()->Add(caption, 0, wxEXPAND | wxALL, FromDIP(8));
    GetSizer()->Add(new Label(this, _L("Edits stay in this draft until Apply. Saving a preset does not activate it.")),
                    0, wxEXPAND | wxALL, FromDIP(8));
    auto *search = new SearchField(this, _L("Search draft settings"));
    GetSizer()->Add(search, 0, wxEXPAND | wxALL, FromDIP(8));
    auto *scroll = new MD3ScrolledWindow(this); scroll->SetScrollRate(0, FromDIP(12));
    auto *rows = new wxBoxSizer(wxVERTICAL); scroll->SetSizer(rows);
    std::vector<std::pair<std::string, std::shared_ptr<ConfigOptionsGroup>>> fields;
    for (const auto &key : draft->config.keys()) {
        if (!draft->config.def()->get(key)) continue;
        auto group = std::make_shared<ConfigOptionsGroup>(scroll, wxString::FromUTF8(key), &draft->config, false);
        group->label_width = 15;
        group->m_get_initial_config = [this] { return m_store.find(m_active)->baseline; };
        group->m_on_change = [this](const t_config_option_key &, const boost::any &) { Persist(); };
        const auto *vector = dynamic_cast<const ConfigOptionVectorBase *>(draft->config.option(key));
        if (vector && vector->size() > 0) {
            for (size_t i = 0; i < vector->size(); ++i) group->append_single_option_line(key, {}, int(i));
        } else group->append_single_option_line(key);
        group->activate(); group->reload_config();
        rows->Add(group->sizer, 0, wxEXPAND | wxALL, FromDIP(4));
        fields.emplace_back(key, group); m_groups.push_back(group);
    }
    auto filter = [search, fields, scroll](const wxString &query) {
        auto matcher = MatcherForField(search);
        for (const auto &field : fields) field.second->sizer->ShowItems(query.empty() || matcher(query, wxString::FromUTF8(field.first)));
        scroll->GetSizer()->Layout(); scroll->FitInside();
    };
    search->SetValue(wxString::FromUTF8(draft->search_query));
    search->SetOnQuery([this, filter](const wxString &query) {
        if (auto *current = m_store.find(m_active)) current->search_query = std::string(query.ToUTF8());
        filter(query); Persist();
    });
    search->SetOnRegexToggle([search, filter](bool) { filter(search->GetValue()); });
    filter(search->GetValue());
    const auto edited_id = m_active;
    const auto view_generation = m_view_generation;
    const auto remember_scroll = [this, scroll, edited_id, view_generation](wxScrollWinEvent &event) {
        event.Skip();
        CallAfter([this, scroll, edited_id, view_generation] {
            if (view_generation != m_view_generation || edited_id != m_active || !m_store.find(edited_id)) return;
            int x, y; scroll->GetViewStart(&x, &y); m_store.find(edited_id)->scroll_y = y; Persist();
        });
    };
    scroll->Bind(wxEVT_SCROLLWIN_TOP, remember_scroll);
    scroll->Bind(wxEVT_SCROLLWIN_BOTTOM, remember_scroll);
    scroll->Bind(wxEVT_SCROLLWIN_LINEUP, remember_scroll);
    scroll->Bind(wxEVT_SCROLLWIN_LINEDOWN, remember_scroll);
    scroll->Bind(wxEVT_SCROLLWIN_PAGEUP, remember_scroll);
    scroll->Bind(wxEVT_SCROLLWIN_PAGEDOWN, remember_scroll);
    scroll->Bind(wxEVT_SCROLLWIN_THUMBTRACK, remember_scroll);
    scroll->Bind(wxEVT_SCROLLWIN_THUMBRELEASE, remember_scroll);
    GetSizer()->Add(scroll, 1, wxEXPAND);
    auto *actions = new wxWrapSizer(wxHORIZONTAL);
    auto button = [this, actions](const wxString &label, std::function<void()> action) {
        auto *b = new Button(this, label); actions->Add(b, 0, wxALL, FromDIP(4));
        b->Bind(wxEVT_BUTTON, [action](wxCommandEvent &) { action(); });
    };
    button(_L("Apply"), [this] { Apply(); });
    button(_L("Undo Apply"), [this] { UndoApply(); });
    button(_L("Save as preset"), [this] { SaveAs(); });
    button(_L("Duplicate"), [this] {
        const auto id = m_store.duplicate(m_active); const auto *copy = m_store.find(id);
        if (!copy) return;
        m_strip->AddTab(id, draft_title(copy->type) + " [" + wxString::FromUTF8(id.substr(id.size() - 4)) + "]", {}, true); Persist();
    });
    GetSizer()->Add(actions, 0, wxEXPAND); Layout(); scroll->FitInside();
    scroll->Scroll(0, draft->scroll_y);
}
void SettingsDraftPanel::OpenPicker() {
    MD3Dialog dialog(this, _L("Open page or create draft"), _L("Existing pages remain shared views. Draft settings are independent."), MaterialIcon::Add);
    auto *search = new SearchField(&dialog, _L("Search pages and draft types"));
    auto *list = new ListBox(&dialog, wxID_ANY);
    auto *body = dialog.GetContentSizer(); body->Add(search, 0, wxEXPAND | wxALL, FromDIP(8));
    body->Add(list, 1, wxEXPAND | wxALL, FromDIP(8));
    std::vector<std::pair<std::string, wxString>> choices;
    for (const auto &tab : m_strip->GetModel().tabs()) choices.emplace_back(tab.id, tab.title);
    choices.emplace_back("new:print", _L("New Process draft"));
    choices.emplace_back("new:filament", _L("New Ink preset draft"));
    choices.emplace_back("new:printer", _L("New printer preset draft"));
    std::vector<int> visible;
    auto filter = [&](const wxString &query) {
        list->Clear(); visible.clear(); auto match = MatcherForField(search);
        for (size_t i = 0; i < choices.size(); ++i) if (query.empty() || match(query, choices[i].second)) {
            list->Append(choices[i].second); visible.push_back(int(i));
        }
        if (!visible.empty()) list->SetSelection(0);
    };
    search->SetOnQuery(filter); search->SetOnRegexToggle([&](bool) { filter(search->GetValue()); }); filter({});
    auto accept = [&](wxCommandEvent &) { if (list->GetSelection() >= 0) dialog.EndModal(wxID_OK); };
    list->Bind(wxEVT_LISTBOX_DCLICK, accept);
    auto *open = new Button(&dialog, _L("Open")); body->Add(open, 0, wxALL, FromDIP(8)); open->Bind(wxEVT_BUTTON, accept);
    dialog.SetSize(FromDIP(wxSize(520, 440))); PlaceDialogBesideAnchor(&dialog, m_strip);
    if (dialog.ShowModal() != wxID_OK || list->GetSelection() < 0) return;
    auto id = choices[visible[list->GetSelection()]].first;
    if (id.find("new:") == 0) {
        auto type = id == "new:print" ? Preset::TYPE_PRINT : id == "new:filament" ? Preset::TYPE_FILAMENT : Preset::TYPE_PRINTER;
        auto *tab = live_tab(type); if (!tab) return;
        const auto target = Target(type);
        if (target.project_id.empty()) { md3_message_box(_L("Open a project before creating a settings draft."), _L("Create draft"), wxOK, this); return; }
        id = m_store.create(type, tab->get_presets()->get_selected_preset().name, *tab->get_config(), target);
        if (id.empty()) return;
        m_strip->AddTab(id, draft_title(type) + " [" + wxString::FromUTF8(id.substr(id.size() - 4)) + "]"); Persist();
    }
    m_strip->Activate(id);
}
void SettingsDraftPanel::Apply() {
    auto *draft = m_store.find(m_active); if (!draft) return;
    auto *tab = live_tab(draft->type); if (!tab) return;
    const auto result = m_store.prepare_apply(m_active, *tab->get_config(), Target(draft->type));
    if (!result.ready) { md3_message_box(_L("The target changed. Create a new draft from the current target before applying."), _L("Draft conflict"), wxOK | wxICON_WARNING, this); return; }
    wxString preview;
    for (const auto &key : result.changed_keys) {
        const auto *before = draft->baseline.option(key);
        const auto *after = draft->config.option(key);
        preview += wxString::FromUTF8(key) + ": " + wxString::FromUTF8(before ? before->serialize() : "(missing)") + " -> " + wxString::FromUTF8(after ? after->serialize() : "(removed)") + "\n";
    }
    if (preview.empty()) return;
    if (md3_message_box(_L("Apply these changed settings?\n") + preview, _L("Apply draft"), wxYES_NO | wxNO_DEFAULT, this) != wxYES) return;
    // Recheck after the nested confirmation event loop.
    const auto confirmed = m_store.prepare_apply(m_active, *tab->get_config(), Target(draft->type));
    if (!confirmed.ready) return;
    const DynamicPrintConfig before = *tab->get_config();
    wxGetApp().plater()->take_snapshot("Apply settings draft");
    try {
        DynamicPrintConfig applied(before);
        applied.apply(confirmed.delta);
        for (const auto &key : confirmed.removed_keys) applied.erase(key);
        *tab->get_config() = std::move(applied);
        tab->update_dirty();
        tab->reload_config();
        tab->update();
        wxGetApp().plater()->on_config_change(wxGetApp().preset_bundle->full_config());
    } catch (...) {
        *tab->get_config() = before;
        tab->update_dirty();
        tab->reload_config();
        tab->update();
        wxGetApp().plater()->on_config_change(wxGetApp().preset_bundle->full_config());
        md3_message_box(_L("Apply could not finish. Previous settings were restored."), _L("Apply draft"), wxOK | wxICON_ERROR, this);
        return;
    }
    m_undo_before = before; m_undo_after = *tab->get_config();
    m_undo_type = draft->type; m_undo_target = Target(draft->type);
    draft->baseline = *tab->get_config();
    for (const auto &key : draft->baseline.keys())
        if (!SettingsDraftStore::permitted_key(key)) draft->baseline.erase(key);
    draft->config = draft->baseline;
    draft->baseline_fingerprint = SettingsDraftStore::fingerprint(draft->baseline); Persist();
}
void SettingsDraftPanel::UndoApply() {
    if (m_undo_type == Preset::TYPE_INVALID) return;
    auto *tab = live_tab(m_undo_type);
    if (!tab || !(Target(m_undo_type) == m_undo_target) ||
        SettingsDraftStore::fingerprint(*tab->get_config()) != SettingsDraftStore::fingerprint(m_undo_after)) {
        md3_message_box(_L("Undo Apply is unavailable because the target settings changed."), _L("Undo Apply"), wxOK, this); return;
    }
    wxGetApp().plater()->take_snapshot("Undo settings draft apply");
    *tab->get_config() = m_undo_before;
    tab->update_dirty();
    tab->reload_config();
    tab->update();
    wxGetApp().plater()->on_config_change(wxGetApp().preset_bundle->full_config());
    m_undo_type = Preset::TYPE_INVALID;
}
void SettingsDraftPanel::SaveAs() {
    auto *draft = m_store.find(m_active); if (!draft) return;
    GroupNameDialog prompt(this, _L("Save draft as preset"), wxEmptyString);
    if (prompt.ShowModal() != wxID_OK) return;
    const auto name = std::string(prompt.GetValue().Trim().ToUTF8());
    if (name.empty() || name.find_first_of("/\\:*?\"<>|") != std::string::npos) return;
    auto *tab = live_tab(draft->type); if (!tab) return;
    auto *collection = tab->get_presets();
    if (collection->find_preset(name, false)) { md3_message_box(_L("A preset with this name already exists."), _L("Save draft"), wxOK, this); return; }
    Preset detached = collection->get_edited_preset();
    detached.config = draft->config; detached.name = name; detached.vendor = nullptr;
    detached.is_system = false; detached.is_default = false; detached.is_external = false;
    detached.is_project_embedded = false;
    if (draft->type == Preset::TYPE_PRINT) detached.config.set("print_settings_id", name);
    else if (draft->type == Preset::TYPE_PRINTER) detached.config.set("printer_settings_id", name);
    else detached.config.set_key_value("filament_settings_id", new ConfigOptionStrings({name}));
    detached.inherits().clear(); detached.alias.clear(); detached.setting_id.clear();
    detached.file = collection->path_for_preset(detached);
    try {
        if (!detached.save(nullptr)) throw std::runtime_error("Unable to save preset");
        collection->load_preset(detached.file, name, detached.config, false);
        LocalConfigHistory::record_config("preset", std::to_string(int(draft->type)) + ":" + name, "Save draft as preset", detached.config);
        md3_message_box(_L("Preset saved. The live selection is unchanged."), _L("Save draft"), wxOK, this);
    } catch (...) { md3_message_box(_L("The preset could not be saved. Your draft is retained."), _L("Save draft"), wxOK | wxICON_ERROR, this); }
}
}}
