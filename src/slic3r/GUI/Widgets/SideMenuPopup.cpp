#include "SideMenuPopup.hpp"
#include "Label.hpp"
#include "StateColor.hpp"

#include <wx/display.h>
#include <wx/dcgraph.h>
#include "../GUI_App.hpp"

// MD3 popover geometry for the Slice / Print option menus. The radius matches
// the floating card DropDown.cpp draws for the ComboBox popup, and the inset
// exists so the surface and its frame stay visible: the row stack used to run
// edge to edge, which left nothing of the container to see.
static constexpr int SIDE_POPUP_RADIUS  = 18;
static constexpr int SIDE_POPUP_PADDING = 8;

wxBEGIN_EVENT_TABLE(SidePopup,PopupWindow)
EVT_PAINT(SidePopup::paintEvent)
wxEND_EVENT_TABLE()

SidePopup::SidePopup(wxWindow* parent)
    :PopupWindow(parent,
    wxBORDER_NONE |
    wxPU_CONTAINS_CONTROLS)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
    // paintEvent covers the whole client area, so suppress the system erase and
    // keep the fill in one place. Both role values are stored raw (light) and
    // remapped per paint -- see the note on the members.
    SetBackgroundColour(MD3::Light::sc);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    border_color = MD3::Light::outlineVariant;
    radius       = FromDIP(SIDE_POPUP_RADIUS);
    Bind(wxEVT_CHAR_HOOK, &SidePopup::keyDown, this);
}

SidePopup::~SidePopup()
{
    ;
}

void SidePopup::OnDismiss()
{
    Slic3r::GUI::wxGetApp().set_side_menu_popup_status(false);
    PopupWindow::OnDismiss();
    restoreInvokerFocus();
}

bool SidePopup::ProcessLeftDown(wxMouseEvent& event)
{
    return PopupWindow::ProcessLeftDown(event);
}
bool SidePopup::Show( bool show )
{
    return PopupWindow::Show(show);
}

void SidePopup::Popup(wxWindow* focus)
{
    // The argument remains the positioning anchor for compatibility. The actual
    // invoker is the focused control (the narrow options segment in MainFrame),
    // because pointer-down and keyboard activation both focus it before opening.
    wxWindow* focused = wxWindow::FindFocus();
    for (wxWindow* owner = focused; owner; owner = owner->GetParent()) {
        if (owner == this) {
            focused = nullptr;
            break;
        }
    }
    invoker = focused ? focused : focus;
    Create();
    auto drect = wxDisplay(GetParent()).GetGeometry();
    int screenwidth = drect.x + drect.width;
    //int screenwidth = wxSystemSettings::GetMetric(wxSYS_SCREEN_X,NULL);

    // Create() has already sized the frame to the widest row plus the container
    // inset, so the off-screen test has to measure the frame, not the row --
    // otherwise the menu overhangs the right edge by the padding.
    const int popup_width = GetSize().x;

    if (focus) {
        wxPoint pos = focus->ClientToScreen(wxPoint(0, -6));

#ifdef __APPLE__
         pos.x = pos.x - FromDIP(20);
#endif // __APPLE__

        if (pos.x + popup_width > screenwidth)
            Position({pos.x - (pos.x + popup_width - screenwidth),pos.y}, {0, focus->GetSize().y + 12});
        else
            Position(pos, {0, focus->GetSize().y + 12});
    }
    Slic3r::GUI::wxGetApp().set_side_menu_popup_status(true);
    PopupWindow::Popup();
    focusBoundaryButton(true);
}

