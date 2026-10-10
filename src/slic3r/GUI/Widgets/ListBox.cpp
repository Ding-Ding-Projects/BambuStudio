#include "ListBox.hpp"

#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include <wx/control.h>
#include <wx/dcclient.h>
#include <wx/tooltip.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#include <wx/variant.h>
#endif
#include <algorithm>
#include <cmath>

namespace Slic3r { namespace GUI {

namespace {
constexpr int kInsetX  = 4; // row pane inset from the field edge, DIP
constexpr int kInsetY  = 2;
constexpr int kPadX    = 12; // text inset inside the pane, DIP
constexpr int kCheck   = 20; // check box glyph, DIP
constexpr int kCheckGap = 8; // between the glyph and the text, DIP

// Pixel inputs have already been scaled by the owning window exactly once.
struct AtlasListGeometry {
    int pane_x, pane_y, pane_width, pane_height;
    int text_x, text_width;
};
AtlasListGeometry atlasListGeometry(int width, int height, int inset_x, int inset_y, int padding, int check_width)
{
    width = std::max(0, width);
    height = std::max(0, height);
    const int x = std::min(std::max(0, inset_x), width / 2);
    const int y = std::min(std::max(0, inset_y), height / 2);
    const int pane_width = width - 2 * x;
    const int pad = std::min(std::max(0, padding), pane_width / 2);
    const int content_width = pane_width - 2 * pad;
    const int check = std::min(std::max(0, check_width), content_width);
    return {x, y, pane_width, height - 2 * y, x + pad + check, content_width - check};
}
int atlasListRowHeight(int minimum, int text_height, int glyph_height, int inset, int padding)
{
    return std::max(minimum, std::max(text_height, glyph_height) + 2 * (inset + padding));
}
wxColour atlasListBlend(const wxColour& from, const wxColour& to, double amount)
{
    const auto channel = [amount](int a, int b) { return static_cast<unsigned char>(std::lround(a + (b - a) * amount)); };
    return wxColour(channel(from.Red(), to.Red()), channel(from.Green(), to.Green()), channel(from.Blue(), to.Blue()));
}
} // namespace

#if wxUSE_ACCESSIBILITY
// The owner-drawn list is one generic window to Windows, so without this peer a
// screen reader found an unnamed client area with no rows. It answers the way
// the stock list box did: a list named by GetName(), whose children are the
// rows, each named by its text and carrying its selected, focused and (in a
// check list) checked states. Child ids are rows plus one; wxACC_SELF is the list.
class ListBox::Accessible final : public wxWindowAccessible
{
public:
    explicit Accessible(ListBox *list) : wxWindowAccessible(list), m_list(list) {}

    wxAccStatus GetChildCount(int *count) override
    {
        if (!count) return wxACC_FAIL;
        *count = int(m_list->GetCount());
        return wxACC_OK;
    }

