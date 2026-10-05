#include "Widgets/TextArea.hpp"
#include "HumanDate.hpp"
#include "ProjectHistoryDialog.hpp"
#include "PreferencesHistory.hpp"
#include "PrinterHistory.hpp"
#include "LocalConfigHistory.hpp"
#include "HistorySearchStore.hpp"
#include "Widgets/TabStrip.hpp"
#include "libslic3r/Utils.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <set>
#include <wx/choice.h>
#include <wx/textctrl.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/wrapsizer.h>
#include "Widgets/MD3DataView.hpp"

#include "Export/ExportDatasets.hpp"
#include "Export/ExportDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"
#include "Plater.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <system_error>

#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/dcclient.h>
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/utils.h>
#include <wx/variant.h>

namespace Slic3r::GUI {

namespace {

constexpr int HISTORY_POLL_INTERVAL_MS = 75;
constexpr std::size_t HISTORY_INITIAL_LIMIT = 500;

// Project-history commit messages are persisted verbatim, in English, into an
// on-disk Git repository (see ProjectHistoryManager and the L() markers on
// their call sites in Plater.cpp, MainFrame.cpp and ConfigProfilesDialog.cpp).
// Translating the stored text would bake a non-English string into that
// history forever, so the raw English is kept on disk and only ever
// translated here, at display time, for the dialog's Message column. A
// message this table does not recognize (an arbitrary Undo/Redo snapshot
// name, for instance) is shown exactly as stored.
wxString translate_known_history_reason(const wxString &reason)
{
    // Only the English keys are cached in this table; each match is translated
    // with a fresh _() call below so a live language-mode switch is picked up
    // immediately, the same as every other _L()/_() call in this file.
    static const wxString known[] = {
        "Project edit",
        "Project settings changed",
        "Assembly undo or redo",
        "Undo or redo",
        "Autosave before closing project",
        "Autosave before shutdown",
        "Project edit before workspace save",
        "Project edit before save",
        "Project edit before current-version export",
        "Project edit before restore",
        "Project edit before opening version history",
        // Undo/Redo snapshot names that appear in the list as "Autosave: <name>".
        "Change Filament",
        "Change Filaments",
    };
    for (const wxString &candidate : known)
        if (reason == candidate)
            return _(candidate);
    return reason;
}

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

} // namespace

ProjectHistoryDialog::ProjectHistoryDialog(wxWindow *parent, Plater *plater)
    : DPIDialog(parent, wxID_ANY, _L("Local history"), wxDefaultPosition, wxDefaultSize,
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
    root->Add(new MD3DialogCaption(this, _L("Local history")), 0, wxEXPAND);

    m_title_label = new Label(this, Label::Head_24, _L("Local history"));
    root->Add(m_title_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // TRN: Subtitle in the project Version history dialog.
    m_subtitle_label = new Label(this, Label::Body_14,
        _L("Browse project, settings, draft and printer history saved on this device. No history is uploaded."));
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

    TabStrip::Options options;
    options.surface_key = "local-history"; options.surface_name = _L("Local history"); options.strip_name = _L("History views");
    options.default_edge = MD3::Tabs::DockEdge::Top; options.allow_close = false;
    auto *tabs = new TabStrip(this, options);
    tabs->AddTab("timeline", _L("Timeline")); tabs->AddTab("table", _L("Table")); tabs->AddTab("graph", _L("Git graph"));
    tabs->AddTab("compare", _L("Compare")); tabs->AddTab("searches", _L("Past searches"));
    tabs->LoadLayout(); tabs->Activate("timeline", false);
    tabs->Bind(EVT_TABSTRIP_ACTIVATE, [this](wxCommandEvent &event) { set_view(event.GetString().ToStdString()); });
    root->Add(tabs, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));
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
    auto *filters = new wxFlexGridSizer(2, 4, FromDIP(6), FromDIP(8));
    filters->AddGrowableCol(1); filters->AddGrowableCol(3);
    m_category_filter = new wxChoice(m_list_card, wxID_ANY);
    for (const auto &name : {_L("All categories"), _L("Project"), _L("Preferences"), _L("Preset"), _L("Draft"), _L("Printer")}) m_category_filter->Append(name);
    m_category_filter->SetSelection(0);
    m_status_filter = new wxChoice(m_list_card, wxID_ANY);
    for (const auto &name : {_L("All statuses"), _L("Active"), _L("Unknown"), _L("Resolved")}) m_status_filter->Append(name);
    m_status_filter->SetSelection(0);
    m_device_filter = new wxTextCtrl(m_list_card, wxID_ANY); m_device_filter->SetHint(_L("Device identifier"));
    m_store_filter = new wxChoice(m_list_card, wxID_ANY); m_store_filter->Append(_L("Current project")); m_store_filter->SetSelection(0);
    m_from_filter = new wxTextCtrl(m_list_card, wxID_ANY); m_from_filter->SetHint(_L("From YYYY-MM-DD"));
    m_to_filter = new wxTextCtrl(m_list_card, wxID_ANY); m_to_filter->SetHint(_L("Through YYYY-MM-DD"));
    for (auto *control : std::vector<wxWindow*>{m_category_filter, m_status_filter, m_device_filter, m_store_filter, m_from_filter, m_to_filter}) {
        filters->Add(control, 1, wxEXPAND);
        control->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { populate_versions(); update_selection(); });
        control->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { populate_versions(); update_selection(); });
    }
    m_submit_button = new Button(m_list_card, _L("Search")); m_submit_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { submit_search(); });
    filters->Add(m_submit_button, 0, wxEXPAND); list_sizer->Add(filters, 0, wxEXPAND | wxALL, FromDIP(8));
    m_search_field->GetTextCtrl()->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &event) { if (event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER) submit_search(); else event.Skip(); });
    m_version_list = new MD3DataViewListCtrl(m_list_card, wxID_ANY, wxDefaultPosition, wxDefaultSize,
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
    wxGetApp().UpdateDVCDarkUI(m_version_list); // native header follows the theme
    md3_style_data_view(m_version_list);
    list_sizer->Add(m_version_list, 1, wxEXPAND | wxALL, FromDIP(8));

    m_detail = new TextAreaEditor(m_list_card, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(400, 120)), wxTE_MULTILINE | wxTE_READONLY);
    m_detail->Hide(); list_sizer->Add(m_detail, 1, wxEXPAND | wxALL, FromDIP(8));
    auto *history_actions = new wxWrapSizer(wxHORIZONTAL);
    m_compare_button = new Button(m_list_card, _L("Compare selected")); m_pin_button = new Button(m_list_card, _L("Pin / unpin search"));
    m_rerun_button = new Button(m_list_card, _L("Run selected search")); m_delete_button = new Button(m_list_card, _L("Delete search")); m_clear_button = new Button(m_list_card, _L("Clear unpinned searches"));
    m_compare_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { compare_selection(); }); m_pin_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { pin_selection(); });
    m_rerun_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { rerun_search(); }); m_delete_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { delete_search(); });
    m_clear_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { clear_searches(); });
    for (Button *button : {m_compare_button, m_pin_button, m_rerun_button, m_delete_button, m_clear_button}) history_actions->Add(button, 0, wxALL, FromDIP(4));
    list_sizer->Add(history_actions, 0, wxEXPAND | wxALL, FromDIP(4));
    for (Button *button : {m_pin_button, m_rerun_button, m_delete_button, m_clear_button}) button->Hide();
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
    // TRN: Opens the shared Export dialog for the version list.
    m_export_button  = new Button(this, _L("Export..."));
    m_restore_button = new Button(this, _L("Restore selected"), "", 0, 0, wxID_APPLY);
    m_close_button   = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);

    m_refresh_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_export_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_restore_button->SetMinSize(FromDIP(wxSize(154, 40)));
    m_close_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_restore_button->Enable(false);
    m_export_button->SetToolTip(_L("Export every listed version as JSON, CSV, YAML, TOML, XML, Markdown, HTML or an archive"));

    m_refresh_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_refresh, this);
    m_export_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_export, this);
    m_restore_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_restore, this);
    m_close_button->Bind(wxEVT_BUTTON, &ProjectHistoryDialog::on_close_button, this);

    actions->AddStretchSpacer();
    actions->Add(m_refresh_button, 0, wxRIGHT, FromDIP(8));
    actions->Add(m_export_button, 0, wxRIGHT, FromDIP(8));
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
    for (Label *label : {m_project_label, m_status_label})
        label->SetBackgroundColour(card);

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
    for (Button *button : {m_refresh_button, m_export_button, m_load_all_button, m_close_button}) {
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
    if (m_pending != PendingOperation::None) return;
    const int row = m_version_list->GetSelectedRow();
    if (row >= 0 && static_cast<std::size_t>(row) < m_filtered_rows.size()) { const auto index = m_filtered_rows[row]; m_selected_id = m_versions[index].commit_id; m_selected_store = m_origins[index].category + ":" + m_origins[index].identity.string() + ":" + m_origins[index].device; }
    set_busy(PendingOperation::List, _L("Loading local history..."));
    const auto identity = m_project_identity; auto *manager = m_manager;
    const auto config_sources = LocalConfigHistory::sources(); const auto limit = m_show_all ? 0 : HISTORY_INITIAL_LIMIT;
    auto *prefs = PreferencesHistory::manager(); const auto prefs_identity = PreferencesHistory::identity();
    auto *printer = PrinterHistory::instance().manager(); const auto printer_identity = PrinterHistory::instance().identity();
    m_aggregate_future = std::async(std::launch::async, [identity, manager, config_sources, limit, prefs, prefs_identity, printer, printer_identity]() {
        Aggregate result;
        auto append = [&result, limit](ProjectHistoryManager *store, const std::filesystem::path &key, const std::string &category, const std::string &name) {
            if (!store || key.empty()) return;
            auto listed = store->list_versions(key, limit).get();
            if (!listed.ok()) { if (listed.error.code != ProjectHistoryErrorCode::NotFound && result.project.error.ok()) result.project.error = listed.error; return; }
            for (auto &version : listed.versions) { result.versions.push_back(std::move(version)); result.origins.push_back({category, name, "", "", "", key, store}); }
        };
        append(manager, identity, "project", "Current project"); append(prefs, prefs_identity, "preferences", "Preferences");
        append(printer, printer_identity, "printer_transition", "Printer transitions");
        for (const auto &source : config_sources) append(LocalConfigHistory::manager(), source.identity, source.category, source.name);
        return result;
    }); m_poll_timer.Start(HISTORY_POLL_INTERVAL_MS);
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
    if (selected_row == wxNOT_FOUND || static_cast<std::size_t>(selected_row) >= m_filtered_rows.size())
        return;
    const std::size_t selected_version = m_filtered_rows[selected_row];

    const Origin origin=m_origins[selected_version];
    if(origin.category!="project"&&origin.category!="preferences"&&origin.category!="preset"&&origin.category!="draft"){set_status(_L("Printer incident records cannot replace editor settings."));return;}
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
        m_restore_future = origin.manager->restore_version(origin.identity, m_versions[selected_version].commit_id, destination);
        m_restore_category = origin.category;
        m_restore_identity = origin.name;
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
    if (m_compare_future.valid() && m_compare_future.wait_for(0ms) == std::future_status::ready) {
        try { m_detail->SetValue(m_compare_future.get()); } catch (...) { m_detail->SetValue(_L("Could not compare these versions.")); }
        m_compare_button->Enable(true); Layout();
    }
    if (m_pending == PendingOperation::List && m_aggregate_future.valid() && m_aggregate_future.wait_for(0ms) == std::future_status::ready) {
        try {
            auto result = m_aggregate_future.get(); m_origins = std::move(result.origins); result.project.versions = std::move(result.versions);
            for (const auto &incident : PrinterHistory::instance().entries()) {
                ProjectHistoryVersion version; version.commit_id = "incident-" + std::to_string(incident.id);
                version.message = incident.code + ": " + incident.description;
                version.committed_at = std::chrono::system_clock::time_point(std::chrono::milliseconds(incident.first_seen_ms));
                result.project.versions.push_back(version);
                std::string detail = "Code: " + incident.code + "\nSeverity: " + std::to_string(incident.severity) + "\nFirst seen: " + wxDateTime(static_cast<time_t>(incident.first_seen_ms / 1000)).FormatISOCombined().ToStdString() + "\nLast seen: " + wxDateTime(static_cast<time_t>(incident.last_seen_ms / 1000)).FormatISOCombined().ToStdString() + "\nState: " + incident.state;
                if (incident.resolved_ms) detail += "\nResolved: " + wxDateTime(static_cast<time_t>(incident.resolved_ms / 1000)).FormatISOCombined().ToStdString();
                m_origins.push_back({"printer", incident.category, incident.device_id, incident.state, detail, {}, nullptr});
            }
            std::vector<std::size_t> order(result.project.versions.size()); for (std::size_t i=0;i<order.size();++i) order[i]=i;
            std::stable_sort(order.begin(),order.end(),[&result](auto a,auto b){return result.project.versions[a].committed_at > result.project.versions[b].committed_at;});
            auto versions=std::move(result.project.versions);auto origins=std::move(m_origins);
            for(auto index:order){result.project.versions.push_back(std::move(versions[index]));m_origins.push_back(std::move(origins[index]));}
            finish_list(std::move(result.project));
        } catch(const std::exception &error){m_pending=PendingOperation::None;show_error(wxString::FromUTF8(error.what()));}
    } else if (m_pending == PendingOperation::Restore && m_restore_future.valid() && m_restore_future.wait_for(0ms) == std::future_status::ready) {
        try { finish_restore(m_restore_future.get()); } catch (...) { m_pending=PendingOperation::None;cleanup_restore_temp();show_error(_L("Could not restore the selected version.")); }
    }
    if(m_pending==PendingOperation::None && !m_compare_future.valid())m_poll_timer.Stop();
}

