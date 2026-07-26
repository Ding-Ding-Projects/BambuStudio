#include "BulkFilamentDialog.hpp"

#include <algorithm>

#include <boost/format.hpp>

#include <wx/sizer.h>
#include <wx/scrolwin.h>
#include <wx/statbmp.h>

#include "libslic3r/Preset.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Utils.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "SingleChoiceDialog.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3ColorPicker.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/SpinInput.hpp"

namespace Slic3r { namespace GUI {

// Staged-value placeholder shown before a preset / colour is picked.
static const wxString kNotStaged = wxString::FromUTF8("\xE2\x80\x94"); // em dash

BulkFilamentDialog::BulkFilamentDialog(wxWindow* parent,
                                       const std::vector<std::string>& physical_colors,
                                       const std::vector<std::string>& physical_names,
                                       size_t current_filament_count,
                                       size_t max_filament_count)
    : DPIDialog(parent, wxID_ANY, _L("Bulk filament actions"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
    , m_physical_colors(physical_colors)
    , m_physical_names(physical_names)
{
    m_max_addable = max_filament_count > current_filament_count ? max_filament_count - current_filament_count : 0;

    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    SetMinSize(wxSize(FromDIP(420), -1));
    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % Slic3r::resources_dir()).str();
    SetIcon(wxIcon(Slic3r::encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    collect_preset_choices();

    auto main_sizer = new wxBoxSizer(wxVERTICAL);

    auto hint = new Label(this, _L("Actions apply to the checked filament slots."));
    hint->SetFont(Label::Body_12);
    hint->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    main_sizer->Add(hint, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // ---- Select all/none toggle ----
    {
        auto row = new wxBoxSizer(wxHORIZONTAL);
        m_chk_select_all = new CheckBox(this);
        m_chk_select_all->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) {
            e.Skip();
            set_all_rows(m_chk_select_all->GetValue());
        });
        row->Add(m_chk_select_all, 0, wxALIGN_CENTER_VERTICAL);
        auto lbl = new Label(this, _L("Select all"));
        lbl->SetFont(Label::Body_13);
        lbl->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        lbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
            bool target = !m_chk_select_all->GetValue();
            m_chk_select_all->SetValue(target);
            set_all_rows(target);
        });
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        main_sizer->Add(row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));
    }

    // ---- Physical slot list: checkbox + swatch(+index) + preset name ----
    {
        auto scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxVSCROLL);
        scroll->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
        scroll->SetScrollRate(0, FromDIP(8));
        auto rows_sizer = new wxBoxSizer(wxVERTICAL);

        for (size_t i = 0; i < m_physical_names.size(); ++i) {
            auto row = new wxBoxSizer(wxHORIZONTAL);

            auto chk = new CheckBox(scroll);
            chk->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) {
                e.Skip();
                sync_select_all_state();
                update_apply_enabled();
            });
            m_row_checks.push_back(chk);
            row->Add(chk, 0, wxALIGN_CENTER_VERTICAL);

            // Sidebar-identical swatch, numbered with the 1-based slot index.
            const std::string color_hex = i < m_physical_colors.size() ? m_physical_colors[i] : std::string("#D9D9D9");
            int swatch_sz = FromDIP(20);
            wxBitmap* swatch = get_extruder_color_icon(color_hex, std::to_string(i + 1), swatch_sz, swatch_sz);
            if (swatch && swatch->IsOk()) {
                auto bmp = new wxStaticBitmap(scroll, wxID_ANY, *swatch);
                row->Add(bmp, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
            }

            auto name = new Label(scroll, wxString::FromUTF8(m_physical_names[i]));
            name->SetFont(Label::Body_13);
            name->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
            name->Bind(wxEVT_LEFT_DOWN, [this, chk](wxMouseEvent&) {
                chk->SetValue(!chk->GetValue());
                sync_select_all_state();
                update_apply_enabled();
            });
            row->Add(name, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));

