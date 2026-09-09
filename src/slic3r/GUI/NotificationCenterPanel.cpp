#include "NotificationCenterPanel.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "NotificationManager.hpp"
#include "Plater.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/SearchField.hpp"
#include "Bulk/BulkActionPlan.hpp"
#include "Bulk/BulkActionPreviewDialog.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"

#include <algorithm>
#include <fstream>

#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>

#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/variant.h>

namespace Slic3r { namespace GUI {

namespace {

// Column order of the list; the selection column is native (row highlight).
enum Column { COL_LEVEL = 0, COL_TIME, COL_TITLE, COL_TEXT, COL_STATUS, COL_ACTION };

constexpr int TIMER_INTERVAL_MS = 1000;

} // namespace

NotificationCenterPanel::NotificationCenterPanel(wxWindow *parent, NotificationManager *manager)
    : MD3Dialog(parent, _L("Notification centre"),
                _L("Every toast this app has shown, newest first. Nothing here blocks your work."),
                MaterialIcon::Notifications, MD3Dialog::Options{/*resizable=*/true, /*forced_dark=*/false})
    , m_manager(manager)
    , m_timer(this)
{
    SetName("notification_center");
    build_ui();
    apply_theme();
    SetMinSize(FromDIP(wxSize(520, 420)));
    SetSize(FromDIP(wxSize(720, 560)));
    Layout();

    Bind(wxEVT_TIMER, &NotificationCenterPanel::on_timer, this, m_timer.GetId());
    // Escape closes (hides) the popover from anywhere inside it.
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &event) {
        if (event.GetKeyCode() == WXK_ESCAPE) {
            OnHeaderClose();
            return;
        }
        event.Skip();
    });
    Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
        if (event.IsShown()) {
            if (m_manager != nullptr)
                m_manager->mark_history_seen();
            RefreshNow();
            m_timer.Start(TIMER_INTERVAL_MS);
            if (m_list != nullptr)
                m_list->SetFocus();
        } else {
            m_timer.Stop();
        }
        event.Skip();
    });

    RefreshNow();
}

NotificationCenterPanel::~NotificationCenterPanel() { m_timer.Stop(); }

void NotificationCenterPanel::OnHeaderClose()
{
    // Non-modal popover: hiding keeps the filter, page and selection for the
    // next open (the bell toggles it back).
    Hide();
}

void NotificationCenterPanel::AnchorBelow(const wxRect &screen_anchor)
{
    const wxSize size = GetSize();
    int          x    = screen_anchor.GetRight() - size.GetWidth();
    int          y    = screen_anchor.GetBottom() + FromDIP(4);
    const int    idx  = wxDisplay::GetFromPoint(screen_anchor.GetPosition());
    const wxRect area = wxDisplay(idx == wxNOT_FOUND ? 0u : static_cast<unsigned>(idx)).GetClientArea();
    x = std::max(area.GetLeft(), std::min(x, area.GetRight() - size.GetWidth()));
    if (y + size.GetHeight() > area.GetBottom())
        y = std::max(area.GetTop(), area.GetBottom() - size.GetHeight());
    SetPosition(wxPoint(x, y));
}

// ---------------------------------------------------------------------------
// Construction

