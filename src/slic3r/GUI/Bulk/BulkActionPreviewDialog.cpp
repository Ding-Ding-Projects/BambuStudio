#include "BulkActionPreviewDialog.hpp"

#include <memory>

#include <wx/dataview.h>
#include <wx/sizer.h>

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/ProgressDialog.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/SuperConfirmGate.hpp"

namespace Slic3r { namespace GUI { namespace Bulk {

// ---------------------------------------------------------------------------
// Static entry points

bool BulkActionPreviewDialog::Run(wxWindow *parent, const BulkActionPlan &plan)
{
    BulkActionPreviewDialog dlg(parent, plan);
    dlg.ShowModal();
    return dlg.m_proceeded;
}

void BulkActionPreviewDialog::Show(wxWindow *parent, const BulkActionPlan &plan, std::function<void()> on_proceed,
                                   std::function<void()> on_cancel)
{
    auto *dlg = new BulkActionPreviewDialog(parent, plan);
    dlg->Bind(wxEVT_CLOSE_WINDOW, [dlg, on_proceed, on_cancel](wxCloseEvent &) {
        const bool ok = dlg->m_proceeded;
        dlg->Destroy();
        if (ok) {
            if (on_proceed) on_proceed();
        } else if (on_cancel) {
            on_cancel();
        }
    });
    dlg->wxDialog::Show();
}

std::size_t BulkActionPreviewDialog::RunWithProgress(wxWindow *parent, const wxString &title, std::size_t count,
                                                     const std::function<wxString(std::size_t)> &label,
                                                     const std::function<bool(std::size_t)> &step, bool *cancelled,
                                                     std::size_t *failed)
{
    if (cancelled) *cancelled = false;
    if (failed) *failed = 0;
    if (count == 0)
        return 0;
    ProgressDialog progress(title, wxEmptyString, static_cast<int>(count), parent,
                            wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME);
    std::size_t done = 0;
    for (std::size_t i = 0; i < count; ++i) {
        // TRN: Progress line of a bulk action; %1$d current item, %2$d total, %3$s item name.
        const wxString msg = wxString::Format(_L("%d of %d: %s"), static_cast<int>(i + 1), static_cast<int>(count),
                                              label ? label(i) : wxString());
        if (!progress.Update(static_cast<int>(i), msg) || progress.WasCancelled()) {
            if (cancelled) *cancelled = true;
            break;
        }
        if (!step(i) && failed)
            ++*failed;
        ++done;
    }
    progress.Update(static_cast<int>(count));
    return done;
}

// ---------------------------------------------------------------------------
// Dialog

BulkActionPreviewDialog::BulkActionPreviewDialog(wxWindow *parent, const BulkActionPlan &plan)
    : MD3Dialog(parent, wxString::FromUTF8(plan.action), wxEmptyString,
                plan.destructive ? MaterialIcon::DeleteForever : MaterialIcon::TaskAlt,
                MD3Dialog::Options{/*resizable=*/true, /*forced_dark=*/false})
    , m_plan(plan)
{
    build();
    populate();
    update_counts();
    SetMinSize(wxSize(FromDIP(560), FromDIP(420)));
    Layout();
    Fit();
    CenterOnParent();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
}

void BulkActionPreviewDialog::build()
{
    auto *content = GetContentSizer();

    m_counts = new Label(this, Label::Head_14, wxEmptyString);
    m_counts->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    content->Add(m_counts, 0, wxEXPAND);

    m_consequence = new Label(this, Label::Body_13, wxString::FromUTF8(m_plan.consequence));
    m_consequence->SetForegroundColour(StateColor::semantic(m_plan.destructive ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
    m_consequence->Wrap(FromDIP(520));
    content->Add(m_consequence, 0, wxEXPAND | wxTOP, FromDIP(4));

    // TRN: Placeholder of the search field filtering the bulk-action preview list.
    m_search = new SearchField(this, _L("Filter the preview"));
    m_search->SetMinSize(wxSize(-1, FromDIP(40)));
    m_search->SetOnQuery([this](const wxString &) { populate(); });
    m_search->SetOnRegexToggle([this](bool) { populate(); });
    content->Add(m_search, 0, wxEXPAND | wxTOP, FromDIP(12));

    m_list = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(220)),
                                    wxDV_ROW_LINES | wxBORDER_NONE);
    // TRN: Accessible name of the list that previews the items a bulk action will touch.
    m_list->SetName(_L("Bulk action preview"));
    m_list->AppendTextColumn(_L("Item"), wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    if (m_plan.has_transform())
        m_list->AppendTextColumn(_L("Becomes"), wxDATAVIEW_CELL_INERT, FromDIP(180), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Outcome"), wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Details"), wxDATAVIEW_CELL_INERT, FromDIP(160), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_list->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    content->Add(m_list, 1, wxEXPAND | wxTOP, FromDIP(8));

    m_cancel = new Button(this, _L("Cancel"));
    m_cancel->SetVariant(Button::Variant::Text);
    m_cancel->SetButtonSize(Button::Size::Medium);
    m_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (IsModal()) EndModal(wxID_CANCEL); else Close();
    });
    AddFooterButton(m_cancel);