void ProjectHistoryDialog::finish_list(ProjectHistoryListResult result)
{
    m_pending = PendingOperation::None;
    m_refresh_button->Enable(true);
    m_close_button->Enable(true);
    refresh_retained_failures();

    if (!result.ok() && result.versions.empty()) {
        if (result.error.code == ProjectHistoryErrorCode::NotFound) {
            show_empty_state();
            return;
        }
        show_error(wxString::Format(_L("Could not load version history: %s"), wxString::FromUTF8(result.error.message)));
        return;
    }

    // Previous row indices belong to the previous aggregate. Keep the captured
    // stable source selection instead of reading those indices in the new list.
    m_filtered_rows.clear();
    m_version_list->DeleteAllItems();
    m_versions = std::move(result.versions);
    m_list_truncated = !m_show_all && m_versions.size() >= HISTORY_INITIAL_LIMIT;

    m_load_all_button->Show(m_list_truncated);
    m_load_all_button->Enable(m_list_truncated);
    if (m_versions.empty()) {
        show_empty_state();
        return;
    }

    const wxString selected_store=m_store_filter->GetStringSelection(); m_store_filter->Clear();
    std::vector<std::string> stores;
    for(const auto &origin:m_origins)if(origin.manager && std::find(stores.begin(),stores.end(),origin.name)==stores.end()){stores.push_back(origin.name);m_store_filter->Append(wxString::FromUTF8(origin.name));}
    if(!stores.empty()){m_store_filter->SetSelection(0);const int index=m_store_filter->FindString(selected_store);if(index!=wxNOT_FOUND)m_store_filter->SetSelection(index);}
    populate_versions();update_history_status();update_selection();
    if(!result.ok())show_error(wxString::FromUTF8(result.error.message));
    const auto error=PrinterHistory::instance().last_error();if(!error.empty())show_error(wxString::FromUTF8(error));
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

    if(m_restore_category=="preferences"){std::string error;if(!PreferencesHistory::apply_snapshot(result.restored_path,error)){cleanup_restore_temp();show_error(wxString::FromUTF8(error));return;}cleanup_restore_temp();refresh_versions();return;}
    if (m_restore_category == "preset" || m_restore_category == "draft") {
        std::string error;
        const bool restored = LocalConfigHistory::restore_config(m_restore_category, m_restore_identity, result.restored_path, error);
        cleanup_restore_temp();
        if (!restored) { show_error(wxString::FromUTF8(error)); return; }
        refresh_versions(); return;
    }
    m_restored_snapshot = std::move(result.restored_path);
    EndModal(wxID_APPLY);
}