void NotificationCenterPanel::build_ui()
{
    wxBoxSizer *root = GetContentSizer();

    // --- Search + filter chips -------------------------------------------
    // TRN: Placeholder of the notification centre search field.
    m_search = new SearchField(this, _L("Search notifications"));
    m_search->SetOnQuery([this](const wxString &) { RefreshNow(); });
    m_search->SetOnRegexToggle([this](bool) { RefreshNow(); });
    m_search->SetName("notification_center_search");
    root->Add(m_search, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    auto *chips = new wxBoxSizer(wxHORIZONTAL);
    const wxString chip_labels[] = {
        // TRN: Level filter chips of the notification centre.
        _L("All levels"), _L("Info"), _L("Important"), _L("Warnings"), _L("Errors")};
    for (int i = 0; i < static_cast<int>(LevelChip::Count); ++i) {
        auto *chip = new Button(this, chip_labels[i]);
        chip->SetVariant(Button::Variant::Outlined);
        chip->SetButtonSize(Button::Size::Small);
        chip->SetName("notification_center_level_chip");
        chip->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) {
            m_level_chip = static_cast<LevelChip>(i);
            update_level_chips();
            RefreshNow();
        });
        m_level_buttons[i] = chip;
        chips->Add(chip, 0, wxRIGHT, FromDIP(6));
    }
    chips->AddStretchSpacer(1);
    // TRN: Toggle chip: include dismissed toasts in the notification centre list.
    m_dismissed_chip = new Button(this, _L("Show dismissed"));
    m_dismissed_chip->SetVariant(Button::Variant::Tonal);
    m_dismissed_chip->SetButtonSize(Button::Size::Small);
    m_dismissed_chip->SetGlyph(MaterialIcon::Visibility);
    m_dismissed_chip->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_show_dismissed = !m_show_dismissed;
        update_level_chips();
        RefreshNow();
    });
    chips->Add(m_dismissed_chip, 0);
    root->Add(chips, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    // --- Status line -----------------------------------------------------
    m_status_label = new Label(this, Label::Body_13, wxEmptyString);
    root->Add(m_status_label, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    // --- List ------------------------------------------------------------
    m_list = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxDV_MULTIPLE | wxDV_ROW_LINES | wxBORDER_NONE);
    // TRN: Accessible name of the notification history list.
    m_list->SetName(_L("Notification history"));
    // TRN: Column headers of the notification centre list.
    m_list->AppendTextColumn(_L("Level"), wxDATAVIEW_CELL_INERT, FromDIP(96), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Time"), wxDATAVIEW_CELL_INERT, FromDIP(132), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Title"), wxDATAVIEW_CELL_INERT, FromDIP(180), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Details"), wxDATAVIEW_CELL_INERT, FromDIP(220), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Status"), wxDATAVIEW_CELL_INERT, FromDIP(84), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Action taken"), wxDATAVIEW_CELL_INERT, FromDIP(120), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &NotificationCenterPanel::on_selection_changed, this);
    m_list->Bind(wxEVT_CHAR_HOOK, &NotificationCenterPanel::on_list_key, this);
    root->Add(m_list, 1, wxEXPAND);

    // TRN: Empty state of the notification centre.
    m_empty_label = new Label(this, Label::Body_13, _L("No notifications match. New toasts will appear here as they are shown."));
    m_empty_label->Hide();
    root->Add(m_empty_label, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(12));

    // --- Selection row ---------------------------------------------------
    auto *selection_row = new wxBoxSizer(wxHORIZONTAL);
    m_select_page_button = new Button(this, wxEmptyString);
    m_select_page_button->SetVariant(Button::Variant::Text);
    m_select_page_button->SetButtonSize(Button::Size::Small);
    m_select_page_button->SetGlyph(MaterialIcon::SelectAll);
    m_select_page_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_select_page, this);
    selection_row->Add(m_select_page_button, 0, wxRIGHT, FromDIP(4));

    m_select_all_button = new Button(this, wxEmptyString);
    m_select_all_button->SetVariant(Button::Variant::Text);
    m_select_all_button->SetButtonSize(Button::Size::Small);
    m_select_all_button->SetGlyph(MaterialIcon::DoneAll);
    m_select_all_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_select_all_matches, this);
    selection_row->Add(m_select_all_button, 0, wxRIGHT, FromDIP(4));

    // TRN: Invert the current selection in the notification centre.
    m_invert_button = new Button(this, _L("Invert selection"));
    m_invert_button->SetVariant(Button::Variant::Text);
    m_invert_button->SetButtonSize(Button::Size::Small);
    m_invert_button->SetGlyph(MaterialIcon::SwapHoriz);
    m_invert_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_invert_selection, this);
    selection_row->Add(m_invert_button, 0, wxRIGHT, FromDIP(4));

    // TRN: Clear the selection in the notification centre.
    m_clear_button = new Button(this, _L("Clear selection"));
    m_clear_button->SetVariant(Button::Variant::Text);
    m_clear_button->SetButtonSize(Button::Size::Small);
    m_clear_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_clear_selection, this);
    selection_row->Add(m_clear_button, 0, wxRIGHT, FromDIP(4));

    selection_row->AddStretchSpacer(1);
    m_load_more_button = new Button(this, wxEmptyString);
    m_load_more_button->SetVariant(Button::Variant::Text);
    m_load_more_button->SetButtonSize(Button::Size::Small);
    m_load_more_button->SetGlyph(MaterialIcon::ExpandMore);
    m_load_more_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_load_more, this);
    selection_row->Add(m_load_more_button, 0);
    root->Add(selection_row, 0, wxEXPAND | wxTOP, FromDIP(8));

    // --- Bulk action row -------------------------------------------------
    auto *bulk_row = new wxBoxSizer(wxHORIZONTAL);
    // TRN: Bulk action: close the selected toasts and mark them dismissed.
    m_dismiss_button = new Button(this, _L("Dismiss selected"));
    m_dismiss_button->SetVariant(Button::Variant::Tonal);
    m_dismiss_button->SetButtonSize(Button::Size::Small);
    m_dismiss_button->SetGlyph(MaterialIcon::Done);
    m_dismiss_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_dismiss_selected, this);
    bulk_row->Add(m_dismiss_button, 0, wxRIGHT, FromDIP(6));

    // TRN: Bulk action: export the selected (or all matching) notifications to a file.
    m_export_button = new Button(this, _L("Export…"));
    m_export_button->SetVariant(Button::Variant::Tonal);
    m_export_button->SetButtonSize(Button::Size::Small);
    m_export_button->SetGlyph(MaterialIcon::Download);
    m_export_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_export, this);
    bulk_row->Add(m_export_button, 0, wxRIGHT, FromDIP(6));

    bulk_row->AddStretchSpacer(1);
    // TRN: Bulk action: permanently delete the selected history entries (opens the confirmation gate).
    m_delete_button = new Button(this, _L("Delete selected…"));
    m_delete_button->SetVariant(Button::Variant::Danger);
    m_delete_button->SetButtonSize(Button::Size::Small);
    m_delete_button->SetGlyph(MaterialIcon::DeleteForever);
    m_delete_button->Bind(wxEVT_BUTTON, &NotificationCenterPanel::on_delete_requested, this);
    bulk_row->Add(m_delete_button, 0);
    root->Add(bulk_row, 0, wxEXPAND | wxTOP, FromDIP(8));

    // The bulk delete's review and its two-key gate live in the shared
    // Bulk::BulkActionPreviewDialog (opened from on_delete_requested), not in
    // an inline card, so this panel carries no armed control of its own.

    update_level_chips();
}

