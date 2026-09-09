#include "ProjectHistoryDialog.hpp"

#include "Bulk/BulkActionPlan.hpp"
#include "Bulk/BulkActionPreviewDialog.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"
#include "NotificationManager.hpp"
#include "Plater.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/MD3Dialog.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"
#include "Widgets/TextInput.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <set>
#include <system_error>

#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/dcclient.h>
#include <wx/dirdlg.h>
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include <wx/variant.h>

namespace Slic3r::GUI {

namespace {

constexpr int HISTORY_POLL_INTERVAL_MS = 75;
constexpr std::size_t HISTORY_INITIAL_LIMIT = 500;

wxRect active_display_work_area(wxWindow *window)
{
    int display_index = window != nullptr ? wxDisplay::GetFromWindow(window) : wxNOT_FOUND;
    if (display_index == wxNOT_FOUND && wxDisplay::GetCount() > 0)
        display_index = 0;
    return display_index == wxNOT_FOUND ? wxRect(wxDefaultPosition, wxGetDisplaySize())
                                        : wxDisplay(static_cast<unsigned int>(display_index)).GetClientArea();
}

StateColor filled_button_background()
{
    return StateColor(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Disabled),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Primary), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Primary), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Primary), StateColor::Normal));
}

StateColor filled_button_text()
{
    return StateColor(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::Disabled),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnPrimary), StateColor::Normal));
}

StateColor outlined_button_background()
{
    return StateColor(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Surface), StateColor::Normal));
}

// A native tooltip never wraps, so handing it a 200-character libgit2 message
// would re-clip the very text the tooltip exists to reveal. Break it onto lines
// of a fixed reading measure first. The label's own width is not usable here
// because callers set tooltips during create_ui(), before any layout has run.
// Text that still fits one line at that measure is already fully readable in
// the wrapped label, so it gets no tooltip rather than one echoing what is
// visibly on screen. The measure is taken with the font the label carries at
// this moment, so callers must settle the face before asking for a tooltip.
void set_wrapped_tooltip(Label *label, const wxString &text)
{
    if (label == nullptr)
        return;
    wxClientDC dc(label);
    dc.SetFont(label->GetFont());
    wxString wrapped;
    Label::split_lines(dc, label->FromDIP(480), text, wrapped);
    // split_lines only ever inserts '\n' at a break it made, so its absence
    // means the whole string fit a single line.
    if (wrapped.Find('\n') == wxNOT_FOUND)
        label->UnsetToolTip();
    else
        label->SetToolTip(wrapped);
}

std::string short_commit(const std::string &commit_id)
{
    return commit_id.substr(0, std::min<std::size_t>(12, commit_id.size()));
}

// One-line prompt for the label text used by "Label selected...". Built on the
// shared MD3 shell so it matches every other dialog in the kit.
class LabelPromptDialog final : public MD3Dialog
{
public:
    LabelPromptDialog(wxWindow *parent, std::size_t version_count)
        // TRN: Title of the prompt asking for a label to attach to project versions.
        : MD3Dialog(parent, _L("Label versions"),
                    // TRN: %d is how many project versions will receive the label.
                    wxString::Format(_L("The label is attached to %d selected versions"), static_cast<int>(version_count)),
                    MaterialIcon::History)
    {
        auto *content = GetContentSizer();
        // TRN: Explains which characters a version label may contain.
        auto *hint = new Label(this, Label::Body_13,
                               _L("Letters, digits, '.', '_' and '-' are kept; other characters become '-'. Labels never change history."));
        hint->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        hint->Wrap(FromDIP(420));
        content->Add(hint, 0, wxEXPAND);
        // TRN: Field label of the version label prompt.
        m_input = new TextInput(this, wxEmptyString, _L("Label"), "", wxDefaultPosition, wxSize(FromDIP(420), FromDIP(40)),
                                wxTE_PROCESS_ENTER);
        m_input->GetTextCtrl()->SetMaxLength(64);
        m_input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { accept(); });
        m_input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update_ok(); });
        content->Add(m_input, 0, wxEXPAND | wxTOP, FromDIP(12));

        auto *cancel = new Button(this, _L("Cancel"));
        cancel->SetVariant(Button::Variant::Text);
        cancel->SetButtonSize(Button::Size::Medium);
        cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        AddFooterButton(cancel);
        // TRN: Confirms the version label prompt.
        m_ok = new Button(this, _L("Continue"));
        m_ok->SetVariant(Button::Variant::Filled);
        m_ok->SetButtonSize(Button::Size::Medium);
        m_ok->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { accept(); });
        AddFooterButton(m_ok);
        update_ok();

        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
            if (e.GetKeyCode() == WXK_ESCAPE) EndModal(wxID_CANCEL); else e.Skip();
        });
        Layout();
        Fit();
        CenterOnParent();
        UpdateShape();
        wxGetApp().UpdateDlgDarkUI(this);
        m_input->GetTextCtrl()->SetFocus();
    }

    wxString value() const { return m_input->GetTextCtrl()->GetValue().Strip(wxString::both); }

private:
    void update_ok() { m_ok->Enable(!value().IsEmpty()); }
    void accept()
    {
        if (!value().IsEmpty()) EndModal(wxID_OK);
    }

    TextInput *m_input{nullptr};
    Button *   m_ok{nullptr};
};

} // namespace

ProjectHistoryDialog::ProjectHistoryDialog(wxWindow *parent, Plater *plater)
    : DPIDialog(parent, wxID_ANY, _L("Version history"), wxDefaultPosition, wxDefaultSize,
                // MD3 caption strip instead of the native title bar.
                wxRESIZE_BORDER | wxBORDER_NONE)
    , m_plater(plater)
    , m_manager(plater != nullptr ? plater->project_history_manager() : nullptr)
    , m_project_identity(plater != nullptr ? plater->project_history_identity() : std::filesystem::path{})
    , m_poll_timer(this)
{
    create_ui();
    apply_theme();

    Bind(wxEVT_TIMER, &ProjectHistoryDialog::poll_operation, this, m_poll_timer.GetId());
    Bind(wxEVT_CLOSE_WINDOW, &ProjectHistoryDialog::on_close_window, this);
    Bind(wxEVT_SIZE, &ProjectHistoryDialog::on_size, this);
    Bind(wxEVT_CHAR_HOOK, &ProjectHistoryDialog::on_char_hook, this);

    SetEscapeId(wxID_CANCEL);
    SetAffirmativeId(wxID_APPLY);
    update_window_constraints(true);
    CentreOnParent();
    update_responsive_layout();

    refresh_versions();
    refresh_retained_failures();
    MD3DialogCaption::FinishChrome(this);
}

ProjectHistoryDialog::~ProjectHistoryDialog()
{
    m_poll_timer.Stop();
    cleanup_restore_temp();
}