void ProjectHistoryDialog::populate_versions()
{
    if(m_view=="searches"){populate_searches();return;}
    const int selected=m_version_list->GetSelectedRow();
    if(selected>=0 && static_cast<std::size_t>(selected)<m_filtered_rows.size()){ const auto index = m_filtered_rows[selected]; m_selected_id=m_versions[index].commit_id; m_selected_store=m_origins[index].category+":"+m_origins[index].identity.string()+":"+m_origins[index].device; }
    m_version_list->DeleteAllItems();m_filtered_rows.clear();
    SearchField::MatchPass matcher(m_search_field->GetValue(),m_search_field->IsRegexEnabled(),m_search_field->IsCaseSensitive(),m_search_field->IsWholeWord(),m_search_field->IsMultiline());
    static const char *categories[]={"","project","preferences","preset","draft","printer"};static const char *states[]={"","active","unknown","resolved"};
    const int category=std::max(0,m_category_filter->GetSelection()),status=std::max(0,m_status_filter->GetSelection());
    const wxString from_text=m_from_filter->GetValue(),to_text=m_to_filter->GetValue();wxDateTime from,to;
    if((!from_text.empty()&&!from.ParseISODate(from_text))||(!to_text.empty()&&!to.ParseISODate(to_text))||(from.IsValid()&&to.IsValid()&&from>to)){set_status(_L("Enter valid dates in YYYY-MM-DD order, with From before Through."));return;}
    wxString graph;
    for(std::size_t i=0;i<m_versions.size()&&i<m_origins.size();++i){
        const auto &version=m_versions[i];const auto &origin=m_origins[i];
        if(category && origin.category!=categories[category] && !(category==5&&origin.category=="printer_transition"))continue;
        if(status && origin.status!=states[status])continue;
        if(!m_device_filter->GetValue().empty()&&wxString::FromUTF8(origin.device).Find(m_device_filter->GetValue())==wxNOT_FOUND)continue;
        const wxDateTime when(std::chrono::system_clock::to_time_t(version.committed_at));
        if(from.IsValid()&&when.GetDateOnly()<from.GetDateOnly())continue;if(to.IsValid()&&when.GetDateOnly()>to.GetDateOnly())continue;
        if(m_view=="graph"&&(!origin.manager||wxString::FromUTF8(origin.name)!=m_store_filter->GetStringSelection()))continue;
        const wxString id=wxString::FromUTF8(version.commit_id.substr(0,12));const wxString timestamp=format_timestamp(version.committed_at);
        const wxString message=wxString::FromUTF8(origin.category+" / "+origin.name+" / "+origin.status)+" : "+display_message(version.message);
        if(!matcher.matches(id+" "+message+" "+timestamp+" "+wxString::FromUTF8(origin.device)))continue;
        wxVector<wxVariant> row;row.push_back(wxVariant(m_view=="timeline"?timestamp:id));row.push_back(wxVariant(message));row.push_back(wxVariant(m_view=="timeline"?id:timestamp));
        row.push_back(wxVariant(origin.category=="printer"?wxString::FromUTF8(origin.status):format_size(version.snapshot_size)));
        m_version_list->AppendItem(row);m_filtered_rows.push_back(i);
        if(version.commit_id==m_selected_id && origin.category+":"+origin.identity.string()+":"+origin.device==m_selected_store)m_version_list->SelectRow(static_cast<unsigned>(m_filtered_rows.size()-1));
        if(m_view=="graph"){graph+="o "+wxString::FromUTF8(version.commit_id)+"  "+display_message(version.message)+"\n";for(const auto &parent:version.parent_ids)graph+="|  -> "+wxString::FromUTF8(parent)+"\n";if(version.parent_ids.empty())graph+=_L("Root commit")+"\n";}
    }
    if(m_view=="graph")m_detail->SetValue(graph.empty()?_L("No versions in the selected store."):graph);
    update_history_status();
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
        _L("Browse project, settings, draft and printer history saved on this device. No history is uploaded."));
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

    const wxSize target = initialize_size ? FromDIP(wxSize(980, 760)) : GetSize();
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
    Layout();
}