void NotificationCenterPanel::apply_theme()
{
    const wxColour surface   = StateColor::semantic(MD3::Role::Surface);
    const wxColour list      = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
    const wxColour text      = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour secondary = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const wxColour error_bg  = StateColor::semantic(MD3::Role::ErrorContainer);
    const wxColour error_fg  = StateColor::semantic(MD3::Role::OnErrorContainer);
    const wxColour outline   = StateColor::semantic(MD3::Role::OutlineVariant);

    SetBackgroundColour(surface);
    for (Label *label : {m_status_label, m_empty_label}) {
        label->SetBackgroundColour(surface);
        label->SetForegroundColour(secondary);
    }
    if (m_list != nullptr) {
        m_list->SetBackgroundColour(list);
        m_list->SetForegroundColour(text);
    }
    Refresh();
}

void NotificationCenterPanel::on_dpi_changed(const wxRect &suggested_rect)
{
    MD3Dialog::on_dpi_changed(suggested_rect);
    if (m_search != nullptr)
        m_search->Rescale();
    Layout();
}

void NotificationCenterPanel::on_sys_color_changed()
{
    apply_theme();
    update_level_chips();
}

// ---------------------------------------------------------------------------
// Filter + list

std::set<int> NotificationCenterPanel::chip_levels(LevelChip chip)
{
    // Values mirror NotificationManager::NotificationLevel.
    switch (chip) {
    case LevelChip::Info: return {2, 3, 4, 5};
    case LevelChip::Important: return {6};
    case LevelChip::Warning: return {7, 8};
    case LevelChip::Error: return {9};
    default: return {};
    }
}

