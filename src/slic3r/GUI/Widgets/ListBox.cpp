#include "ListBox.hpp"

#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include <wx/control.h>
#include <wx/dcclient.h>
#include <wx/tooltip.h>
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
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) { RefreshAll(); event.Skip(); });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) { RefreshAll(); event.Skip(); });
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) { Rescale(); event.Skip(); });
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
    const double progress = MD3::Motion::reduced() ? 1.0 : m_hover_progress;
    const double hover = !IsEnabled() ? 0.0 : int(n) == m_hover ? progress : int(n) == m_previous_hover ? 1.0 - progress : 0.0;
    if (!selected && hover <= 0.0) return;
    const auto geometry = atlasListGeometry(rect.width, rect.height, FromDIP(kInsetX), FromDIP(kInsetY), 0, 0);
    const wxRect pane(rect.x + geometry.pane_x, rect.y + geometry.pane_y, geometry.pane_width, geometry.pane_height);
    if (pane.IsEmpty()) return;
    const wxColour base = selected ? StateColor::semantic(IsEnabled() ? MD3::Role::SecondaryContainer : MD3::Role::SurfaceContainerLow, m_scheme) : GetBackgroundColour();
    const wxColour layer = StateColor::semantic(selected ? MD3::Role::OnSecondaryContainer : MD3::Role::SurfaceContainerHigh, m_scheme);
    const wxColour fill = atlasListBlend(base, layer, hover * (selected ? 0.08 : 1.0));
    dc.SetBrush(wxBrush(fill));
    const int radius = std::min(FromDIP(MD3::Metrics::active().small_radius), std::min(pane.width, pane.height) / 2);
    dc.DrawRoundedRectangle(pane, radius);
    if (selected && HasFocus() && IsEnabled()) {
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

void ListBox::animateHover(int previous)
{
    m_hover_motion.Stop();
    m_previous_hover = previous;
    m_hover_progress = 0.0;
    m_hover_motion.Play(MD3::Motion::short2, [this](double progress) {
        m_hover_progress = progress;
        if (m_previous_hover >= 0 && size_t(m_previous_hover) < m_rows.size()) RefreshRow(size_t(m_previous_hover));
        if (m_hover >= 0 && size_t(m_hover) < m_rows.size()) RefreshRow(size_t(m_hover));
    }, nullptr, &MD3::Motion::easeStandard, this);
}

}} // namespace Slic3r::GUI
