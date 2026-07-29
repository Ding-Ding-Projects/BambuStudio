#include "SideButton.hpp"
#include "../I18N.hpp"
#include "Label.hpp"
#include "MD3Tokens.hpp"
#include "StateColor.hpp"
#include "MaterialIcon.hpp"

#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

// WCAG relative luminance, used only to keep the keyboard focus indicator at
// 3:1 against whichever state fill a SideButton is currently painting.
double relative_luminance(const wxColour &colour)
{
    auto linear = [](unsigned char channel) {
        const double value = channel / 255.0;
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(colour.Red()) + 0.7152 * linear(colour.Green()) +
           0.0722 * linear(colour.Blue());
}

double contrast_ratio(const wxColour &first, const wxColour &second)
{
    const double first_luminance  = relative_luminance(first);
    const double second_luminance = relative_luminance(second);
    return (std::max(first_luminance, second_luminance) + 0.05) /
           (std::min(first_luminance, second_luminance) + 0.05);
}

wxColour focus_ring_colour(const wxColour &interior, const wxColour &label)
{
    const wxColour primary = StateColor::semantic(MD3::Role::Primary);
    if (contrast_ratio(primary, interior) >= 3.0)
        return primary;
    return label.IsOk() && contrast_ratio(label, interior) > contrast_ratio(primary, interior)
               ? label
               : primary;
}

#if wxUSE_ACCESSIBILITY
class SideButtonAccessible final : public wxWindowAccessible
{
public:
    explicit SideButtonAccessible(SideButton *button)
        : wxWindowAccessible(button), m_button(button)
    {
    }

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (child_id != wxACC_SELF || name == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        const wxString configured_name = m_button->GetName();
        *name = !configured_name.IsEmpty() && configured_name != wxASCII_STR(wxPanelNameStr)
                    ? configured_name
                    : m_button->GetLabel();
        if (name->IsEmpty())
            *name = m_button->GetToolTipText();
        return wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (child_id != wxACC_SELF || role == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *role = wxROLE_SYSTEM_PUSHBUTTON;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (child_id != wxACC_SELF || state == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *state = 0;
        if (m_button->AcceptsFocusFromKeyboard())
            *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
        if (m_button->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (m_button->IsPressedForAccessibility())
            *state |= wxACC_STATE_SYSTEM_PRESSED;
        if (!m_button->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!m_button->IsShown())
            *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (child_id != wxACC_SELF || action_name == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *action_name = _L("Press");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        if (child_id != wxACC_SELF)
            return wxACC_NOT_IMPLEMENTED;
        m_button->AccessibilityActivate();
        return wxACC_OK;
    }

private:
    SideButton *m_button;
};
#endif

bool is_activation_key(int key_code)
{
    return key_code == WXK_SPACE || key_code == WXK_RETURN || key_code == WXK_NUMPAD_ENTER;
}

} // namespace

BEGIN_EVENT_TABLE(SideButton, wxWindow)
EVT_LEFT_DOWN(SideButton::mouseDown)
EVT_LEFT_UP(SideButton::mouseReleased)
EVT_MOUSE_CAPTURE_LOST(SideButton::mouseCaptureLost)
EVT_KEY_DOWN(SideButton::keyDown)
EVT_KEY_UP(SideButton::keyUp)
EVT_SET_FOCUS(SideButton::focusChanged)
EVT_KILL_FOCUS(SideButton::focusChanged)
EVT_PAINT(SideButton::paintEvent)
END_EVENT_TABLE()

SideButton::SideButton(wxWindow* parent, wxString text, wxString icon, long stlye, int iconSize)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, stlye)
    , state_handler(this)
{
    // MD3 shape token rather than a bare literal. radius_rail (12) is the
    // rail/snackbar tier and reproduces the previous default exactly, so nothing
    // moves: both consumers override it anyway (the Slice/Print pills take the
    // pill radius in MainFrame::update_side_button_style, the dropdown rows take
    // SetCornerRadius(0)).
    radius = MD3::Metrics::radius_rail;
#ifdef __APPLE__
    extra_size = wxSize(38 + FromDIP(20), 10);
    text_margin = 15 + FromDIP(20);
#else
    extra_size = wxSize(38, 10);
    text_margin = 15;
#endif

    icon_offset = 0;
    text_orientation = HO_Left;

    applyMenuRowStyle();

    state_handler.attach({ &border_color, &text_color, &background_color });
    state_handler.update_binds();

    // icon only
    if (!icon.IsEmpty()) {
        this->icon = ScalableBitmap(this, icon.ToStdString(), iconSize > 0 ? iconSize : 14);
    }

    SetFont(Label::Body_14);
    wxWindow::SetLabel(text);

#if wxUSE_ACCESSIBILITY
    new SideButtonAccessible(this); // wxWindow owns the accessible object.
#endif
    messureSize();
}

void SideButton::applyMenuRowStyle()
{
    using R = MD3::Role;

    // Every SideButton that is not one of MainFrame's four Slice/Print action
    // controls is a row in the Slice/Print dropdown (SidePopup), and those rows
    // only ever call SetCornerRadius(0) — they keep whatever the constructor
    // leaves behind. That default used to be the whole legacy brand palette
    // (BrandGreen fill, white label, a #3B4446 slate slab painted underneath),
    // so opening either dropdown dropped a stack of solid green bars over the
    // MD3 Prepare workspace. The kit menu row (design-system NavItem, and the
    // list rows DropDown.cpp already paints) is neutral instead: the menu
    // container at rest, SurfaceContainerHigh on hover, SurfaceContainerHighest
    // while pressed.
    //
    // Every role below is a neutral surface/on-surface tone, which is why this
    // takes no ColorScheme: Brand, Preview and Device resolve them identically —
    // only the accent roles diverge (see MD3::resolve).
    const wxColour rest    = StateColor::semantic(R::SurfaceContainer);
    const wxColour hover   = StateColor::semantic(R::SurfaceContainerHigh);
    const wxColour pressed = StateColor::semantic(R::SurfaceContainerHighest);

    // These are construction-time snapshots, but each light tone used here is a
    // key in StateColor.cpp's gDarkColors table mapping to exactly its MD3::Dark
    // counterpart, so a runtime theme toggle re-colours the row at its next
    // paint rather than stranding a light menu on a dark shell.
    background_color = StateColor(
        std::make_pair(rest,    (int) StateColor::Disabled),
        std::make_pair(pressed, (int) StateColor::Pressed),
        std::make_pair(hover,   (int) StateColor::Hovered),
        std::make_pair(rest,    (int) StateColor::Normal));
    background_color.setTakeFocusedAsHovered(false);

    // The ring tracks the fill in every state: a menu row is a highlight band,
    // not an outlined button, so no contrasting border may show.
    border_color = StateColor(
        std::make_pair(rest,    (int) StateColor::Disabled),
        std::make_pair(pressed, (int) StateColor::Pressed),
        std::make_pair(hover,   (int) StateColor::Hovered),
        std::make_pair(rest,    (int) StateColor::Normal));
    border_color.setTakeFocusedAsHovered(false);

    // Disabled label: Outline is the theme-tracking "dimmed content on a
    // container" tone. An OnSurface-at-opacity blend would be closer to the kit,
    // but the blended result is not a gDarkColors key, so it would freeze at
    // whichever theme was active when the row was built.
    text_color = StateColor(
        std::make_pair(StateColor::semantic(R::Outline),   (int) StateColor::Disabled),
        std::make_pair(StateColor::semantic(R::OnSurface), (int) StateColor::Normal));

    // The surface the row sits on, seen through the rounded corners whenever a
    // radius is in play — the SidePopup menu container, replacing the legacy
    // #3B4446 slate.
    SetBottomColour(rest);
}

void SideButton::SetCornerRadius(double radius)
{
    this->radius = radius;
    Refresh();
}

void SideButton::SetCornerEnable(const std::vector<bool>& enable)
{
    radius_enable.clear();
    for (auto en : enable) {
        radius_enable.push_back(en);
    }
}

void SideButton::SetTextLayout(EHorizontalOrientation orient, int margin)
{
    text_orientation = orient;
    text_margin = margin;
    messureSize();
    Refresh();
}

void SideButton::SetLayoutStyle(int style)
{
    layout_style = style;
    messureSize();
    Refresh();
}

void SideButton::SetLabel(const wxString& label)
{
    if (label == GetLabel())
        return;
    wxWindow::SetLabel(label);
    messureSize();
    Refresh();
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
}

void SideButton::SetName(const wxString& name)
{
    if (name == wxWindow::GetName())
        return;
    wxWindow::SetName(name);
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
}

bool SideButton::SetForegroundColour(wxColour const &color)
{
    text_color = StateColor(color);
    state_handler.update_binds();
    return true;
}

bool SideButton::SetBackgroundColour(wxColour const& color)
{
    background_color = StateColor(color);
    state_handler.update_binds();
    return true;
}

bool SideButton::SetBottomColour(wxColour const& color)
{
    bottom_color = color;
    return true;
}

void SideButton::SetMinSize(const wxSize& size)
{
    minSize = size;
    messureSize();
}

void SideButton::SetBorderColor(StateColor const &color)
{
    border_color = color;
    state_handler.update_binds();
    Refresh();
}

void SideButton::SetForegroundColor(StateColor const &color)
{
    text_color = color;
    state_handler.update_binds();
    Refresh();
}

void SideButton::SetBackgroundColor(StateColor const &color)
{
    background_color = color;
    state_handler.update_binds();
    Refresh();
}

bool SideButton::Enable(bool enable)
{
    bool result = wxWindow::Enable(enable);
    if (result) {
        keyboard_pressed = false;
        Refresh(false);
        wxCommandEvent e(EVT_ENABLE_CHANGED);
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(e);
#if wxUSE_ACCESSIBILITY
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    }
    return result;
}

void SideButton::Rescale()
{
    if (this->icon.bmp().IsOk())
        this->icon.msw_rescale();
    messureSize();
}

void SideButton::SetExtraSize(const wxSize& size)
{
    extra_size = size;
    messureSize();
}

void SideButton::SetIconOffset(const int offset)
{
    icon_offset = offset;
    messureSize();
}

void SideButton::SetLeadingGlyph(uint32_t codepoint, int px)
{
    leading_glyph_cp = codepoint;
    leading_glyph_px = px > 0 ? px : 0;
    messureSize();
    Refresh();
}

int SideButton::leadingGlyphPx() const
{
    // Design-px value; MaterialIcon converts it to a point size internally, so it
    // stays DPI-correct without a FromDIP here. The default (20) matches the MD3
    // large-button (h44) icon tier that the Slice/Print pills use.
    return leading_glyph_px > 0 ? leading_glyph_px : 20;
}

void SideButton::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
#ifdef __WXMSW__
    wxGCDC dc2(dc);
#else
    wxDC & dc2(dc);
#endif

    wxDC & dctext(dc);
    dorender(dc2, dctext);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void SideButton::dorender(wxDC& dc, wxDC& text_dc)
{
    wxSize size = GetSize();

    // draw background
    dc.SetPen(wxNullPen);
    dc.SetBrush(StateColor::darkModeColorFor(bottom_color));
    dc.DrawRectangle(0, 0, size.x, size.y);

    int states = state_handler.states();
    dc.SetBrush(wxBrush(background_color.colorForStates(states)));

    dc.SetPen(wxPen(border_color.colorForStates(states)));
    int pen_width = dc.GetPen().GetWidth();

    // wxDC::DrawRoundedRectangle is only well-defined for radius <= half the
    // SMALLER edge. The Slice/Print options segments are 24px wide but carry the
    // shared pill radius (button height / 2 = 22px); un-clamped, GDI+ renders
    // the overlapping corner arcs as a malformed egg/oval blob (seen in the dark
    // Prepare action bar). Clamp per-paint so a narrow segment degrades to a
    // proper capsule instead.
    const double radius = std::min(this->radius,
                                   std::min(size.x, size.y) / 2.0);


    // draw icon style
    if (icon.bmp().IsOk()) {
        if (radius > 1e-5) {
            dc.DrawRoundedRectangle(0, 0, size.x, size.y, radius);
            dc.DrawRectangle(radius, 0, size.x - radius, size.y);
            dc.SetPen(wxNullPen);
            dc.DrawRectangle(radius - pen_width, pen_width, radius, size.y - 2 * pen_width);
        }
        else {
            dc.DrawRectangle(0, 0, size.x, size.y);
        }
    }
    // draw text style
    else {
        if (radius > 1e-5) {
            if (layout_style == 1) {
                dc.DrawRoundedRectangle(0, 0, size.x, size.y, radius);
                dc.SetPen(wxNullPen);
            } else {
                dc.DrawRoundedRectangle(0, 0, size.x, size.y, radius);
                dc.DrawRectangle(0, 0, radius, size.y);
                dc.SetPen(wxNullPen);
                dc.DrawRectangle(pen_width, pen_width, size.x - radius, size.y - 2 * pen_width);
            }
        } else {
            dc.DrawRectangle(0, 0, size.x, size.y);
        }
    }

    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    // A leading Material Symbols glyph (SetLeadingGlyph) is drawn before the
    // label. It is additive and self-gated: when the icon face is unavailable it
    // contributes nothing and the button falls back to its label alone. It is
    // measured/drawn through the plain paint DC (text_dc), matching how the label
    // and the shared Button widget render glyphs.
    const bool draw_glyph = leading_glyph_cp != 0 && MaterialIcon::available();
    const int  glyph_gap  = FromDIP(6);
    wxSize     szGlyph;
    if (draw_glyph)
        szGlyph = MaterialIcon::measure(text_dc, leading_glyph_cp, leadingGlyphPx());

    // calc content size
    wxSize szIcon;
    wxSize szContent = textSize;
    if (draw_glyph) {
        if (szContent.x > 0)
            szContent.x += glyph_gap;
        szContent.x += szGlyph.x;
        if (szGlyph.y > szContent.y)
            szContent.y = szGlyph.y;
    }
    if (icon.bmp().IsOk()) {
        if (szContent.y > 0) {
            //BBS norrow size between text and icon
            szContent.x += 5;
        }
        szIcon = icon.GetBmpSize();
        szContent.x += szIcon.x;
        if (szIcon.y > szContent.y)
            szContent.y = szIcon.y;
    }
    // move to center
    wxRect rcContent = { {0, 0}, size };
    if (text_orientation == EHorizontalOrientation::HO_Center) {
        wxSize offset = (size - szContent) / 2;
        rcContent.Deflate(offset.x, offset.y);
    } else if (text_orientation == EHorizontalOrientation::HO_Left) {
        wxSize offset = (size - szContent) / 2;
        rcContent.Deflate(text_margin, offset.y);
    } else if (text_orientation == EHorizontalOrientation::HO_Right) {
        wxSize offset = (size - szContent) / 2;
        rcContent.Deflate(size.x - text_margin, offset.y);
    }

    // start draw
    wxPoint pt = rcContent.GetLeftTop();
    if (draw_glyph) {
        const int gy = pt.y + (rcContent.height - szGlyph.y) / 2;
        MaterialIcon::draw(text_dc, leading_glyph_cp, leadingGlyphPx(),
                           text_color.colorForStates(states), wxPoint(pt.x, gy));
        pt.x += szGlyph.x + glyph_gap;
    }
    if (icon.bmp().IsOk()) {
        //BBS extra pixels for icon
        pt.x += icon_offset;
        pt.y += (rcContent.height - szIcon.y) / 2;
        dc.DrawBitmap(icon.bmp(), pt);
        //BBS norrow size between text and icon
        pt.x += szIcon.x + 5;
        pt.y = rcContent.y;
    }

    auto text = GetLabel();
    if (!text.IsEmpty()) {
        pt.y += (rcContent.height - textSize.y) / 2;
#ifdef __APPLE__
        pt.y -= FromDIP(2);
#endif
        text_dc.SetFont(GetFont());
        text_dc.SetTextForeground(text_color.colorForStates(states));
        text_dc.DrawText(text, pt);
    }

    if (HasFocus() && IsEnabled()) {
        const int inset = std::max(FromDIP(3), 1);
        wxRect    focus_rect(inset, inset, std::max(0, size.x - inset * 2),
                             std::max(0, size.y - inset * 2));
        const wxColour interior = background_color.colorForStates(states);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(focus_ring_colour(interior, text_color.colorForStates(states)),
                        std::max(FromDIP(2), 1)));
        dc.DrawRoundedRectangle(focus_rect, std::max(0.0, radius - inset));
    }
}

void SideButton::messureSize()
{
    textSize = GetTextExtent(GetLabel());
    if (minSize.GetWidth() > 0) {
        wxWindow::SetMinSize(minSize);
        return;
    }

    wxSize szContent = textSize;
    // Mirror dorender(): reserve room for a leading glyph so the label is not
    // clipped. Suppressed when the icon face is unavailable (label-only fallback).
    if (leading_glyph_cp != 0 && MaterialIcon::available()) {
        wxClientDC dc(this);
        wxSize szGlyph = MaterialIcon::measure(dc, leading_glyph_cp, leadingGlyphPx());
        if (szContent.x > 0)
            szContent.x += FromDIP(6);
        szContent.x += szGlyph.x;
        if (szGlyph.y > szContent.y)
            szContent.y = szGlyph.y;
    }
    if (this->icon.bmp().IsOk()) {
        if (szContent.y > 0) {
            szContent.x += 5;
        }
        wxSize szIcon = this->icon.GetBmpSize();
        szContent.x += szIcon.x;
        if (szIcon.y > szContent.y)
            szContent.y = szIcon.y;
        //BBS icon only
        wxWindow::SetMinSize(szContent + wxSize(szContent.GetX() + extra_size.GetX(), minSize.GetHeight()));
    }
    else {
        if (minSize.GetHeight() > 0) {
            //BBS with text size
            wxWindow::SetMinSize(wxSize(szContent.GetX() + extra_size.GetX(), minSize.GetHeight()));
        } else {
            //BBS with text size
            wxWindow::SetMinSize(szContent + extra_size);
        }
    }
}

void SideButton::mouseDown(wxMouseEvent& event)
{
    event.Skip();
    pressedDown = true;
    SetFocus();
    if (!HasCapture())
        CaptureMouse();
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
}

void SideButton::mouseReleased(wxMouseEvent& event)
{
    event.Skip();

    if (HasCapture())
        ReleaseMouse();

    if (pressedDown) {
        pressedDown = false;
#if wxUSE_ACCESSIBILITY
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
        if (wxRect({0, 0}, GetSize()).Contains(event.GetPosition()))
            sendButtonEvent();
    }
}

void SideButton::mouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    pressedDown = false;
    Refresh(false);
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    event.Skip();
}

void SideButton::keyDown(wxKeyEvent& event)
{
    if (is_activation_key(event.GetKeyCode())) {
        if (!keyboard_pressed) {
            keyboard_pressed = true;
            Refresh(false);
#if wxUSE_ACCESSIBILITY
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
        }
        return;
    }

    switch (event.GetKeyCode()) {
    case WXK_TAB:
    case WXK_LEFT:
    case WXK_RIGHT:
    case WXK_UP:
    case WXK_DOWN: HandleAsNavigationKey(event); break;
    default: event.Skip(); break;
    }
}

void SideButton::keyUp(wxKeyEvent& event)
{
    if (!is_activation_key(event.GetKeyCode())) {
        event.Skip();
        return;
    }

    if (keyboard_pressed) {
        keyboard_pressed = false;
        Refresh(false);
#if wxUSE_ACCESSIBILITY
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
        if (IsEnabled() && IsShown())
            sendButtonEvent();
    }
}

void SideButton::focusChanged(wxFocusEvent& event)
{
    if (event.GetEventType() == wxEVT_KILL_FOCUS)
        keyboard_pressed = false;
    Refresh(false);
#if wxUSE_ACCESSIBILITY
    if (event.GetEventType() == wxEVT_SET_FOCUS)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    event.Skip();
}

bool SideButton::AcceptsFocus() const
{
    return IsEnabled() && IsShown();
}

bool SideButton::AcceptsFocusFromKeyboard() const
{
    return AcceptsFocus();
}

void SideButton::AccessibilityActivate()
{
    if (IsEnabled() && IsShown())
        sendButtonEvent();
}

void SideButton::sendButtonEvent()
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event);
}

#ifdef __WIN32__
WXLRESULT SideButton::MSWWindowProc(WXUINT message, WXWPARAM w_param, WXLPARAM l_param)
{
    if (message == WM_GETDLGCODE)
        return DLGC_WANTMESSAGE;
    return wxWindow::MSWWindowProc(message, w_param, l_param);
}
#endif
