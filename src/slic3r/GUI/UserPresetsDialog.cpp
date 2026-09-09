
#include "UserPresetsDialog.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"

#include <slic3r/GUI/Widgets/CheckBox.hpp>
#include <slic3r/GUI/Widgets/TabCtrl.hpp>
#include <slic3r/GUI/Widgets/SearchField.hpp>

#include "Bulk/BulkActionPlan.hpp"
#include "Bulk/BulkActionPreviewDialog.hpp"
#include "Bulk/BulkRenameDialog.hpp"
#include "NotificationManager.hpp"
#include "Plater.hpp"

#include "Tab.hpp"
#include "MsgDialog.hpp"

#include <wx/dirdlg.h>
#include <wx/filename.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

namespace Slic3r {
namespace GUI {

static void find_compatible_user_presets(PresetCollection const &collection, std::string printer, std::vector<std::string> &presets);

namespace {

// File-system safe stem for an exported preset: every character Windows
// refuses in a file name becomes '_'.
std::string sanitize_file_stem(const std::string &name)
{
    std::string out = name;
    for (char &c : out)
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            static_cast<unsigned char>(c) < 0x20)
            c = '_';
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    return out.empty() ? std::string("preset") : out;
}

void notify(NotificationManager::NotificationLevel level, const wxString &text)
{
    if (Plater *plater = wxGetApp().plater())
        if (NotificationManager *manager = plater->get_notification_manager())
            manager->push_notification(NotificationType::CustomNotification, level, text.ToUTF8().data());
}

} // namespace

UserPresetsDialog::UserPresetsDialog(wxWindow *parent)
    : MD3Dialog(parent, _L("Management user presets"), wxEmptyString, MaterialIcon::Tune)
{
    SetMinSize({FromDIP(788), -1});

    m_tab_ctrl = new TabCtrl(this, wxID_ANY);
    m_tab_ctrl->SetFont(Label::Body_14);
    m_tab_ctrl->AppendItem("");
    m_tab_ctrl->AppendItem("");
    m_tab_ctrl->AppendItem("");
    m_tab_ctrl->SelectItem(0);
    m_tab_ctrl->Bind(wxEVT_TAB_SEL_CHANGED, [this] (auto & evt) { on_collection_changed(evt.GetInt()); });

    m_switch_button = new SwitchButton(this);
    m_switch_button->SetFont(Label::Body_13);
    m_switch_button->SetMaxSize({FromDIP(182), -1});
    m_switch_button->SetLabels(" " + _L("Custom") + " ", _L("Others"));
    m_switch_button->Bind(wxEVT_TOGGLEBUTTON, [this](auto &evt) { evt.Skip(); on_collection_changed(m_collection); });

    // Kit SearchField (r22 pill, sc-highest, leading search glyph) replaces the
    // legacy r12 TextInput + im_text_search raster icon.
    m_search = new SearchField(this, _L("Search"));
    m_search->SetMinSize({FromDIP(568), FromDIP(40)});
    m_search->SetOnQuery([this](const wxString &kw) { on_search(kw); });
    // Re-run the active filter when the regex / case / whole-word chrome toggles.
    m_search->SetOnRegexToggle([this](bool) { on_search(m_search->GetValue()); });

    m_empty_panel = new wxPanel(this);
    m_empty_panel->SetMinSize({-1, FromDIP(360)});
    m_empty_panel->SetMaxSize({-1, FromDIP(360)});
    m_empty_panel->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    {
        wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        wxStaticBitmap *bitmap = new wxStaticBitmap(m_empty_panel, wxID_ANY, create_scaled_bitmap(wxGetApp().dark_mode() ? "preset_empty_dark" : "preset_empty", this, 150));
        sizer->Add(bitmap, 0, wxALIGN_CENTER | wxTOP, FromDIP(70));
        Label *label = new Label(m_empty_panel, _L("No content"));
        label->SetBackgroundColour(this->GetBackgroundColour());
        label->SetForegroundColour(m_empty_panel->GetForegroundColour());
        sizer->Add(label, 0, wxALIGN_CENTER);
        m_empty_panel->SetSizer(sizer);
        m_empty_panel->Hide();
    }

    m_scrolled = new wxScrolledWindow(this);
    m_scrolled->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    m_scrolled->SetScrollbars(0, 100, 1, 2);
    m_scrolled->SetScrollRate(0, 5);
    m_scrolled->SetMinSize({-1, FromDIP(360)});
    m_scrolled->SetMaxSize({-1, FromDIP(360)});
    {
        wxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
        wxSizer *sizerLeft = new wxBoxSizer(wxVERTICAL);
        auto     line      = new StaticLine(m_scrolled, true);
        wxSizer *sizerRight = new wxBoxSizer(wxVERTICAL);
        sizer->Add(sizerLeft, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(20));
        sizer->Add(line, 0, wxEXPAND);
        sizer->Add(sizerRight, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(20));
        m_scrolled->SetSizer(sizer);
    }

    m_check_all = new ::CheckBox(this);
    // TRN: Label beside the master checkbox; toggles every visible preset row.
    auto label = new Label(this, _L("Select visible"));
    m_label_check_count = new Label(this);
    m_label_check_count->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    // Select-scope buttons: their labels state exactly which universe they
    // touch, so "everything" is never ambiguous behind a search filter.
    m_button_select_visible = new Button(this, _L("Select visible"));
    m_button_select_all     = new Button(this, _L("Select all"));
    m_button_invert         = new Button(this, _L("Invert selection"));
    m_button_export         = new Button(this, _L("Export selected..."));
    m_button_rename         = new Button(this, _L("Rename selected..."));
    m_button_delete         = new Button(this, _L("Delete"));
    m_button_delete->SetBorderColorNormal(StateColor::semantic(MD3::Role::Error));
    m_button_delete->SetTextColorNormal(StateColor::semantic(MD3::Role::Error));
    m_button_select_visible->SetToolTip(_L("Select every preset matching the current search (Ctrl+A)"));
    m_button_select_all->SetToolTip(_L("Select every preset in this collection, including rows hidden by the search (Ctrl+Shift+A)"));
    m_button_invert->SetToolTip(_L("Invert the selection within the visible rows (Ctrl+I)"));
    m_button_export->SetToolTip(_L("Write each selected preset as a .json file into a folder you choose"));
    m_button_rename->SetToolTip(_L("Rename the selected presets with a pattern or find/replace"));
    m_check_all->Bind(wxEVT_TOGGLEBUTTON, [this](auto &evt) { evt.Skip(); on_all_checked(evt.IsChecked(), true); });
    label->Bind(wxEVT_LEFT_UP, [this](auto &evt) {
        bool checked = !m_check_all->GetValue();
        m_check_all->SetValue(checked);
        on_all_checked(checked, true);
    });
    m_button_select_visible->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](auto &) { select_visible(); });
    m_button_select_all->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](auto &) { select_all_matches(); });
    m_button_invert->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](auto &) { invert_visible(); });
    m_button_export->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](auto &) { export_selected(); });
    m_button_rename->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](auto &) { rename_selected(); });
    m_button_delete->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](auto &evt) { delete_checked(); });
    // Ctrl+A / Ctrl+Shift+A / Ctrl+I / Delete: the same shortcut set as every
    // other bulk surface. A focused text field keeps its native keys.
    Bind(wxEVT_CHAR_HOOK, &UserPresetsDialog::on_char_hook, this);

    // Body: tab bar + Custom/Others toggle + kit SearchField + list/empty state
    // (the shell already pads the body 24px on each side).
    auto *content = GetContentSizer();
    content->Add(m_tab_ctrl, 0, wxALIGN_CENTER | wxBOTTOM, FromDIP(16));
    content->Add(m_switch_button, 0, wxALIGN_CENTER | wxBOTTOM, FromDIP(10));
    content->Add(m_search, 0, wxALIGN_CENTER | wxBOTTOM, FromDIP(10));
    content->Add(m_scrolled, 1, wxEXPAND);
    content->Add(m_empty_panel, 1, wxEXPAND);

    // Selection scope row under the list: visible / all / invert.
    auto *select_row = new wxBoxSizer(wxHORIZONTAL);
    select_row->Add(m_button_select_visible, 0, wxRIGHT, FromDIP(8));
    select_row->Add(m_button_select_all, 0, wxRIGHT, FromDIP(8));
    select_row->Add(m_button_invert, 0);
    content->Add(select_row, 0, wxTOP, FromDIP(10));

    // Footer action bar: leading select-visible + selection count, trailing
    // Export / Rename / Delete.
    auto *footer = GetFooterSizer();
    footer->Insert(0, m_check_all, 0, wxALIGN_CENTER_VERTICAL);
    footer->Insert(1, label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    footer->Insert(2, m_label_check_count, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    AddFooterButton(m_button_export);
    AddFooterButton(m_button_rename);
    AddFooterButton(m_button_delete);

    wxGetApp().UpdateDlgDarkUI(this);
    m_switch_button->Rescale();

    init_preset_list();
    update_preset_counts();
    on_collection_changed(0);

    Layout();
    Fit();
    CenterOnParent();
    UpdateShape();
}

void UserPresetsDialog::init_preset_list()
{
    auto bundle = wxGetApp().preset_bundle;
    for (PresetCollection *collection : {(PresetCollection *) &bundle->printers, &bundle->filaments, &bundle->prints}) {
        std::vector<std::string> presets;
        for (auto &preset : *collection) {
            if (!preset.is_user()) continue;
            if (collection->type() == Preset::TYPE_FILAMENT) {
                if (preset.base_id.empty()) {
                    auto id = preset.filament_id;
                    m_filament_names[id] = preset.alias;
                    m_filament_presets[id].emplace_back(preset.name);
                    continue;
                }
            }
            presets.push_back(preset.name);
        }
        m_presets.emplace_back(std::move(presets));
    }
}

void UserPresetsDialog::create_preset_list(wxWindow *parent)
{
    if (is_filament_list()) {
        for (auto &filament : m_filament_presets)
            m_filament_sizers.emplace(filament.first, create_filament_group(parent, filament));
    } else {
        auto &presets = m_presets[m_collection];
        for (auto &preset : presets)
            m_preset_sizers.emplace(preset, create_preset_line(m_scrolled, preset));
    }
}

wxSizer *UserPresetsDialog::create_preset_line(wxWindow *parent, std::string const &preset)
{
    wxSizer *vsizer = new wxBoxSizer(wxVERTICAL);
    wxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
    auto check = new ::CheckBox(parent);
    auto label = new Label(parent, from_u8(preset), wxST_ELLIPSIZE_END);
    auto line  = new StaticLine(parent);
    label->SetMaxSize({FromDIP(268), -1});
    label->SetToolTip(label->GetLabel());
    check->Bind(wxEVT_TOGGLEBUTTON, [this, preset](auto &evt) {
        evt.Skip();
        on_preset_checked(preset, evt.IsChecked(), true);
    });
    label->Bind(wxEVT_LEFT_UP, [this, preset, check](auto &evt) {
        bool checked = !check->GetValue();
        check->SetValue(checked);
        on_preset_checked(preset, checked, true);
    });
    hsizer->Add(check, 0, wxALIGN_CENTER | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(6));
    hsizer->Add(label, 1, wxALIGN_CENTER);
    vsizer->Add(hsizer, 0, wxEXPAND);
    vsizer->Add(line, 0, wxEXPAND);
    return vsizer;
}

wxSizer *UserPresetsDialog::create_filament_group(wxWindow *parent, std::pair<std::string const, std::vector<std::string>> const &filament)
{
    wxSizer * vsizer = new wxBoxSizer(wxVERTICAL);
    wxSizer * hsizer = new wxBoxSizer(wxHORIZONTAL);
    auto check  = new ::CheckBox(parent);
    auto label = new Label(parent, from_u8(m_filament_names[filament.first]), wxST_ELLIPSIZE_END);
    auto line  = new StaticLine(parent);
    label->SetMaxSize({-1, FromDIP(268)});
    label->SetToolTip(label->GetLabel());
    check->Bind(wxEVT_TOGGLEBUTTON, [this, filament = filament.first](auto &evt) {
        evt.Skip();
        on_filament_checked(filament, evt.IsChecked(), true);
    });
    label->Bind(wxEVT_LEFT_UP, [this, filament = filament.first, check](auto &evt) {
        bool checked = !check->GetValue();
        check->SetValue(checked);
        on_filament_checked(filament, checked, true);
    });
    hsizer->Add(check, 0, wxALIGN_CENTER | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(6));
    hsizer->Add(label, 1, wxALIGN_CENTER);
    vsizer->Add(hsizer, 0, wxEXPAND);
    vsizer->Add(line, 0, wxEXPAND);
    for (auto preset : filament.second) {
        auto sizer3 = create_preset_line(parent, preset);
        m_preset_sizers.emplace(preset, sizer3);
        vsizer->Add(sizer3, 0, wxEXPAND | wxLEFT, FromDIP(20));
    }
    return vsizer;
}

void UserPresetsDialog::layout_preset_list(bool delete_old)
{
    wxSizer *sizer      = m_scrolled->GetSizer();
    wxSizer *sizerLeft  = sizer->GetItem(size_t(0))->GetSizer();
    wxSizer *sizerRight = sizer->GetItem(size_t(2))->GetSizer();
    if (delete_old) {
        sizerLeft->Clear(true);
        sizerRight->Clear(true);
    } else {
        while (!sizerLeft->IsEmpty()) sizerLeft->Detach(0);
        while (!sizerRight->IsEmpty()) sizerRight->Detach(0);
    }
    sizer = sizerLeft;
    if (is_filament_list()) {
        size_t total = (std::accumulate(m_filament_presets.begin(), m_filament_presets.end(), 0,
                [](size_t t, auto &filament) { return t + filament.second.size() + 1; }) - m_hiden_sizers.size());
        for (auto &filament : m_filament_presets) {
            auto sizer2 = m_filament_sizers[filament.first];
            if (m_hiden_sizers.count(sizer2) > 0)
                continue;
            size_t count = 1 + std::count_if(filament.second.begin(), filament.second.end(), [this](auto & preset) {
                return m_hiden_sizers.count(m_preset_sizers[preset]) == 0;
            });
            if (sizer == sizerLeft) {
                size_t diff = std::abs(int(total - count * 2));
                if (diff > std::abs(int(total)))
                    sizer = sizerRight;
                else
                    total -= count * 2;
            }
            sizer->Add(sizer2, 0, wxEXPAND);
        }
    } else {
        auto & presets = m_presets[m_collection];
        size_t total   = (presets.size() - m_hiden_sizers.size() + 1) / 2;
        for (auto &preset : presets) {
            auto sizer2 = m_preset_sizers[preset];
            if (m_hiden_sizers.count(sizer2) > 0)
                continue;
            sizer->Add(sizer2, 0, wxEXPAND);
            if (--total == 0)
                sizer = sizerRight;
        }
    }
    bool is_empty = sizerLeft->IsEmpty() && sizerRight->IsEmpty();
    m_scrolled->Show(!is_empty);
    m_empty_panel->Show(is_empty);
    m_scrolled->Layout();
    m_empty_panel->Layout();
}

void UserPresetsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    MD3Dialog::on_dpi_changed(suggested_rect); // reshape the rounded frame
    SetMinSize({FromDIP(788), -1});
    m_tab_ctrl->Rescale();
    m_switch_button->SetMaxSize({FromDIP(182), -1});
    m_switch_button->Rescale();
    m_search->SetMinSize({FromDIP(568), FromDIP(40)});
    m_search->Rescale();
    m_scrolled->SetMinSize({-1, FromDIP(320)});
    m_scrolled->SetMaxSize({-1, FromDIP(320)});
    for (auto sizer : m_preset_sizers) {
        auto *label = dynamic_cast<Label *>(sizer.second->GetItem(size_t(0))->GetSizer()->GetItem(size_t(1))->GetWindow());
        label->SetMaxSize({FromDIP(268), -1});
    }
    Layout();
    Fit();
}

