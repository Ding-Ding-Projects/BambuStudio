#include "SlideToConfirm.hpp"

#include <algorithm>

#include <wx/dcbuffer.h>

#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

namespace {
constexpr int kHeight   = 44; // track height (also the a11y touch target)
constexpr int kKnobPad  = 4;  // track inset around the knob
constexpr int kMinWidth = 260;
constexpr int kKeyStep  = 10; // percent per arrow-key step
} // namespace

SlideToConfirm::SlideToConfirm(wxWindow *parent, const wxString &instruction, const wxString &confirmed_label)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
    , m_instruction(instruction)
    , m_confirmed_label(confirmed_label)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetName(instruction); // screen readers announce the gate's purpose
    SetMinSize(wxSize(FromDIP(kMinWidth), FromDIP(kHeight)));
    Bind(wxEVT_PAINT, &SlideToConfirm::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &SlideToConfirm::OnMouse, this);
    Bind(wxEVT_LEFT_UP, &SlideToConfirm::OnMouse, this);
    Bind(wxEVT_MOTION, &SlideToConfirm::OnMouse, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) {
        m_dragging = false;
        if (!m_confirmed) { m_pos = 0; Refresh(); }
    });
    Bind(wxEVT_KEY_DOWN, &SlideToConfirm::OnKey, this);
    Bind(wxEVT_SET_FOCUS, &SlideToConfirm::OnFocus, this);
    Bind(wxEVT_KILL_FOCUS, &SlideToConfirm::OnFocus, this);
}

wxSize SlideToConfirm::DoGetBestSize() const
{
    return wxSize(FromDIP(kMinWidth), FromDIP(kHeight));
}

int SlideToConfirm::knobDiameter() const { return FromDIP(kHeight - 2 * kKnobPad); }
int SlideToConfirm::trackWidth() const { return GetClientSize().GetWidth(); }
int SlideToConfirm::maxTravel() const
{
    return std::max(1, trackWidth() - knobDiameter() - 2 * FromDIP(kKnobPad));
}

void SlideToConfirm::Reset()
{
    m_confirmed = false;
    m_dragging  = false;
    m_pos       = 0;
    Refresh();
}

void SlideToConfirm::Rescale()
{
    SetMinSize(wxSize(FromDIP(kMinWidth), FromDIP(kHeight)));
    Refresh();
}

void SlideToConfirm::complete()
{
    if (m_confirmed)
        return;
    m_confirmed = true;
    m_pos       = maxTravel();
    Refresh();
    if (m_on_confirm)
        m_on_confirm();
}

void SlideToConfirm::OnMouse(wxMouseEvent &event)
{
    const int d = knobDiameter();
    const wxRect knob(FromDIP(kKnobPad) + m_pos, FromDIP(kKnobPad), d, d);
    if (event.LeftDown()) {
        SetFocus();
        if (m_confirmed)
            return;
        if (knob.Contains(event.GetPosition())) {
            m_dragging     = true;
            m_drag_grab_dx = event.GetX() - knob.x;
            CaptureMouse();
        }
    } else if (event.Dragging() && m_dragging) {
        m_pos = std::clamp(event.GetX() - m_drag_grab_dx - FromDIP(kKnobPad), 0, maxTravel());
        Refresh();
    } else if (event.LeftUp() && m_dragging) {
        m_dragging = false;
        if (HasCapture())
            ReleaseMouse();
        if (m_pos >= maxTravel() * 88 / 100)
            complete();
        else {
            m_pos = 0; // an incomplete slide snaps back: no partial arming
            Refresh();
        }
    }
}

void SlideToConfirm::OnKey(wxKeyEvent &event)
{
    if (m_confirmed) { event.Skip(); return; }
    const int step = maxTravel() * kKeyStep / 100;
    switch (event.GetKeyCode()) {
    case WXK_RIGHT: case WXK_UP:
        m_pos = std::min(m_pos + step, maxTravel());
        if (m_pos >= maxTravel()) complete();
        Refresh();
        return;
    case WXK_LEFT: case WXK_DOWN:
        m_pos = std::max(0, m_pos - step);
        Refresh();
        return;
    case WXK_END:
        complete();
        return;
    case WXK_HOME:
        m_pos = 0;
        Refresh();
        return;
    default:
        event.Skip();
    }
}

void SlideToConfirm::OnFocus(wxFocusEvent &event)
{
    Refresh();
    event.Skip();
}

void SlideToConfirm::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
    dc.Clear();

    const wxColour track_fill = m_confirmed
        ? StateColor::semantic(MD3::Role::PrimaryContainer)
        : (m_danger ? StateColor::semantic(MD3::Role::ErrorContainer)
                    : StateColor::semantic(MD3::Role::SurfaceContainerHighest));
    const wxColour border = HasFocus() ? StateColor::semantic(MD3::Role::Primary)
                                       : StateColor::semantic(MD3::Role::Outline);
    const wxColour label_clr = m_confirmed
        ? StateColor::semantic(MD3::Role::OnPrimaryContainer)
        : (m_danger ? StateColor::semantic(MD3::Role::OnErrorContainer)
                    : StateColor::semantic(MD3::Role::OnSurfaceVariant));

    const int radius = sz.GetHeight() / 2;
    dc.SetPen(wxPen(border, HasFocus() ? 2 : 1));
    dc.SetBrush(wxBrush(track_fill));
    dc.DrawRoundedRectangle(0, 0, sz.GetWidth(), sz.GetHeight(), radius);

    dc.SetFont(Label::Body_13);
    dc.SetTextForeground(label_clr);
    const wxString label = m_confirmed ? m_confirmed_label : m_instruction;
    const wxSize   te    = dc.GetTextExtent(label);
    dc.DrawText(label, (sz.GetWidth() - te.GetWidth()) / 2, (sz.GetHeight() - te.GetHeight()) / 2);

    // knob
    const int d = knobDiameter();
    const int kx = FromDIP(kKnobPad) + m_pos;
    const int ky = FromDIP(kKnobPad);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary)));
    dc.DrawEllipse(kx, ky, d, d);
    const wxColour glyph_clr = StateColor::semantic(MD3::Role::OnPrimary);
    if (MaterialIcon::available()) {
        const int px = d * 5 / 9;
        const wxSize gs = MaterialIcon::measure(dc, MaterialIcon::ChevronRight, px);
        // double chevron: two overlapping single chevrons read as "keep going"
        MaterialIcon::draw(dc, MaterialIcon::ChevronRight, px, glyph_clr,
                           wxPoint(kx + (d - gs.x) / 2 - px / 5, ky + (d - gs.y) / 2));
        MaterialIcon::draw(dc, MaterialIcon::ChevronRight, px, glyph_clr,
                           wxPoint(kx + (d - gs.x) / 2 + px / 5, ky + (d - gs.y) / 2));
    } else {
        dc.SetTextForeground(glyph_clr);
        const wxSize gs = dc.GetTextExtent(">>");
        dc.DrawText(">>", kx + (d - gs.x) / 2, ky + (d - gs.y) / 2);
    }
}