            rows_sizer->Add(row, 0, wxEXPAND | wxTOP, i == 0 ? 0 : FromDIP(6));
        }

        scroll->SetSizer(rows_sizer);
        scroll->FitInside();
        int content_h = rows_sizer->GetMinSize().GetHeight();
        scroll->SetMinSize(wxSize(-1, std::min(content_h, FromDIP(220))));
        main_sizer->Add(scroll, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));
    }

    // Shared outlined-action-button chrome (PurgeModeDialog cancel recipe).
    StateColor action_btn_bg(
        std::pair<wxColour, int>(ThemeColor::Grey350, StateColor::Pressed),
        std::pair<wxColour, int>(ThemeColor::Grey300, StateColor::Hovered),
        std::pair<wxColour, int>(ThemeColor::White, StateColor::Normal));
    StateColor action_btn_bd(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Outline), StateColor::Normal));
    StateColor action_btn_text(
        std::pair<wxColour, int>(ThemeColor::TextPrimary, StateColor::Normal));

    auto style_action_btn = [&](Button* btn) {
        btn->SetMinSize(wxSize(FromDIP(110), FromDIP(24)));
        btn->SetCornerRadius(FromDIP(12));
        btn->SetBackgroundColor(action_btn_bg);
        btn->SetFont(Label::Body_12);
        btn->SetBorderColor(action_btn_bd);
        btn->SetTextColor(action_btn_text);
    };

    // ---- Action: set preset ----
    {
        auto row = new wxBoxSizer(wxHORIZONTAL);
        m_btn_set_preset = new Button(this, _L("Set preset..."));
        style_action_btn(m_btn_set_preset);
        m_btn_set_preset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { on_pick_preset(); });
        row->Add(m_btn_set_preset, 0, wxALIGN_CENTER_VERTICAL);
        m_lbl_staged_preset = new Label(this, kNotStaged);
        m_lbl_staged_preset->SetFont(Label::Body_13);
        m_lbl_staged_preset->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        row->Add(m_lbl_staged_preset, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
        main_sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));
        if (m_preset_full_names.empty())
            m_btn_set_preset->Enable(false);
    }

    // ---- Action: set colour ----
    {
        auto row = new wxBoxSizer(wxHORIZONTAL);
        m_btn_set_color = new Button(this, _L("Set color..."));
        style_action_btn(m_btn_set_color);
        m_btn_set_color->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { on_pick_color(); });
        row->Add(m_btn_set_color, 0, wxALIGN_CENTER_VERTICAL);
        m_lbl_staged_color = new Label(this, kNotStaged);
        m_lbl_staged_color->SetFont(Label::Body_13);
        m_lbl_staged_color->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        row->Add(m_lbl_staged_color, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
        main_sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    }

    // ---- Action: delete checked slots ----
    {
        auto row = new wxBoxSizer(wxHORIZONTAL);
        m_chk_delete = new CheckBox(this);
        m_chk_delete->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) {
            e.Skip();
            update_apply_enabled();
        });
        row->Add(m_chk_delete, 0, wxALIGN_CENTER_VERTICAL);
        auto lbl = new Label(this, _L("Delete selected filaments"));
        lbl->SetFont(Label::Body_13);
        lbl->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        lbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
            m_chk_delete->SetValue(!m_chk_delete->GetValue());
            update_apply_enabled();
        });
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        main_sizer->Add(row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

        auto min_hint = new Label(this, _L("At least one filament always remains."));
        min_hint->SetFont(Label::Body_12);
        min_hint->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        main_sizer->Add(min_hint, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20 + 28));
    }

    // ---- Action: add N filaments ----
    {
        auto row = new wxBoxSizer(wxHORIZONTAL);
        m_chk_add = new CheckBox(this);
        m_chk_add->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) {
            e.Skip();
            if (m_spin_add) m_spin_add->Enable(m_chk_add->GetValue());
            update_apply_enabled();
        });
        row->Add(m_chk_add, 0, wxALIGN_CENTER_VERTICAL);
        auto lbl = new Label(this, _L("Add filaments"));
        lbl->SetFont(Label::Body_13);
        lbl->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        lbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
            if (!m_chk_add->IsEnabled()) return;
            m_chk_add->SetValue(!m_chk_add->GetValue());
            if (m_spin_add) m_spin_add->Enable(m_chk_add->GetValue());
            update_apply_enabled();
        });
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));

        m_spin_add = new SpinInput(this, "1", wxEmptyString, wxDefaultPosition,
                                   wxSize(FromDIP(96), FromDIP(24)), wxTE_PROCESS_ENTER,
                                   1, m_max_addable > 0 ? (int)m_max_addable : 1, 1);
        m_spin_add->Enable(false);
        row->Add(m_spin_add, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
        main_sizer->Add(row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

        if (m_max_addable == 0) {
            // Already at the EnforcerBlockerType::ExtruderMax capacity.
            m_chk_add->Enable(false);
            m_spin_add->Enable(false);
        }
    }

    // ---- Footer (PurgeModeDialog recipe: MD3 filled Apply / outlined Cancel) ----
    auto btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();

    StateColor ok_btn_bg(
        std::pair<wxColour, int>(ThemeColor::BrandGreenPressed, StateColor::Pressed),
        std::pair<wxColour, int>(ThemeColor::BrandGreenHovered, StateColor::Hovered),
        std::pair<wxColour, int>(ThemeColor::BrandGreen, StateColor::Normal));
    StateColor ok_btn_text(
        std::pair<wxColour, int>(ThemeColor::White, StateColor::Normal));

    m_btn_apply = new Button(this, _L("Apply"));
    m_btn_apply->SetMinSize(wxSize(FromDIP(62), FromDIP(24)));
    m_btn_apply->SetCornerRadius(FromDIP(12));
    m_btn_apply->SetBackgroundColor(ok_btn_bg);
    m_btn_apply->SetFont(Label::Body_12);
    m_btn_apply->SetBorderColor(ThemeColor::BrandGreen);
    m_btn_apply->SetTextColor(ok_btn_text);
    m_btn_apply->SetId(wxID_OK);

    auto cancel_btn = new Button(this, _L("Cancel"));
    cancel_btn->SetMinSize(wxSize(FromDIP(62), FromDIP(24)));
    cancel_btn->SetCornerRadius(FromDIP(12));
    cancel_btn->SetBackgroundColor(action_btn_bg);
    cancel_btn->SetFont(Label::Body_12);
    cancel_btn->SetBorderColor(action_btn_bd);
    cancel_btn->SetTextColor(action_btn_text);
    cancel_btn->SetId(wxID_CANCEL);

    btn_sizer->Add(m_btn_apply, 0, wxRIGHT, FromDIP(12));
    btn_sizer->Add(cancel_btn, 0);

    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, FromDIP(20));

    update_apply_enabled();

    SetSizer(main_sizer);
    Fit();
    wxGetApp().UpdateDlgDarkUI(this);
    MD3DialogCaption::Adopt(this);
    CenterOnParent();
}