bool UserPresetsDialog::is_filament_list() const { return m_collection == 1 && m_switch_button->GetValue() == false; }

void UserPresetsDialog::on_collection_changed(int collection)
{
    std::swap(m_collection, collection);
    m_checked_filaments.clear();
    m_selection.clear();
    m_preset_sizers.clear();
    m_filament_sizers.clear();
    m_hiden_sizers.clear();
    m_tab_ctrl->SetItemBold(collection, false);
    m_tab_ctrl->SetItemTextColour(collection, StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_tab_ctrl->SetItemBold(m_collection, true);
    m_tab_ctrl->SetItemTextColour(m_collection, StateColor::semantic(MD3::Role::Primary));
    m_search->SetValue("");
    m_switch_button->Show(m_collection == 1);
    Freeze();
    create_preset_list(m_scrolled);
    layout_preset_list(true);
    wxGetApp().UpdateDarkUIWin(m_scrolled);
    Thaw();
    update_checked();
    Layout();
    Refresh();
}

void UserPresetsDialog::on_search(wxString const &keyword)
{
    if (keyword.IsEmpty()) {
        for (auto sizer : m_hiden_sizers)
            sizer->Show(true);
        m_hiden_sizers.clear();
        m_scrolled->Freeze();
        layout_preset_list();
        m_scrolled->Thaw();
        update_checked();
        return;
    }
    auto & hiden_sizers = m_hiden_sizers;
    auto show_sizers = [&hiden_sizers](wxSizer* sizer, bool show) {
        auto iter = hiden_sizers.find(sizer);
        if (show) {
            if (iter != hiden_sizers.end()) {
                sizer->Show(true);
                hiden_sizers.erase(iter);
            }
        } else {
            if (iter == hiden_sizers.end()) {
                sizer->Show(false);
                hiden_sizers.insert(sizer);
            }
        }
    };
    m_scrolled->Freeze();
    // Route the preset filter through the shared MD3 matcher so the field's
    // regex / case / whole-word chrome drives it (was a hard-coded
    // case-insensitive substring search). Invalid half-typed regex matches
    // everything, so a partial pattern never blanks the list.
    const bool regex         = m_search->IsRegexEnabled();
    const bool caseSensitive = m_search->IsCaseSensitive();
    const bool wholeWord     = m_search->IsWholeWord();
    const bool multiline     = m_search->IsMultiline();
    SearchField::MatchPass match_pass(keyword, regex, caseSensitive, wholeWord, multiline);
    auto match = [&](std::string & preset) {
        return match_pass.matches(from_u8(preset));
    };
    if (is_filament_list()) {
        for (auto &filament : m_filament_presets) {
            size_t count = 0;
            for (auto &preset : filament.second) {
                bool show = match(preset);
                show_sizers(m_preset_sizers[preset], show);
                if (!show) ++count;
            }
            show_sizers(m_filament_sizers[filament.first], count != filament.second.size());
        }
    } else {
        auto &presets = m_presets[m_collection];
        for (auto &preset : presets)
            show_sizers(m_preset_sizers[preset], match(preset));
    }
    layout_preset_list();
    m_scrolled->Thaw();
    // The selection is kept across a filter change; only the hidden count moves.
    update_checked();
}

std::vector<std::string> UserPresetsDialog::all_ids() const
{
    std::vector<std::string> ids;
    if (is_filament_list()) {
        for (auto &filament : m_filament_presets)
            ids.insert(ids.end(), filament.second.begin(), filament.second.end());
    } else if (m_collection >= 0 && static_cast<size_t>(m_collection) < m_presets.size()) {
        ids = m_presets[m_collection];
    }
    return ids;
}

std::vector<std::string> UserPresetsDialog::visible_ids() const
{
    std::vector<std::string> ids;
    for (const std::string &id : all_ids()) {
        auto it = m_preset_sizers.find(id);
        if (it != m_preset_sizers.end() && m_hiden_sizers.count(it->second) == 0)
            ids.push_back(id);
    }
    return ids;
}

std::vector<std::string> UserPresetsDialog::selected_sorted() const
{
    std::vector<std::string> out = m_selection.ordered_within(all_ids());
    std::sort(out.begin(), out.end());
    return out;
}

void UserPresetsDialog::update_filament_group_checkbox(std::string const &filament)
{
    auto group = m_filament_presets.find(filament);
    auto sizer = m_filament_sizers.find(filament);
    if (group == m_filament_presets.end() || sizer == m_filament_sizers.end())
        return;
    const size_t count = m_selection.count_within(group->second);
    auto *cb = dynamic_cast<::CheckBox *>(sizer->second->GetItem(size_t(0))->GetSizer()->GetItem(size_t(0))->GetWindow());
    if (count == 0) {
        cb->SetValue(false);
        cb->SetHalfChecked(false);
        m_checked_filaments.erase(filament);
    } else if (count == group->second.size()) {
        cb->SetValue(true);
        cb->SetHalfChecked(false);
        m_checked_filaments[filament] = count;
    } else {
        cb->SetValue(false);
        cb->SetHalfChecked(true);
        m_checked_filaments[filament] = count;
    }
}

void UserPresetsDialog::sync_checkboxes()
{
    for (auto &entry : m_preset_sizers) {
        auto *cb = dynamic_cast<::CheckBox *>(entry.second->GetItem(size_t(0))->GetSizer()->GetItem(size_t(0))->GetWindow());
        if (cb)
            cb->SetValue(m_selection.contains(entry.first));
    }
    if (m_collection == 1)
        for (auto &filament : m_filament_presets)
            update_filament_group_checkbox(filament.first);
    update_checked();
}

void UserPresetsDialog::select_visible()
{
    m_selection.select_page(visible_ids());
    sync_checkboxes();
}

void UserPresetsDialog::select_all_matches()
{
    m_selection.select_all_matches(all_ids());
    sync_checkboxes();
}

void UserPresetsDialog::invert_visible()
{
    m_selection.invert(visible_ids());
    sync_checkboxes();
}

void UserPresetsDialog::on_char_hook(wxKeyEvent &event)
{
    // A focused text control keeps Ctrl+A (select text) and Delete.
    const bool text_focused = dynamic_cast<wxTextCtrl *>(wxWindow::FindFocus()) != nullptr;
    if (event.ControlDown() && event.GetKeyCode() == 'A') {
        if (event.ShiftDown()) {
            select_all_matches();
            return;
        }
        if (!text_focused) {
            select_visible();
            return;
        }
    } else if (event.ControlDown() && event.GetKeyCode() == 'I') {
        invert_visible();
        return;
    } else if (event.GetKeyCode() == WXK_DELETE && !text_focused && !m_selection.empty()) {
        delete_checked();
        return;
    }
    event.Skip();
}

void UserPresetsDialog::on_preset_checked(std::string const &preset, bool checked, bool from_user)
{
    if (m_selection.contains(preset) == checked)
        return;
    m_selection.set(preset, checked);
    if (!from_user) {
        auto *cb = dynamic_cast<::CheckBox *>(m_preset_sizers[preset]->GetItem(size_t(0))->GetSizer()->GetItem(size_t(0))->GetWindow());
        cb->SetValue(checked);
    } else {
        if (m_collection == 1) {
            for (auto &filament : m_filament_presets) {
                if (std::find(filament.second.begin(), filament.second.end(), preset) != filament.second.end()) {
                    update_filament_group_checkbox(filament.first);
                    break;
                }
            }
        }
        update_checked();
    }
}

void UserPresetsDialog::on_filament_checked(std::string const &filament, bool checked, bool from_user)
{
    auto iter = m_filament_presets.find(filament);
    for (auto &preset : iter->second)
        on_preset_checked(preset, checked, false);
    if (checked)
        m_checked_filaments[filament] = iter->second.size();
    else
        m_checked_filaments.erase(filament);
    if (!from_user) {
        auto *cb = dynamic_cast<::CheckBox *>(m_filament_sizers[filament]->GetItem(size_t(0))->GetSizer()->GetItem(size_t(0))->GetWindow());
        cb->SetValue(checked);
        cb->SetHalfChecked(false);
    } else {
        update_checked();
    }
}

void UserPresetsDialog::on_all_checked(bool checked, bool from_user)
{
    // The master checkbox is scoped to the visible rows (the page).
    if (checked)
        m_selection.select_page(visible_ids());
    else
        for (const std::string &id : visible_ids())
            m_selection.set(id, false);
    if (!from_user)
        m_check_all->SetValue(checked);
    sync_checkboxes();
}

void UserPresetsDialog::update_preset_counts()
{
    wxString labels[] = {_L("Printer presets (%d)"), _L("Filament presets (%d)"), _L("Process presets (%d)")};
    for (int i = 0; i < 3; ++i) {
        size_t n = i == 1 ? std::accumulate(m_filament_presets.begin(), m_filament_presets.end(), size_t(0),
            [](size_t t, auto &filament) { return t + filament.second.size(); }) : 0;
        if (m_preset_sizers.empty()) {
            m_tab_ctrl->SetItemTextColour(i, StateColor::semantic(MD3::Role::OnSurfaceVariant));
            m_tab_ctrl->SetItemPaddingSize(i, {FromDIP(20), FromDIP(4)});
        }
        m_tab_ctrl->SetItemText(i, wxString::Format(labels[i], int(m_presets[i].size() + n)));
    }
}

void UserPresetsDialog::update_checked()
{
    const std::vector<std::string> all     = all_ids();
    const std::vector<std::string> visible = visible_ids();
    // Drop ids that no longer exist (a deleted or renamed preset).
    m_selection.retain(std::set<std::string>(all.begin(), all.end()));
    const size_t count          = m_selection.size();
    const size_t visible_count  = m_selection.count_within(visible);
    const size_t hidden_count   = count - visible_count;
    if (visible_count == 0) {
        m_check_all->SetValue(false);
        m_check_all->SetHalfChecked(false);
    } else if (visible_count == visible.size()) {
        m_check_all->SetValue(true);
        m_check_all->SetHalfChecked(false);
    } else {
        m_check_all->SetValue(false);
        m_check_all->SetHalfChecked(true);
    }
    wxString counts;
    if (count > 0) {
        counts = wxString::Format(_L("%u Selected"), static_cast<unsigned>(count));
        if (hidden_count > 0)
            // TRN: %u selected presets are hidden by the current search filter.
            counts += wxString::Format(_L(" (%u hidden by the search)"), static_cast<unsigned>(hidden_count));
    }
    m_label_check_count->SetLabel(counts);
    m_button_select_visible->SetLabel(wxString::Format(_L("Select visible (%d)"), static_cast<int>(visible.size())));
    m_button_select_visible->Enable(!visible.empty());
    m_button_select_all->SetLabel(wxString::Format(_L("Select all %d presets"), static_cast<int>(all.size())));
    m_button_select_all->Enable(!all.empty());
    m_button_invert->Enable(!visible.empty());
    m_button_delete->Enable(count > 0);
    m_button_delete->SetToolTip(count > 0 ? _L("Review and delete the selected presets (Delete)")
                                          : _L("Please select the preset to be deleted"));
    m_button_export->Enable(count > 0);
    m_button_rename->Enable(count > 0);
    Layout();
}

void UserPresetsDialog::delete_checked()
{
    if (m_selection.empty())
        return;
    Preset::Type types[] = {Preset::TYPE_PRINTER, Preset::TYPE_FILAMENT, Preset::TYPE_PRINT};
    Tab *tab = wxGetApp().get_tab(types[m_collection % 3]);
    PresetCollection *collection = tab->get_presets();
    const std::string edited = collection->get_edited_preset().name;

    // Build the reviewable plan. A preset is skipped when another preset that
    // is NOT part of this deletion still inherits from it (the same refusal
    // Tab::delete_preset applies); everything else will change.
    Bulk::BulkActionPlan plan;
    plan.action      = _u8L("Delete presets");
    plan.consequence = _u8L("The preset files are removed from the user preset folder and cannot be recovered "
                            "except from a config-profile snapshot. Deleting a custom printer also deletes the "
                            "filament and process presets attached to it.");
    plan.destructive = true;
    for (const std::string &name : m_selection.ordered_within(all_ids())) {
        const Preset *preset = collection->find_preset(name, false);
        if (preset == nullptr) {
            plan.items.push_back(Bulk::BulkItem::skipped(name, _u8L("Preset no longer exists")));
            continue;
        }
        std::string blocker;
        if (collection->get_preset_base(*preset) == preset) {
            for (const Preset &other : *collection) {
                if (other.inherits() == name && !m_selection.contains(other.name)) {
                    blocker = other.name;
                    break;
                }
            }
        }
        if (!blocker.empty()) {
            plan.items.push_back(Bulk::BulkItem::skipped(
                name, (boost::format(_u8L("Inherited by \"%1%\", which is not selected")) % blocker).str()));
            continue;
        }
        std::string detail;
        if (name == edited)
            detail = _u8L("Currently selected in the tab; another preset will be selected");
        if (m_collection == 0 && !preset->is_system) {
            std::vector<std::string> filaments, prints;
            find_compatible_user_presets(wxGetApp().preset_bundle->filaments, name, filaments);
            find_compatible_user_presets(wxGetApp().preset_bundle->prints, name, prints);
            if (!filaments.empty() || !prints.empty())
                detail = (boost::format(_u8L("Also deletes %1% filament and %2% process presets attached to this printer")) %
                          filaments.size() % prints.size()).str();
        }
        plan.items.push_back(Bulk::BulkItem::changed(name, detail));
    }
    if (!Bulk::BulkActionPreviewDialog::Run(this, plan))
        return;

    std::vector<std::string> presets = plan.changed_labels();
    std::sort(presets.begin(), presets.end());
    if (presets.empty())
        return;
    // Everything below already ran through the preview and the two-key gate:
    // the legacy confirmation dialogs must not prompt a second time.
    struct ConfirmedScope {
        bool &flag;
        explicit ConfirmedScope(bool &f) : flag(f) { flag = true; }
        ~ConfirmedScope() { flag = false; }
    } confirmed(m_bulk_confirmed);

    if (!delete_presets(m_collection + 3, presets)) // check only
        return;

    // Collect checked sizer of presets (need the name list, so do it before delete_presets)
    std::set<wxSizer*> checked_sizers;
    for (auto &preset : presets) {
        auto iter = m_preset_sizers.find(preset);
        if (iter == m_preset_sizers.end())
            continue;
        checked_sizers.insert(iter->second);
        m_preset_sizers.erase(iter);
    }

    delete_presets(m_collection, presets); // real delete

    // Collect checked sizer of filaments
    if (is_filament_list()) {
        for (auto &filament : m_checked_filaments) {
            auto iter = m_filament_presets.find(filament.first);
            auto iter2 = m_filament_sizers.find(filament.first);
            if (iter == m_filament_presets.end()) {
                // Collect checked sizer (filament removed)
                checked_sizers.insert(iter2->second);
                while (iter2->second->GetItemCount() > 1)
                    iter2->second->Detach(1);
                m_filament_sizers.erase(iter2);
            } else {
                for (auto sizer : checked_sizers)
                    iter2->second->Detach(sizer);
                // Update check box
                auto *cb = dynamic_cast<::CheckBox *>(iter2->second->GetItem(size_t(0))->GetSizer()->GetItem(size_t(0))->GetWindow());
                cb->SetValue(false);
                cb->SetHalfChecked(false);
            }
        }
        m_checked_filaments.clear();
    }

    update_preset_counts();
    layout_preset_list();
    // Deleted names fall out of the selection (retain), skipped ones stay selected.
    sync_checkboxes();

    for (auto sizer : checked_sizers) {
        sizer->DeleteWindows();
        m_hiden_sizers.erase(sizer);
        delete sizer;
    }
}

void UserPresetsDialog::export_selected()
{
    if (m_selection.empty())
        return;
    Preset::Type types[] = {Preset::TYPE_PRINTER, Preset::TYPE_FILAMENT, Preset::TYPE_PRINT};
    PresetCollection *collection = wxGetApp().get_tab(types[m_collection % 3])->get_presets();

    wxDirDialog dir_dialog(this, _L("Choose the folder to export the selected presets into"), wxEmptyString,
                           wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dir_dialog.ShowModal() != wxID_OK)
        return;
    const boost::filesystem::path folder(dir_dialog.GetPath().ToStdWstring());

    // Plan: one .json per preset, named after the preset; an existing file is
    // never overwritten, the row is skipped and says so.
    Bulk::BulkActionPlan plan;
    plan.action      = _u8L("Export presets");
    plan.consequence = (boost::format(_u8L("Each preset is written as a .json file into %1%. Existing files are left untouched.")) %
                        folder.string()).str();
    std::vector<std::pair<std::string, boost::filesystem::path>> targets;
    for (const std::string &name : m_selection.ordered_within(all_ids())) {
        const boost::filesystem::path path = folder / (sanitize_file_stem(name) + ".json");
        boost::system::error_code ec;
        if (collection->find_preset(name, false) == nullptr) {
            plan.items.push_back(Bulk::BulkItem::skipped(name, _u8L("Preset no longer exists")));
        } else if (boost::filesystem::exists(path, ec)) {
            plan.items.push_back(Bulk::BulkItem::skipped(name, _u8L("A file with this name already exists"), path.filename().string()));
        } else {
            plan.items.push_back(Bulk::BulkItem::changed(name, path.filename().string()));
            targets.emplace_back(name, path);
        }
    }
    if (!Bulk::BulkActionPreviewDialog::Run(this, plan) || targets.empty())
        return;

    bool cancelled = false;
    std::size_t failed = 0;
    const std::size_t done = Bulk::BulkActionPreviewDialog::RunWithProgress(
        this, _L("Exporting presets"), targets.size(),
        [&targets](std::size_t i) { return from_u8(targets[i].first); },
        [&targets, collection](std::size_t i) {
            const Preset *preset = collection->find_preset(targets[i].first, false);
            if (preset == nullptr)
                return false;
            try {
                // Same serialization Preset::save uses for a full (non-diff) user preset.
                preset->config.save_to_json(targets[i].second.string(), preset->name, std::string("User"), preset->version.to_string());
            } catch (const std::exception &err) {
                BOOST_LOG_TRIVIAL(error) << "export preset " << preset->name << " failed: " << err.what();
                return false;
            }
            return true;
        },
        &cancelled, &failed);
    const std::size_t exported = done - failed;
    const std::size_t skipped  = plan.skipped() + (targets.size() - done);
    // TRN: %1$d exported, %2$d skipped (existing file / cancelled), %3$d failed; %4$s folder.
    wxString text = wxString::Format(_L("Exported %d presets, skipped %d, failed %d - %s"),
                                     static_cast<int>(exported), static_cast<int>(skipped), static_cast<int>(failed),
                                     wxString::FromUTF8(folder.string()));
    if (cancelled)
        text += " " + _L("(cancelled)");
    notify(failed > 0 ? NotificationManager::NotificationLevel::WarningNotificationLevel
                      : NotificationManager::NotificationLevel::RegularNotificationLevel, text);
}

void UserPresetsDialog::rename_selected()
{
    if (m_selection.empty())
        return;
    Preset::Type types[] = {Preset::TYPE_PRINTER, Preset::TYPE_FILAMENT, Preset::TYPE_PRINT};
    Tab *tab = wxGetApp().get_tab(types[m_collection % 3]);
    PresetCollection *collection = tab->get_presets();
    const std::string edited = collection->get_edited_preset().name;
    const bool edited_dirty  = collection->current_is_dirty();

    // A rename is "save under the new name, then delete the old file", so it
    // is refused for a preset other presets inherit from (their inherits
    // field would dangle) and for the edited preset while it has unsaved
    // changes (they belong to neither name).
    std::vector<std::string> names;
    std::vector<std::string> refused;
    for (const std::string &name : m_selection.ordered_within(all_ids())) {
        const Preset *preset = collection->find_preset(name, false);
        if (preset == nullptr)
            continue;
        bool inherited = false;
        for (const Preset &other : *collection)
            if (other.inherits() == name) { inherited = true; break; }
        if (inherited || (name == edited && edited_dirty))
            refused.push_back(name);
        else
            names.push_back(name);
    }
    if (names.empty()) {
        notify(NotificationManager::NotificationLevel::WarningNotificationLevel,
               _L("None of the selected presets can be renamed: they are inherited by other presets or have unsaved changes."));
        return;
    }
    std::set<std::string> reserved;
    for (const Preset &other : *collection)
        if (std::find(names.begin(), names.end(), other.name) == names.end())
            reserved.insert(other.name);

    Bulk::RenamePlan rename_plan;
    if (!Bulk::BulkRenameDialog::Run(this, _L("Rename presets"), names, reserved, rename_plan))
        return;
    std::vector<Bulk::RenameRow> rows;
    for (const Bulk::RenameRow &row : rename_plan.rows)
        if (row.outcome == Bulk::RenameOutcome::Changed)
            rows.push_back(row);
    if (rows.empty())
        return;

    std::string target = edited; // the preset the tab shows once the batch is done
    bool cancelled = false;
    std::size_t failed = 0;
    const std::size_t done = Bulk::BulkActionPreviewDialog::RunWithProgress(
        this, _L("Renaming presets"), rows.size(),
        [&rows](std::size_t i) { return from_u8(rows[i].before) + " -> " + from_u8(rows[i].after); },
        [&rows, &target, collection](std::size_t i) {
            const Bulk::RenameRow &row = rows[i];
            Preset *real = collection->find_preset(row.before, false, true);
            if (real == nullptr || !real->is_user() || collection->find_preset(row.after, false) != nullptr)
                return false;
            Preset       copy        = *real;
            const std::string old_setting_id = real->setting_id;
            const std::string old_base_id    = real->base_id;
            // Save the same configuration under the new name (creates the new
            // file and preset entry), keeping the inherits field as it was.
            collection->save_current_preset(row.after, false, false, &copy);
            Preset *renamed = collection->find_preset(row.after, false, true);
            if (renamed == nullptr)
                return false;
            if (renamed->inherits() == row.before) {
                // A base preset: save_current_preset chained it onto the old
                // name, which is about to disappear. Detach and re-save in full.
                renamed->inherits().clear();
                renamed->base_id = old_base_id;
                renamed->save(nullptr);
            }
            // Retire the old name exactly like a delete does (cloud sync included).
            if (!old_setting_id.empty()) {
                collection->set_sync_info_and_save(row.before, old_setting_id, "delete", 0);
                wxGetApp().delete_preset_from_cloud(old_setting_id);
            }
            collection->delete_preset(row.before);
            if (target == row.before)
                target = row.after;
            return true;
        },
        &cancelled, &failed);

    // Re-select in the tab so the sidebar, tab combo and edited copy follow the new names.
    tab->select_preset(target, false, "", true);
    wxGetApp().plater()->sidebar().update_presets(collection->type());
    rebuild_from_bundle();

    const std::size_t renamed = done - failed;
    // TRN: %1$d renamed, %2$d refused (inherited / unsaved), %3$d failed.
    wxString text = wxString::Format(_L("Renamed %d presets, refused %d, failed %d"),
                                     static_cast<int>(renamed), static_cast<int>(refused.size()), static_cast<int>(failed));
    if (cancelled)
        text += " " + _L("(cancelled)");
    notify(failed > 0 ? NotificationManager::NotificationLevel::WarningNotificationLevel
                      : NotificationManager::NotificationLevel::RegularNotificationLevel, text);
}

void UserPresetsDialog::rebuild_from_bundle()
{
    m_presets.clear();
    m_filament_names.clear();
    m_filament_presets.clear();
    m_selection.clear();
    init_preset_list();
    update_preset_counts();
    on_collection_changed(m_collection);
}

}}