std::filesystem::path ProjectHistoryDialog::release_restored_snapshot()
{
    std::filesystem::path result = std::move(m_restored_snapshot);
    m_restored_snapshot.clear();
    m_restore_temp_dir.clear();
    return result;
}

void ProjectHistoryDialog::create_ui()
{
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(new MD3DialogCaption(this, _L("Version history")), 0, wxEXPAND);

    m_title_label = new Label(this, Label::Head_24, _L("Version history"));
    root->Add(m_title_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // TRN: Subtitle in the project Version history dialog.
    m_subtitle_label = new Label(this, Label::Body_14,
        _L("Browse complete project snapshots saved automatically in a private local Git repository."));
    root->Add(m_subtitle_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    m_info_card = new StaticBox(this);
    auto *info_sizer = new wxBoxSizer(wxVERTICAL);
    const wxString saved_project = m_plater != nullptr ? m_plater->get_project_filename(".3mf") : wxString{};
    const wxString project_name  = saved_project.empty() ? _L("Untitled project") : wxFileName(saved_project).GetFullName();
    // TRN: %s is a project filename, or the localized text "Untitled project".
    const wxString project_line = wxString::Format(_L("Project: %s"), project_name);
    // A project filename is unbounded user input, so this label wraps rather than
    // letting the native STATIC control cut the name off; the tooltip keeps the
    // whole name reachable when even the wrapped form is squeezed.
    m_project_label = new Label(m_info_card, Label::Head_14, project_line,
                                LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    // wxEXPAND hands the label the sizer's full width, so its unwrapped text
    // extent must not be allowed to push the card's CalcMin out with it.
    m_project_label->SetMinSize(wxSize(0, -1));
    set_wrapped_tooltip(m_project_label, project_line);
    info_sizer->Add(m_project_label, 0, wxEXPAND | wxALL, FromDIP(14));
    m_info_card->SetSizer(info_sizer);
    root->Add(m_info_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // Recovery banner for snapshots whose commit failed terminally. Hidden until
    // refresh_retained_failures() finds quarantined snapshots to surface.
    m_failure_card = new StaticBox(this);
    auto *failure_sizer = new wxBoxSizer(wxVERTICAL);
    // TRN: Heading of the recovery banner in the Version history dialog.
    m_failure_title_label = new Label(m_failure_card, Label::Head_14, _L("Some versions could not be saved"));
    failure_sizer->Add(m_failure_title_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(14));
    m_failure_detail_label = new Label(m_failure_card, Label::Body_13);
    failure_sizer->Add(m_failure_detail_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
    m_retry_failures_button = new Button(m_failure_card, _L("Retry saving"));
    m_retry_failures_button->SetMinSize(FromDIP(wxSize(140, 36)));
    m_retry_failures_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_retry_failures, this);
    failure_sizer->Add(m_retry_failures_button, 0, wxALIGN_RIGHT | wxALL, FromDIP(14));
    m_failure_card->SetSizer(failure_sizer);
    root->Add(m_failure_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));
    m_failure_card->Hide();

    m_list_card = new StaticBox(this);
    auto *list_sizer = new wxBoxSizer(wxVERTICAL);
    // TRN: Placeholder of the search field filtering the version list.
    m_search_field = new SearchField(m_list_card, _L("Search versions"));
    m_search_field->SetOnQuery([this](const wxString &) {
        populate_versions();
        update_history_status();
        update_selection();
    });
    m_search_field->SetOnRegexToggle([this](bool) {
        populate_versions();
        update_history_status();
        update_selection();
    });
    list_sizer->Add(m_search_field, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    m_version_list = new wxDataViewListCtrl(m_list_card, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                            wxDV_MULTIPLE | wxBORDER_NONE);
    // TRN: Accessible name of the project version list.
    m_version_list->SetName(_L("Project versions"));
    m_version_list->AppendTextColumn(_L("Commit"), wxDATAVIEW_CELL_INERT, FromDIP(104), wxALIGN_LEFT,
                                     wxDATAVIEW_COL_RESIZABLE);
    m_version_list->AppendTextColumn(_L("Message"), wxDATAVIEW_CELL_INERT, FromDIP(280), wxALIGN_LEFT,
                                     wxDATAVIEW_COL_RESIZABLE);
    m_version_list->AppendTextColumn(_L("Time"), wxDATAVIEW_CELL_INERT, FromDIP(150), wxALIGN_LEFT,
                                     wxDATAVIEW_COL_RESIZABLE);
    m_version_list->AppendTextColumn(_L("Size"), wxDATAVIEW_CELL_INERT, FromDIP(80), wxALIGN_RIGHT,
                                     wxDATAVIEW_COL_RESIZABLE);
    m_version_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ProjectHistoryDialog::on_selection_changed, this);
    m_version_list->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &ProjectHistoryDialog::on_item_activated, this);
    wxGetApp().UpdateDVCDarkUI(m_version_list); // native header follows the theme
    list_sizer->Add(m_version_list, 1, wxEXPAND | wxALL, FromDIP(8));

    // Bulk selection strip: page/all/invert selection plus the bulk actions.
    // There is deliberately no bulk delete here: the version history is
    // append-only by design (a restore is recorded as a new version and nothing
    // is ever removed), so there is no delete action to run in bulk.
    auto *bulk_row = new wxBoxSizer(wxHORIZONTAL);
    const auto make_bulk_button = [this](const wxString &text, Button::Variant variant) {
        auto *button = new Button(m_list_card, text);
        button->SetVariant(variant);
        button->SetButtonSize(Button::Size::Small);
        return button;
    };
    m_select_visible_button = make_bulk_button(_L("Select visible"), Button::Variant::Tonal);
    m_select_all_button     = make_bulk_button(_L("Select all"), Button::Variant::Tonal);
    m_invert_button         = make_bulk_button(_L("Invert"), Button::Variant::Tonal);
    m_export_button         = make_bulk_button(_L("Export selected..."), Button::Variant::Outlined);
    m_label_button          = make_bulk_button(_L("Label selected..."), Button::Variant::Outlined);
    m_select_visible_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { select_visible(); });
    m_select_all_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { select_all_loaded(); });
    m_invert_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { invert_selection(); });
    m_export_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { bulk_export(); });
    m_label_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { bulk_label(); });
    // TRN: Tooltip of the "Export selected..." bulk action in Version history.
    m_export_button->SetToolTip(_L("Writes each selected version as its own .3mf file into a folder you choose"));
    // TRN: Tooltip of the "Label selected..." bulk action in Version history.
    m_label_button->SetToolTip(_L("Attaches a label to every selected version; labels never change or remove history"));
    m_bulk_counts_label = new Label(m_list_card, Label::Body_12, wxEmptyString);
    bulk_row->Add(m_select_visible_button, 0, wxRIGHT, FromDIP(6));
    bulk_row->Add(m_select_all_button, 0, wxRIGHT, FromDIP(6));
    bulk_row->Add(m_invert_button, 0, wxRIGHT, FromDIP(12));
    bulk_row->Add(m_bulk_counts_label, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    bulk_row->Add(m_export_button, 0, wxRIGHT, FromDIP(6));
    bulk_row->Add(m_label_button, 0);
    list_sizer->Add(bulk_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));

    // This line is where every failure lands, and a libgit2 error carries its
    // message plus an absolute repository path, far more than one line holds at
    // the dialog's minimum width. Wrap it instead of clipping the actual cause
    // away; set_status() mirrors the untruncated text into the tooltip.
    m_status_label = new Label(m_list_card, Label::Body_13, wxEmptyString,
                               LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_status_label->SetMinSize(wxSize(0, -1)); // as above: the sizer owns the width, the text does not
    list_sizer->Add(m_status_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));

    m_load_all_button = new Button(m_list_card, _L("Load all versions"));
    m_load_all_button->SetMinSize(FromDIP(wxSize(144, 36)));
    m_load_all_button->Hide();
    m_load_all_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_load_all, this);
    list_sizer->Add(m_load_all_button, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));

    m_list_card->SetSizer(list_sizer);
    root->Add(m_list_card, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // TRN: Safety note in the Version history dialog.
    m_safety_label = new Label(this, Label::Body_12,
        _L("Restoring adds a new version. It never overwrites the project file or rewinds Git history."));
    root->Add(m_safety_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_refresh_button = new Button(this, _L("Refresh"));
    m_restore_button = new Button(this, _L("Restore selected"), "", 0, 0, wxID_APPLY);
    m_close_button   = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);

    m_refresh_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_restore_button->SetMinSize(FromDIP(wxSize(154, 40)));
    m_close_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_restore_button->Enable(false);

    m_refresh_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_refresh, this);
    m_restore_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_restore, this);
    m_close_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_close_button, this);

    actions->AddStretchSpacer();
    actions->Add(m_refresh_button, 0, wxRIGHT, FromDIP(8));
    actions->Add(m_close_button, 0, wxRIGHT, FromDIP(8));
    actions->Add(m_restore_button, 0);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(24));

    SetSizer(root);
}