void ProjectHistoryDialog::update_selection()
{
    if(m_view=="searches"){m_restore_button->Enable(false);return;}
    const int row=m_version_list->GetSelectedRow();const bool selected=row>=0&&static_cast<std::size_t>(row)<m_filtered_rows.size();bool can_restore=false;
    if(selected){const auto index=m_filtered_rows[row];const auto &origin=m_origins[index];m_selected_id=m_versions[index].commit_id;m_selected_store=origin.category+":"+origin.identity.string()+":"+origin.device;can_restore=origin.category=="project"||origin.category=="preferences"||origin.category=="preset"||origin.category=="draft";
        if(m_view!="graph"&&m_view!="compare")set_status(wxString::FromUTF8(origin.category+" / "+origin.name+"\n"+m_versions[index].commit_id+"\n"+origin.detail));}
    m_restore_button->Enable(m_pending==PendingOperation::None&&selected&&can_restore);m_compare_button->Enable(selected&&!m_compare_future.valid());
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
    return date.IsValid() ? HumanDate::date_time(date) : _L("Unknown time");
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
    if (result.empty())
        return _L("Project snapshot");

    // A handful of standalone reasons are stored exactly as one of these
    // literals (see the L() markers in Plater.cpp and ConfigProfilesDialog.cpp).
    if (result == "Autosave project snapshot")  return _L("Autosave project snapshot");
    if (result == "Recovered unsaved project")  return _L("Recovered unsaved project");
    if (result == "Saved workspace member")     return _L("Saved workspace member");
    if (result == "Saved project")              return _L("Saved project");
    if (result == "Manual profile snapshot")    return _L("Manual profile snapshot");

    // Everything else that went through materialize_project_history_event()
    // is stored as "Autosave: <reason>"; translate the recognized reasons and
    // the fixed prefix, and leave an unrecognized (free-form snapshot name)
    // reason exactly as stored.
    static const wxString autosave_prefix = "Autosave: ";
    if (result.StartsWith(autosave_prefix)) {
        const wxString inner = result.Mid(autosave_prefix.size());
        return wxString::Format(_L("Autosave: %s"), translate_known_history_reason(inner));
    }

    return translate_known_history_reason(result);
}