#include "Tab.hpp"
#include "MsgDialog.hpp"

namespace Slic3r {
namespace GUI {

static void find_compatible_user_presets(PresetCollection const &collection, std::string printer, std::vector<std::string> &presets)
{
    for (auto& preset : collection) {
        if (!preset.is_user()) continue;
        auto *compatible_printers = dynamic_cast<const ConfigOptionStrings *>(preset.config.option("compatible_printers"));
        if (compatible_printers &&
                std::find(compatible_printers->values.begin(), compatible_printers->values.end(), printer) != compatible_printers->values.end())
            presets.insert(std::lower_bound(presets.begin(), presets.end(), preset.name), preset.name);
    }
}

static void remove_both(std::vector<std::string> &l, std::vector<std::string> &r)
{
    auto i = l.begin();
    auto j = r.begin();
    while (i != l.end() && j != r.end()) {
        if (*i == *j) {
            i = l.erase(i);
            j = r.erase(j);
        } else if (*i < *j) {
            ++i;
        } else {
            ++j;
        }
    }
}

bool UserPresetsDialog::delete_confirm(int collection, int preset_num)
{
    if (m_bulk_confirmed) // the reviewable preview and two-key gate already ran
        return true;
    wxString types[] = {_L("Printer"), _L("Filament"), _L("Process")};
    DeleteConfirmDialog dlg(this, wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Delete"),
                            wxString::Format(_L("%d %s Preset will be deleted."), preset_num, types[collection % 3]));
    int res = dlg.ShowModal();
    return res == wxID_OK;
}

bool UserPresetsDialog::delete_confirm(int collection, int filament_preset_num, int print_preset_num)
{
    if (m_bulk_confirmed) // attached-preset counts were already shown in the preview
        return true;
    DeleteConfirmDialog
        dlg(this, wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Delete"),
            wxString::Format(_L("%d Filament Preset and %d Process Preset is attached to this printer. Those presets would be deleted if the printer is deleted."),
                             filament_preset_num, print_preset_num));
    int res = dlg.ShowModal();
    return res == wxID_OK;
}

bool UserPresetsDialog::delete_presets(int collection, std::vector<std::string> &presets)
{
    Preset::Type types[] = {Preset::TYPE_PRINTER, Preset::TYPE_FILAMENT, Preset::TYPE_PRINT};
    Tab *tab = wxGetApp().get_tab(types[collection % 3]);
    auto collection2 = tab->get_presets();
    // Find attached filaments & print presets for custom printers and delete together
    if (collection == 3) {
        auto filament_presets = std::make_shared<std::vector<std::string>>();
        auto print_presets = std::make_shared<std::vector<std::string>>();
        for (auto &preset : presets) {
            auto preset2 = collection2->find_preset(preset);
            if (!preset2->is_system && collection2->get_preset_base(*preset2) == preset2) { // Root printer preset
                find_compatible_user_presets(wxGetApp().preset_bundle->filaments, preset, *filament_presets);
                find_compatible_user_presets(wxGetApp().preset_bundle->prints, preset, *print_presets);
            }
        }
        if (!filament_presets->empty() || !print_presets->empty()) {
            if (!delete_confirm(collection, int(filament_presets->size()), int(print_presets->size())))
                return false;
            // Remove filaments & print presets attached to current custom printer
            auto current = tab->get_presets()->get_edited_preset().name;
            auto iter = std::lower_bound(presets.begin(), presets.end(), current);
            if (iter != presets.end() && *iter == current) {
                auto preset2 = collection2->find_preset(current);
                if (!preset2->is_system && collection2->get_preset_base(*preset2) == preset2) {
                    std::vector<std::string> filament_presets2;
                    std::vector<std::string> print_presets2;
                    find_compatible_user_presets(wxGetApp().preset_bundle->filaments, current, filament_presets2);
                    find_compatible_user_presets(wxGetApp().preset_bundle->prints, current, print_presets2);
                    remove_both(*filament_presets, filament_presets2);
                    remove_both(*print_presets, print_presets2);
                }
            }
            CallAfter([this, filament_presets, print_presets] {
                delete_presets(1, *filament_presets);
                delete_presets(2, *print_presets);
                update_preset_counts();
            });
            return true;
        }
    }
    if (collection >= 3) {
        return delete_confirm(collection, int(presets.size()));
    }

    // Delete current specially
    auto current = tab->get_presets()->get_edited_preset().name;
    auto iter    = std::lower_bound(presets.begin(), presets.end(), current);
    if (iter != presets.end() && *iter == current)
        iter = presets.erase(iter);
    else
        current.clear();

    // Delete all not current
    for (auto &preset : presets) {
        auto preset2 = collection2->find_preset(preset);
        if (!preset2->setting_id.empty()) {
            BOOST_LOG_TRIVIAL(info) << "delete preset = " << preset << ", setting_id = " << preset2->setting_id;
            collection2->set_sync_info_and_save(preset, preset2->setting_id, "delete", 0);
            wxGetApp().delete_preset_from_cloud(preset2->setting_id);
        }
        collection2->delete_preset(preset);
    }
    // Delete current
    if (!current.empty())
        tab->select_preset("", true);
    else
        wxGetApp().plater()->sidebar().update_presets(collection2->type());

    // Remove from preset/filament list
    if (!current.empty())
        presets.insert(iter, current);
    remove_both(m_presets[collection], presets);
    if (collection == 1) {
        auto iter = m_filament_presets.begin();
        for (; iter != m_filament_presets.end(); ) {
            remove_both(iter->second, presets);
            if (iter->second.empty())
                iter = m_filament_presets.erase(iter);
            else
                ++iter;
        }
    }
    assert(presets.empty());
    return true;
}

}}