void ProjectHistoryDialog::apply_theme()
{
    const wxColour surface       = StateColor::semantic(MD3::Role::Surface);
    const wxColour card          = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour list_surface  = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
    const wxColour alternate     = StateColor::semantic(MD3::Role::SurfaceContainer);
    const wxColour text          = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour secondary     = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const wxColour outline       = StateColor::semantic(MD3::Role::OutlineVariant);

    SetBackgroundColour(surface);
    m_title_label->SetForegroundColour(text);
    m_subtitle_label->SetForegroundColour(secondary);
    m_project_label->SetForegroundColour(text);
    m_status_label->SetForegroundColour(secondary);
    m_safety_label->SetForegroundColour(secondary);

    // Label caches its parent's background at CONSTRUCTION (see
    // Label::Label -> StaticBox::GetParentBackgroundColor), and the layout is
    // built before any theme is applied. Recoloring only the foreground left
    // every label sitting on a stale light plate in dark mode. Re-seed each
    // label with the surface it actually sits on, every time the theme changes.
    for (Label *label : {m_title_label, m_subtitle_label, m_safety_label})
        label->SetBackgroundColour(surface);
    for (Label *label : {m_project_label, m_status_label, m_bulk_counts_label})
        label->SetBackgroundColour(card);
    m_bulk_counts_label->SetForegroundColour(secondary);

    for (StaticBox *box : {m_info_card, m_list_card}) {
        box->SetBackgroundColorNormal(card);
        box->SetBorderColorNormal(outline);
        box->SetBorderWidth(1);
    }

    // The recovery banner uses the MD3 error-container roles to read as a
    // problem that needs attention without shouting like a hard error.
    const wxColour error_container    = StateColor::semantic(MD3::Role::ErrorContainer);
    const wxColour on_error_container  = StateColor::semantic(MD3::Role::OnErrorContainer);
    const wxColour error_accent        = StateColor::semantic(MD3::Role::Error);
    m_failure_card->SetBackgroundColorNormal(error_container);
    m_failure_card->SetBorderColorNormal(error_accent);
    m_failure_card->SetBorderWidth(1);
    m_failure_title_label->SetForegroundColour(on_error_container);
    m_failure_detail_label->SetForegroundColour(on_error_container);
    for (Label *label : {m_failure_title_label, m_failure_detail_label})
        label->SetBackgroundColour(error_container);
    m_retry_failures_button->SetBackgroundColor(StateColor(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Error), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Error), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Error), StateColor::Normal)));
    m_retry_failures_button->SetBorderColor(StateColor(error_accent));
    m_retry_failures_button->SetTextColor(StateColor(StateColor::semantic(MD3::Role::OnError)));

    m_version_list->SetBackgroundColour(list_surface);
    m_version_list->SetForegroundColour(text);
    m_version_list->SetAlternateRowColour(alternate);

    const StateColor outlined_bg = outlined_button_background();
    const StateColor outlined_border(outline);
    const StateColor outlined_text(text);
    for (Button *button : {m_refresh_button, m_load_all_button, m_close_button}) {
        button->SetBackgroundColor(outlined_bg);
        button->SetBorderColor(outlined_border);
        button->SetTextColor(outlined_text);
    }
    m_restore_button->SetBackgroundColor(filled_button_background());
    m_restore_button->SetBorderColor(StateColor(StateColor::semantic(MD3::Role::Primary)));
    m_restore_button->SetTextColor(filled_button_text());

    Refresh();
}

void ProjectHistoryDialog::refresh_versions()
{
    if (m_pending != PendingOperation::None)
        return;
    if (m_manager == nullptr) {
        show_error(_L("Version history is unavailable because its local repository could not be initialized."));
        return;
    }
    if (m_project_identity.empty()) {
        show_error(_L("Version history is unavailable for this project."));
        return;
    }

    m_version_list->DeleteAllItems();
    m_versions.clear();
    m_list_truncated = false;
    m_load_all_button->Hide();
    set_busy(PendingOperation::List, _L("Loading versions..."));
    try {
        m_list_future = m_manager->list_versions(
            m_project_identity, m_show_all ? 0 : HISTORY_INITIAL_LIMIT + 1);
        m_poll_timer.Start(HISTORY_POLL_INTERVAL_MS);
    } catch (const std::exception &exception) {
        m_pending = PendingOperation::None;
        show_error(wxString::Format(_L("Could not load version history: %s"), wxString::FromUTF8(exception.what())));
    } catch (...) {
        m_pending = PendingOperation::None;
        show_error(_L("Could not load version history."));
    }
}