void BulkFilamentDialog::collect_preset_choices()
{
    m_preset_display.clear();
    m_preset_full_names.clear();
    const PresetCollection& filaments = wxGetApp().preset_bundle->filaments;
    for (auto it = filaments.begin(); it != filaments.end(); ++it) {
        const Preset& preset = *it;
        if (!preset.is_visible || !preset.is_compatible)
            continue;
        m_preset_full_names.push_back(preset.name);
        m_preset_display.push_back(preset.alias.empty() ? preset.name : preset.alias);
    }
}

void BulkFilamentDialog::on_pick_preset()
{
    if (m_preset_full_names.empty())
        return;

    wxArrayString choices;
    int initial = 0;
    for (size_t i = 0; i < m_preset_display.size(); ++i) {
        choices.Add(wxString::FromUTF8(m_preset_display[i]));
        if (!m_staged_preset.empty() && m_preset_full_names[i] == m_staged_preset)
            initial = (int)i;
    }

    SingleChoiceDialog dlg(_L("Choose a filament preset"), _L("Bulk filament actions"), choices, initial, this);
    int sel = dlg.GetSingleChoiceIndex();
    if (sel >= 0 && sel < (int)m_preset_full_names.size()) {
        m_staged_preset = m_preset_full_names[sel];
        update_staged_labels();
        update_apply_enabled();
    }
}

void BulkFilamentDialog::on_pick_color()
{
    wxColour initial(0xD9, 0xD9, 0xD9);
    if (!m_staged_color.empty())
        initial = wxColour(m_staged_color);
    else {
        // Default the picker to the first checked slot's colour.
        for (size_t i = 0; i < m_row_checks.size(); ++i) {
            if (m_row_checks[i]->GetValue() && i < m_physical_colors.size()) {
                wxColour c(m_physical_colors[i]);
                if (c.IsOk()) initial = c;
                break;
            }
        }
    }

    MD3ColorPickerDialog dlg(this, initial);
    if (dlg.ShowModal() == wxID_OK) {
        m_staged_color = dlg.GetColour().GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
        update_staged_labels();
        update_apply_enabled();
    }
}