    wxAccStatus GetChild(int child_id, wxAccessible **child) override
    {
        if (!child) return wxACC_FAIL;
        if (child_id == wxACC_SELF) {
            *child = this;
            return wxACC_OK;
        }
        if (row(child_id) < 0) return wxACC_INVALID_ARG;
        *child = nullptr; // a row is a simple element of the list
        return wxACC_OK;
    }

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (!name) return wxACC_FAIL;
        if (child_id == wxACC_SELF) {
            *name = m_list->GetName();
            if (*name == wxASCII_STR(wxVListBoxNameStr)) name->clear();
            return name->IsEmpty() ? wxACC_NOT_IMPLEMENTED : wxACC_OK;
        }
        const int i = row(child_id);
        if (i < 0) return wxACC_INVALID_ARG;
        *name = m_list->GetString(unsigned(i));
        return wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (!role) return wxACC_FAIL;
        if (child_id != wxACC_SELF && row(child_id) < 0) return wxACC_INVALID_ARG;
        *role = child_id == wxACC_SELF ? wxROLE_SYSTEM_LIST : wxROLE_SYSTEM_LISTITEM;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (!state) return wxACC_FAIL;
        *state = 0;
        const bool enabled = m_list->IsEnabled();
        const bool shown = m_list->IsShownOnScreen();
        if (child_id == wxACC_SELF) {
            if (enabled && shown) *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
            if (m_list->HasFocus()) *state |= wxACC_STATE_SYSTEM_FOCUSED;
            if (m_list->HasMultipleSelection()) *state |= wxACC_STATE_SYSTEM_MULTISELECTABLE | wxACC_STATE_SYSTEM_EXTSELECTABLE;
            if (!enabled) *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
            if (!shown) *state |= wxACC_STATE_SYSTEM_INVISIBLE;
            return wxACC_OK;
        }
        const int i = row(child_id);
        if (i < 0) return wxACC_INVALID_ARG;
        if (enabled) *state |= wxACC_STATE_SYSTEM_SELECTABLE | wxACC_STATE_SYSTEM_FOCUSABLE;
        else *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (m_list->IsSelected(size_t(i))) *state |= wxACC_STATE_SYSTEM_SELECTED;
        if (m_list->HasFocus() && m_list->IsCurrent(size_t(i))) *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (m_list->m_checks && m_list->IsChecked(unsigned(i))) *state |= wxACC_STATE_SYSTEM_CHECKED;
        if (!shown) *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        else if (m_list->GetItemRect(size_t(i)).IsEmpty()) *state |= wxACC_STATE_SYSTEM_OFFSCREEN;
        return wxACC_OK;
    }

    wxAccStatus GetLocation(wxRect &rect, int element_id) override
    {
        if (element_id == wxACC_SELF) return wxWindowAccessible::GetLocation(rect, element_id);
        const int i = row(element_id);
        if (i < 0) return wxACC_INVALID_ARG;
        // A row scrolled out of view has no rectangle and reports offscreen.
        rect = m_list->GetItemRect(size_t(i));
        if (!rect.IsEmpty()) rect.SetPosition(m_list->ClientToScreen(rect.GetPosition()));
        return wxACC_OK;
    }

    wxAccStatus HitTest(const wxPoint &point, int *child_id, wxAccessible **child) override
    {
        if (!child_id || !child) return wxACC_FAIL;
        *child_id = wxACC_SELF;
        *child = nullptr;
        const wxPoint local = m_list->ScreenToClient(point);
        if (!m_list->GetClientRect().Contains(local)) return wxACC_FALSE;
        const int i = m_list->VirtualHitTest(local.y);
        if (i != wxNOT_FOUND && size_t(i) < m_list->GetCount()) *child_id = i + 1;
        else *child = this;
        return wxACC_OK;
    }

    wxAccStatus GetFocus(int *child_id, wxAccessible **child) override
    {
        if (!child_id || !child) return wxACC_FAIL;
        *child_id = wxACC_SELF;
        *child = nullptr;
        if (!m_list->HasFocus()) return wxACC_OK;
        const int current = m_list->GetCurrentRow();
        if (current != wxNOT_FOUND) *child_id = current + 1;
        else *child = this;
        return wxACC_OK;
    }

    wxAccStatus Navigate(wxNavDir direction, int from_id, int *to_id, wxAccessible **to_object) override
    {
        if (!to_id || !to_object) return wxACC_FAIL;
        const int count = int(m_list->GetCount());
        if (from_id == wxACC_SELF) {
            if (direction != wxNAVDIR_FIRSTCHILD && direction != wxNAVDIR_LASTCHILD)
                return wxWindowAccessible::Navigate(direction, from_id, to_id, to_object);
            *to_object = nullptr;
            if (count == 0) return wxACC_FALSE;
            *to_id = direction == wxNAVDIR_FIRSTCHILD ? 1 : count;
            return wxACC_OK;
        }
        const int i = row(from_id);
        if (i < 0) return wxACC_INVALID_ARG;
        *to_object = nullptr;
        int next = wxNOT_FOUND;
        if (direction == wxNAVDIR_NEXT || direction == wxNAVDIR_DOWN) next = i + 1;
        else if (direction == wxNAVDIR_PREVIOUS || direction == wxNAVDIR_UP) next = i - 1;
        if (next < 0 || next >= count) return wxACC_FALSE;
        *to_id = next + 1;
        return wxACC_OK;
    }

#if wxUSE_VARIANT
    wxAccStatus GetSelections(wxVariant *selections) override
    {
        if (!selections) return wxACC_FAIL;
        wxArrayInt rows;
        m_list->GetSelections(rows);
        if (rows.empty()) {
            *selections = wxVariant();
        } else if (rows.size() == 1) {
            *selections = wxVariant(long(rows[0] + 1));
        } else {
            selections->NullList();
            for (int r : rows) selections->Append(wxVariant(long(r + 1)));
        }
        return wxACC_OK;
    }
#endif

private:
    // The row behind a child id, or -1 for wxACC_SELF and ids past the end.
    int row(int child_id) const
    {
        return child_id >= 1 && size_t(child_id) <= m_list->GetCount() ? child_id - 1 : -1;
    }