void ProjectHistoryDialog::refresh_retained_failures()
{
    if (m_failure_card == nullptr)
        return;

    const bool available = m_plater != nullptr && m_plater->has_project_history_retained_failures();
    if (!available) {
        if (m_failure_card->IsShown()) {
            m_failure_card->Hide();
            Layout();
        }
        return;
    }

    const std::vector<RetainedProjectHistoryFailure> failures = m_plater->project_history_retained_failures();
    // TRN: Body text of the recovery banner in the Version history dialog.
    wxString detail = _L("These recovery snapshots were kept on this device. Retry to save them to version history.");

    // Enumerate a bounded number of snapshots so a large backlog cannot force the
    // banner to grow without limit; the rest are summarized on a final line.
    constexpr std::size_t max_listed = 6;
    const std::size_t     listed     = std::min(max_listed, failures.size());
    for (std::size_t index = 0; index < listed; ++index) {
        const RetainedProjectHistoryFailure &failure = failures[index];
        wxString name = failure.untitled ? _L("Untitled project") : wxString::FromUTF8(failure.display_name);
        if (name.empty())
            name = _L("Untitled project");
        wxString reason = wxString::FromUTF8(failure.reason);
        reason.Replace("\r", " ");
        reason.Replace("\n", " ");
        reason.Trim(true).Trim(false);
        if (reason.length() > 72)
            reason = reason.Left(69) + "...";
        detail += "\n- " + name;
        if (!reason.empty())
            detail += ": " + reason;
    }
    if (failures.size() > listed) {
        // TRN: %d is the number of additional unsaved snapshots not listed above.
        detail += "\n" + wxString::Format(_L("and %d more"), static_cast<int>(failures.size() - listed));
    }
    m_failure_detail_label->SetLabel(detail);

    m_retry_failures_button->Enable(m_pending == PendingOperation::None);
    if (!m_failure_card->IsShown())
        m_failure_card->Show();
    Layout();
}