wxString NotificationCenterPanel::level_label(int level)
{
    // TRN: Level chip text in the notification centre list.
    switch (level) {
    case 1: return _L("Progress");
    case 2: return _L("Hint");
    case 3: return _L("Info");
    case 4:
    case 5: return _L("Print info");
    case 6: return _L("Important");
    case 7: return _L("Warning");
    case 8: return _L("Serious warning");
    case 9: return _L("Error");
    default: return wxString::FromUTF8(NotificationHistory::level_display(level));
    }
}

wxString NotificationCenterPanel::format_time(std::int64_t timestamp_ms) const
{
    wxDateTime dt(static_cast<time_t>(timestamp_ms / 1000));
    return dt.FormatISODate() + " " + dt.FormatISOTime();
}

NotificationHistory::Filter NotificationCenterPanel::current_filter() const
{
    NotificationHistory::Filter filter;
    filter.levels            = chip_levels(m_level_chip);
    filter.include_dismissed = m_show_dismissed;
    if (m_search != nullptr) {
        filter.query = m_search->GetValue().ToUTF8().data();
        const bool regex      = m_search->IsRegexEnabled();
        const bool case_sense = m_search->IsCaseSensitive();
        const bool whole_word = m_search->IsWholeWord();
        const bool multiline  = m_search->IsMultiline();
        // One MatchPass per filter run shares the bounded-regex deadline across rows.
        auto pass = std::make_shared<SearchField::MatchPass>(m_search->GetValue(), regex, case_sense, whole_word, multiline);
        filter.matcher = [pass](const std::string &, const std::string &haystack) {
            return pass->matches(wxString::FromUTF8(haystack));
        };
    }
    return filter;
}

void NotificationCenterPanel::recompute_matches()
{
    if (m_manager == nullptr)
        return;
    const NotificationHistory &history = m_manager->history();
    m_matches = history.filtered_ids(current_filter());
    m_page.assign(m_matches.begin(), m_matches.begin() + std::min(m_matches.size(), m_page_limit));
    std::set<std::uint64_t> existing;
    for (const auto &entry : history.entries())
        existing.insert(entry.id);
    m_selection.retain(existing);
    m_seen_revision = history.revision();
}

void NotificationCenterPanel::populate_list()
{
    if (m_list == nullptr || m_manager == nullptr)
        return;
    const NotificationHistory &history = m_manager->history();
    m_syncing_selection = true;
    m_list->DeleteAllItems();
    for (std::uint64_t id : m_page) {
        const NotificationHistoryEntry *e = history.find(id);
        if (e == nullptr)
            continue;
        wxVector<wxVariant> row;
        row.push_back(wxVariant(level_label(e->level)));
        row.push_back(wxVariant(format_time(e->timestamp_ms)));
        row.push_back(wxVariant(wxString::FromUTF8(e->title)));
        row.push_back(wxVariant(wxString::FromUTF8(NotificationHistory::first_line(e->text) == e->text ? std::string() : e->text)));
        // TRN: Status cell values in the notification centre list.
        row.push_back(wxVariant(e->dismissed ? _L("Dismissed") : _L("Active")));
        row.push_back(wxVariant(wxString::FromUTF8(e->action)));
        m_list->AppendItem(row);
    }
    m_syncing_selection = false;
    sync_selection_to_view();

    const bool empty = m_page.empty();
    m_list->Show(!empty);
    m_empty_label->Show(empty);
    update_status();
    update_bulk_buttons();
    Layout();
}

void NotificationCenterPanel::RefreshNow()
{
    recompute_matches();
    populate_list();
}