    ListBox *m_list;
};
#endif

ListBox::ListBox(wxWindow *parent, wxWindowID id, const wxSize &size, long style)
{
    // Created here rather than through the base constructor, so that Create()
    // already runs with this class's MSWGetStyle() and MSWWindowProc() and
    // Windows never gets a scrollbar of its own to draw.
    Create(parent, id, wxDefaultPosition, size, (style & ~wxBORDER_MASK) | wxBORDER_NONE);
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    wxVListBox::SetFont(MD3::Metrics::isCompact() ? Label::Body_13 : Label::Body_14);
    Bind(wxEVT_MOTION, &ListBox::onMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, &ListBox::onLeave, this);
    Bind(wxEVT_LEFT_DOWN, &ListBox::onLeftDown, this);
    Bind(wxEVT_KEY_DOWN, &ListBox::onKey, this);
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
        RefreshAll();
#if wxUSE_ACCESSIBILITY
        // Windows announces the list itself; the row the keyboard is on follows.
        const int current = GetCurrentRow();
        if (current != wxNOT_FOUND)
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, current + 1);
#endif
        event.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) { RefreshAll(); event.Skip(); });
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) { Rescale(); event.Skip(); });
    // Bound last, so these run first: wxVListBox moves the current row and the
    // selection, and onKey()/onLeftDown() flip a check, after them, and
    // announce() reports the result once the event has been handled.
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) { CallAfter([this] { announce(); }); event.Skip(); });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& event) { CallAfter([this] { announce(); }); event.Skip(); });
    Bind(wxEVT_LEFT_DCLICK, [this](wxMouseEvent& event) { CallAfter([this] { announce(); }); event.Skip(); });
#if wxUSE_ACCESSIBILITY
    SetAccessible(new Accessible(this));
#endif
}

ListBox::~ListBox()
{
    m_hover_motion.Stop();
#ifdef __WXMSW__
    m_bars.Abandon();
#endif
}

#ifdef __WXMSW__
// The kit scrollbar: the list's scroll helper reports here instead of to
// Windows (MD3ScrollBars).
void ListBox::SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh)
{
    m_bars.SetScrollbar(orient, pos, thumbVisible, range, refresh);
}

void ListBox::SetScrollPos(int orient, int pos, bool refresh) { m_bars.SetScrollPos(orient, pos, refresh); }

int ListBox::GetScrollPos(int orient) const { return m_bars.GetScrollPos(orient); }

int ListBox::GetScrollThumb(int orient) const { return m_bars.GetScrollThumb(orient); }

int ListBox::GetScrollRange(int orient) const { return m_bars.GetScrollRange(orient); }

WXDWORD ListBox::MSWGetStyle(long flags, WXDWORD *exstyle) const
{
    return MD3ScrollBars::WithoutNativeBars(wxVListBox::MSWGetStyle(flags, exstyle));
}

WXLRESULT ListBox::MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam)
{
    WXLRESULT result = 0;
    if (m_bars.Before(msg, wParam, lParam, result))
        return result;
    result = wxVListBox::MSWWindowProc(msg, wParam, lParam);
    m_bars.After(msg, wParam, lParam, result);
    return result;
}
#endif // __WXMSW__

