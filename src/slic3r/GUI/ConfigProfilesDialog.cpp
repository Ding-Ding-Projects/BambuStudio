#include "ConfigProfilesDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/SlideToConfirm.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"

#include "libslic3r/Utils.hpp"

#include <wx/choicdlg.h>
#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/dir.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <chrono>

namespace Slic3r::GUI {

namespace {

constexpr int POLL_INTERVAL_MS = 120;

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

// Pack `source_dir` recursively into a zip at `archive`. Locked or unreadable
// files are skipped (their count is reported) so a live data directory can be
// exported without failing on transient logs / lock files.
wxString zip_directory(const std::filesystem::path &source_dir, const std::filesystem::path &archive, int *skipped)
{
    wxFFileOutputStream file_out(wxString::FromUTF8(archive.string()));
    if (!file_out.IsOk())
        return _L("The backup file could not be created at the selected location.");
    wxZipOutputStream zip(file_out, 6);
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(source_dir, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec)
            break;
        const std::filesystem::path &p = it->path();
        std::error_code type_ec;
        if (!it->is_regular_file(type_ec) || type_ec)
            continue;
        const std::filesystem::path rel = std::filesystem::relative(p, source_dir, type_ec);
        if (type_ec)
            continue;
        wxFFileInputStream in(wxString::FromUTF8(p.string()));
        if (!in.IsOk()) {
            if (skipped) ++*skipped;
            continue;
        }
        zip.PutNextEntry(wxString::FromUTF8(rel.generic_string()));
        zip.Write(in);
    }
    if (!zip.Close() || !file_out.Close())
        return _L("Writing the backup archive failed. The destination may be full or unwritable.");
    return wxString{};
}

wxString unzip_to_directory(const std::filesystem::path &archive, const std::filesystem::path &dest_dir)
{
    wxFFileInputStream file_in(wxString::FromUTF8(archive.string()));
    if (!file_in.IsOk())
        return _L("The selected backup file could not be opened.");
    wxZipInputStream zip(file_in);
    std::error_code ec;
    std::filesystem::create_directories(dest_dir, ec);
    if (ec)
        return _L("The new profile folder could not be created.");
    const std::filesystem::path canon_dest = std::filesystem::weakly_canonical(dest_dir, ec);
    for (wxZipEntry *entry = zip.GetNextEntry(); entry != nullptr; entry = zip.GetNextEntry()) {
        std::unique_ptr<wxZipEntry> guard(entry);
        if (entry->IsDir())
            continue;
        const std::filesystem::path rel = std::filesystem::path(entry->GetName().ToStdWstring());
        const std::filesystem::path out = dest_dir / rel;
        // Zip-slip guard: every extracted path must stay inside the profile.
        const std::filesystem::path canon_out = std::filesystem::weakly_canonical(out, ec);
        if (ec || canon_out.native().rfind(canon_dest.native(), 0) != 0)
            return _L("The backup contains an unsafe path and was rejected.");
        std::filesystem::create_directories(out.parent_path(), ec);
        wxFFileOutputStream out_stream(wxString::FromUTF8(out.string()));
        if (!out_stream.IsOk())
            return _L("A file inside the backup could not be written.");
        out_stream.Write(zip);
        out_stream.Close();
    }
    return wxString{};
}

} // namespace

