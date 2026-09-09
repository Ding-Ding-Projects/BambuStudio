#include "BulkRenameDialog.hpp"

#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/TextInput.hpp"

namespace Slic3r { namespace GUI { namespace Bulk {

bool BulkRenameDialog::Run(wxWindow *parent, const wxString &title, const std::vector<std::string> &names,
                           const std::set<std::string> &reserved, RenamePlan &plan)
{
    BulkRenameDialog dlg(parent, title, names, reserved);
    dlg.ShowModal();
    if (!dlg.m_applied)
        return false;
    plan = dlg.m_plan;
    return true;
}

BulkRenameDialog::BulkRenameDialog(wxWindow *parent, const wxString &title, std::vector<std::string> names,
                                   std::set<std::string> reserved)
    : MD3Dialog(parent, title, wxEmptyString, MaterialIcon::Edit, MD3Dialog::Options{/*resizable=*/true, false})
    , m_names(std::move(names))
    , m_reserved(std::move(reserved))
{
    build();
    replan();
    SetMinSize(wxSize(FromDIP(640), FromDIP(480)));
    Layout();
    Fit();
    CenterOnParent();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
    if (auto *tc = m_pattern->GetTextCtrl())
        tc->SetFocus();
}

void BulkRenameDialog::build()
{
    auto *     content = GetContentSizer();
    const int  field_h = FromDIP(MD3::Metrics::active().row_height);
    const auto muted   = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    auto add_caption = [&](const wxString &text, int top) {
        auto *l = new Label(this, Label::Body_12, text);
        l->SetForegroundColour(muted);
        content->Add(l, 0, wxEXPAND | wxTOP, top);
    };

    // TRN: Caption above the naming-pattern field of the bulk rename dialog; placeholders are literal.
    add_caption(_L("Pattern: {name} keeps the current name, {n} numbers from the start index, {i} from 0, {stem} and {ext} split a file name; {{ and }} are literal braces"), 0);
    m_pattern = new TextInput(this, "{name}", wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(-1, field_h));
    m_pattern->SetName(_L("Naming pattern"));
    if (auto *tc = m_pattern->GetTextCtrl()) {
        tc->SetName(_L("Naming pattern"));
        tc->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) { e.Skip(); replan(); });
    }
    content->Add(m_pattern, 0, wxEXPAND | wxTOP, FromDIP(4));

    auto *find_row = new wxBoxSizer(wxHORIZONTAL);
    auto *find_col = new wxBoxSizer(wxVERTICAL);
    auto *find_cap = new Label(this, Label::Body_12, _L("Find (plain text; use the .* toggle or the builder for a regex with $1 back-references)"));
    find_cap->SetForegroundColour(muted);
    find_col->Add(find_cap, 0, wxEXPAND);
    // TRN: Placeholder of the find field in the bulk rename dialog.
    m_find = new SearchField(this, _L("Find"));
    m_find->SetMinSize(wxSize(-1, FromDIP(40)));
    m_find->SetOnQuery([this](const wxString &) { replan(); });
    m_find->SetOnRegexToggle([this](bool) { replan(); });
    find_col->Add(m_find, 0, wxEXPAND | wxTOP, FromDIP(4));
    find_row->Add(find_col, 1, wxEXPAND);