void NotificationCenterPanel::on_timer(wxTimerEvent &)
{
    if (m_manager == nullptr || !IsShown())
        return;
    if (m_manager->history().revision() != m_seen_revision)
        RefreshNow();
}

// ---------------------------------------------------------------------------
// Selection

void NotificationCenterPanel::sync_selection_from_view()
{
    if (m_syncing_selection || m_list == nullptr)
        return;
    for (std::size_t row = 0; row < m_page.size(); ++row)
        m_selection.set(m_page[row], m_list->IsRowSelected(static_cast<unsigned>(row)));
    update_status();
    update_bulk_buttons();
}

void NotificationCenterPanel::sync_selection_to_view()
{
    if (m_list == nullptr)
        return;
    m_syncing_selection = true;
    m_list->UnselectAll();
    for (std::size_t row = 0; row < m_page.size(); ++row)
        if (m_selection.contains(m_page[row]))
            m_list->SelectRow(static_cast<unsigned>(row));
    m_syncing_selection = false;
    update_status();
    update_bulk_buttons();
}

void NotificationCenterPanel::on_selection_changed(wxDataViewEvent &event)
{
    sync_selection_from_view();
    event.Skip();
}

void NotificationCenterPanel::on_list_key(wxKeyEvent &event)
{
    // Ctrl+A selects the page (what is on screen); Ctrl+Shift+A every match.
    if (event.GetKeyCode() == 'A' && event.ControlDown()) {
        if (event.ShiftDown())
            m_selection.select_all_matches(m_matches);
        else
            m_selection.select_page(m_page);
        sync_selection_to_view();
        return;
    }
    // Ctrl+I inverts within the current matches; Delete opens the bulk delete
    // review for the selection (never deletes directly).
    if (event.GetKeyCode() == 'I' && event.ControlDown()) {
        m_selection.invert(m_matches);
        sync_selection_to_view();
        return;
    }
    if (event.GetKeyCode() == WXK_DELETE && !m_selection.empty()) {
        wxCommandEvent dummy;
        on_delete_requested(dummy);
        return;
    }
    event.Skip();
}

void NotificationCenterPanel::on_select_page(wxCommandEvent &)
{
    m_selection.select_page(m_page);
    sync_selection_to_view();
}

void NotificationCenterPanel::on_select_all_matches(wxCommandEvent &)
{
    m_selection.select_all_matches(m_matches);
    sync_selection_to_view();
}

void NotificationCenterPanel::on_invert_selection(wxCommandEvent &)
{
    m_selection.invert(m_matches);
    sync_selection_to_view();
}

void NotificationCenterPanel::on_clear_selection(wxCommandEvent &)
{
    m_selection.clear();
    sync_selection_to_view();
}

void NotificationCenterPanel::on_load_more(wxCommandEvent &)
{
    m_page_limit += PAGE_SIZE;
    RefreshNow();
}

// ---------------------------------------------------------------------------
// Status + buttons