void ListBox::EnableChecks(bool enable)
{
    m_checks = enable;
    RefreshAll();
}

void ListBox::Check(unsigned index, bool checked)
{
    if (index >= m_checked.size()) return;
    m_checked[index] = checked ? 1 : 0;
    RefreshRow(index);
}

int ListBox::checkWidth() const
{
    return m_checks ? FromDIP(kCheck + kCheckGap) : 0;
}

void ListBox::toggle(size_t row)
{
    if (row >= m_checked.size()) return;
    m_checked[row] = m_checked[row] ? 0 : 1;
    RefreshRow(row);
    wxCommandEvent event(wxEVT_CHECKLISTBOX, GetId());
    event.SetEventObject(this);
    event.SetInt(int(row));
    GetEventHandler()->ProcessEvent(event);
}

void ListBox::onLeftDown(wxMouseEvent &evt)
{
    // The glyph toggles; anywhere on the row still selects it.
    if (m_checks) {
        const int row = VirtualHitTest(evt.GetY());
        const int glyph_end = FromDIP(kInsetX) + FromDIP(kPadX) + FromDIP(kCheck);
        if (row >= 0 && evt.GetX() < glyph_end)
            toggle(size_t(row));
    }
    evt.Skip();
}

void ListBox::onKey(wxKeyEvent &evt)
{
    if (m_checks && evt.GetKeyCode() == WXK_SPACE && GetSelection() != wxNOT_FOUND) {
        toggle(size_t(GetSelection()));
        return;
    }
    evt.Skip();
}

void ListBox::Set(const std::vector<wxString> &rows)
{
    m_hover_motion.Stop();
    m_previous_hover = -1;
    m_hover_progress = 1.0;
    m_rows = rows;
    m_checked.assign(m_rows.size(), 0);
    m_hover = -1;
    SetItemCount(m_rows.size());
    if (GetSelection() != wxNOT_FOUND && size_t(GetSelection()) >= m_rows.size()) SetSelection(wxNOT_FOUND);
    RefreshAll();
}

void ListBox::Append(const wxString &row)
{
    m_rows.push_back(row);
    m_checked.push_back(0);
    SetItemCount(m_rows.size());
    RefreshRow(m_rows.size() - 1);
}

void ListBox::Clear()
{
    m_hover_motion.Stop();
    m_previous_hover = -1;
    m_hover_progress = 1.0;
    m_rows.clear();
    m_checked.clear();
    m_hover = -1;
    SetSelection(wxNOT_FOUND);
    SetItemCount(0);
    RefreshAll();
}

void ListBox::SetColorScheme(MD3::ColorScheme scheme)
{
    m_scheme = scheme;
    RefreshAll();
}

void ListBox::Rescale()
{
    m_hover_motion.Stop();
    m_previous_hover = -1;
    m_hover_progress = 1.0;
    // Explicit caller fonts remain authoritative; uncustomized lists follow density.
    if (!m_custom_font)
        wxVListBox::SetFont(MD3::Metrics::isCompact() ? Label::Body_13 : Label::Body_14);
    RefreshAll();
}

bool ListBox::SetFont(const wxFont& font)
{
    m_custom_font = true;
    const bool changed = wxVListBox::SetFont(font);
    if (changed) RefreshAll(); // invalidate cached row heights after caller font changes
    return changed;
}

wxCoord ListBox::OnMeasureItem(size_t n) const
{
    wxClientDC dc(const_cast<ListBox*>(this));
    dc.SetFont(GetFont());
    const int text_height = n < m_rows.size() ? dc.GetMultiLineTextExtent(m_rows[n]).y : GetCharHeight();
    return atlasListRowHeight(FromDIP(MD3::Metrics::active().row_height), text_height,
        m_checks ? FromDIP(kCheck) : 0, FromDIP(kInsetY), FromDIP(MD3::Metrics::isCompact() ? 4 : 6));
}

