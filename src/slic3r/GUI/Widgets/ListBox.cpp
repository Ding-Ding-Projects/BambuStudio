#include "ListBox.hpp"

#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include <wx/control.h>
#include <wx/dcclient.h>
#include <wx/tooltip.h>

namespace Slic3r { namespace GUI {

namespace {
constexpr int kInsetX  = 4; // row pane inset from the field edge, DIP
constexpr int kInsetY  = 1;
constexpr int kRadius  = 8; // row pane radius, DIP
constexpr int kPadX    = 12; // text inset inside the pane, DIP
constexpr int kCheck   = 20; // check box glyph, DIP
constexpr int kCheckGap = 8; // between the glyph and the text, DIP
} // namespace

ListBox::ListBox(wxWindow *parent, wxWindowID id, const wxSize &size, long style)
    : wxVListBox(parent, id, wxDefaultPosition, size, (style & ~wxBORDER_MASK) | wxBORDER_NONE)
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    SetFont(Label::Body_13);
    Bind(wxEVT_MOTION, &ListBox::onMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, &ListBox::onLeave, this);
    Bind(wxEVT_LEFT_DOWN, &ListBox::onLeftDown, this);
    Bind(wxEVT_KEY_DOWN, &ListBox::onKey, this);
}

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
    SetFont(Label::Body_13);
    RefreshAll();
}

wxCoord ListBox::OnMeasureItem(size_t) const
{
    return FromDIP(MD3::Metrics::active().row_height);
}

void ListBox::OnDrawBackground(wxDC &dc, const wxRect &rect, size_t n) const
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.DrawRectangle(rect);

    const bool selected = IsSelected(n);
    const bool hovered  = int(n) == m_hover && !selected;
    if (!selected && !hovered) return;
    wxRect pane = rect;
    pane.Deflate(FromDIP(kInsetX), FromDIP(kInsetY));
    const wxColour fill = selected
        ? StateColor::semantic(MD3::Role::SecondaryContainer, m_scheme)
        : StateColor::semantic(MD3::Role::SurfaceContainerHigh);
    dc.SetBrush(wxBrush(fill));
    dc.DrawRoundedRectangle(pane, FromDIP(kRadius));
}

void ListBox::OnDrawItem(wxDC &dc, const wxRect &rect, size_t n) const
{
    if (n >= m_rows.size()) return;
    dc.SetFont(GetFont());
    dc.SetTextForeground(StateColor::semantic(IsSelected(n) ? MD3::Role::OnSecondaryContainer : MD3::Role::OnSurface, m_scheme));
    wxRect text = rect;
    text.Deflate(FromDIP(kInsetX) + FromDIP(kPadX), 0);
    if (m_checks) {
        const bool checked = IsChecked(unsigned(n));
        const wxRect glyph(text.x, rect.y, FromDIP(kCheck), rect.height);
        MaterialIcon::drawCentered(dc, checked ? MaterialIcon::CheckBox : MaterialIcon::CheckBoxOutlineBlank, FromDIP(kCheck),
                                   StateColor::semantic(checked ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant, m_scheme), glyph);
        text.x += checkWidth();
        text.width -= checkWidth();
    }
    const wxString shown = wxControl::Ellipsize(m_rows[n], dc, wxELLIPSIZE_END, text.width);
    dc.DrawLabel(shown, text, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
}

void ListBox::onMotion(wxMouseEvent &evt)
{
    const int row = VirtualHitTest(evt.GetY());
    if (row != m_hover) {
        const int old = m_hover;
        m_hover = row;
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
        RefreshRow(size_t(old));
    }
    evt.Skip();
}

}} // namespace Slic3r::GUI
