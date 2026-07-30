#include "GUI_App.hpp"
#include "CapsuleButton.hpp"
#include "I18N.hpp"
#include <wx/dcbuffer.h>
#include "wx/graphics.h"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"

#include <algorithm>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

namespace Slic3r { namespace GUI {

namespace {

#if wxUSE_ACCESSIBILITY
class CapsuleButtonAccessible final : public wxWindowAccessible
{
public:
    explicit CapsuleButtonAccessible(CapsuleButton *button)
        : wxWindowAccessible(button), m_button(button)
    {
    }

    wxAccStatus GetChildCount(int *child_count) override
    {
        if (!child_count)
            return wxACC_NOT_IMPLEMENTED;
        *child_count = 0;
        return wxACC_OK;
    }

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (child_id != wxACC_SELF || !name)
            return wxACC_NOT_IMPLEMENTED;
        *name = m_button->GetLabel();
        if (name->IsEmpty())
            *name = m_button->GetName();
        return name->IsEmpty() ? wxACC_NOT_IMPLEMENTED : wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (child_id != wxACC_SELF || !role)
            return wxACC_NOT_IMPLEMENTED;
        *role = wxROLE_SYSTEM_RADIOBUTTON;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (child_id != wxACC_SELF || !state)
            return wxACC_NOT_IMPLEMENTED;
        *state = 0;
        if (m_button->AcceptsFocusFromKeyboard())
            *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
        if (m_button->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (m_button->IsSelected())
            *state |= wxACC_STATE_SYSTEM_CHECKED;
        if (!m_button->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!m_button->IsShown())
            *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (child_id != wxACC_SELF || !action_name)
            return wxACC_NOT_IMPLEMENTED;
        *action_name = _L("Select");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        if (child_id != wxACC_SELF)
            return wxACC_NOT_IMPLEMENTED;
        return m_button->AccessibilityActivate() ? wxACC_OK : wxACC_FAIL;
    }

private:
    CapsuleButton *m_button;
};
#endif

} // namespace

// MD3 tokens for the capsule chip. Light-mode token values are fed through the
// StateColor dark map (OnPaint) / UpdateDarkUIWin (UpdateStatus) so the same
// values resolve correctly in both themes:
//   fill      normal SurfaceContainerLowest (White) / selected SecondaryContainer
//   text      normal OnSurface (TextPrimary)        / selected Primary (BrandGreen)
//   border    normal OutlineVariant (Grey400)       / hover+selected Primary
CapsuleButton::CapsuleButton(wxWindow *parent, wxWindowID id, const wxString &label, bool selected) : wxPanel(parent, id)
{
    SetBackgroundColour(*wxWHITE);
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    m_hovered  = false;
    m_selected = selected;

    auto sizer = new wxBoxSizer(wxHORIZONTAL);

    tag_on_bmp = create_scaled_bitmap("capsule_tag_on", nullptr, FromDIP(16));
    tag_off_bmp = create_scaled_bitmap("capsule_tag_off", nullptr, FromDIP(16));

    m_btn = new wxBitmapButton(this, wxID_ANY, selected?tag_on_bmp:tag_off_bmp, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    m_btn->SetBackgroundColour(*wxWHITE);
    m_btn->DisableFocusFromKeyboard();

    m_label = new Label(this, label);
    wxWindow::SetLabel(label);

    sizer->AddSpacer(FromDIP(8));
    sizer->Add(m_btn, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, FromDIP(6));
    sizer->AddSpacer(FromDIP(8));
    sizer->Add(m_label, 0, wxALIGN_CENTER);
    sizer->AddSpacer(FromDIP(8));

    SetSizer(sizer);
    Layout();
    Fit();

    auto forward_click_to_parent = [this](wxMouseEvent &) {
        SetFocus();
        SendButtonEvent();
    };

    m_btn->Bind(wxEVT_LEFT_DOWN, forward_click_to_parent);
    m_label->Bind(wxEVT_LEFT_DOWN, forward_click_to_parent);
    this->Bind(wxEVT_LEFT_DOWN, forward_click_to_parent);

    Bind(wxEVT_PAINT, &CapsuleButton::OnPaint, this);
    Bind(wxEVT_ENTER_WINDOW, &CapsuleButton::OnEnterWindow, this);
    Bind(wxEVT_LEAVE_WINDOW, &CapsuleButton::OnLeaveWindow, this);
    Bind(wxEVT_KEY_DOWN, &CapsuleButton::OnKeyDown, this);
    Bind(wxEVT_KEY_UP, &CapsuleButton::OnKeyUp, this);
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &event) {
        Refresh(false);
#if wxUSE_ACCESSIBILITY
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
        event.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
        m_key_pressed = false;
        Refresh(false);
        event.Skip();
    });

#if wxUSE_ACCESSIBILITY
    SetAccessible(new CapsuleButtonAccessible(this));
#endif
    UpdateStatus();
}
void CapsuleButton::OnPaint(wxPaintEvent &event)
{
    wxAutoBufferedPaintDC dc(this);
    wxGraphicsContext    *gc = wxGraphicsContext::Create(dc);

    if (gc) {
        dc.Clear();
        wxRect rect = GetClientRect();
        gc->SetBrush(wxTransparentColour);
        gc->DrawRoundedRectangle(0, 0, rect.width, rect.height, 0);
        wxColour bg_color     = m_selected ? MD3::Light::secondaryContainer : ThemeColor::White;
        wxColour border_color = m_hovered || m_selected || HasFocus() ? ThemeColor::BrandGreen : ThemeColor::Grey400;
        bg_color = StateColor::darkModeColorFor(bg_color);
        border_color = StateColor::darkModeColorFor(border_color);
        gc->SetBrush(wxBrush(bg_color));
        gc->SetPen(wxPen(border_color, HasFocus() ? std::max(FromDIP(2), 1) : 1));
        // Chips are pills (components/selection/Chip): corner radius is half the
        // paint-time height, derived from the inset border rect so it stays a
        // stadium at any DPI/density rather than a fixed 5px corner.
        gc->DrawRoundedRectangle(1, 1, rect.width - 2, rect.height - 2, MD3::Metrics::pill_radius(rect.height - 2));
        delete gc;
    }
}
void CapsuleButton::Select(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    UpdateStatus();
    Refresh();
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
}

void CapsuleButton::OnKeyDown(wxKeyEvent &event)
{
    const int key = event.GetKeyCode();
    if (key != WXK_SPACE && key != WXK_RETURN && key != WXK_NUMPAD_ENTER) {
        event.Skip();
        return;
    }
    if (!IsEnabled() || !IsShown())
        return;
    m_key_pressed = true;
    Refresh(false);
}

void CapsuleButton::OnKeyUp(wxKeyEvent &event)
{
    const int key = event.GetKeyCode();
    if (key != WXK_SPACE && key != WXK_RETURN && key != WXK_NUMPAD_ENTER) {
        event.Skip();
        return;
    }
    const bool activate = m_key_pressed && IsEnabled() && IsShown();
    m_key_pressed = false;
    Refresh(false);
    if (activate)
        SendButtonEvent();
}

void CapsuleButton::SendButtonEvent()
{
    wxCommandEvent click_event(wxEVT_BUTTON, GetId());
    click_event.SetEventObject(this);
    ProcessEvent(click_event);
}

bool CapsuleButton::AccessibilityActivate()
{
    if (!IsEnabled() || !IsShown())
        return false;
    SendButtonEvent();
    return true;
}

#ifdef __WIN32__
WXLRESULT CapsuleButton::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if (nMsg == WM_GETDLGCODE)
        return DLGC_WANTMESSAGE;
    return wxPanel::MSWWindowProc(nMsg, wParam, lParam);
}
#endif