void NotificationCenterPanel::update_status()
{
    if (m_status_label == nullptr || m_manager == nullptr)
        return;
    const std::size_t total    = m_manager->history().size();
    const std::size_t selected = m_selection.count_within(m_matches);
    wxString text;
    if (m_page.size() < m_matches.size()) {
        // TRN: %1$d rows shown, %2$d matches, %3$d recorded, %4$d selected.
        text = wxString::Format(_L("Showing %d of %d matches (%d recorded) · %d selected"),
                                static_cast<int>(m_page.size()), static_cast<int>(m_matches.size()),
                                static_cast<int>(total), static_cast<int>(selected));
    } else {
        // TRN: %1$d matches, %2$d recorded, %3$d selected.
        text = wxString::Format(_L("%d matches (%d recorded) · %d selected"), static_cast<int>(m_matches.size()),
                                static_cast<int>(total), static_cast<int>(selected));
    }
    m_status_label->SetLabel(text);

    // TRN: %d is the number of rows currently rendered.
    m_select_page_button->SetLabel(wxString::Format(_L("Select this page (%d)"), static_cast<int>(m_page.size())));
    // TRN: %d is the number of notifications matching the filter, rendered or not.
    m_select_all_button->SetLabel(wxString::Format(_L("Select all %d matches"), static_cast<int>(m_matches.size())));
    // The tooltips carry the shortcuts that actually fire in on_list_key().
    // TRN: Tooltip; Ctrl+A is the shortcut.
    m_select_page_button->SetToolTip(_L("Select the rows currently rendered (Ctrl+A)"));
    // TRN: Tooltip; Ctrl+Shift+A is the shortcut.
    m_select_all_button->SetToolTip(_L("Select every entry matching the filter, rendered or not (Ctrl+Shift+A)"));
    // TRN: Tooltip; Ctrl+I is the shortcut.
    m_invert_button->SetToolTip(_L("Invert the selection within the current matches (Ctrl+I)"));
    // TRN: %d is how many more matches the next page reveals.
    m_load_more_button->SetLabel(wxString::Format(_L("Show %d more"),
        static_cast<int>(std::min<std::size_t>(PAGE_SIZE, m_matches.size() - m_page.size()))));
    m_load_more_button->Show(m_page.size() < m_matches.size());
}

void NotificationCenterPanel::update_bulk_buttons()
{
    const std::size_t selected = m_selection.count_within(m_matches);
    const bool        any      = selected > 0;
    m_dismiss_button->Enable(any);
    m_delete_button->Enable(any);
    m_invert_button->Enable(!m_matches.empty());
    m_clear_button->Enable(!m_selection.empty());
    m_select_page_button->Enable(!m_page.empty());
    m_select_all_button->Enable(!m_matches.empty());
    m_export_button->Enable(!m_matches.empty());
    // TRN: Tooltip of the disabled bulk buttons naming the unmet condition.
    const wxString need_selection = _L("Select at least one notification first");
    m_dismiss_button->SetToolTip(any ? _L("Close the selected toasts and mark them dismissed") : need_selection);
    // TRN: Tooltip; Delete is the shortcut while the list has focus.
    m_delete_button->SetToolTip(any ? _L("Review and permanently remove the selected entries from the history (Delete)") : need_selection);
    m_export_button->SetToolTip(m_matches.empty() ? _L("Nothing matches the current filter")
                                                  : (any ? _L("Export the selected notifications")
                                                         : _L("Export every notification matching the current filter")));
}

void NotificationCenterPanel::update_level_chips()
{
    for (int i = 0; i < static_cast<int>(LevelChip::Count); ++i) {
        Button *chip = m_level_buttons[i];
        if (chip == nullptr)
            continue;
        const bool active = i == static_cast<int>(m_level_chip);
        chip->SetVariant(active ? Button::Variant::Tonal : Button::Variant::Outlined);
        chip->SetButtonSize(Button::Size::Small);
        chip->SetGlyph(active ? MaterialIcon::Check : 0);
    }
    if (m_dismissed_chip != nullptr) {
        m_dismissed_chip->SetVariant(m_show_dismissed ? Button::Variant::Tonal : Button::Variant::Outlined);
        m_dismissed_chip->SetButtonSize(Button::Size::Small);
        m_dismissed_chip->SetGlyph(m_show_dismissed ? MaterialIcon::Visibility : MaterialIcon::VisibilityOff);
        m_dismissed_chip->SetToolTip(m_show_dismissed ? _L("Dismissed toasts are listed; click to hide them")
                                                      : _L("Dismissed toasts are hidden; click to list them"));
    }
    Layout();
}

// ---------------------------------------------------------------------------
// Bulk actions

void NotificationCenterPanel::on_dismiss_selected(wxCommandEvent &)
{
    if (m_manager == nullptr)
        return;
    std::set<std::uint64_t> ids;
    for (std::uint64_t id : m_matches)
        if (m_selection.contains(id))
            ids.insert(id);
    if (ids.empty())
        return;
    m_manager->dismiss_history_entries(ids);
    m_selection.clear();
    RefreshNow();
}

