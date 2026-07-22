#include "RadioBox.hpp"

#include "../wxExtensions.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include <wx/dcmemory.h>
#include <wx/graphics.h>

#include <algorithm>
#include <cmath>

namespace Slic3r {
namespace GUI {

namespace {
// 18px logical control, matching the legacy radio footprint.
constexpr int kRadioPx = 18;

inline wxColour withAlpha(const wxColour &c, int a)
{
    return wxColour(c.Red(), c.Green(), c.Blue(), a);
}
} // namespace

RadioBox::RadioBox(wxWindow *parent)
    : wxBitmapToggleButton(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    // SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
    if (parent) SetBackgroundColour(parent->GetBackgroundColour());
    // Bind(wxEVT_TOGGLEBUTTON, [this](auto& e) { update(); e.Skip(); });
    SetSize(wxSize(deviceSide(), deviceSide()));
    SetMinSize(wxSize(deviceSide(), deviceSide()));
    update();
}

void RadioBox::SetValue(bool value)
{
    wxBitmapToggleButton::SetValue(value);
    update();
}

bool RadioBox::GetValue()
{
    return wxBitmapToggleButton::GetValue();
}

void RadioBox::SetColorScheme(MD3::ColorScheme scheme)
{
    if (m_scheme == scheme)
        return;
    m_scheme = scheme;
    update();
}

void RadioBox::Rescale()
{
    SetSize(wxSize(deviceSide(), deviceSide()));
    SetMinSize(wxSize(deviceSide(), deviceSide()));
    update();
}

int RadioBox::deviceSide() const
{
    double scale = GetDPIScaleFactor();
    if (scale <= 0.0)
        scale = 1.0;
    return std::max(1, static_cast<int>(std::ceil(kRadioPx * scale)));
}

wxBitmap RadioBox::renderBitmap(bool selected, bool disabled) const
{
    double scale = GetDPIScaleFactor();
    if (scale <= 0.0)
        scale = 1.0;
    const int dev = std::max(1, static_cast<int>(std::ceil(kRadioPx * scale)));

    const wxColour primary  = StateColor::semantic(MD3::Role::Primary, m_scheme);
    const wxColour onSurfVar = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const wxColour colour = disabled ? withAlpha(onSurfVar, 97)
                                     : (selected ? primary : onSurfVar);

    // Draw into a square canvas so the control keeps its 18px footprint and the
    // glyph stays centered regardless of the font's own metrics.
    wxBitmap bmp(dev, dev);
#if defined(__WXMSW__) || defined(__WXOSX__)
    bmp.UseAlpha();
#endif
    {
        wxMemoryDC mdc(bmp);
        mdc.SetBackground(*wxTRANSPARENT_BRUSH);
        mdc.Clear();
        wxGraphicsContext *gc = wxGraphicsContext::Create(mdc);
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->Scale(scale, scale); // logical 0..kRadioPx coordinates

            bool drawn = false;
            if (MaterialIcon::available()) {
                const uint32_t cp = selected ? MaterialIcon::RadioButtonChecked
                                             : MaterialIcon::RadioButtonUnchecked;
                gc->SetFont(MaterialIcon::font(kRadioPx), colour);
                double tw = 0, th = 0;
                gc->GetTextExtent(MaterialIcon::text(cp), &tw, &th);
                gc->DrawText(MaterialIcon::text(cp), (kRadioPx - tw) / 2, (kRadioPx - th) / 2);
                drawn = true;
            }
            if (!drawn) {
                // Font missing: hand-draw the ring (+ inner dot when selected).
                const double c = kRadioPx / 2.0;
                const double rOuter = c - 2.0;
                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                gc->SetPen(wxPen(colour, 2));
                gc->DrawEllipse(c - rOuter, c - rOuter, rOuter * 2, rOuter * 2);
                if (selected) {
                    const double rDot = rOuter * 0.5;
                    gc->SetPen(wxPen(colour));
                    gc->SetBrush(wxBrush(colour));
                    gc->DrawEllipse(c - rDot, c - rDot, rDot * 2, rDot * 2);
                }
            }
            delete gc;
        }
        mdc.SelectObject(wxNullBitmap);
    }
    return bmp;
}

void RadioBox::update()
{
    const bool selected = GetValue();
    // Provide both states explicitly so wx shows our dimmed render when disabled
    // instead of auto-greying the enabled bitmap on top of it.
    SetBitmap(renderBitmap(selected, false));
    SetBitmapDisabled(renderBitmap(selected, true));
}

}
}
