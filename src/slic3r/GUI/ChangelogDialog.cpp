#include "ChangelogDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "NotificationManager.hpp"
#include "Plater.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/LinkLabel.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/PopupWindow.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"

#include "libslic3r/Utils.hpp"

#include <algorithm>
#include <fstream>

#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/intl.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

namespace Slic3r::GUI {

namespace {

constexpr int INITIAL_RELEASE_CARDS = 30;

Changelog::CivilDate today_civil()
{
    const wxDateTime today = wxDateTime::Today();
    return Changelog::CivilDate{today.GetYear(), static_cast<int>(today.GetMonth()) + 1, today.GetDay()};
}

wxString category_label(const std::string &category)
{
    if (category == "added")      return _L("Added");
    if (category == "fixed")      return _L("Fixed");
    if (category == "removed")    return _L("Removed");
    if (category == "documented") return _L("Documented");
    return _L("Changed");
}

// Category chips reuse the MD3 container roles so the badge reads as metadata,
// never as a status the user must act on.
std::pair<MD3::Role, MD3::Role> category_roles(const std::string &category)
{
    if (category == "added")   return {MD3::Role::PrimaryContainer, MD3::Role::OnPrimaryContainer};
    if (category == "fixed")   return {MD3::Role::SecondaryContainer, MD3::Role::OnSecondaryContainer};
    if (category == "removed") return {MD3::Role::ErrorContainer, MD3::Role::OnErrorContainer};
    return {MD3::Role::SurfaceContainerHighest, MD3::Role::OnSurfaceVariant};
}

wxString month_year_title(int year, int month)
{
    return wxString::Format("%s %d", wxDateTime::GetMonthName(static_cast<wxDateTime::Month>(month - 1)), year);
}

} // namespace

// ---------------------------------------------------------------------------
// ChangelogCalendarPopup — anchored month grid with range selection.
// ---------------------------------------------------------------------------

// A transient popover under the date fields: month/year jump (chevrons, a
// month choice and a year spinner), a 7x6 day grid painted from MD3 tokens,
// and range selection by two clicks (or two Enter presses). Every change is
// pushed to the dialog immediately so the typed fields and the list follow the
// grid live; Escape or a click outside dismisses it.
class ChangelogCalendarPopup final : public PopupWindow
{
public:
    ChangelogCalendarPopup(wxWindow *parent, const Changelog::DateRange &range,
                           std::function<void(const Changelog::DateRange &)> on_change)
        : PopupWindow(parent, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS)
        , m_range(range)
        , m_on_change(std::move(on_change))
    {
        const Changelog::CivilDate anchor = range.to ? *range.to : (range.from ? *range.from : today_civil());
        m_year   = anchor.year;
        m_month  = anchor.month;
        m_cursor = anchor;
        build();
    }

    void ShowUnder(wxWindow *anchor)
    {
        Layout();
        Fit();
        const wxPoint origin = anchor->GetScreenPosition();
        const wxSize  size   = GetSize();
        wxPoint       pos(origin.x, origin.y + anchor->GetSize().y + FromDIP(4));
        // Stay inside the display: flip above the anchor or slide left when the
        // default placement would clip.
        int display_index = wxDisplay::GetFromWindow(anchor);
        if (display_index == wxNOT_FOUND)
            display_index = 0;
        const wxRect area = wxDisplay(static_cast<unsigned int>(display_index)).GetClientArea();
        if (pos.y + size.y > area.GetBottom())
            pos.y = std::max(area.GetTop(), origin.y - size.y - FromDIP(4));
        if (pos.x + size.x > area.GetRight())
            pos.x = std::max(area.GetLeft(), area.GetRight() - size.x);
        Position(pos, wxSize(0, 0));
        Popup(m_grid);
        m_grid->SetFocus();
    }

    // Rebuilt on every open (see ChangelogDialog::open_calendar), so a
    // dismissed popover deletes itself instead of piling up under the dialog.
    void OnDismiss() override { Destroy(); }

private:
    static constexpr int COLUMNS = 7;
    static constexpr int ROWS    = 6;

    void build()
    {
        SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHigh));
        auto *root = new wxBoxSizer(wxVERTICAL);

