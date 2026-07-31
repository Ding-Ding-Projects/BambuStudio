#include <wx/sizer.h>
#include <wx/dcclient.h>

#include <algorithm>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

#include "LinkLabel.hpp"
#include "../I18N.hpp"
#include "StateColor.hpp"

namespace {
#if wxUSE_ACCESSIBILITY
class LinkLabelAccessible final : public wxWindowAccessible
{
public:
    explicit LinkLabelAccessible(LinkLabel *link)
        : wxWindowAccessible(link), m_link(link)
    {
    }

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (child_id != wxACC_SELF || name == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        const wxString configured_name = m_link->GetName();
        *name = !configured_name.IsEmpty() && configured_name != wxASCII_STR(wxPanelNameStr)
                    ? configured_name
                    : m_link->getLabel()->GetLabel();
        return wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (child_id != wxACC_SELF || role == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *role = wxROLE_SYSTEM_LINK;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (child_id != wxACC_SELF || state == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *state = 0;
        if (m_link->AcceptsFocusFromKeyboard())
            *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
        if (m_link->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (!m_link->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!m_link->IsShown())
            *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (child_id != wxACC_SELF || action_name == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *action_name = _L("Open");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        if (child_id != wxACC_SELF)
            return wxACC_NOT_IMPLEMENTED;
        m_link->AccessibilityActivate();
        return wxACC_OK;
    }

private:
    LinkLabel *m_link;
};
#endif
} // namespace

wxDEFINE_EVENT(EVT_LINK_LABEL_LEFT_DOWN, wxCommandEvent);

LinkLabel::LinkLabel(wxWindow *parent, wxString const &text, std::string url, long style, wxSize size)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, size, style)
{
    m_url = wxString(url);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    m_txt = new Label(this, text);
    m_underline = new wxPanel(this);

    Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_HAND); });
    m_txt->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_HAND); });
    m_underline->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_ARROW); });

    Bind(wxEVT_LEFT_DOWN, &LinkLabel::link, this);
    m_txt->Bind(wxEVT_LEFT_DOWN, &LinkLabel::link, this);
    m_underline->Bind(wxEVT_LEFT_DOWN, &LinkLabel::link, this);
    Bind(wxEVT_KEY_DOWN, &LinkLabel::keyDown, this);
    Bind(wxEVT_SET_FOCUS, &LinkLabel::focusChanged, this);
    Bind(wxEVT_KILL_FOCUS, &LinkLabel::focusChanged, this);
    Bind(wxEVT_PAINT, &LinkLabel::paintFocus, this);
    m_txt->SetCanFocus(false);
    m_underline->SetCanFocus(false);

    m_underline->SetMinSize(wxSize(-1, FromDIP(1)));
    m_underline->SetMaxSize(wxSize(-1, FromDIP(1)));

    SeLinkLabelFColour(ThemeColor::Link);
    SeLinkLabelBColour(ThemeColor::White);

    const int focus_inset = FromDIP(2);
    // Reserve a narrow parent-owned gutter: child windows otherwise cover every
    // pixel of this wrapper and would hide a focus rectangle painted by it.
    sizer->Add(m_txt, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, focus_inset);
    sizer->Add(m_underline, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, focus_inset);
    SetSizer(sizer);
    Layout();
    Fit();

#if wxUSE_ACCESSIBILITY
    SetAccessible(new LinkLabelAccessible(this));
#endif
}

void LinkLabel::setLinkUrl(wxString url)
{
    m_url = url;
}

void LinkLabel::setLabel(wxString label)
{
    if (m_txt->GetLabel() == label)
        return;
    m_txt->SetLabel(label);
    Layout();
    Fit();
    Refresh();
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
}

void LinkLabel::SetName(const wxString& name)
{
    if (name == wxWindow::GetName())
        return;
    wxWindow::SetName(name);
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
}

void LinkLabel::link(wxMouseEvent &evt)
{
    if (!HasFocus())
        SetFocus();
    activate();
    evt.Skip();
}

void LinkLabel::activate()
{
    if (!IsEnabled() || !IsShown())
        return;
    if (!m_url.IsEmpty())
        wxLaunchDefaultBrowser(m_url);

    wxCommandEvent event(EVT_LINK_LABEL_LEFT_DOWN, GetId());
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event);
}

void LinkLabel::keyDown(wxKeyEvent &event)
{
    const int key_code = event.GetKeyCode();
    if (key_code == WXK_RETURN || key_code == WXK_NUMPAD_ENTER || key_code == WXK_SPACE) {
        activate();
        return;
    }
    if (key_code == WXK_TAB || key_code == WXK_LEFT || key_code == WXK_RIGHT ||
        key_code == WXK_UP || key_code == WXK_DOWN) {
        HandleAsNavigationKey(event);
        return;
    }
    event.Skip();
}

void LinkLabel::focusChanged(wxFocusEvent &event)
{
    Refresh(false);
#if wxUSE_ACCESSIBILITY
    if (event.GetEventType() == wxEVT_SET_FOCUS)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    event.Skip();
}

void LinkLabel::paintFocus(wxPaintEvent &event)
{
    wxPaintDC dc(this);
    if (HasFocus() && IsEnabled()) {
        const int inset = std::max(FromDIP(1), 1);
        wxRect focus_rect(inset, inset, std::max(0, GetClientSize().x - inset * 2),
                          std::max(0, GetClientSize().y - inset * 2));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Primary), std::max(FromDIP(2), 1)));
        dc.DrawRoundedRectangle(focus_rect, FromDIP(3));
    }
    event.Skip();
}

bool LinkLabel::AcceptsFocus() const
{
    return IsEnabled() && IsShown();
}

bool LinkLabel::AcceptsFocusFromKeyboard() const
{
    return AcceptsFocus();
}

void LinkLabel::AccessibilityActivate()
{
    activate();
}

bool LinkLabel::SeLinkLabelFColour(const wxColour &colour)
{
    SetForegroundColour(colour);
    m_txt->SetForegroundColour(colour);
    m_underline->SetBackgroundColour(colour);
    return true;
}

bool LinkLabel::SeLinkLabelBColour(const wxColour &colour)
{
    SetBackgroundColour(colour);
    m_txt->SetBackgroundColour(colour);
    return true;
}