void NotificationCenterPanel::on_export(wxCommandEvent &)
{
    if (m_manager == nullptr || m_matches.empty())
        return;
    std::vector<std::uint64_t> ids;
    for (std::uint64_t id : m_matches)
        if (m_selection.contains(id))
            ids.push_back(id);
    if (ids.empty())
        ids = m_matches;

    // Format is chosen through the file-type filter; the wildcard order maps
    // onto ExportFormat below.
    wxFileDialog dialog(this, _L("Export notifications"), wxEmptyString, "notification-history",
                        "JSON (*.json)|*.json|CSV (*.csv)|*.csv|Markdown (*.md)|*.md|Plain text (*.txt)|*.txt",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const NotificationHistory::ExportFormat formats[] = {
        NotificationHistory::ExportFormat::Json, NotificationHistory::ExportFormat::Csv,
        NotificationHistory::ExportFormat::Markdown, NotificationHistory::ExportFormat::PlainText};
    const int index = std::max(0, std::min(3, dialog.GetFilterIndex()));
    const NotificationHistory::ExportFormat format = formats[index];

    wxString path = dialog.GetPath();
    const wxString ext = wxString(".") + NotificationHistory::export_extension(format);
    if (!path.Lower().EndsWith(ext))
        path += ext;

    const std::string payload = m_manager->history().export_entries(ids, format, current_filter());
    boost::nowide::ofstream out(path.ToUTF8().data(), std::ios::binary | std::ios::trunc);
    const bool ok = static_cast<bool>(out) && static_cast<bool>(out << payload);
    if (ok) {
        // TRN: %1$d exported entries; %2$s file path.
        m_manager->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::RegularNotificationLevel,
            wxString::Format(_L("Exported %d notifications to %s"), static_cast<int>(ids.size()), path).ToUTF8().data());
    } else {
        // TRN: %s is the file path the export could not write.
        m_manager->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::ErrorNotificationLevel,
            wxString::Format(_L("Could not write the export to %s. Check the folder is writable and try again."), path).ToUTF8().data());
    }
}

void NotificationCenterPanel::on_delete_requested(wxCommandEvent &)
{
    if (m_manager == nullptr)
        return;
    const std::vector<std::uint64_t> ids = m_selection.ordered_within(m_matches);
    if (ids.empty())
        return;
    Bulk::BulkActionPlan plan;
    plan.action      = _u8L("Delete notification entries");
    plan.consequence = _u8L("The selected entries are removed from the history permanently. This cannot be undone; use Export first to keep a copy.");
    plan.destructive = true;
    const NotificationHistory &history = m_manager->history();
    for (std::uint64_t id : ids) {
        const NotificationHistoryEntry *entry = history.find(id);
        if (entry == nullptr) {
            plan.items.push_back(Bulk::BulkItem::skipped(std::to_string(id), _u8L("No longer in the history")));
            continue;
        }
        plan.items.push_back(Bulk::BulkItem::changed(entry->title, NotificationHistory::format_iso8601(entry->timestamp_ms)));
    }
    if (Bulk::BulkActionPreviewDialog::Run(this, plan))
        on_delete_confirmed();
    m_delete_button->SetFocus();
}

void NotificationCenterPanel::on_delete_confirmed()
{
    if (m_manager == nullptr)
        return;
    std::set<std::uint64_t> ids;
    for (std::uint64_t id : m_matches)
        if (m_selection.contains(id))
            ids.insert(id);
    // Deleting a live toast's record also closes the toast so the two never disagree.
    m_manager->dismiss_history_entries(ids);
    const std::size_t removed = m_manager->history().erase(ids);
    m_selection.clear();
    RefreshNow();
    m_delete_button->SetFocus();
    // TRN: %d is the number of history entries removed.
    m_manager->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::RegularNotificationLevel,
        wxString::Format(_L("Deleted %d notification entries from the history"), static_cast<int>(removed)).ToUTF8().data());
}

} } // namespace Slic3r::GUI