void ProjectHistoryDialog::begin_restore()
{
    if (m_pending != PendingOperation::None || m_manager == nullptr)
        return;

    // Restore stays single-only: restoring several versions into the same
    // project makes no sense, so exactly one selected version is required.
    const std::vector<std::string> selected = selected_ids_in_order();
    if (selected.size() != 1)
        return;
    const auto version_it = std::find_if(m_versions.begin(), m_versions.end(),
                                         [&selected](const ProjectHistoryVersion &v) { return v.commit_id == selected.front(); });
    if (version_it == m_versions.end())
        return;
    const std::size_t selected_version = static_cast<std::size_t>(version_it - m_versions.begin());

    MessageDialog confirmation(
        this,
        _L("Restore the selected version in the editor?\n\nThe project file will not be overwritten. The restored state will be recorded as a new version."),
        _L("Restore project version"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    if (confirmation.ShowModal() != wxID_YES)
        return;

    const std::filesystem::path destination = make_restore_destination();
    if (destination.empty()) {
        show_error(_L("Could not create a secure temporary location for the restored project."));
        return;
    }

    set_busy(PendingOperation::Restore, _L("Preparing selected version..."));
    m_close_button->Enable(false);
    try {
        m_restore_future = m_manager->restore_version(m_project_identity, m_versions[selected_version].commit_id, destination);
        m_poll_timer.Start(HISTORY_POLL_INTERVAL_MS);
    } catch (const std::exception &exception) {
        m_pending = PendingOperation::None;
        m_close_button->Enable(true);
        cleanup_restore_temp();
        show_error(wxString::Format(_L("Could not restore the selected version: %s"), wxString::FromUTF8(exception.what())));
    } catch (...) {
        m_pending = PendingOperation::None;
        m_close_button->Enable(true);
        cleanup_restore_temp();
        show_error(_L("Could not restore the selected version."));
    }
}

void ProjectHistoryDialog::poll_operation(wxTimerEvent &)
{
    using namespace std::chrono_literals;

    if (m_pending == PendingOperation::List && m_list_future.valid() && m_list_future.wait_for(0ms) == std::future_status::ready) {
        m_poll_timer.Stop();
        try {
            finish_list(m_list_future.get());
        } catch (const std::exception &exception) {
            m_pending = PendingOperation::None;
            show_error(wxString::Format(_L("Could not load version history: %s"), wxString::FromUTF8(exception.what())));
        } catch (...) {
            m_pending = PendingOperation::None;
            show_error(_L("Could not load version history."));
        }
    } else if (m_pending == PendingOperation::Restore && m_restore_future.valid() &&
               m_restore_future.wait_for(0ms) == std::future_status::ready) {
        m_poll_timer.Stop();
        try {
            finish_restore(m_restore_future.get());
        } catch (const std::exception &exception) {
            m_pending = PendingOperation::None;
            m_close_button->Enable(true);
            cleanup_restore_temp();
            show_error(wxString::Format(_L("Could not restore the selected version: %s"), wxString::FromUTF8(exception.what())));
        } catch (...) {
            m_pending = PendingOperation::None;
            m_close_button->Enable(true);
            cleanup_restore_temp();
            show_error(_L("Could not restore the selected version."));
        }
    }
}

void ProjectHistoryDialog::finish_list(ProjectHistoryListResult result)
{
    m_pending = PendingOperation::None;
    m_refresh_button->Enable(true);
    m_close_button->Enable(true);
    refresh_retained_failures();

    if (!result.ok()) {
        if (result.error.code == ProjectHistoryErrorCode::NotFound) {
            show_empty_state();
            return;
        }
        show_error(wxString::Format(_L("Could not load version history: %s"), wxString::FromUTF8(result.error.message)));
        return;
    }

    m_versions = std::move(result.versions);
    m_list_truncated = !m_show_all && m_versions.size() > HISTORY_INITIAL_LIMIT;
    if (m_list_truncated)
        m_versions.resize(HISTORY_INITIAL_LIMIT);
    m_load_all_button->Show(m_list_truncated);
    m_load_all_button->Enable(m_list_truncated);
    if (m_versions.empty()) {
        m_bulk.clear();
        show_empty_state();
        return;
    }

    // Ids that vanished from the loaded set must never be acted on.
    m_bulk.retain_listed(loaded_ids());
    populate_versions();
    update_history_status();
    update_selection();
    Layout();
}

void ProjectHistoryDialog::finish_restore(ProjectHistoryRestoreResult result)
{
    m_pending = PendingOperation::None;
    m_close_button->Enable(true);
    refresh_retained_failures();

    if (!result.ok()) {
        cleanup_restore_temp();
        show_error(wxString::Format(_L("Could not restore the selected version: %s"), wxString::FromUTF8(result.error.message)));
        return;
    }

    m_restored_snapshot = std::move(result.restored_path);
    EndModal(wxID_APPLY);
}

void ProjectHistoryDialog::populate_versions()
{
    m_version_list->DeleteAllItems();
    m_filtered_rows.clear();
    const wxString query = m_search_field != nullptr ? m_search_field->GetValue() : wxString{};
    const bool regex      = m_search_field != nullptr && m_search_field->IsRegexEnabled();
    const bool case_sense = m_search_field != nullptr && m_search_field->IsCaseSensitive();
    const bool whole_word = m_search_field != nullptr && m_search_field->IsWholeWord();
    const bool multiline  = m_search_field != nullptr && m_search_field->IsMultiline();
    SearchField::MatchPass match_pass(query, regex, case_sense, whole_word, multiline);
    for (std::size_t i = 0; i < m_versions.size(); ++i) {
        const ProjectHistoryVersion &version = m_versions[i];
        const std::string short_id = version.commit_id.substr(0, std::min<std::size_t>(12, version.commit_id.size()));
        const wxString commit    = wxString::FromUTF8(short_id);
        wxString       message   = display_message(version.message);
        const wxString timestamp = format_timestamp(version.committed_at);
        // User labels lead the message so they are visible and searchable.
        for (const std::string &label : version.labels)
            message = "[" + wxString::FromUTF8(label) + "] " + message;
        if (!query.IsEmpty()) {
            const wxString haystack = commit + " " + message + " " + timestamp;
            if (!match_pass.matches(haystack))
                continue;
        }
        wxVector<wxVariant> row;
        row.push_back(wxVariant(commit));
        row.push_back(wxVariant(message));
        row.push_back(wxVariant(timestamp));
        row.push_back(wxVariant(format_size(version.snapshot_size)));
        m_version_list->AppendItem(row);
        m_filtered_rows.push_back(i);
    }
    apply_bulk_to_list();
}

void ProjectHistoryDialog::update_history_status()
{
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_status_label->SetFont(Label::Body_13);
    if (m_search_field != nullptr && !m_search_field->GetValue().IsEmpty()) {
        // TRN: %1$d is how many project versions match the search; %2$d is the total.
        set_status(wxString::Format(_L("%d of %d versions match the search"),
            static_cast<int>(m_filtered_rows.size()), static_cast<int>(m_versions.size())));
        return;
    }
    if (m_list_truncated) {
        // TRN: %d is the number of newest local project versions currently displayed.
        set_status(wxString::Format(
            _L("Showing the newest %d versions. Load all versions to browse older history."),
            static_cast<int>(HISTORY_INITIAL_LIMIT)));
    } else {
        // TRN: %d is the number of immutable project versions in local history.
        set_status(
            wxString::Format(_L("%d versions saved on this device"), static_cast<int>(m_versions.size())));
    }
}

void ProjectHistoryDialog::update_responsive_layout()
{
    if (m_subtitle_label == nullptr || m_safety_label == nullptr || m_version_list == nullptr)
        return;

    const int content_width = std::max(FromDIP(240), GetClientSize().GetWidth() - FromDIP(48));

    // wxStaticText::Wrap() mutates its rendered label. Restore the localized
    // source before every wrap so repeated resizes do not accumulate breaks.
    m_subtitle_label->SetLabel(
        _L("Browse complete project snapshots saved automatically in a private local Git repository."));
    m_subtitle_label->Wrap(content_width);
    m_safety_label->SetLabel(
        _L("Restoring adds a new version. It never overwrites the project file or rewinds Git history."));
    m_safety_label->Wrap(content_width);

    Layout();

    const int commit_width  = FromDIP(104);
    const int time_width    = FromDIP(150);
    const int size_width    = FromDIP(80);
    const int list_width    = m_version_list->GetClientSize().GetWidth();
    const int message_width = std::max(FromDIP(140),
        list_width - commit_width - time_width - size_width - FromDIP(4));
    if (m_version_list->GetColumnCount() >= 4) {
        m_version_list->GetColumn(0)->SetWidth(commit_width);
        m_version_list->GetColumn(1)->SetWidth(message_width);
        m_version_list->GetColumn(2)->SetWidth(time_width);
        m_version_list->GetColumn(3)->SetWidth(size_width);
    }

    Layout();
}

void ProjectHistoryDialog::update_window_constraints(bool initialize_size)
{
    wxWindow *display_anchor = initialize_size && GetParent() != nullptr ? GetParent() : this;
    const wxSize display_size = active_display_work_area(display_anchor).GetSize();
    const wxSize margin       = FromDIP(wxSize(32, 32));
    const wxSize available(std::max(1, display_size.GetWidth() - margin.GetWidth()),
                           std::max(1, display_size.GetHeight() - margin.GetHeight()));
    const wxSize desired_min = FromDIP(wxSize(560, 420));
    const wxSize minimum(std::min(desired_min.GetWidth(), available.GetWidth()),
                         std::min(desired_min.GetHeight(), available.GetHeight()));
    SetMinSize(minimum);

    const wxSize target = initialize_size ? FromDIP(wxSize(820, 560)) : GetSize();
    const wxSize bounded(std::max(minimum.GetWidth(), std::min(target.GetWidth(), available.GetWidth())),
                         std::max(minimum.GetHeight(), std::min(target.GetHeight(), available.GetHeight())));
    if (initialize_size || bounded != GetSize())
        SetSize(bounded);
}

void ProjectHistoryDialog::set_busy(PendingOperation operation, const wxString &message)
{
    m_pending = operation;
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_status_label->SetFont(Label::Body_13);
    set_status(message);
    m_refresh_button->Enable(false);
    m_load_all_button->Enable(false);
    m_restore_button->Enable(false);
    m_close_button->Enable(false);
    if (m_retry_failures_button != nullptr)
        m_retry_failures_button->Enable(false);
    update_bulk_controls();
    Layout();
}

void ProjectHistoryDialog::set_status(const wxString &message)
{
    // The label wraps (LB_AUTO_WRAP) so nothing is cut off horizontally; a
    // message that needs a second line also carries the tooltip, which covers
    // the remaining case where a short card cannot show every wrapped line.
    // wxST_NO_AUTORESIZE matters more than it looks here: without it,
    // wxStaticText::SetFont() runs AutoResizeIfNecessary() -> SetSize(GetBestSize())
    // and widens the control past its sizer slot. Callers such as update_selection()
    // change the face to Mono_13 immediately before this, and Label::SetLabel()
    // wraps against GetSize().x - so the wrap would be measured against that
    // widened width and the text would paint outside its slot.
    m_status_label->SetLabel(message);
    set_wrapped_tooltip(m_status_label, message);
    // LB_AUTO_WRAP does not imply wxST_NO_AUTORESIZE, so SetLabel() grows the
    // control to its wrapped height in place, outside the sizer that owns its
    // slot. Relayout here rather than in each caller: update_history_status()
    // and update_selection() have no layout pass of their own, and at the
    // dialog's 560 DIP minimum both of their strings wrap onto a second line
    // that would otherwise be drawn over the Load all versions button.
    Layout();
}

void ProjectHistoryDialog::show_empty_state()
{
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_status_label->SetFont(Label::Body_13);
    set_status(_L("No versions yet. A version is created automatically after completed edits and project saves."));
    m_refresh_button->Enable(true);
    m_list_truncated = false;
    m_load_all_button->Hide();
    m_load_all_button->Enable(false);
    m_restore_button->Enable(false);
    m_close_button->Enable(true);
    update_bulk_controls();
    Layout();
}

void ProjectHistoryDialog::show_error(const wxString &message)
{
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::Error));
    // The status line is shared with update_selection()'s mono commit id, and
    // begin_restore() can reach here without an intervening set_busy(). Reset
    // the face so prose never renders (or gets measured for its tooltip) in
    // the technical-value font.
    m_status_label->SetFont(Label::Body_13);
    set_status(message);
    m_refresh_button->Enable(m_pending == PendingOperation::None);
    const bool can_load_all = !m_versions.empty() && m_list_truncated;
    m_load_all_button->Show(can_load_all);
    m_load_all_button->Enable(can_load_all && m_pending == PendingOperation::None);
    m_restore_button->Enable(false);
    m_close_button->Enable(m_pending == PendingOperation::None);
    update_bulk_controls();
    Layout();
}