void ListBox::OnDrawBackground(wxDC &dc, const wxRect &rect, size_t n) const
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.DrawRectangle(rect);

    const bool selected = IsSelected(n);
    // The ring marks the row the keyboard is on. In a multiple-selection list
    // that row need not be selected (Ctrl+arrows move it alone) and it alone
    // takes the ring; the other selected rows keep just their pane. In a
    // single-selection list the current row is the selected row.
    const bool keyboard = IsCurrent(n) && HasFocus() && IsEnabled();
    const double progress = MD3::Motion::reduced() ? 1.0 : m_hover_progress;
    const double hover = !IsEnabled() ? 0.0 : int(n) == m_hover ? progress : int(n) == m_previous_hover ? 1.0 - progress : 0.0;
    if (!selected && hover <= 0.0 && !keyboard) return;
    const auto geometry = atlasListGeometry(rect.width, rect.height, FromDIP(kInsetX), FromDIP(kInsetY), 0, 0);
    const wxRect pane(rect.x + geometry.pane_x, rect.y + geometry.pane_y, geometry.pane_width, geometry.pane_height);
    if (pane.IsEmpty()) return;
    const int radius = std::min(FromDIP(MD3::Metrics::active().small_radius), std::min(pane.width, pane.height) / 2);
    if (selected || hover > 0.0) {
        const wxColour base = selected ? StateColor::semantic(IsEnabled() ? MD3::Role::SecondaryContainer : MD3::Role::SurfaceContainerLow, m_scheme) : GetBackgroundColour();
        const wxColour layer = StateColor::semantic(selected ? MD3::Role::OnSecondaryContainer : MD3::Role::SurfaceContainerHigh, m_scheme);
        const wxColour fill = atlasListBlend(base, layer, hover * (selected ? 0.08 : 1.0));
        dc.SetBrush(wxBrush(fill));
        dc.DrawRoundedRectangle(pane, radius);
    }
    if (keyboard) {
        wxRect focus = pane;
        const int pen_width = FromDIP(2);
        focus.Deflate(pen_width);
        if (!focus.IsEmpty()) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Primary, m_scheme), pen_width));
            dc.DrawRoundedRectangle(focus, std::max(0, radius - pen_width));
        }
    }
}