        auto *header = new wxBoxSizer(wxHORIZONTAL);
        m_prev = new Button(this, wxEmptyString);
        m_prev->SetIconButton(Button::IconShape::Circle, FromDIP(36));
        m_prev->SetGlyph(MaterialIcon::ChevronLeft);
        m_prev->SetToolTip(_L("Previous month"));
        m_prev->SetName(_L("Previous month"));
        m_prev->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { shift_month(-1); });

        m_month_choice = new wxChoice(this, wxID_ANY);
        for (int m = 0; m < 12; ++m)
            m_month_choice->Append(wxDateTime::GetMonthName(static_cast<wxDateTime::Month>(m)));
        m_month_choice->SetName(_L("Month"));
        m_month_choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
            m_month = m_month_choice->GetSelection() + 1;
            clamp_cursor();
            refresh_header();
        });

        m_year_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 1970, 9999, m_year);
        m_year_spin->SetName(_L("Year"));
        m_year_spin->SetMinSize(FromDIP(wxSize(84, -1)));
        m_year_spin->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent &) {
            m_year = m_year_spin->GetValue();
            clamp_cursor();
            refresh_header();
        });

        m_next = new Button(this, wxEmptyString);
        m_next->SetIconButton(Button::IconShape::Circle, FromDIP(36));
        m_next->SetGlyph(MaterialIcon::ChevronRight);
        m_next->SetToolTip(_L("Next month"));
        m_next->SetName(_L("Next month"));
        m_next->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { shift_month(+1); });

        header->Add(m_prev, 0, wxALIGN_CENTER_VERTICAL);
        header->AddSpacer(FromDIP(4));
        header->Add(m_month_choice, 1, wxALIGN_CENTER_VERTICAL);
        header->AddSpacer(FromDIP(6));
        header->Add(m_year_spin, 0, wxALIGN_CENTER_VERTICAL);
        header->AddSpacer(FromDIP(4));
        header->Add(m_next, 0, wxALIGN_CENTER_VERTICAL);
        root->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

        m_grid = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                             FromDIP(wxSize(COLUMNS * 40, (ROWS + 1) * 36)), wxWANTS_CHARS | wxBORDER_NONE);
        m_grid->SetBackgroundStyle(wxBG_STYLE_PAINT);
        m_grid->Bind(wxEVT_PAINT, [this](wxPaintEvent &) { paint_grid(); });
        m_grid->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { on_grid_click(e.GetPosition()); });
        m_grid->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) { on_grid_key(e); });
        m_grid->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) { m_grid->Refresh(); e.Skip(); });
        m_grid->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { m_grid->Refresh(); e.Skip(); });
        root->Add(m_grid, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

        // TRN: Instruction under the calendar grid of the changelog date picker.
        m_hint = new Label(this, Label::Body_12, _L("Click a start day, then an end day. Arrow keys move, Enter picks, Esc closes."),
                           LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        m_hint->SetMinSize(wxSize(0, -1));
        m_hint->SetBackgroundColour(GetBackgroundColour());
        m_hint->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        root->Add(m_hint, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

        auto *footer = new wxBoxSizer(wxHORIZONTAL);
        auto *clear  = new Button(this, _L("Clear dates"));
        clear->SetVariant(Button::Variant::Text);
        clear->SetButtonSize(Button::Size::Small);
        clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            m_range = Changelog::DateRange::all();
            emit();
            m_grid->Refresh();
        });
        auto *done = new Button(this, _L("Done"));
        done->SetVariant(Button::Variant::Filled);
        done->SetButtonSize(Button::Size::Small);
        done->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Dismiss(); });
        footer->Add(clear, 0, wxALIGN_CENTER_VERTICAL);
        footer->AddStretchSpacer();
        footer->Add(done, 0, wxALIGN_CENTER_VERTICAL);
        root->Add(footer, 0, wxEXPAND | wxALL, FromDIP(12));

        SetSizer(root);
        refresh_header();
    }

    void shift_month(int delta)
    {
        m_month += delta;
        while (m_month < 1) { m_month += 12; --m_year; }
        while (m_month > 12) { m_month -= 12; ++m_year; }
        clamp_cursor();
        refresh_header();
    }

    void clamp_cursor()
    {
        m_cursor.year  = m_year;
        m_cursor.month = m_month;
        m_cursor.day   = std::min(m_cursor.day, Changelog::CivilDate::days_in_month(m_year, m_month));
        if (m_cursor.day < 1)
            m_cursor.day = 1;
    }

    void refresh_header()
    {
        m_month_choice->SetSelection(m_month - 1);
        if (m_year_spin->GetValue() != m_year)
            m_year_spin->SetValue(m_year);
        // TRN: Accessible name of the calendar grid; %s is "September 2026".
        m_grid->SetName(wxString::Format(_L("Calendar, %s"), month_year_title(m_year, m_month)));
        m_grid->Refresh();
    }

    // Column of the 1st of the shown month, Monday-first (0..6).
    int first_column() const
    {
        // 1970-01-01 was a Thursday; to_days() counts from that epoch.
        const long days = Changelog::CivilDate{m_year, m_month, 1}.to_days();
        return static_cast<int>(((days % 7) + 7 + 3) % 7);
    }

    std::optional<Changelog::CivilDate> cell_to_date(int column, int row) const
    {
        const int day = row * COLUMNS + column - first_column() + 1;
        if (day < 1 || day > Changelog::CivilDate::days_in_month(m_year, m_month))
            return std::nullopt;
        return Changelog::CivilDate{m_year, m_month, day};
    }

    void pick(const Changelog::CivilDate &date)
    {
        if (!m_range.from || m_range.to) {
            m_range.from = date;
            m_range.to.reset();
        } else {
            if (date < *m_range.from) {
                m_range.to   = m_range.from;
                m_range.from = date;
            } else {
                m_range.to = date;
            }
        }
        emit();
        m_grid->Refresh();
    }

    void emit()
    {
        if (m_on_change)
            m_on_change(m_range);
    }

    void on_grid_click(const wxPoint &pos)
    {
        const wxSize cell = cell_size();
        const int    row  = pos.y / cell.y - 1; // row 0 is the weekday header
        const int    col  = pos.x / cell.x;
        if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS)
            return;
        if (auto date = cell_to_date(col, row)) {
            m_cursor = *date;
            pick(*date);
        }
        m_grid->SetFocus();
    }

    void on_grid_key(wxKeyEvent &event)
    {
        switch (event.GetKeyCode()) {
        case WXK_LEFT:  move_cursor(-1); break;
        case WXK_RIGHT: move_cursor(+1); break;
        case WXK_UP:    move_cursor(-7); break;
        case WXK_DOWN:  move_cursor(+7); break;
        case WXK_PAGEUP:   shift_month(-1); break;
        case WXK_PAGEDOWN: shift_month(+1); break;
        case WXK_HOME:
            m_cursor = today_civil();
            m_year   = m_cursor.year;
            m_month  = m_cursor.month;
            refresh_header();
            break;
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
        case WXK_SPACE:
            pick(m_cursor);
            break;
        case WXK_ESCAPE:
            Dismiss();
            break;
        default:
            event.Skip();
            return;
        }
    }

    void move_cursor(long delta)
    {
        m_cursor = m_cursor.plus_days(delta);
        if (m_cursor.year != m_year || m_cursor.month != m_month) {
            m_year  = m_cursor.year;
            m_month = m_cursor.month;
            refresh_header();
        } else {
            m_grid->Refresh();
        }
    }

    wxSize cell_size() const
    {
        const wxSize size = m_grid->GetClientSize();
        return wxSize(std::max(1, size.x / COLUMNS), std::max(1, size.y / (ROWS + 1)));
    }

    void paint_grid()
    {
        wxAutoBufferedPaintDC dc(m_grid);
        const wxColour surface   = GetBackgroundColour();
        const wxColour on        = StateColor::semantic(MD3::Role::OnSurface);
        const wxColour on_var    = StateColor::semantic(MD3::Role::OnSurfaceVariant);
        const wxColour primary   = StateColor::semantic(MD3::Role::Primary);
        const wxColour on_prim   = StateColor::semantic(MD3::Role::OnPrimary);
        const wxColour range_bg  = StateColor::semantic(MD3::Role::PrimaryContainer);
        const wxColour range_fg  = StateColor::semantic(MD3::Role::OnPrimaryContainer);
        const wxColour outline   = StateColor::semantic(MD3::Role::Outline);
        dc.SetBackground(wxBrush(surface));
        dc.Clear();

        const wxSize cell = cell_size();
        const Changelog::CivilDate today = today_civil();
        const bool focused = m_grid->HasFocus();

        // Weekday initials, Monday first.
        dc.SetFont(Label::Body_11);
        dc.SetTextForeground(on_var);
        for (int c = 0; c < COLUMNS; ++c) {
            const wxDateTime::WeekDay wd = static_cast<wxDateTime::WeekDay>((c + 1) % 7);
            const wxString name = wxDateTime::GetWeekDayName(wd, wxDateTime::Name_Abbr);
            const wxSize   ext  = dc.GetTextExtent(name);
            dc.DrawText(name, c * cell.x + (cell.x - ext.x) / 2, (cell.y - ext.y) / 2);
        }

        dc.SetFont(Label::Body_13);
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLUMNS; ++c) {
                const auto date = cell_to_date(c, r);
                if (!date)
                    continue;
                const wxRect rect(c * cell.x, (r + 1) * cell.y, cell.x, cell.y);
                wxRect pill = rect;
                pill.Deflate(FromDIP(3));
                const bool is_from = m_range.from && *date == *m_range.from;
                const bool is_to   = m_range.to && *date == *m_range.to;
                const bool in_range = m_range.from && m_range.to && m_range.contains(*date);

                dc.SetPen(*wxTRANSPARENT_PEN);
                if (in_range) {
                    dc.SetBrush(wxBrush(range_bg));
                    dc.DrawRectangle(rect.x, pill.y, rect.width, pill.height);
                }
                wxColour fg = in_range ? range_fg : on;
                if (is_from || is_to) {
                    dc.SetBrush(wxBrush(primary));
                    dc.DrawEllipse(pill);
                    fg = on_prim;
                } else if (*date == today) {
                    dc.SetPen(wxPen(primary, FromDIP(1)));
                    dc.SetBrush(*wxTRANSPARENT_BRUSH);
                    dc.DrawEllipse(pill);
                }
                if (focused && *date == m_cursor) {
                    dc.SetPen(wxPen(outline, FromDIP(2)));
                    dc.SetBrush(*wxTRANSPARENT_BRUSH);
                    wxRect focus_ring = rect;
                    focus_ring.Deflate(FromDIP(1));
                    dc.DrawRoundedRectangle(focus_ring, FromDIP(6));
                }
                const wxString text = wxString::Format("%d", date->day);
                const wxSize   ext  = dc.GetTextExtent(text);
                dc.SetTextForeground(fg);
                dc.DrawText(text, rect.x + (rect.width - ext.x) / 2, rect.y + (rect.height - ext.y) / 2);
            }
        }
    }

    Changelog::DateRange m_range;
    std::function<void(const Changelog::DateRange &)> m_on_change;
    int                  m_year  = 2026;
    int                  m_month = 1;
    Changelog::CivilDate m_cursor;

    Button    *m_prev         = nullptr;
    Button    *m_next         = nullptr;
    wxChoice  *m_month_choice = nullptr;
    wxSpinCtrl *m_year_spin   = nullptr;
    wxPanel   *m_grid         = nullptr;
    Label     *m_hint         = nullptr;
};