    auto *replace_col = new wxBoxSizer(wxVERTICAL);
    auto *replace_cap = new Label(this, Label::Body_12, _L("Replace with"));
    replace_cap->SetForegroundColour(muted);
    replace_col->Add(replace_cap, 0, wxEXPAND);
    m_replace = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(-1, field_h));
    m_replace->SetName(_L("Replace with"));
    if (auto *tc = m_replace->GetTextCtrl()) {
        tc->SetName(_L("Replace with"));
        tc->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) { e.Skip(); replan(); });
    }
    replace_col->Add(m_replace, 0, wxEXPAND | wxTOP, FromDIP(4));
    find_row->Add(replace_col, 1, wxEXPAND | wxLEFT, FromDIP(12));
    content->Add(find_row, 0, wxEXPAND | wxTOP, FromDIP(12));

    auto *num_row = new wxBoxSizer(wxHORIZONTAL);
    auto *start_cap = new Label(this, Label::Body_12, _L("Start {n} at"));
    start_cap->SetForegroundColour(muted);
    num_row->Add(start_cap, 0, wxALIGN_CENTER_VERTICAL);
    m_start = new SpinInput(this, "1", wxEmptyString, wxDefaultPosition, wxSize(FromDIP(96), field_h), 0, -999999, 999999, 1);
    m_start->SetName(_L("Start {n} at"));
    num_row->Add(m_start, 0, wxLEFT, FromDIP(8));
    auto *pad_cap = new Label(this, Label::Body_12, _L("Zero-pad to digits"));
    pad_cap->SetForegroundColour(muted);
    num_row->Add(pad_cap, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(16));
    m_pad = new SpinInput(this, "0", wxEmptyString, wxDefaultPosition, wxSize(FromDIP(80), field_h), 0, 0, kRenameMaxPad, 0);
    m_pad->SetName(_L("Zero-pad to digits"));
    num_row->Add(m_pad, 0, wxLEFT, FromDIP(8));
    for (SpinInput *spin : {m_start, m_pad}) {
        spin->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &e) { e.Skip(); replan(); });
        if (auto *tc = spin->GetTextCtrl())
            tc->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) { e.Skip(); replan(); });
    }
    content->Add(num_row, 0, wxEXPAND | wxTOP, FromDIP(12));

    m_counts = new Label(this, Label::Head_14, wxEmptyString);
    m_counts->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    content->Add(m_counts, 0, wxEXPAND | wxTOP, FromDIP(16));

    m_error = new Label(this, Label::Body_12, wxEmptyString);
    m_error->SetForegroundColour(StateColor::semantic(MD3::Role::Error));
    content->Add(m_error, 0, wxEXPAND | wxTOP, FromDIP(2));

    m_preview = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(200)),
                                       wxDV_ROW_LINES | wxBORDER_NONE);
    // TRN: Accessible name of the live before/after list in the bulk rename dialog.
    m_preview->SetName(_L("Rename preview"));
    m_preview->AppendTextColumn(_L("Before"), wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_preview->AppendTextColumn(_L("After"), wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_preview->AppendTextColumn(_L("Outcome"), wxDATAVIEW_CELL_INERT, FromDIP(220), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_preview->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_preview->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    content->Add(m_preview, 1, wxEXPAND | wxTOP, FromDIP(8));

    auto *cancel = new Button(this, _L("Cancel"));
    cancel->SetVariant(Button::Variant::Text);
    cancel->SetButtonSize(Button::Size::Medium);
    cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    AddFooterButton(cancel);

    // TRN: Primary button of the bulk rename dialog.
    m_apply = new Button(this, _L("Rename"));
    m_apply->SetVariant(Button::Variant::Filled);
    m_apply->SetButtonSize(Button::Size::Medium);
    m_apply->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (!m_plan.applicable() || m_plan.has_collisions())
            return;
        m_applied = true;
        EndModal(wxID_OK);
    });
    AddFooterButton(m_apply);

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE)
            EndModal(wxID_CANCEL);
        else
            e.Skip();
    });
}

RenameSpec BulkRenameDialog::current_spec() const
{
    RenameSpec spec;
    spec.pattern        = m_pattern->GetTextCtrl()->GetValue().ToUTF8().data();
    spec.find           = m_find->GetValue().ToUTF8().data();
    spec.replace        = m_replace->GetTextCtrl()->GetValue().ToUTF8().data();
    spec.regex          = m_find->IsRegexEnabled();
    spec.case_sensitive = m_find->IsCaseSensitive();
    spec.start_index    = m_start->GetValue();
    spec.pad            = m_pad->GetValue();
    // "{name}" alone is the identity pattern; treat it as no pattern so a
    // find/replace-only rename does not report every row as unchanged twice.
    if (spec.pattern == "{name}")
        spec.pattern.clear();
    return spec;
}

void BulkRenameDialog::replan()
{
    if (m_preview == nullptr || m_apply == nullptr)
        return;
    m_plan = plan_rename(m_names, current_spec(), m_reserved);

    m_preview->DeleteAllItems();
    for (const RenameRow &row : m_plan.rows) {
        wxVector<wxVariant> cells;
        cells.push_back(wxString::FromUTF8(row.before));
        cells.push_back(wxString::FromUTF8(row.after));
        wxString outcome;
        switch (row.outcome) {
        // TRN: Outcome column of the bulk rename preview.
        case RenameOutcome::Changed: outcome = _L("Will rename"); break;
        case RenameOutcome::Unchanged: outcome = _L("Skipped: name would not change"); break;
        case RenameOutcome::Collision: outcome = wxString::Format(_L("Collision: %s"), wxString::FromUTF8(row.reason)); break;
        case RenameOutcome::Invalid: outcome = wxString::Format(_L("Invalid: %s"), wxString::FromUTF8(row.reason)); break;
        }
        cells.push_back(outcome);
        m_preview->AppendItem(cells);
    }

    // TRN: Bulk rename counts; %1$d selected, %2$d will rename, %3$d skipped.
    m_counts->SetLabel(wxString::Format(_L("%d selected / %d will rename / %d skipped"), static_cast<int>(m_plan.selected()),
                                        static_cast<int>(m_plan.will_change()), static_cast<int>(m_plan.skipped())));
    wxString error = wxString::FromUTF8(m_plan.error);
    if (error.IsEmpty() && m_plan.has_collisions())
        // TRN: Shown under the counts when two planned names collide; Rename stays disabled.
        error = _L("Two or more results collide; adjust the pattern so every new name is unique");
    m_error->SetLabel(error);
    m_error->Show(!error.IsEmpty());
    const bool ok = m_plan.applicable() && !m_plan.has_collisions();
    m_apply->Enable(ok);
    m_apply->SetToolTip(ok ? wxString() : (error.IsEmpty() ? _L("Nothing would change with this pattern") : error));
    Layout();
}

} } } // namespace Slic3r::GUI::Bulk