void BulkFilamentDialog::set_all_rows(bool checked)
{
    m_syncing_checks = true;
    for (auto* chk : m_row_checks)
        chk->SetValue(checked);
    m_syncing_checks = false;
    sync_select_all_state();
    update_apply_enabled();
}

void BulkFilamentDialog::sync_select_all_state()
{
    if (m_syncing_checks || !m_chk_select_all)
        return;
    size_t checked = checked_row_count();
    if (checked == 0) {
        // CheckBox::SetValue does not clear the indeterminate flag; do both.
        m_chk_select_all->SetHalfChecked(false);
        m_chk_select_all->SetValue(false);
    } else if (checked == m_row_checks.size()) {
        m_chk_select_all->SetHalfChecked(false);
        m_chk_select_all->SetValue(true);
    } else {
        m_chk_select_all->SetValue(false);
        m_chk_select_all->SetHalfChecked(true);
    }
}

void BulkFilamentDialog::update_staged_labels()
{
    if (m_lbl_staged_preset) {
        wxString text = kNotStaged;
        if (!m_staged_preset.empty()) {
            for (size_t i = 0; i < m_preset_full_names.size(); ++i)
                if (m_preset_full_names[i] == m_staged_preset) {
                    text = wxString::FromUTF8(m_preset_display[i]);
                    break;
                }
        }
        m_lbl_staged_preset->SetLabel(text);
        m_lbl_staged_preset->SetForegroundColour(StateColor::semantic(
            m_staged_preset.empty() ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    }
    if (m_lbl_staged_color) {
        m_lbl_staged_color->SetLabel(m_staged_color.empty() ? kNotStaged : wxString::FromUTF8(m_staged_color));
        m_lbl_staged_color->SetForegroundColour(StateColor::semantic(
            m_staged_color.empty() ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    }
    Layout();
}

void BulkFilamentDialog::update_apply_enabled()
{
    if (!m_btn_apply)
        return;
    bool add_staged = m_chk_add && m_chk_add->IsEnabled() && m_chk_add->GetValue();
    bool slot_action_staged = (!m_staged_preset.empty() || !m_staged_color.empty()
                               || (m_chk_delete && m_chk_delete->GetValue()));
    m_btn_apply->Enable(add_staged || (slot_action_staged && checked_row_count() > 0));
}

size_t BulkFilamentDialog::checked_row_count() const
{
    size_t n = 0;
    for (auto* chk : m_row_checks)
        if (chk->GetValue()) ++n;
    return n;
}

BulkFilamentResult BulkFilamentDialog::get_result() const
{
    BulkFilamentResult res;
    for (size_t i = 0; i < m_row_checks.size(); ++i)
        if (m_row_checks[i]->GetValue())
            res.selected_physical.push_back(i);
    res.preset_name = m_staged_preset;
    res.color_hex   = m_staged_color;
    res.do_preset   = !res.preset_name.empty() && !res.selected_physical.empty();
    res.do_color    = !res.color_hex.empty() && !res.selected_physical.empty();
    res.do_delete   = m_chk_delete && m_chk_delete->GetValue() && !res.selected_physical.empty();
    res.add_count   = (m_chk_add && m_chk_add->IsEnabled() && m_chk_add->GetValue() && m_spin_add)
                          ? std::max(0, std::min(m_spin_add->GetValue(), (int)m_max_addable))
                          : 0;
    return res;
}

void BulkFilamentDialog::on_dpi_changed(const wxRect& /*suggested_rect*/)
{
    const int& em = em_unit();
    msw_buttons_rescale(this, em, {wxID_OK, wxID_CANCEL});
    if (m_btn_set_preset) m_btn_set_preset->Rescale();
    if (m_btn_set_color)  m_btn_set_color->Rescale();
    if (m_btn_apply)      m_btn_apply->Rescale();
    if (m_chk_select_all) m_chk_select_all->Rescale();
    for (auto* chk : m_row_checks) chk->Rescale();
    if (m_chk_delete) m_chk_delete->Rescale();
    if (m_chk_add)    m_chk_add->Rescale();
    if (m_spin_add)   m_spin_add->Rescale();
    Layout();
    Fit();
    Refresh();
}

}} // namespace Slic3r::GUI