void ListBox::OnDrawItem(wxDC &dc, const wxRect &rect, size_t n) const
{
    if (n >= m_rows.size()) return;
    dc.SetFont(GetFont());
    const auto foreground = !IsEnabled() ? MD3::Role::OnSurfaceVariant : IsSelected(n) ? MD3::Role::OnSecondaryContainer : MD3::Role::OnSurface;
    dc.SetTextForeground(StateColor::semantic(foreground, m_scheme));
    const auto geometry = atlasListGeometry(rect.width, rect.height, FromDIP(kInsetX), FromDIP(kInsetY), FromDIP(kPadX), checkWidth());
    const wxRect text(rect.x + geometry.text_x, rect.y + geometry.pane_y, geometry.text_width, geometry.pane_height);
    wxDCClipper row_clip(dc, rect);
    if (m_checks) {
        const bool checked = IsChecked(unsigned(n));
        const wxRect glyph(rect.x + FromDIP(kInsetX) + FromDIP(kPadX), rect.y, FromDIP(kCheck), rect.height);
        MaterialIcon::drawCentered(dc, checked ? MaterialIcon::CheckBox : MaterialIcon::CheckBoxOutlineBlank, FromDIP(kCheck),
            StateColor::semantic(!IsEnabled() ? MD3::Role::OnSurfaceVariant : IsSelected(n) ? MD3::Role::OnSecondaryContainer : checked ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant, m_scheme), glyph);
    }
    if (text.IsEmpty()) return;
    const wxString shown = wxControl::Ellipsize(m_rows[n], dc, wxELLIPSIZE_END, text.width);
    dc.DrawLabel(shown, text, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
}

void ListBox::onMotion(wxMouseEvent &evt)
{
    const int row = VirtualHitTest(evt.GetY());
    if (row != m_hover) {
        const int old = m_hover;
        m_hover = row;
        animateHover(old);
        if (old >= 0) RefreshRow(size_t(old));
        if (row >= 0) RefreshRow(size_t(row));
        // The full text stays reachable through the tooltip when a row is ellipsized.
        SetToolTip(row >= 0 && size_t(row) < m_rows.size() ? m_rows[size_t(row)] : wxString());
    }
    evt.Skip();
}

void ListBox::onLeave(wxMouseEvent &evt)
{
    if (m_hover >= 0) {
        const int old = m_hover;
        m_hover = -1;
        animateHover(old);
        RefreshRow(size_t(old));
    }
    evt.Skip();
}

void ListBox::SetSelection(int selection)
{
    wxVListBox::SetSelection(selection);
    announce();
}

void ListBox::SetItemCount(size_t count)
{
    wxVListBox::SetItemCount(count);
    // Set(), Append() and Clear() all land here: the rows were replaced, added
    // or removed, so screen readers read the children again.
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_REORDER, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    m_announced_checked = m_checked; // new rows, not flipped check boxes
    announce();
}

int ListBox::GetCurrentRow() const
{
    if (!HasMultipleSelection()) return GetSelection();
    for (size_t row = 0; row < m_rows.size(); ++row)
        if (IsCurrent(row)) return int(row);
    return wxNOT_FOUND;
}

void ListBox::announce()
{
    const int current = GetCurrentRow();
    wxArrayInt rows;
    GetSelections(rows);
    std::vector<int> selection(rows.begin(), rows.end());
    const bool moved = current != m_announced_current;
    const bool selection_changed = selection != m_announced_selection;
    // Check boxes the user flipped since the last call; replaced rows are
    // covered by the reorder event instead.
    std::vector<size_t> flipped;
    if (m_announced_checked.size() == m_checked.size())
        for (size_t row = 0; row < m_checked.size(); ++row)
            if (m_checked[row] != m_announced_checked[row]) flipped.push_back(row);
    m_announced_current = current;
    m_announced_selection = std::move(selection);
    m_announced_checked = m_checked;
#if wxUSE_ACCESSIBILITY
    for (size_t row : flipped)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, int(row) + 1);
    if (selection_changed) {
        // A single selected row is announced as the selection; any other change
        // asks the screen reader to read the selection again.
        if (m_announced_selection.size() == 1)
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_SELECTION, this, wxOBJID_CLIENT, m_announced_selection.front() + 1);
        else
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_SELECTIONWITHIN, this, wxOBJID_CLIENT, wxACC_SELF);
    }
    if (moved && current != wxNOT_FOUND && HasFocus())
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, current + 1);
#else
    wxUnusedVar(moved);
    wxUnusedVar(selection_changed);
    wxUnusedVar(flipped);
#endif
}

int ListBox::GetSelections(wxArrayInt &selections) const
{
    selections.clear();
    if (!HasMultipleSelection()) {
        if (GetSelection() != wxNOT_FOUND) selections.push_back(GetSelection());
        return int(selections.size());
    }
    unsigned long cookie = 0;
    for (int row = GetFirstSelected(cookie); row != wxNOT_FOUND; row = GetNextSelected(cookie))
        selections.push_back(row);
    return int(selections.size());
}

void ListBox::animateHover(int previous)
{
    m_hover_motion.Stop();
    const int displaced = m_previous_hover;
    m_previous_hover = previous;
    m_hover_progress = 0.0;
    // Stop cancels without a final tick. Repaint the outgoing fade's old row
    // after replacing its state, even when the new transition settles immediately.
    if (displaced >= 0 && size_t(displaced) < m_rows.size()) RefreshRow(size_t(displaced));
    m_hover_motion.Play(MD3::Motion::short2, [this](double progress) {
        m_hover_progress = progress;
        if (m_previous_hover >= 0 && size_t(m_previous_hover) < m_rows.size()) RefreshRow(size_t(m_previous_hover));
        if (m_hover >= 0 && size_t(m_hover) < m_rows.size()) RefreshRow(size_t(m_hover));
    }, nullptr, &MD3::Motion::easeStandard, this);
}

}} // namespace Slic3r::GUI