void ProjectHistoryDialog::on_export(wxCommandEvent &)
{
    Export::Dataset dataset;dataset.name="Local history";dataset.schema_id="bambustudio.local-history";dataset.file_stem="local-history";
    using V=Export::Value;dataset.columns={{"id",V::Type::String},{"category",V::Type::String},{"description",V::Type::String},{"date",V::Type::String},{"status",V::Type::String}};
    if(m_view=="searches"){for(const auto &record:HistorySearchStore::instance().records())dataset.rows.push_back({V::from_string(record.id),V::from_string("search"),V::from_string(record.query),V::from_string(wxDateTime(static_cast<time_t>(record.submitted_at/1000)).FormatISOCombined().ToStdString()),V::from_string(record.pinned?"pinned":"recent")});}
    else for(auto index:m_filtered_rows){const auto &origin=m_origins[index];const auto &version=m_versions[index];dataset.rows.push_back({V::from_string(version.commit_id),V::from_string(origin.category),V::from_string(version.message),V::from_string(wxDateTime(std::chrono::system_clock::to_time_t(version.committed_at)).FormatISOCombined().ToStdString()),V::from_string(origin.status)});}
    ExportDialog::run(this,dataset);
}


void ProjectHistoryDialog::set_view(const std::string &view)
{
    if (view != "timeline" && view != "table" && view != "graph" && view != "compare" && view != "searches") return;
    m_view = view;
    const bool searches = view == "searches";
    m_detail->Show(view == "graph" || view == "compare");
    for (Button *button : {m_pin_button, m_rerun_button, m_delete_button, m_clear_button}) button->Show(searches);
    m_compare_button->Show(!searches);
    m_restore_button->Show(!searches);
    m_version_list->GetColumn(0)->SetTitle(searches ? _L("Pinned") : view == "timeline" ? _L("Time") : _L("Commit"));
    m_version_list->GetColumn(1)->SetTitle(searches ? _L("Search and filters") : _L("Category / description"));
    m_version_list->GetColumn(2)->SetTitle(searches ? _L("Submitted") : view == "timeline" ? _L("Commit") : _L("Time"));
    m_version_list->GetColumn(3)->SetTitle(searches ? _L("View") : _L("Size / state"));
    populate_versions(); update_selection(); Layout();
}

