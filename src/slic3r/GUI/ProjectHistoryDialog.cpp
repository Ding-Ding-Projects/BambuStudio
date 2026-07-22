#include "ProjectHistoryDialog.hpp"

#include "I18N.hpp"
#include "MsgDialog.hpp"
#include "Plater.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <system_error>

#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/sizer.h>
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

} // namespace

ProjectHistoryDialog::ProjectHistoryDialog(wxWindow *parent, Plater *plater)
    : DPIDialog(parent, wxID_ANY, _L("Version history"), wxDefaultPosition, wxDefaultSize,
                wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER)
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

    SetEscapeId(wxID_CANCEL);
    SetAffirmativeId(wxID_APPLY);
    update_window_constraints(true);
    CentreOnParent();
    update_responsive_layout();

    refresh_versions();
    refresh_retained_failures();
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
    m_project_label = new Label(m_info_card, Label::Head_14,
        wxString::Format(_L("Project: %s"), project_name));
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
    m_version_list = new wxDataViewListCtrl(m_list_card, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                            wxDV_SINGLE | wxBORDER_NONE);
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
    list_sizer->Add(m_version_list, 1, wxEXPAND | wxALL, FromDIP(8));

    m_status_label = new Label(m_list_card, Label::Body_13);
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

    const int selected_row = m_version_list->GetSelectedRow();
    if (selected_row == wxNOT_FOUND || static_cast<std::size_t>(selected_row) >= m_versions.size())
        return;

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
        m_restore_future = m_manager->restore_version(m_project_identity, m_versions[selected_row].commit_id, destination);
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
        show_empty_state();
        return;
    }

    populate_versions();
    update_history_status();
    m_restore_button->Enable(false);
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
    for (const ProjectHistoryVersion &version : m_versions) {
        wxVector<wxVariant> row;
        const std::string short_id = version.commit_id.substr(0, std::min<std::size_t>(12, version.commit_id.size()));
        row.push_back(wxVariant(wxString::FromUTF8(short_id)));
        row.push_back(wxVariant(display_message(version.message)));
        row.push_back(wxVariant(format_timestamp(version.committed_at)));
        row.push_back(wxVariant(format_size(version.snapshot_size)));
        m_version_list->AppendItem(row);
    }
}

void ProjectHistoryDialog::update_history_status()
{
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_status_label->SetFont(Label::Body_13);
    if (m_list_truncated) {
        // TRN: %d is the number of newest local project versions currently displayed.
        m_status_label->SetLabel(wxString::Format(
            _L("Showing the newest %d versions. Load all versions to browse older history."),
            static_cast<int>(HISTORY_INITIAL_LIMIT)));
    } else {
        // TRN: %d is the number of immutable project versions in local history.
        m_status_label->SetLabel(
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
    m_status_label->SetLabel(message);
    m_refresh_button->Enable(false);
    m_load_all_button->Enable(false);
    m_restore_button->Enable(false);
    m_close_button->Enable(false);
    if (m_retry_failures_button != nullptr)
        m_retry_failures_button->Enable(false);
    Layout();
}

void ProjectHistoryDialog::show_empty_state()
{
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_status_label->SetLabel(_L("No versions yet. A version is created automatically after completed edits and project saves."));
    m_refresh_button->Enable(true);
    m_list_truncated = false;
    m_load_all_button->Hide();
    m_load_all_button->Enable(false);
    m_restore_button->Enable(false);
    m_close_button->Enable(true);
    Layout();
}

void ProjectHistoryDialog::show_error(const wxString &message)
{
    m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::Error));
    m_status_label->SetLabel(message);
    m_refresh_button->Enable(m_pending == PendingOperation::None);
    const bool can_load_all = !m_versions.empty() && m_list_truncated;
    m_load_all_button->Show(can_load_all);
    m_load_all_button->Enable(can_load_all && m_pending == PendingOperation::None);
    m_restore_button->Enable(false);
    m_close_button->Enable(m_pending == PendingOperation::None);
    Layout();
}

void ProjectHistoryDialog::update_selection()
{
    const int selected_row = m_version_list->GetSelectedRow();
    const bool has_selection = selected_row != wxNOT_FOUND && static_cast<std::size_t>(selected_row) < m_versions.size();
    m_restore_button->Enable(m_pending == PendingOperation::None && has_selection);
    if (has_selection) {
        // Showing the complete object name here makes the abbreviated table id
        // unambiguous without forcing an excessively wide first column. Commit
        // ids are technical values, so render them in the MD3 mono face.
        m_status_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        m_status_label->SetFont(Label::Mono_13);
        m_status_label->SetLabel(_L("Selected commit: ") + wxString::FromUTF8(m_versions[selected_row].commit_id));
    } else if (!m_versions.empty())
        update_history_status();
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

void ProjectHistoryDialog::on_selection_changed(wxDataViewEvent &) { update_selection(); }

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
    m_refresh_button->Rescale();
    m_load_all_button->Rescale();
    m_retry_failures_button->Rescale();
    m_restore_button->Rescale();
    m_close_button->Rescale();
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