ConfigProfilesDialog::ConfigProfilesDialog(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("Config profiles & backup"), wxDefaultPosition, wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_poll_timer(this)
{
    try {
        std::error_code ec;
        std::filesystem::create_directories(profiles_root(), ec);
        m_history = std::make_unique<ProjectHistoryManager>(profiles_root());
    } catch (const std::exception &) {
        m_history.reset();
    }
    create_ui();
    // MSW dark hook first, then the MD3 theme pass so semantic colours land
    // last and are not re-remapped into illegibility.
    wxGetApp().UpdateDlgDarkUI(this);
    apply_theme();
    refresh_profiles();
    Bind(wxEVT_TIMER, &ConfigProfilesDialog::poll_operation, this);
    SetMinSize(FromDIP(wxSize(680, 640)));
    SetSize(FromDIP(wxSize(720, 700)));
    CenterOnParent();
}

ConfigProfilesDialog::~ConfigProfilesDialog() = default;

std::filesystem::path ConfigProfilesDialog::profiles_root() const
{
    return std::filesystem::path(data_dir()).parent_path() / "BambuStudio-profiles";
}

std::filesystem::path ConfigProfilesDialog::profile_archive_path(const ProfileRow &row) const
{
    // Virtual identity file for the history engine; the actual snapshot
    // content is a freshly zipped archive committed against this identity.
    return profiles_root() / (row.name.ToStdString() + ".profile");
}

void ConfigProfilesDialog::create_ui()
{
    auto *root = new wxBoxSizer(wxVERTICAL);

    m_title_label = new Label(this, Label::Head_24, wxEmptyString);
    // SetLabelText: the '&' must render literally, not become a mnemonic.
    m_title_label->SetLabelText(_L("Config profiles & backup"));
    root->Add(m_title_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));
    m_subtitle_label = new Label(this, Label::Body_14,
        _L("Export everything in your data folder, import it on another PC, and keep unlimited profiles - each with its own local Git-backed snapshot history."));
    m_subtitle_label->Wrap(FromDIP(660));
    root->Add(m_subtitle_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    m_list_card = new StaticBox(this);
    auto *list_sizer = new wxBoxSizer(wxVERTICAL);
    // TRN: Placeholder of the search field filtering config profiles.
    m_search_field = new SearchField(m_list_card, _L("Search profiles"));
    m_search_field->SetOnQuery([this](const wxString &) { populate_profiles(); update_buttons(); });
    m_search_field->SetOnRegexToggle([this](bool) { populate_profiles(); update_buttons(); });
    list_sizer->Add(m_search_field, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    m_profile_list = new wxDataViewListCtrl(m_list_card, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                            wxDV_SINGLE | wxBORDER_NONE);
    m_profile_list->AppendTextColumn(_L("Profile"), wxDATAVIEW_CELL_INERT, FromDIP(180));
    m_profile_list->AppendTextColumn(_L("Data folder"), wxDATAVIEW_CELL_INERT, FromDIP(360));
    wxGetApp().UpdateDVCDarkUI(m_profile_list); // native header follows the theme
    m_profile_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) { update_buttons(); });
    list_sizer->Add(m_profile_list, 1, wxEXPAND | wxALL, FromDIP(8));

    auto *profile_actions = new wxBoxSizer(wxHORIZONTAL);
    m_launch_button   = new Button(m_list_card, _L("Launch profile"));
    m_snapshot_button = new Button(m_list_card, _L("Snapshot now"));
    m_history_button  = new Button(m_list_card, _L("History..."));
    for (Button *b : {m_launch_button, m_snapshot_button, m_history_button})
        b->SetMinSize(FromDIP(wxSize(140, 36)));
    m_launch_button->Bind(wxEVT_BUTTON, &ConfigProfilesDialog::on_launch, this);
    m_snapshot_button->Bind(wxEVT_BUTTON, &ConfigProfilesDialog::on_snapshot, this);
    m_history_button->Bind(wxEVT_BUTTON, &ConfigProfilesDialog::on_history, this);
    profile_actions->Add(m_launch_button, 0, wxRIGHT, FromDIP(8));
    profile_actions->Add(m_snapshot_button, 0, wxRIGHT, FromDIP(8));
    profile_actions->Add(m_history_button, 0);
    list_sizer->Add(profile_actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    m_list_card->SetSizer(list_sizer);
    root->Add(m_list_card, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    m_transfer_card = new StaticBox(this);
    auto *transfer_sizer = new wxBoxSizer(wxVERTICAL);
    m_secrets_label = new Label(m_transfer_card, Label::Body_13,
        _L("A full backup includes secrets: printer access codes, network-plugin login state, API tokens and physical-printer passwords. Anyone with the file can use them. Slide to confirm before exporting or importing."));
    m_secrets_label->Wrap(FromDIP(620));
    transfer_sizer->Add(m_secrets_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(14));
    // TRN: Instruction inside the slide-to-confirm control guarding config export/import.
    m_confirm_slider = new SlideToConfirm(m_transfer_card, _L("Slide to allow export / import"),
                                          // TRN: Shown inside the slide-to-confirm control once completed.
                                          _L("Confirmed - transfers unlocked"));
    m_confirm_slider->SetOnConfirm([this]() { update_buttons(); });
    transfer_sizer->Add(m_confirm_slider, 0, wxEXPAND | wxALL, FromDIP(14));

    auto *transfer_actions = new wxBoxSizer(wxHORIZONTAL);
    m_export_button = new Button(m_transfer_card, _L("Export everything..."));
    m_import_button = new Button(m_transfer_card, _L("Import backup..."));
    for (Button *b : {m_export_button, m_import_button})
        b->SetMinSize(FromDIP(wxSize(180, 40)));
    m_export_button->Bind(wxEVT_BUTTON, &ConfigProfilesDialog::on_export, this);
    m_import_button->Bind(wxEVT_BUTTON, &ConfigProfilesDialog::on_import, this);
    transfer_actions->Add(m_export_button, 0, wxRIGHT, FromDIP(8));
    transfer_actions->Add(m_import_button, 0);
    transfer_sizer->Add(transfer_actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    m_transfer_card->SetSizer(transfer_sizer);
    root->Add(m_transfer_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    m_status_label = new Label(this, Label::Body_13);
    root->Add(m_status_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_close_button = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
    m_close_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_close_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    actions->AddStretchSpacer();
    actions->Add(m_close_button, 0);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(24));

    SetSizer(root);
    update_buttons();
}

void ConfigProfilesDialog::apply_theme()
{
    const wxColour surface   = StateColor::semantic(MD3::Role::Surface);
    const wxColour card      = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour text      = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour secondary = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const wxColour outline   = StateColor::semantic(MD3::Role::OutlineVariant);

    SetBackgroundColour(surface);
    for (Label *label : {m_title_label, m_subtitle_label, m_status_label})
        label->SetBackgroundColour(surface);
    m_title_label->SetForegroundColour(text);
    m_subtitle_label->SetForegroundColour(secondary);
    m_status_label->SetForegroundColour(secondary);
    m_secrets_label->SetBackgroundColour(StateColor::semantic(MD3::Role::ErrorContainer));

    for (StaticBox *box : {m_list_card, m_transfer_card}) {
        box->SetBackgroundColorNormal(card);
        box->SetBorderColorNormal(outline);
        box->SetBorderWidth(1);
    }
    // The secrets warning uses the error-container roles: prominent without
    // reading as a blocking failure.
    m_transfer_card->SetBackgroundColorNormal(StateColor::semantic(MD3::Role::ErrorContainer));
    m_transfer_card->SetBorderColorNormal(StateColor::semantic(MD3::Role::Error));
    m_secrets_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));

    m_profile_list->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_profile_list->SetForegroundColour(text);
    m_profile_list->SetAlternateRowColour(StateColor::semantic(MD3::Role::SurfaceContainer));

    const StateColor outlined_bg = outlined_button_background();
    const StateColor outlined_border(outline);
    const StateColor outlined_text(text);
    for (Button *button : {m_launch_button, m_snapshot_button, m_history_button, m_import_button, m_close_button}) {
        button->SetBackgroundColor(outlined_bg);
        button->SetBorderColor(outlined_border);
        button->SetTextColor(outlined_text);
    }
    m_export_button->SetBackgroundColor(filled_button_background());
    m_export_button->SetBorderColor(StateColor(StateColor::semantic(MD3::Role::Primary)));
    m_export_button->SetTextColor(filled_button_text());
    Refresh();
}

void ConfigProfilesDialog::refresh_profiles()
{
    m_profiles.clear();
    ProfileRow active;
    active.name     = _L("Current (active)");
    active.data_dir = std::filesystem::path(data_dir());
    active.active   = true;
    m_profiles.push_back(active);

    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(profiles_root(), ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        std::error_code type_ec;
        if (!it->is_directory(type_ec) || type_ec)
            continue;
        const std::string name = it->path().filename().string();
        if (name == "project_history" || name.rfind('.', 0) == 0)
            continue; // history engine's own storage, not a profile
        ProfileRow row;
        row.name     = wxString::FromUTF8(name);
        row.data_dir = it->path();
        m_profiles.push_back(row);
    }
    populate_profiles();
    update_buttons();
}

void ConfigProfilesDialog::populate_profiles()
{
    m_profile_list->DeleteAllItems();
    m_filtered_rows.clear();
    const wxString query = m_search_field != nullptr ? m_search_field->GetValue() : wxString{};
    const bool regex      = m_search_field != nullptr && m_search_field->IsRegexEnabled();
    const bool case_sense = m_search_field != nullptr && m_search_field->IsCaseSensitive();
    const bool whole_word = m_search_field != nullptr && m_search_field->IsWholeWord();
    for (std::size_t i = 0; i < m_profiles.size(); ++i) {
        const ProfileRow &row = m_profiles[i];
        const wxString path_text = wxString::FromUTF8(row.data_dir.string());
        if (!query.IsEmpty() &&
            !SearchField::textMatches(query, row.name + " " + path_text, regex, case_sense, whole_word))
            continue;
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(row.name));
        cells.push_back(wxVariant(path_text));
        m_profile_list->AppendItem(cells);
        m_filtered_rows.push_back(i);
    }
}

const ConfigProfilesDialog::ProfileRow *ConfigProfilesDialog::selected_profile() const
{
    const int row = m_profile_list->GetSelectedRow();
    if (row == wxNOT_FOUND || static_cast<std::size_t>(row) >= m_filtered_rows.size())
        return nullptr;
    return &m_profiles[m_filtered_rows[row]];
}

void ConfigProfilesDialog::update_buttons()
{
    const ProfileRow *sel = selected_profile();
    const bool armed = m_confirm_slider != nullptr && m_confirm_slider->IsConfirmed();
    m_export_button->Enable(!m_busy && armed);
    m_import_button->Enable(!m_busy && armed);
    m_launch_button->Enable(!m_busy && sel != nullptr && !sel->active);
    m_snapshot_button->Enable(!m_busy && sel != nullptr && m_history != nullptr);
    m_history_button->Enable(!m_busy && sel != nullptr && m_history != nullptr);
}

void ConfigProfilesDialog::poll_operation(wxTimerEvent &)
{
    if (!m_busy_future.valid() ||
        m_busy_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    m_poll_timer.Stop();
    m_busy = false;
    const wxString error = m_busy_future.get();
    if (m_busy_done)
        m_busy_done(error);
    m_busy_done = nullptr;
    update_buttons();
}

void ConfigProfilesDialog::on_export(wxCommandEvent &)
{
    if (m_busy || m_confirm_slider == nullptr || !m_confirm_slider->IsConfirmed())
        return;
    const wxString default_name = wxString::Format("BambuStudio-data-%s.zip", wxDateTime::Now().Format("%Y%m%d-%H%M"));
    wxFileDialog dialog(this, _L("Export the complete data folder"), wxEmptyString, default_name,
                        _L("Backup archive (*.zip)|*.zip"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const std::filesystem::path destination = std::filesystem::path(dialog.GetPath().ToStdWstring());
    const std::filesystem::path source      = std::filesystem::path(data_dir());
    m_busy = true;
    m_status_label->SetLabel(_L("Exporting the data folder... this can take a while."));
    m_busy_future = std::async(std::launch::async, [source, destination]() -> wxString {
        int skipped = 0;
        wxString err = zip_directory(source, destination, &skipped);
        if (!err.IsEmpty())
            return err;
        return wxString{};
    });
    m_busy_done = [this, destination](wxString error) {
        m_confirm_slider->Reset();
        m_status_label->SetLabel(error.IsEmpty()
            ? wxString::Format(_L("Backup written to %s. It contains secrets - store and transfer it safely."),
                               wxString::FromUTF8(destination.string()))
            : error);
    };
    m_poll_timer.Start(POLL_INTERVAL_MS);
    update_buttons();
}

void ConfigProfilesDialog::on_import(wxCommandEvent &)
{
    if (m_busy || m_confirm_slider == nullptr || !m_confirm_slider->IsConfirmed())
        return;
    wxFileDialog dialog(this, _L("Import a data folder backup"), wxEmptyString, wxEmptyString,
                        _L("Backup archive (*.zip)|*.zip"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const std::filesystem::path archive = std::filesystem::path(dialog.GetPath().ToStdWstring());
    // Import never touches the live data directory: it lands in a fresh
    // profile which the user can then launch (or snapshot / inspect) safely.
    std::string base = archive.stem().string();
    if (base.empty())
        base = "imported-profile";
    std::filesystem::path target = profiles_root() / base;
    std::error_code ec;
    for (int n = 2; std::filesystem::exists(target, ec); ++n)
        target = profiles_root() / (base + "-" + std::to_string(n));
    m_busy = true;
    m_status_label->SetLabel(_L("Importing the backup into a new profile..."));
    m_busy_future = std::async(std::launch::async, [archive, target]() -> wxString {
        return unzip_to_directory(archive, target);
    });
    m_busy_done = [this, target](wxString error) {
        m_confirm_slider->Reset();
        if (error.IsEmpty()) {
            m_status_label->SetLabel(wxString::Format(
                _L("Imported as profile \"%s\". Select it and press Launch profile to run Bambu Studio on it."),
                wxString::FromUTF8(target.filename().string())));
            refresh_profiles();
        } else {
            m_status_label->SetLabel(error);
        }
    };
    m_poll_timer.Start(POLL_INTERVAL_MS);
    update_buttons();
}

void ConfigProfilesDialog::on_launch(wxCommandEvent &)
{
    const ProfileRow *sel = selected_profile();
    if (sel == nullptr || sel->active)
        return;
    const wxString exe = wxStandardPaths::Get().GetExecutablePath();
    const wxString cmd = wxString::Format("\"%s\" --datadir \"%s\"", exe, wxString::FromUTF8(sel->data_dir.string()));
    if (wxExecute(cmd, wxEXEC_ASYNC) <= 0)
        m_status_label->SetLabel(_L("The profile could not be launched."));
    else
        m_status_label->SetLabel(wxString::Format(_L("Launching Bambu Studio on profile \"%s\"..."), sel->name));
}

void ConfigProfilesDialog::on_snapshot(wxCommandEvent &)
{
    const ProfileRow *sel = selected_profile();
    if (m_busy || sel == nullptr || m_history == nullptr)
        return;
    const ProfileRow row = *sel;
    const std::filesystem::path identity = profile_archive_path(row);
    const std::filesystem::path staging  = profiles_root() / ".staging" /
        (identity.filename().string() + ".zip");
    m_busy = true;
    m_status_label->SetLabel(_L("Recording a complete profile snapshot..."));
    ProjectHistoryManager *history = m_history.get();
    m_busy_future = std::async(std::launch::async, [row, identity, staging, history]() -> wxString {
        std::error_code ec;
        std::filesystem::create_directories(staging.parent_path(), ec);
        int skipped = 0;
        wxString err = zip_directory(row.data_dir, staging, &skipped);
        if (!err.IsEmpty())
            return err;
        ProjectHistoryCommitOptions options;
        options.message = "Manual profile snapshot";
        auto result = history->commit_snapshot(identity, staging, options).get();
        std::filesystem::remove(staging, ec);
        if (!result.ok())
            return wxString::Format(_L("The snapshot could not be recorded: %s"),
                                    wxString::FromUTF8(result.error.message));
        return wxString{};
    });
    m_busy_done = [this, name = row.name](wxString error) {
        m_status_label->SetLabel(error.IsEmpty()
            ? wxString::Format(_L("Snapshot of \"%s\" recorded in its local history."), name)
            : error);
    };
    m_poll_timer.Start(POLL_INTERVAL_MS);
    update_buttons();
}

void ConfigProfilesDialog::on_history(wxCommandEvent &)
{
    const ProfileRow *sel = selected_profile();
    if (m_busy || sel == nullptr || m_history == nullptr)
        return;
    const ProfileRow row = *sel;
    auto versions = m_history->list_versions(profile_archive_path(row)).get();
    if (!versions.ok()) {
        m_status_label->SetLabel(wxString::Format(_L("Profile history could not be read: %s"),
                                                  wxString::FromUTF8(versions.error.message)));
        return;
    }
    if (versions.versions.empty()) {
        m_status_label->SetLabel(_L("This profile has no snapshots yet. Use Snapshot now to record one."));
        return;
    }
    wxArrayString choices;
    for (const auto &v : versions.versions) {
        const wxDateTime when(std::chrono::system_clock::to_time_t(v.committed_at));
        choices.Add(wxString::Format("%s  |  %s  |  %s",
                                     when.Format("%Y-%m-%d %H:%M"),
                                     wxString::FromUTF8(v.message),
                                     wxString::FromUTF8(v.commit_id.substr(0, 12))));
    }
    // TRN: %s is a profile name; the dialog lists its recorded snapshots.
    wxSingleChoiceDialog picker(this, wxString::Format(_L("Snapshots of \"%s\". Restoring creates a NEW profile; nothing is overwritten."), row.name),
                                _L("Profile history"), choices);
    if (picker.ShowModal() != wxID_OK)
        return;
    const auto &version = versions.versions[picker.GetSelection()];
    std::string base = row.name.ToStdString() + "-restored";
    std::filesystem::path target = profiles_root() / base;
    std::error_code ec;
    for (int n = 2; std::filesystem::exists(target, ec); ++n)
        target = profiles_root() / (base + "-" + std::to_string(n));
    const std::filesystem::path staging = profiles_root() / ".staging" / (base + "-restore.zip");
    const std::filesystem::path identity = profile_archive_path(row);
    m_busy = true;
    m_status_label->SetLabel(_L("Restoring the selected snapshot into a new profile..."));
    ProjectHistoryManager *history = m_history.get();
    const std::string commit_id = version.commit_id;
    m_busy_future = std::async(std::launch::async, [history, identity, commit_id, staging, target]() -> wxString {
        std::error_code ec2;
        std::filesystem::create_directories(staging.parent_path(), ec2);
        std::filesystem::remove(staging, ec2);
        auto restored = history->restore_version(identity, commit_id, staging).get();
        if (!restored.ok())
            return wxString::Format(_L("The snapshot could not be restored: %s"),
                                    wxString::FromUTF8(restored.error.message));
        wxString err = unzip_to_directory(staging, target);
        std::filesystem::remove(staging, ec2);
        return err;
    });
    m_busy_done = [this, target](wxString error) {
        if (error.IsEmpty()) {
            m_status_label->SetLabel(wxString::Format(
                _L("Snapshot restored as profile \"%s\"."), wxString::FromUTF8(target.filename().string())));
            refresh_profiles();
        } else {
            m_status_label->SetLabel(error);
        }
    };
    m_poll_timer.Start(POLL_INTERVAL_MS);
    update_buttons();
}

void ConfigProfilesDialog::on_dpi_changed(const wxRect &)
{
    if (m_search_field) m_search_field->Rescale();
    if (m_confirm_slider) m_confirm_slider->Rescale();
    Layout();
    Refresh();
}

void ConfigProfilesDialog::on_sys_color_changed()
{
    apply_theme();
}

} // namespace Slic3r::GUI