void ProjectHistoryDialog::submit_search()
{
    SearchRecord record;
    record.query = m_search_field->GetValue().ToUTF8().data(); record.regex = m_search_field->IsRegexEnabled();
    record.case_sensitive = m_search_field->IsCaseSensitive(); record.whole_word = m_search_field->IsWholeWord(); record.multiline = m_search_field->IsMultiline();
    record.view = m_view == "searches" ? "timeline" : m_view;
    record.category = std::to_string(m_category_filter->GetSelection()); record.status = std::to_string(m_status_filter->GetSelection());
    record.device = m_device_filter->GetValue().ToUTF8().data(); record.from = m_from_filter->GetValue().ToUTF8().data(); record.to = m_to_filter->GetValue().ToUTF8().data();
    wxDateTime from, to;
    if ((!record.from.empty() && !from.ParseISODate(wxString::FromUTF8(record.from))) || (!record.to.empty() && !to.ParseISODate(wxString::FromUTF8(record.to))) || (from.IsValid() && to.IsValid() && from > to)) {
        set_status(_L("Enter valid dates in YYYY-MM-DD order, with From before Through.")); return;
    }
    if (record.query.empty() && record.device.empty() && record.from.empty() && record.to.empty() && record.category == "0" && record.status == "0") {
        populate_versions(); return;
    }
    if (!HistorySearchStore::instance().submit(record)) { set_status(_L("This search was not saved. Check the query for sensitive data or check local storage.")); return; }
    populate_versions(); update_selection();
}

void ProjectHistoryDialog::populate_searches()
{
    m_version_list->DeleteAllItems(); m_search_ids.clear();
    SearchField::MatchPass match(m_search_field->GetValue(), m_search_field->IsRegexEnabled(), m_search_field->IsCaseSensitive(), m_search_field->IsWholeWord(), m_search_field->IsMultiline());
    const auto &records = HistorySearchStore::instance().records();
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        const auto &record = *it;
        const wxString description = wxString::FromUTF8(record.query + " [" + record.category + "/" + record.device + "/" + record.status + "] " + record.from + ".." + record.to);
        if (!match.matches(description)) continue;
        wxVector<wxVariant> row;
        row.push_back(wxVariant(record.pinned ? _L("Pinned") : _L("Recent")));
        row.push_back(wxVariant(description));
        row.push_back(wxVariant(format_timestamp(std::chrono::system_clock::time_point(std::chrono::milliseconds(record.submitted_at)))));
        row.push_back(wxVariant(wxString::FromUTF8(record.view)));
        m_version_list->AppendItem(row); m_search_ids.push_back(record.id);
    }
    set_status(_L("The last 100 submitted searches are retained. Pinned searches remain until explicitly deleted."));
}