void CapsuleButton::OnEnterWindow(wxMouseEvent &event)
{
    if (!m_hovered) {
        m_hovered = true;
        UpdateStatus();
        Refresh();
    }
    event.Skip();
}

void CapsuleButton::OnLeaveWindow(wxMouseEvent &event)
{
    if (m_hovered) {
        wxPoint pos = this->ScreenToClient(wxGetMousePosition());
        if (this->GetClientRect().Contains(pos)) return;
        m_hovered = false;
        UpdateStatus();
        Refresh();
    }
    event.Skip();
}

void CapsuleButton::UpdateStatus()
{
    if (m_selected) {
        m_btn->SetBitmap(tag_on_bmp);
        m_label->SetForegroundColour(ThemeColor::BrandGreen);
        m_label->SetBackgroundColour(MD3::Light::secondaryContainer);
        m_btn->SetBackgroundColour(MD3::Light::secondaryContainer);
    } else {
        m_btn->SetBitmap(tag_off_bmp);
        m_label->SetForegroundColour(ThemeColor::TextPrimary);
        m_label->SetBackgroundColour(ThemeColor::White);
        m_btn->SetBackgroundColour(ThemeColor::White);
    }

    GUI::wxGetApp().UpdateDarkUIWin(this);
}
}} // namespace Slic3r::GUI