void ProjectHistoryDialog::update_selection()
{
    const std::size_t selected_count = m_bulk.size();
    const bool        single         = selected_count == 1;
    m_restore_button->Enable(m_pending == PendingOperation::None && single);
    if (selected_count > 1)
        // TRN: Tooltip of the disabled Restore button while several versions are selected.
        m_restore_button->SetToolTip(_L("Restore works on one version at a time: restoring several versions into the same project makes no sense. Select exactly one version."));
    else
        m_restore_button->SetToolTip(wxString());
    update_bulk_controls();
    if (single) {
        // Showing the complete object name here makes the abbreviated table id
        // unambiguous without forcing an excessively wide first column. Commit
        // ids are technical values, so render them in the MD3 mono face.
        m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        m_status_label->SetFont(Label::Mono_13);
        set_status(_L("Selected commit: ") + wxString::FromUTF8(*m_bulk.ids().begin()));
    } else if (!m_versions.empty())
        update_history_status();
}

std::vector<std::string> ProjectHistoryDialog::visible_ids() const
{
    std::vector<std::string> out;
    out.reserve(m_filtered_rows.size());
    for (const std::size_t index : m_filtered_rows)
        out.push_back(m_versions[index].commit_id);
    return out;
}

std::vector<std::string> ProjectHistoryDialog::loaded_ids() const
{
    std::vector<std::string> out;
    out.reserve(m_versions.size());
    for (const ProjectHistoryVersion &version : m_versions)
        out.push_back(version.commit_id);
    return out;
}

std::vector<std::string> ProjectHistoryDialog::selected_ids_in_order() const { return m_bulk.ordered_within(loaded_ids()); }

void ProjectHistoryDialog::sync_bulk_from_list()
{
    if (m_syncing_selection)
        return;
    // Only the visible rows are authoritative here: a filtered-out id keeps
    // whatever state it had, so a search never silently drops a selection.
    for (int row = 0; row < static_cast<int>(m_filtered_rows.size()); ++row)
        m_bulk.set(m_versions[m_filtered_rows[row]].commit_id, m_version_list->IsRowSelected(row));
}

void ProjectHistoryDialog::apply_bulk_to_list()
{
    m_syncing_selection = true;
    m_version_list->UnselectAll();
    for (int row = 0; row < static_cast<int>(m_filtered_rows.size()); ++row)
        if (m_bulk.contains(m_versions[m_filtered_rows[row]].commit_id))
            m_version_list->SelectRow(row);
    m_syncing_selection = false;
}

void ProjectHistoryDialog::select_visible()
{
    m_bulk.select_page(visible_ids());
    apply_bulk_to_list();
    update_selection();
}

void ProjectHistoryDialog::select_all_loaded()
{
    m_bulk.select_all_matches(loaded_ids());
    apply_bulk_to_list();
    update_selection();
}

void ProjectHistoryDialog::invert_selection()
{
    m_bulk.invert(visible_ids());
    apply_bulk_to_list();
    update_selection();
}

void ProjectHistoryDialog::update_bulk_controls()
{
    if (m_bulk_counts_label == nullptr)
        return;
    const bool idle    = m_pending == PendingOperation::None;
    const int  visible = static_cast<int>(m_filtered_rows.size());
    const int  loaded  = static_cast<int>(m_versions.size());
    // TRN: Bulk selection button; %d is the number of versions currently listed.
    m_select_visible_button->SetLabel(wxString::Format(_L("Select visible (%d)"), visible));
    m_select_visible_button->SetToolTip(_L("Selects every listed version") + " (Ctrl+A)");
    if (m_list_truncated) {
        // TRN: %d is the number of versions loaded so far; older ones are not loaded yet.
        m_select_all_button->SetLabel(wxString::Format(_L("Select all %d loaded versions"), loaded));
        m_select_all_button->SetToolTip(
            // TRN: Tooltip explaining that "Select all" stops at the loaded versions.
            _L("Selects every loaded version, including ones hidden by the search. Load all versions to include older ones.") +
            " (Ctrl+Shift+A)");
    } else {
        // TRN: Bulk selection button; %d is the total number of versions.
        m_select_all_button->SetLabel(wxString::Format(_L("Select all %d versions"), loaded));
        m_select_all_button->SetToolTip(_L("Selects every version, including ones hidden by the search") + " (Ctrl+Shift+A)");
    }
    m_invert_button->SetToolTip(_L("Inverts the selection of the listed versions") + " (Ctrl+I)");
    const std::size_t selected = m_bulk.size();
    const std::size_t hidden   = selected - m_bulk.count_within(visible_ids());
    wxString counts = wxString::Format(_L("%d selected"), static_cast<int>(selected));
    if (hidden > 0)
        // TRN: %d selected versions are currently hidden by the search filter.
        counts += " " + wxString::Format(_L("(%d hidden by the search)"), static_cast<int>(hidden));
    m_bulk_counts_label->SetLabel(counts);
    m_select_visible_button->Enable(idle && visible > 0);
    m_select_all_button->Enable(idle && loaded > 0);
    m_invert_button->Enable(idle && visible > 0);
    m_export_button->Enable(idle && selected > 0);
    m_label_button->Enable(idle && selected > 0);
    m_list_card->Layout();
}

