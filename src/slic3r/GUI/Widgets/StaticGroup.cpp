#include "StaticGroup.hpp"
#include "Button.hpp"
#include "Label.hpp"
#include "StateColor.hpp"
#include "slic3r/GUI/I18N.hpp"

StaticGroup::StaticGroup(wxWindow *parent, wxWindowID id)
#ifdef __WXOSX__
    : wxStaticBox(parent, id, ".")
#else
    : wxStaticBox(parent, id, "")
#endif
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    borderColor_ = StateColor::semantic(MD3::Role::OutlineVariant);
#ifdef __WXMSW__
    Bind(wxEVT_PAINT, &StaticGroup::OnPaint, this);
#endif
}

bool StaticGroup::Show(bool show)
{
    bool ret = wxStaticBox::Show(show);
    return ret;
}

void StaticGroup_layoutBadge(void * group, void * badge);

void StaticGroup::ShowBadge(bool show)
{
#ifdef __WXMSW__
    if (show && badge.name() != "badge") {
        badge = ScalableBitmap(this, "badge", 18);
        Refresh();
    } else if (!show && !badge.name().empty()) {
        badge = ScalableBitmap{};
        Refresh();
    }
#endif
#ifdef __WXOSX__
    if (show && badge == nullptr) {
        badge = new ScalableButton(this, wxID_ANY, "badge", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, false, 18);
        badge->SetSize(badge->GetBestSize());
        badge->SetBackgroundColour(ThemeColor::Grey250);
        StaticGroup_layoutBadge(GetHandle(), badge->GetHandle());
    }
    if (badge && badge->IsShown() != show)
        badge->Show(show);
#endif
}

void StaticGroup::SetBorderColor(const wxColour &color)
{
    borderColor_ = color;
}

#ifdef __WXMSW__
void StaticGroup::OnPaint(wxPaintEvent &evt)
{
    wxStaticBox::OnPaint(evt);
    if (badge.bmp().IsOk()) {
        auto s = badge.bmp().GetScaledSize();
        wxPaintDC dc(this);
        dc.DrawBitmap(badge.bmp(), GetSize().x - s.x, 8);
    }
}

void StaticGroup::PaintForeground(wxDC &dc, const struct tagRECT &rc)
{
    wxStaticBox::PaintForeground(dc, rc);
    auto mdc = dynamic_cast<wxMemoryDC *>(&dc);
    auto image = mdc->GetSelectedBitmap().ConvertToImage();
    // Found border coords
    int top = 0;
    int left = 0;
    int right = rc.right - 1;
    int bottom = rc.bottom - 1;
    auto blue  = GetBackgroundColour().Blue();
    while (image.GetBlue(0, top) == blue && top < bottom) ++top;
    while (image.GetBlue(left, top) != blue && left < right) ++left; // --left; // fix start
    while (image.GetBlue(right, top) != blue && right > 0) --right;
    ++right;
    while (image.GetBlue(0, bottom) == blue && bottom > 0) --bottom;
    // Draw border with foreground color
    wxPoint polygon[] = { {left, top}, {0, top}, {0, bottom}, {rc.right - 1, bottom}, {rc.right - 1, top}, {right, top} };
    dc.SetPen(wxPen(borderColor_, 1));
    for (int i = 1; i < 6; ++i) {
        if (i == 4) // fix bottom right corner
            ++polygon[i - 1].y;
        dc.DrawLine(polygon[i - 1], polygon[i]);
    }
}

#endif

MD3GroupBox::MD3GroupBox(wxWindow *parent, const wxString &label) : wxStaticBox(parent, wxID_ANY, label)
{
    // The title font sets the height of the band the title is drawn in, so it is
    // chosen before the box is laid out. The box shares its parent's surface.
    SetFont(::Label::Head_14);
    SetBackgroundColour(parent->GetBackgroundColour());
    SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
#ifdef __WXMSW__
    Bind(wxEVT_PAINT, &MD3GroupBox::OnPaint, this);
#endif
}

#ifdef __WXMSW__
void MD3GroupBox::OnPaint(wxPaintEvent &evt)
{
    // The base composes the box in a memory DC through PaintForeground() below and
    // copies only the border band to the screen, so the controls inside are never
    // painted over.
    wxStaticBox::OnPaint(evt);
}

void MD3GroupBox::PaintForeground(wxDC &dc, const struct tagRECT &rc)
{
    const wxString label = GetLabel();
    dc.SetFont(GetFont());
    const wxSize text = label.IsEmpty() ? wxSize(0, 0) : dc.GetTextExtent(label);
    // The outline runs through the middle of the title, like the native frame, and
    // its corners stay inside the border band the base copies.
    const int top = label.IsEmpty() ? FromDIP(2) : text.GetHeight() / 2;
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), 1));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRoundedRectangle(0, top, rc.right, rc.bottom - top, FromDIP(4));
    if (!label.IsEmpty()) {
        const int x = FromDIP(10);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(GetBackgroundColour()));
        dc.DrawRectangle(x - FromDIP(4), 0, text.GetWidth() + FromDIP(8), text.GetHeight());
        dc.SetTextForeground(StateColor::semantic(IsEnabled() ? MD3::Role::OnSurface : MD3::Role::OnSurfaceVariant));
        dc.DrawText(label, x, 0);
    }
}
#endif