void SidePopup::Create()
{
    wxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    int max_width = 0;
    int height = 0;
    for (auto btn : btn_list)
    {
        max_width = std::max(btn->GetMinSize().x, max_width);
    }

    // Re-derived on every Popup() rather than only in the ctor, so a monitor or
    // DPI change between two openings of the menu is picked up.
    radius = FromDIP(SIDE_POPUP_RADIUS);
    const int padding = FromDIP(SIDE_POPUP_PADDING);

    sizer->AddSpacer(padding);
    for (auto btn : btn_list)
    {
        wxSize size = btn->GetMinSize();
        height += size.y;
        size.x = max_width;
        btn->SetMinSize(size);
        btn->SetSize(size);
        sizer->Add(btn, 0, wxLEFT | wxRIGHT, padding);
    }
    sizer->AddSpacer(padding);

    SetSize(wxSize(max_width + 2 * padding, height + 2 * padding));

    SetSizer(sizer, true);

    Layout();
    Refresh();
}

void SidePopup::paintEvent(wxPaintEvent& evt)
{
    wxPaintDC dc(this);
#ifdef __WXMSW__
    // wxGCDC so the rounded frame gets antialiased corners; plain GDI
    // stair-steps them (this is why SideButton paints through one too).
    wxGCDC dc2(dc);
#else
    wxDC & dc2(dc);
#endif

    const wxSize   size    = GetSize();
    const wxColour surface = StateColor::darkModeColorFor(GetBackgroundColour());

    // The popup HWND has square corners, so fill the whole client area with the
    // surface tone first: the four triangles left outside the rounded frame are
    // then clean rather than uninitialised. Same approach as DropDown::render().
    dc2.SetPen(*wxTRANSPARENT_PEN);
    dc2.SetBrush(wxBrush(surface));
    dc2.DrawRectangle(0, 0, size.x, size.y);

    // Kit floating surface: SurfaceContainer fill inside a 1px OutlineVariant
    // frame, which is what separates the menu from the 3D viewport behind it.
    // Inset by a pixel so the stroke lands inside the window instead of being
    // clipped in half along the right and bottom edges.
    dc2.SetPen(wxPen(StateColor::darkModeColorFor(border_color)));
    dc2.SetBrush(wxBrush(surface));
    dc2.DrawRoundedRectangle(0, 0, size.x - 1, size.y - 1, radius);
}

void SidePopup::append_button(SideButton* btn)
{
    btn_list.push_back(btn);
}

void SidePopup::keyDown(wxKeyEvent& event)
{
    auto* current = dynamic_cast<SideButton*>(wxWindow::FindFocus());
    switch (event.GetKeyCode()) {
    case WXK_UP: focusRelativeButton(current, -1); break;
    case WXK_DOWN: focusRelativeButton(current, 1); break;
    case WXK_HOME: focusBoundaryButton(true); break;
    case WXK_END: focusBoundaryButton(false); break;
    case WXK_ESCAPE:
        Dismiss();
        break;
    default: event.Skip(); break;
    }
}

void SidePopup::focusBoundaryButton(bool first)
{
    if (first) {
        for (SideButton* button : btn_list) {
            if (button && button->IsShown() && button->IsEnabled()) {
                button->SetFocus();
                return;
            }
        }
    } else {
        for (auto it = btn_list.rbegin(); it != btn_list.rend(); ++it) {
            SideButton* button = *it;
            if (button && button->IsShown() && button->IsEnabled()) {
                button->SetFocus();
                return;
            }
        }
    }
}

void SidePopup::focusRelativeButton(SideButton* current, int direction)
{
    if (btn_list.empty())
        return;

    auto current_it = std::find(btn_list.begin(), btn_list.end(), current);
    int  index      = current_it == btn_list.end() ? (direction > 0 ? -1 : 0)
                                                    : static_cast<int>(current_it - btn_list.begin());
    for (size_t count = 0; count < btn_list.size(); ++count) {
        index = (index + direction + static_cast<int>(btn_list.size())) %
                static_cast<int>(btn_list.size());
        SideButton* candidate = btn_list[index];
        if (candidate && candidate->IsShown() && candidate->IsEnabled()) {
            candidate->SetFocus();
            return;
        }
    }
}

void SidePopup::restoreInvokerFocus()
{
    if (restoring_focus)
        return;
    restoring_focus = true;
    if (invoker && invoker->IsShown() && invoker->IsEnabled())
        invoker->SetFocus();
    restoring_focus = false;
}