    m_proceed = new Button(this, wxString::FromUTF8(m_plan.action));
    m_proceed->SetVariant(m_plan.destructive ? Button::Variant::Danger : Button::Variant::Filled);
    m_proceed->SetButtonSize(Button::Size::Medium);
    m_proceed->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_proceed(); });
    AddFooterButton(m_proceed);

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE) {
            if (IsModal()) EndModal(wxID_CANCEL); else Close();
        } else {
            e.Skip();
        }
    });
}

void BulkActionPreviewDialog::populate()
{
    m_list->DeleteAllItems();
    std::unique_ptr<SearchField::MatchPass> pass;
    const wxString query = m_search ? m_search->GetValue() : wxString();
    if (!query.IsEmpty())
        pass = std::make_unique<SearchField::MatchPass>(query, m_search->IsRegexEnabled(), m_search->IsCaseSensitive(),
                                                        m_search->IsWholeWord(), m_search->IsMultiline());
    const bool transform = m_plan.has_transform();
    for (const BulkItem &item : m_plan.items) {
        const wxString label  = wxString::FromUTF8(item.label);
        const wxString detail = wxString::FromUTF8(item.detail);
        const wxString after  = wxString::FromUTF8(item.after);
        if (pass && !pass->matches(label + " " + detail + " " + after + " " + wxString::FromUTF8(item.skip_reason)))
            continue;
        wxVector<wxVariant> row;
        row.push_back(label);
        if (transform)
            row.push_back(after);
        // TRN: Outcome column of the bulk preview when the row will be acted on.
        row.push_back(item.will_change ? _L("Will change") : wxString::Format(_L("Skipped: %s"), wxString::FromUTF8(item.skip_reason)));
        row.push_back(detail);
        m_list->AppendItem(row);
    }
}

void BulkActionPreviewDialog::update_counts()
{
    // TRN: Bulk preview counts; %1$d selected, %2$d will change, %3$d skipped.
    wxString text = wxString::Format(_L("%d selected / %d will change / %d skipped"), static_cast<int>(m_plan.selected()),
                                     static_cast<int>(m_plan.will_change()), static_cast<int>(m_plan.skipped()));
    m_counts->SetLabel(text);
    m_proceed->Enable(m_plan.applicable());
    if (!m_plan.applicable())
        // TRN: Tooltip on the disabled Proceed button of a bulk preview.
        m_proceed->SetToolTip(_L("Nothing in the selection would change; every item is skipped for the reason shown"));
    else
        m_proceed->SetToolTip(m_plan.destructive ? _L("Opens the two-key confirmation before anything is deleted") : wxString());
}

void BulkActionPreviewDialog::on_proceed()
{
    if (!m_plan.applicable())
        return;
    if (!m_plan.destructive) {
        m_proceeded = true;
        if (IsModal()) EndModal(wxID_OK); else Close();
        return;
    }
    SuperConfirmGate::Spec spec;
    spec.action      = wxString::FromUTF8(m_plan.action);
    spec.consequence = wxString::FromUTF8(m_plan.consequence);
    for (const std::string &label : m_plan.changed_labels())
        spec.affected.push_back(wxString::FromUTF8(label));
    spec.affected_count = static_cast<int>(m_plan.will_change());
    if (SuperConfirmGate::Run(m_proceed, spec)) {
        m_proceeded = true;
        if (IsModal()) EndModal(wxID_OK); else Close();
    } else {
        m_proceed->SetFocus();
    }
}

} } } // namespace Slic3r::GUI::Bulk