void ProjectHistoryDialog::pin_selection()
{
    const int row = m_version_list->GetSelectedRow();
    if (row < 0 || static_cast<std::size_t>(row) >= m_search_ids.size()) return;
    const auto id = m_search_ids[row];
    for (const auto &record : HistorySearchStore::instance().records()) if (record.id == id) {
        if (!HistorySearchStore::instance().set_pinned(id, !record.pinned)) set_status(_L("Could not save this pinned search."));
        else populate_searches(); return;
    }
}

void ProjectHistoryDialog::rerun_search()
{
    const int row = m_version_list->GetSelectedRow(); if (row < 0 || static_cast<std::size_t>(row) >= m_search_ids.size()) return;
    const auto id = m_search_ids[row];
    for (const auto &stored : HistorySearchStore::instance().records()) if (stored.id == id) {
        const auto record = stored;
        m_search_field->SetValue(wxString::FromUTF8(record.query)); m_search_field->SetRegexEnabled(record.regex);
        m_search_field->SetCaseSensitive(record.case_sensitive); m_search_field->SetWholeWord(record.whole_word); m_search_field->SetMultiline(record.multiline);
        long category = 0, status = 0;
        wxString::FromUTF8(record.category).ToLong(&category); wxString::FromUTF8(record.status).ToLong(&status);
        m_category_filter->SetSelection(category >= 0 && category < m_category_filter->GetCount() ? static_cast<int>(category) : 0);
        m_status_filter->SetSelection(status >= 0 && status < m_status_filter->GetCount() ? static_cast<int>(status) : 0);
        m_device_filter->ChangeValue(wxString::FromUTF8(record.device)); m_from_filter->ChangeValue(wxString::FromUTF8(record.from)); m_to_filter->ChangeValue(wxString::FromUTF8(record.to));
        set_view(record.view); return;
    }
}
void ProjectHistoryDialog::delete_search()
{
    const int row = m_version_list->GetSelectedRow(); if (row < 0 || static_cast<std::size_t>(row) >= m_search_ids.size()) return;
    if (HistorySearchStore::instance().remove(m_search_ids[row])) populate_searches(); else set_status(_L("Could not delete the saved search."));
}
void ProjectHistoryDialog::clear_searches()
{
    if (HistorySearchStore::instance().clear(false)) populate_searches(); else set_status(_L("Could not clear submitted searches."));
}