void ProjectHistoryDialog::notify(const wxString &text)
{
    if (m_plater != nullptr && m_plater->get_notification_manager() != nullptr)
        m_plater->get_notification_manager()->push_notification(std::string(text.ToUTF8()));
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_status_label->SetFont(Label::Body_13);
    set_status(text);
}

void ProjectHistoryDialog::bulk_export()
{
    if (m_pending != PendingOperation::None || m_manager == nullptr)
        return;
    const std::vector<std::string> selected = selected_ids_in_order();
    if (selected.empty())
        return;

    // TRN: Title of the folder picker for exporting project versions.
    wxDirDialog picker(this, _L("Choose a folder for the exported versions"), wxEmptyString,
                       wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (picker.ShowModal() != wxID_OK)
        return;
    const std::filesystem::path folder(picker.GetPath().ToStdWstring());

    const wxString saved_project = m_plater != nullptr ? m_plater->get_project_filename(".3mf") : wxString{};
    wxString       stem          = saved_project.empty() ? wxString("untitled") : wxFileName(saved_project).GetName();
    if (stem.empty())
        stem = "untitled";

    struct ExportItem
    {
        std::string           commit_id;
        std::filesystem::path destination;
    };
    std::vector<ExportItem> items;
    Bulk::BulkActionPlan     plan;
    plan.action = _u8L("Export versions");
    plan.consequence =
        _u8L("Each version is written as its own .3mf named <project>-<commit>-<date>.3mf in the chosen folder. Nothing in the history changes.");
    for (const std::string &commit_id : selected) {
        const auto version_it = std::find_if(m_versions.begin(), m_versions.end(),
                                             [&commit_id](const ProjectHistoryVersion &v) { return v.commit_id == commit_id; });
        if (version_it == m_versions.end())
            continue;
        const std::time_t seconds = std::chrono::system_clock::to_time_t(version_it->committed_at);
        const wxDateTime  when(seconds);
        const wxString    file_name = wxString::Format("%s-%s-%s.3mf", stem, wxString::FromUTF8(short_commit(commit_id)),
                                                       when.IsValid() ? when.Format("%Y%m%d-%H%M%S") : wxString("unknown-time"));
        const std::filesystem::path destination = folder / std::filesystem::path(file_name.ToStdWstring());
        const std::string           label       = std::string(file_name.ToUTF8());
        const std::string           detail      = display_message(version_it->message).ToStdString(wxConvUTF8);
        std::error_code             ec;
        if (std::filesystem::exists(destination, ec)) {
            plan.items.push_back(Bulk::BulkItem::skipped(label, _u8L("a file with this name already exists"), detail));
            continue;
        }
        plan.items.push_back(Bulk::BulkItem::changed(label, detail));
        items.push_back({commit_id, destination});
    }
    if (!Bulk::BulkActionPreviewDialog::Run(this, plan))
        return;

    std::vector<std::string> failures;
    bool                     cancelled = false;
    std::size_t              failed    = 0;
    const std::size_t        done      = Bulk::BulkActionPreviewDialog::RunWithProgress(
        this, _L("Exporting versions"), items.size(),
        [&items](std::size_t i) { return wxString::FromUTF8(items[i].destination.filename().u8string()); },
        [this, &items, &failures](std::size_t i) {
            // The manager runs on its own worker; waiting here keeps the
            // operations serialized while the progress dialog owns the UI.
            std::future<ProjectHistoryRestoreResult> future =
                m_manager->restore_version(m_project_identity, items[i].commit_id, items[i].destination);
            const ProjectHistoryRestoreResult result = future.get();
            if (!result.ok())
                failures.push_back(items[i].destination.filename().u8string() + ": " + result.error.message);
            return result.ok();
        },
        &cancelled, &failed);

    // TRN: %1$d versions were exported out of %2$d selected.
    wxString summary = wxString::Format(_L("Exported %d of %d selected versions"), static_cast<int>(done - failed),
                                        static_cast<int>(selected.size()));
    if (plan.skipped() > 0)
        summary += wxString::Format(_L(", %d skipped"), static_cast<int>(plan.skipped()));
    if (failed > 0)
        summary += wxString::Format(_L(", %d failed"), static_cast<int>(failed));
    if (cancelled)
        summary += " " + _L("(cancelled)");
    if (!failures.empty())
        summary += "\n" + wxString::FromUTF8(failures.front());
    notify(summary);
}

void ProjectHistoryDialog::bulk_label()
{
    if (m_pending != PendingOperation::None || m_manager == nullptr)
        return;
    const std::vector<std::string> selected = selected_ids_in_order();
    if (selected.empty())
        return;

    LabelPromptDialog prompt(this, selected.size());
    if (prompt.ShowModal() != wxID_OK)
        return;
    const std::string label = std::string(prompt.value().ToUTF8());

    Bulk::BulkActionPlan plan;
    plan.action = _u8L("Label versions");
    // TRN: %s is the label text the user typed.
    plan.consequence = std::string(wxString::Format(_L("The label \"%s\" is attached to each version. History is never changed or removed."),
                                                    wxString::FromUTF8(label)).ToUTF8());
    for (const std::string &commit_id : selected) {
        const auto version_it = std::find_if(m_versions.begin(), m_versions.end(),
                                             [&commit_id](const ProjectHistoryVersion &v) { return v.commit_id == commit_id; });
        if (version_it == m_versions.end())
            continue;
        const std::string detail = display_message(version_it->message).ToStdString(wxConvUTF8);
        plan.items.push_back(Bulk::BulkItem::changed(short_commit(commit_id), detail, label));
    }
    if (!Bulk::BulkActionPreviewDialog::Run(this, plan))
        return;

    std::vector<std::string> failures;
    bool                     cancelled = false;
    std::size_t              failed    = 0;
    const std::size_t        done      = Bulk::BulkActionPreviewDialog::RunWithProgress(
        this, _L("Labelling versions"), selected.size(),
        [&selected](std::size_t i) { return wxString::FromUTF8(short_commit(selected[i])); },
        [this, &selected, &label, &failures](std::size_t i) {
            std::future<ProjectHistoryLabelResult> future = m_manager->label_version(m_project_identity, selected[i], label);
            const ProjectHistoryLabelResult        result = future.get();
            if (!result.ok())
                failures.push_back(short_commit(selected[i]) + ": " + result.error.message);
            return result.ok();
        },
        &cancelled, &failed);

    // TRN: %1$d versions were labelled out of %2$d selected.
    wxString summary = wxString::Format(_L("Labelled %d of %d selected versions"), static_cast<int>(done - failed),
                                        static_cast<int>(selected.size()));
    if (failed > 0)
        summary += wxString::Format(_L(", %d failed"), static_cast<int>(failed));
    if (cancelled)
        summary += " " + _L("(cancelled)");
    if (!failures.empty())
        summary += "\n" + wxString::FromUTF8(failures.front());
    notify(summary);
    // Reload so the new labels show in the list; the selection survives.
    refresh_versions();
}

void ProjectHistoryDialog::on_char_hook(wxKeyEvent &event)
{
    const bool ctrl  = event.ControlDown() || event.RawControlDown();
    const bool shift = event.ShiftDown();
    if (ctrl && !event.AltDown() && m_pending == PendingOperation::None && !m_versions.empty()) {
        if (event.GetKeyCode() == 'A') {
            if (shift) select_all_loaded(); else select_visible();
            return;
        }
        if (event.GetKeyCode() == 'I' && !shift) {
            invert_selection();
            return;
        }
    }
    event.Skip();
}

void ProjectHistoryDialog::cleanup_restore_temp()
{
    std::error_code error;
    if (!m_restored_snapshot.empty())
        std::filesystem::remove(m_restored_snapshot, error);
    if (!m_restore_temp_dir.empty()) {
        error.clear();
        std::filesystem::remove_all(m_restore_temp_dir, error);
    }
    m_restored_snapshot.clear();
    m_restore_temp_dir.clear();
}

void ProjectHistoryDialog::on_refresh(wxCommandEvent &) { refresh_versions(); }

void ProjectHistoryDialog::on_retry_failures(wxCommandEvent &)
{
    if (m_plater == nullptr || m_pending != PendingOperation::None)
        return;
    // The retry re-queues every quarantined snapshot onto the shared history
    // FIFO (draining asynchronously), so the banner clears optimistically here.
    // Any snapshot that fails again re-surfaces its own durable notification.
    m_plater->retry_project_history_failures();
    refresh_retained_failures();
    refresh_versions();
}

void ProjectHistoryDialog::on_load_all(wxCommandEvent &)
{
    if (m_pending != PendingOperation::None || !m_list_truncated)
        return;
    m_show_all = true;
    refresh_versions();
}

void ProjectHistoryDialog::on_restore(wxCommandEvent &) { begin_restore(); }

void ProjectHistoryDialog::on_close_button(wxCommandEvent &)
{
    if (m_pending == PendingOperation::None)
        EndModal(wxID_CANCEL);
}

void ProjectHistoryDialog::on_close_window(wxCloseEvent &event)
{
    if (m_pending != PendingOperation::None) {
        event.Veto();
        return;
    }
    event.Skip();
}

void ProjectHistoryDialog::on_selection_changed(wxDataViewEvent &)
{
    sync_bulk_from_list();
    update_selection();
}

void ProjectHistoryDialog::on_item_activated(wxDataViewEvent &)
{
    update_selection();
    begin_restore();
}

void ProjectHistoryDialog::on_size(wxSizeEvent &event)
{
    event.Skip();
    update_responsive_layout();
}

std::filesystem::path ProjectHistoryDialog::make_restore_destination()
{
    static std::atomic<std::uint64_t> sequence{0};

    if (m_manager == nullptr)
        return {};

    std::error_code error;
    const std::filesystem::path base = m_manager->history_root().parent_path() / "restore";
    std::filesystem::file_status base_status = std::filesystem::symlink_status(base, error);
    if (error && error != std::errc::no_such_file_or_directory)
        return {};
    if (!std::filesystem::exists(base_status)) {
        error.clear();
        if (!std::filesystem::create_directories(base, error) || error)
            return {};
        error.clear();
        base_status = std::filesystem::symlink_status(base, error);
    }
    if (error || std::filesystem::is_symlink(base_status) || !std::filesystem::is_directory(base_status))
        return {};

#ifndef _WIN32
    std::filesystem::permissions(base, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, error);
    if (error)
        return {};
#endif

    for (unsigned int attempt = 0; attempt < 32; ++attempt) {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path directory = base /
            ("restore-" + std::to_string(wxGetProcessId()) + "-" + std::to_string(nonce) + "-" +
             std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
        error.clear();
        if (!std::filesystem::create_directory(directory, error)) {
            error.clear();
            continue;
        }

        const std::filesystem::file_status directory_status = std::filesystem::symlink_status(directory, error);
        if (error || std::filesystem::is_symlink(directory_status) ||
            !std::filesystem::is_directory(directory_status)) {
            error.clear();
            std::filesystem::remove_all(directory, error);
            continue;
        }

#ifndef _WIN32
        std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace, error);
        if (error) {
            error.clear();
            std::filesystem::remove_all(directory, error);
            continue;
        }
#endif

        m_restore_temp_dir = directory;
        return directory / "snapshot.3mf";
    }
    return {};
}

wxString ProjectHistoryDialog::format_timestamp(const std::chrono::system_clock::time_point &timestamp)
{
    const std::time_t value = std::chrono::system_clock::to_time_t(timestamp);
    wxDateTime date(value);
    return date.IsValid() ? date.Format("%Y-%m-%d %H:%M") : _L("Unknown time");
}

wxString ProjectHistoryDialog::format_size(std::uint64_t bytes)
{
    constexpr double kib = 1024.0;
    constexpr double mib = 1024.0 * 1024.0;
    if (bytes >= static_cast<std::uint64_t>(mib))
        return wxString::Format("%.1f MiB", static_cast<double>(bytes) / mib);
    if (bytes >= static_cast<std::uint64_t>(kib))
        return wxString::Format("%.1f KiB", static_cast<double>(bytes) / kib);
    return wxString::Format("%llu B", static_cast<unsigned long long>(bytes));
}

wxString ProjectHistoryDialog::display_message(const std::string &message)
{
    wxString result = wxString::FromUTF8(message);
    result.Replace("\r", " ");
    result.Replace("\n", " ");
    result.Trim(true).Trim(false);
    return result.empty() ? _L("Project snapshot") : result;
}

void ProjectHistoryDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    (void) suggested_rect;
    for (Button *button : {m_refresh_button, m_load_all_button, m_retry_failures_button, m_restore_button, m_close_button,
                           m_select_visible_button, m_select_all_button, m_invert_button, m_export_button, m_label_button})
        button->Rescale();
    m_refresh_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_load_all_button->SetMinSize(FromDIP(wxSize(144, 36)));
    m_retry_failures_button->SetMinSize(FromDIP(wxSize(140, 36)));
    m_restore_button->SetMinSize(FromDIP(wxSize(154, 40)));
    m_close_button->SetMinSize(FromDIP(wxSize(104, 40)));
    update_window_constraints(false);
    update_responsive_layout();
}

void ProjectHistoryDialog::on_sys_color_changed()
{
    apply_theme();
}

} // namespace Slic3r::GUI