// ---------------------------------------------------------------------------
// ChangelogDialog
// ---------------------------------------------------------------------------

std::string ChangelogDialog::data_path()
{
    return resources_dir() + "/changelog/changelog.json";
}

ChangelogDialog::ChangelogDialog(wxWindow *parent)
    : MD3Dialog(parent, _L("What's new"),
                // TRN: Subtitle of the in-app changelog viewer.
                _L("Every released version, with the commits behind each change."),
                MaterialIcon::History, MD3Dialog::Options{/*resizable*/ true, /*forced_dark*/ false})
{
    load_data();
    build_ui();
    apply_theme();
    set_range(Changelog::DateRange::all(), true);

    SetMinSize(FromDIP(wxSize(720, 500)));
    SetSize(FromDIP(wxSize(880, 640)));
    Layout();
    CentreOnParent();
}

ChangelogDialog::~ChangelogDialog() = default;

void ChangelogDialog::load_data()
{
    try {
        m_document = Changelog::load_document(data_path());
        m_load_error.clear();
    } catch (const std::exception &e) {
        m_document.reset();
        m_load_error = e.what();
    }
}

void ChangelogDialog::build_ui()
{
    wxBoxSizer *content = GetContentSizer();
    const wxColour bg   = GetBackgroundColour();

    // --- Search -------------------------------------------------------------
    // TRN: Placeholder of the changelog search field.
    m_search = new SearchField(this, _L("Search changes"));
    m_search->SetOnQuery([this](const wxString &) { apply_filters(); });
    m_search->SetOnRegexToggle([this](bool) { apply_filters(); });
    content->Add(m_search, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    // --- Date range ---------------------------------------------------------
    auto *dates = new wxBoxSizer(wxHORIZONTAL);
    auto make_date_field = [&](const wxString &name, Bound bound) {
        auto *field = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(132, 40)),
                                     wxTE_PROCESS_ENTER | wxBORDER_SIMPLE);
        field->SetHint(locale_date_hint());
        field->SetName(name);
        field->SetToolTip(wxString::Format("%s. %s", name, _L("Type a date as YYYY-MM-DD or in your locale's short format.")));
        field->Bind(wxEVT_TEXT, [this, bound](wxCommandEvent &) { on_date_typed(bound); });
        field->Bind(wxEVT_TEXT_ENTER, [this, bound](wxCommandEvent &) { on_date_typed(bound); });
        return field;
    };
    auto *from_label = new Label(this, Label::Body_13, _L("From"));
    from_label->SetBackgroundColour(bg);
    m_from_field = make_date_field(_L("From date"), Bound::From);
    auto *to_label = new Label(this, Label::Body_13, _L("To"));
    to_label->SetBackgroundColour(bg);
    m_to_field = make_date_field(_L("To date"), Bound::To);

    m_calendar_button = new Button(this, wxEmptyString);
    m_calendar_button->SetIconButton(Button::IconShape::Circle, FromDIP(40));
    m_calendar_button->SetGlyph(MaterialIcon::Schedule);
    m_calendar_button->SetToolTip(_L("Pick dates on a calendar"));
    m_calendar_button->SetName(_L("Pick dates on a calendar"));
    m_calendar_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { open_calendar(); });

    auto make_preset = [&](const wxString &text, std::function<Changelog::DateRange()> make) {
        auto *button = new Button(this, text);
        button->SetVariant(Button::Variant::Tonal);
        button->SetButtonSize(Button::Size::Small);
        button->Bind(wxEVT_BUTTON, [this, make](wxCommandEvent &) { set_range(make(), true); });
        return button;
    };
    m_preset_30   = make_preset(_L("Last 30 days"), [] { return Changelog::DateRange::last_days(today_civil(), 30); });
    m_preset_year = make_preset(_L("This year"),    [] { return Changelog::DateRange::this_year(today_civil()); });
    m_preset_all  = make_preset(_L("All versions"), [] { return Changelog::DateRange::all(); });

    dates->Add(from_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    dates->Add(m_from_field, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    dates->Add(to_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    dates->Add(m_to_field, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    dates->Add(m_calendar_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    dates->AddStretchSpacer();
    dates->Add(m_preset_30, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    dates->Add(m_preset_year, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    dates->Add(m_preset_all, 0, wxALIGN_CENTER_VERTICAL);
    content->Add(dates, 0, wxEXPAND);

    // Inline validation: what was typed stays in the field; this line says why
    // it is not being applied yet. Hidden while both fields are valid.
    m_date_error = new Label(this, Label::Body_12, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_date_error->SetMinSize(wxSize(0, -1));
    m_date_error->SetBackgroundColour(bg);
    m_date_error->Hide();
    content->Add(m_date_error, 0, wxEXPAND | wxTOP, FromDIP(6));

    m_status = new Label(this, Label::Body_12, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_status->SetMinSize(wxSize(0, -1));
    m_status->SetBackgroundColour(bg);
    content->Add(m_status, 0, wxEXPAND | wxTOP, FromDIP(8));

    // --- Release list -------------------------------------------------------
    m_scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxVSCROLL | wxTAB_TRAVERSAL | wxBORDER_NONE);
    m_scroll->SetScrollRate(0, FromDIP(16));
    m_scroll->SetName(_L("Released versions"));
    m_scroll->SetSizer(new wxBoxSizer(wxVERTICAL));
    content->Add(m_scroll, 1, wxEXPAND | wxTOP, FromDIP(8));

    // --- Footer -------------------------------------------------------------
    m_copy_button = new Button(this, _L("Copy"));
    m_copy_button->SetVariant(Button::Variant::Outlined);
    m_copy_button->SetToolTip(_L("Copy the shown changes to the clipboard as Markdown"));
    m_copy_button->Bind(wxEVT_BUTTON, &ChangelogDialog::on_copy, this);
    m_export_button = new Button(this, _L("Export") + dots);
    m_export_button->SetVariant(Button::Variant::Outlined);
    m_export_button->SetToolTip(_L("Save the shown changes as Markdown or plain text"));
    m_export_button->Bind(wxEVT_BUTTON, &ChangelogDialog::on_export, this);
    m_close_button = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
    m_close_button->SetVariant(Button::Variant::Filled);
    m_close_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    AddFooterButton(m_copy_button);
    AddFooterButton(m_export_button);
    AddFooterButton(m_close_button);
    SetEscapeId(wxID_CANCEL);

    const bool has_data = m_document.has_value();
    m_copy_button->Enable(has_data);
    m_export_button->Enable(has_data);
    if (!has_data) {
        m_copy_button->SetToolTip(_L("Nothing to copy: the changelog data could not be read."));
        m_export_button->SetToolTip(_L("Nothing to export: the changelog data could not be read."));
    }
}

void ChangelogDialog::apply_theme()
{
    const wxColour bg     = GetBackgroundColour();
    const wxColour on_var = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    m_scroll->SetBackgroundColour(bg);
    m_status->SetForegroundColour(on_var);
    m_date_error->SetForegroundColour(StateColor::semantic(MD3::Role::Error));
    for (wxTextCtrl *field : {m_from_field, m_to_field}) {
        field->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHighest));
        field->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    }
}

void ChangelogDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    MD3Dialog::on_dpi_changed(suggested_rect);
    rebuild_list();
    Layout();
}

// --- Filtering ---------------------------------------------------------------

Changelog::TextMatcher ChangelogDialog::current_matcher() const
{
    const wxString query = m_search->GetValue();
    if (query.IsEmpty())
        return nullptr;
    // One MatchPass per filter run: regex evaluation shares a single deadline
    // across every candidate instead of paying it per entry.
    auto pass = std::make_shared<SearchField::MatchPass>(query, m_search->IsRegexEnabled(),
                                                         m_search->IsCaseSensitive(), m_search->IsWholeWord(),
                                                         m_search->IsMultiline());
    return [pass](const std::string &text) { return pass->matches(wxString::FromUTF8(text)); };
}

std::string ChangelogDialog::search_description() const
{
    const wxString query = m_search->GetValue();
    if (query.IsEmpty())
        return {};
    wxString description = "\"" + query + "\"";
    wxArrayString flags;
    if (m_search->IsRegexEnabled())  flags.Add("regex");
    if (m_search->IsCaseSensitive()) flags.Add("case-sensitive");
    if (m_search->IsWholeWord())     flags.Add("whole word");
    if (!flags.empty())
        description += " (" + wxJoin(flags, ',') + ")";
    return description.ToUTF8().data();
}

void ChangelogDialog::apply_filters()
{
    m_filtered.clear();
    if (m_document)
        m_filtered = Changelog::filter_releases(*m_document, m_range, current_matcher());
    m_show_all = false;
    rebuild_list();
    update_status();
}

void ChangelogDialog::update_status()
{
    if (!m_document) {
        m_status->SetLabel(wxEmptyString);
        return;
    }
    const std::size_t versions = m_filtered.size();
    const std::size_t changes  = Changelog::count_entries(m_filtered);
    wxString text = wxString::Format(
        // TRN: %1$zu versions and %2$zu change lines are currently listed in the changelog viewer.
        _L("%zu versions and %zu changes shown"), versions, changes);
    if (m_document->releases.size() != versions)
        text += wxString::Format(" (%s)", wxString::Format(_L("%zu versions in total"), m_document->releases.size()));
    m_status->SetLabel(text);
    Layout();
}

void ChangelogDialog::rebuild_list()
{
    m_scroll->Freeze();
    wxSizer *sizer = m_scroll->GetSizer();
    sizer->Clear(/*delete_windows*/ true);
    m_show_all_button = nullptr;

    const wxColour bg     = GetBackgroundColour();
    const wxColour on     = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    auto add_message = [&](const wxString &title, const wxString &body) {
        auto *head = new Label(m_scroll, Label::Head_14, title, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        head->SetMinSize(wxSize(0, -1));
        head->SetBackgroundColour(bg);
        head->SetForegroundColour(on);
        sizer->Add(head, 0, wxEXPAND | wxTOP, FromDIP(24));
        auto *text = new Label(m_scroll, Label::Body_13, body, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        text->SetMinSize(wxSize(0, -1));
        text->SetBackgroundColour(bg);
        text->SetForegroundColour(on_var);
        sizer->Add(text, 0, wxEXPAND | wxTOP, FromDIP(8));
    };

    if (!m_document) {
        add_message(_L("The changelog could not be read"),
                    wxString::Format(_L("%s\n\nThe file %s ships with the application; reinstalling restores it. "
                                        "The release history is also published at %s."),
                                     wxString::FromUTF8(m_load_error), wxString::FromUTF8(data_path()),
                                     "https://github.com/Ding-Ding-Projects/BambuStudio/releases"));
    } else if (m_document->releases.empty()) {
        add_message(_L("No versions recorded"),
                    _L("The bundled changelog lists no releases. Refresh it with: node scripts/changelog/export-app-changelog.mjs"));
    } else if (m_filtered.empty()) {
        const bool has_query = !m_search->GetValue().IsEmpty();
        wxString why;
        if (has_query && !m_range.is_unbounded())
            why = _L("No change matches this search inside the selected dates. Widen the dates or clear the search.");
        else if (has_query)
            why = _L("No change matches this search. Plain text is matched by default; the .* toggle switches to regex.");
        else
            why = _L("No version was published in the selected dates.");
        add_message(_L("Nothing to show"), why);
    } else {
        std::size_t shown = 0;
        for (const Changelog::FilteredRelease &filtered : m_filtered) {
            if (!m_show_all && shown >= INITIAL_RELEASE_CARDS)
                break;
            add_release_card(filtered, sizer);
            ++shown;
        }
        if (shown < m_filtered.size()) {
            m_show_all_button = new Button(m_scroll,
                // TRN: %zu is the total number of versions matching the current filters.
                wxString::Format(_L("Show all %zu versions"), m_filtered.size()));
            m_show_all_button->SetVariant(Button::Variant::Tonal);
            m_show_all_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
                m_show_all = true;
                rebuild_list();
            });
            sizer->Add(m_show_all_button, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, FromDIP(16));
        }
    }

    m_scroll->FitInside();
    m_scroll->Scroll(0, 0);
    m_scroll->Thaw();
    Layout();
}

void ChangelogDialog::add_release_card(const Changelog::FilteredRelease &filtered, wxSizer *into)
{
    const Changelog::Release &release = *filtered.release;
    const wxColour card_bg = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour on      = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var  = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    auto *card = new StaticBox(m_scroll);
    card->SetBackgroundColorNormal(card_bg);
    card->SetBorderColorNormal(StateColor::semantic(MD3::Role::OutlineVariant));
    card->SetBorderWidth(1);
    card->SetCornerRadius(FromDIP(12));
    auto *body = new wxBoxSizer(wxVERTICAL);

    wxString heading = wxString::FromUTF8(release.version);
    if (!release.code_name_en.empty()) {
        heading += " — " + wxString::FromUTF8(release.code_name_en);
        if (!release.code_name_yue.empty())
            heading += " " + wxString::FromUTF8(release.code_name_yue);
    }
    auto *title = new Label(card, Label::Head_16, heading, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    title->SetMinSize(wxSize(0, -1));
    title->SetBackgroundColour(card_bg);
    title->SetForegroundColour(on);
    body->Add(title, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    auto *meta_row = new wxBoxSizer(wxHORIZONTAL);
    wxString meta = wxString::FromUTF8(release.date.to_iso()) + " · " + wxString::FromUTF8(release.tag);
    if (release.prerelease)
        meta += " · " + _L("pre-release");
    auto *meta_label = new Label(card, Label::Body_12, meta);
    meta_label->SetBackgroundColour(card_bg);
    meta_label->SetForegroundColour(on_var);
    meta_row->Add(meta_label, 0, wxALIGN_CENTER_VERTICAL);
    if (!release.url.empty()) {
        meta_row->AddSpacer(FromDIP(12));
        auto *link = new LinkLabel(card, _L("Release page"), release.url);
        link->SetName(wxString::Format(_L("Open the release page of %s on GitHub"), wxString::FromUTF8(release.version)));
        link->SeLinkLabelBColour(card_bg);
        meta_row->Add(link, 0, wxALIGN_CENTER_VERTICAL);
    }
    body->Add(meta_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    if (filtered.entries.empty()) {
        wxString note;
        if (release.baseline)
            note = _L("Oldest release: there is no earlier release to compare against.");
        else if (release.same_commit)
            note = _L("Tagged the same commit as the previous release, so it carries no new changes.");
        else
            note = _L("No commits were recorded between this release and the previous one.");
        auto *note_label = new Label(card, Label::Body_13, note, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        note_label->SetMinSize(wxSize(0, -1));
        note_label->SetBackgroundColour(card_bg);
        note_label->SetForegroundColour(on_var);
        body->Add(note_label, 0, wxEXPAND | wxALL, FromDIP(16));
    } else {
        body->AddSpacer(FromDIP(8));
        for (const Changelog::Entry *entry : filtered.entries) {
            auto *row = new wxBoxSizer(wxHORIZONTAL);
            const auto roles = category_roles(entry->category);
            auto *chip = new Label(card, Label::Body_11, category_label(entry->category), wxALIGN_CENTER_HORIZONTAL);
            chip->SetMinSize(FromDIP(wxSize(88, -1)));
            chip->SetBackgroundColour(StateColor::semantic(roles.first));
            chip->SetForegroundColour(StateColor::semantic(roles.second));
            row->Add(chip, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));

            auto *text = new Label(card, Label::Body_13, wxString::FromUTF8(entry->text), LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
            text->SetMinSize(wxSize(0, -1));
            text->SetBackgroundColour(card_bg);
            text->SetForegroundColour(on);
            row->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));

            auto *link = new LinkLabel(card, wxString::FromUTF8(entry->short_sha),
                                       Changelog::commit_url(*m_document, entry->sha));
            link->getLabel()->SetFont(Label::Mono_12);
            link->SeLinkLabelBColour(card_bg);
            // TRN: Accessible name of a commit link; %s is the short commit id.
            link->SetName(wxString::Format(_L("Open commit %s on GitHub"), wxString::FromUTF8(entry->short_sha)));
            link->SetToolTip(wxString::FromUTF8(entry->sha));
            row->Add(link, 0, wxALIGN_CENTER_VERTICAL);

            body->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(16));
        }
    }

    card->SetSizer(body);
    into->Add(card, 0, wxEXPAND | wxBOTTOM, FromDIP(12));
}

// --- Dates ---------------------------------------------------------------------

Changelog::DateOrder ChangelogDialog::locale_date_order() const
{
    const wxString fmt = wxLocale::GetInfo(wxLOCALE_SHORT_DATE_FMT, wxLOCALE_CAT_DATE).Lower();
    const int d = fmt.Find("%d");
    int       m = fmt.Find("%m");
    int       y = fmt.Find("%y");
    if (y == wxNOT_FOUND)
        y = fmt.Find("%Y");
    if (d == wxNOT_FOUND || m == wxNOT_FOUND || y == wxNOT_FOUND)
        return Changelog::DateOrder::YearMonthDay;
    if (y < d && y < m)
        return Changelog::DateOrder::YearMonthDay;
    return d < m ? Changelog::DateOrder::DayMonthYear : Changelog::DateOrder::MonthDayYear;
}

wxString ChangelogDialog::locale_date_hint() const
{
    switch (locale_date_order()) {
    case Changelog::DateOrder::DayMonthYear: return "YYYY-MM-DD / DD/MM/YYYY";
    case Changelog::DateOrder::MonthDayYear: return "YYYY-MM-DD / MM/DD/YYYY";
    default: return "YYYY-MM-DD";
    }
}

void ChangelogDialog::on_date_typed(Bound bound)
{
    wxTextCtrl *field = bound == Bound::From ? m_from_field : m_to_field;
    std::optional<Changelog::CivilDate> &target = bound == Bound::From ? m_range.from : m_range.to;
    bool &invalid = bound == Bound::From ? m_from_invalid : m_to_invalid;

    const wxString text = field->GetValue();
    if (text.Strip(wxString::both).IsEmpty()) {
        target.reset();
        invalid = false;
    } else if (auto date = Changelog::parse_typed_date(text.ToUTF8().data(), locale_date_order())) {
        target  = *date;
        invalid = false;
    } else {
        // Keep what was typed; the previous bound stays applied until the text
        // becomes a date again.
        invalid = true;
    }
    update_date_error();
    if (!invalid)
        apply_filters();
}

void ChangelogDialog::set_range(const Changelog::DateRange &range, bool write_fields)
{
    m_range        = range;
    m_from_invalid = false;
    m_to_invalid   = false;
    if (write_fields) {
        // ChangeValue() does not emit wxEVT_TEXT, so the fields do not re-enter on_date_typed().
        m_from_field->ChangeValue(range.from ? wxString::FromUTF8(range.from->to_iso()) : wxString());
        m_to_field->ChangeValue(range.to ? wxString::FromUTF8(range.to->to_iso()) : wxString());
    }
    update_date_error();
    apply_filters();
}

void ChangelogDialog::update_date_error()
{
    wxString message;
    if (m_from_invalid && m_to_invalid)
        message = _L("Neither date is complete yet.");
    else if (m_from_invalid)
        message = _L("The From date is not complete yet.");
    else if (m_to_invalid)
        message = _L("The To date is not complete yet.");
    else if (m_range.from && m_range.to && *m_range.to < *m_range.from)
        message = _L("The To date is before the From date, so no version can match.");
    if (!message.IsEmpty())
        message += " " + wxString::Format(_L("Expected %s with a four-digit year."), locale_date_hint());
    m_date_error->SetLabel(message);
    m_date_error->Show(!message.IsEmpty());
    Layout();
}

void ChangelogDialog::open_calendar()
{
    // Rebuilt per open so it re-derives theme, DPI and the current range.
    m_calendar = new ChangelogCalendarPopup(this, m_range, [this](const Changelog::DateRange &range) {
        set_range(range, true);
    });
    m_calendar->Bind(wxEVT_DESTROY, [this](wxWindowDestroyEvent &e) {
        if (e.GetWindow() == m_calendar)
            m_calendar = nullptr;
        e.Skip();
    });
    m_calendar->ShowUnder(m_calendar_button);
}

// --- Copy / export ---------------------------------------------------------------

std::string ChangelogDialog::export_text(Changelog::ExportFormat format) const
{
    if (!m_document)
        return {};
    return Changelog::export_text(*m_document, m_filtered, m_range, search_description(), format);
}

void ChangelogDialog::show_toast(const wxString &text, bool error)
{
    Plater *plater = wxGetApp().plater();
    if (plater == nullptr || plater->get_notification_manager() == nullptr)
        return;
    plater->get_notification_manager()->push_notification(
        NotificationType::CustomNotification,
        error ? NotificationManager::NotificationLevel::ErrorNotificationLevel
              : NotificationManager::NotificationLevel::RegularNotificationLevel,
        text.ToUTF8().data());
}

void ChangelogDialog::on_copy(wxCommandEvent &)
{
    const std::string text = export_text(Changelog::ExportFormat::Markdown);
    if (text.empty())
        return;
    bool copied = false;
    if (wxTheClipboard->Open()) {
        copied = wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(text)));
        wxTheClipboard->Close();
    }
    if (copied)
        show_toast(wxString::Format(_L("Copied %zu versions and %zu changes (%s) as Markdown."),
                                    m_filtered.size(), Changelog::count_entries(m_filtered),
                                    wxString::FromUTF8(m_range.describe())));
    else
        show_toast(_L("The clipboard is in use by another application; nothing was copied."), true);
}

void ChangelogDialog::on_export(wxCommandEvent &)
{
    if (!m_document)
        return;
    wxString default_name = "bambu-studio-changelog";
    if (m_range.from) default_name += "-" + wxString::FromUTF8(m_range.from->to_iso());
    if (m_range.to)   default_name += "-" + wxString::FromUTF8(m_range.to->to_iso());
    wxFileDialog dialog(this, _L("Export changelog"), wxEmptyString, default_name + ".md",
                        _L("Markdown") + " (*.md)|*.md|" + _L("Plain text") + " (*.txt)|*.txt",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;
    const bool  markdown = dialog.GetFilterIndex() == 0;
    wxString    path     = dialog.GetPath();
    const std::string text = export_text(markdown ? Changelog::ExportFormat::Markdown : Changelog::ExportFormat::PlainText);

    std::ofstream out(path.ToStdWstring(), std::ios::binary | std::ios::trunc);
    out << text;
    if (!out) {
        show_toast(wxString::Format(_L("The changelog could not be written to %s."), path), true);
        return;
    }
    show_toast(wxString::Format(_L("Exported %zu versions and %zu changes (%s) to %s."),
                                m_filtered.size(), Changelog::count_entries(m_filtered),
                                wxString::FromUTF8(m_range.describe()), path));
}

} // namespace Slic3r::GUI