namespace {
std::string history_payload_summary(const std::filesystem::path &path, const std::string &category)
{
    if (category == "preferences") {
        nlohmann::json values; std::string error;
        if (!PreferencesHistory::read_snapshot(path, values, error)) return "Legacy or unsupported snapshot: content excluded.\n";
        return values.dump(2);
    }
    if (category == "preset" || category == "draft") return nlohmann::json(LocalConfigHistory::read_snapshot(path)).dump(2);
    if (category == "printer_transition") {
        // Incident content is schema-bounded at its writer. Only lifecycle
        // fields are displayed, never arbitrary ledger properties.
        if (std::filesystem::file_size(path) > 4 * 1024 * 1024) return "Snapshot exceeds comparison limits.";
        std::ifstream in(path); auto value = nlohmann::json::parse(in);
        std::string summary;
        if (value.contains("entries") && value["entries"].is_array()) for (const auto &entry : value["entries"]) {
            for (const char *key : {"id", "code", "severity", "state", "first_seen_ms", "last_seen_ms", "resolved_ms"}) if (entry.contains(key)) summary += std::string(key) + ": " + entry[key].dump() + " ";
            summary += "\n";
        }
        return summary;
    }
    wxFileInputStream file(wxString(path.wstring())); wxZipInputStream zip(file);
    std::size_t objects = 0, vertices = 0, triangles = 0, model_files = 0;
    std::unique_ptr<wxZipEntry> entry;
    std::uint64_t expanded = 0;
    while ((entry.reset(zip.GetNextEntry()), entry)) {
        if (!entry->GetName().EndsWith(".model")) continue;
        constexpr std::size_t limit = 64 * 1024 * 1024;
        std::string xml; char buffer[16384];
        while (zip.IsOk() && !zip.Eof()) { zip.Read(buffer, sizeof(buffer)); const auto count = zip.LastRead(); expanded += count; if (expanded > limit) return "Geometry summary exceeds comparison limits."; xml.append(buffer, count); if (!count) break; }
        auto count_tags = [&xml](const char *tag) { std::size_t count=0, at=0; while ((at=xml.find(tag, at)) != std::string::npos) { ++count; at += std::char_traits<char>::length(tag); } return count; };
        objects += count_tags("<object "); vertices += count_tags("<vertex "); triangles += count_tags("<triangle "); ++model_files;
    }
    return "Geometry summary\nModel parts: " + std::to_string(model_files) + "\nObjects: " + std::to_string(objects) + "\nVertices: " + std::to_string(vertices) + "\nTriangles: " + std::to_string(triangles) + "\n";
}
}
void ProjectHistoryDialog::compare_selection()
{
    const int row = m_version_list->GetSelectedRow(); if (row < 0 || static_cast<std::size_t>(row) >= m_filtered_rows.size() || m_compare_future.valid()) return;
    const auto selected = m_filtered_rows[row];
    if (m_compare_id.empty()) { m_compare_id = m_versions[selected].commit_id; m_compare_index = selected; set_status(_L("Comparison base selected. Select another entry and press Compare selected.")); return; }
    if (m_compare_index >= m_versions.size() || m_versions[m_compare_index].commit_id != m_compare_id) { m_compare_id.clear(); set_status(_L("History changed. Select a comparison base again.")); return; }
    const auto before = m_versions[m_compare_index], after = m_versions[selected];
    const auto before_source = m_origins[m_compare_index], after_source = m_origins[selected];
    m_compare_id.clear();
    if (before_source.category != after_source.category || before_source.identity != after_source.identity || before_source.device != after_source.device) { set_status(_L("Choose two entries from the same history store or printer.")); return; }
    set_view("compare"); m_compare_button->Enable(false);
    if (before_source.category == "printer") { m_detail->SetValue(wxString::FromUTF8("Before\n" + before.message + "\n" + before_source.detail + "\n\nAfter\n" + after.message + "\n" + after_source.detail)); m_compare_button->Enable(true); return; }
    if (!before_source.manager) return;
    m_compare_future = std::async(std::launch::async, [before, after, before_source]() {
        const auto root = std::filesystem::path(data_dir()) / "history_compare";
        static std::atomic<unsigned long long> sequence{0};
        const auto temporary = root / (std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" + std::to_string(++sequence));
        std::filesystem::create_directories(temporary);
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{temporary};
        const auto left = before_source.manager->restore_version(before_source.identity, before.commit_id, temporary / "before.3mf").get();
        const auto right = before_source.manager->restore_version(before_source.identity, after.commit_id, temporary / "after.3mf").get();
        if (!left.ok() || !right.ok()) return _L("Could not read the selected versions for comparison.");
        if (before_source.category == "preset" || before_source.category == "draft" || before_source.category == "preferences") {
            std::map<std::string, std::string> a, b;
            if (before_source.category == "preferences") {
                nlohmann::json left_values, right_values; std::string error;
                if (!PreferencesHistory::read_snapshot(left.restored_path, left_values, error) ||
                    !PreferencesHistory::read_snapshot(right.restored_path, right_values, error))
                    return _L("Unsupported settings snapshot.");
                a = left_values["settings"].get<std::map<std::string, std::string>>();
                b = right_values["settings"].get<std::map<std::string, std::string>>();
            } else { a = LocalConfigHistory::read_snapshot(left.restored_path); b = LocalConfigHistory::read_snapshot(right.restored_path); }
            std::set<std::string> keys;
            for (const auto &value : a) keys.insert(value.first);
            for (const auto &value : b) keys.insert(value.first);
            std::string delta;
            for (const auto &key : keys) {
                const auto x = a.find(key), y = b.find(key);
                if (x != a.end() && y != b.end() && x->second == y->second) continue;
                delta += key + "\n  Before: " + (x == a.end() ? "(absent)" : x->second) + "\n  After: " + (y == b.end() ? "(removed)" : y->second) + "\n";
            }
            return wxString::FromUTF8("Before: " + before.commit_id + "\nAfter: " + after.commit_id + "\n\n" + (delta.empty() ? "No changed settings." : delta));
        }
        const auto a=history_payload_summary(left.restored_path,before_source.category), b=history_payload_summary(right.restored_path,before_source.category);
        return wxString::FromUTF8("Before: " + before.commit_id + "\n" + a + "\n\nAfter: " + after.commit_id + "\n" + b + (a==b ? "\nNo summary differences." : "\nSummary differs."));
    }); m_poll_timer.Start(HISTORY_POLL_INTERVAL_MS);
}

void ProjectHistoryDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    (void) suggested_rect;
    m_refresh_button->Rescale();
    m_export_button->Rescale();
    m_load_all_button->Rescale();
    m_retry_failures_button->Rescale();
    m_restore_button->Rescale();
    m_close_button->Rescale();
    m_refresh_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_export_button->SetMinSize(FromDIP(wxSize(104, 40)));
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
