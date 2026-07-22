#include "CheckBox.hpp"

#include "../wxExtensions.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include <wx/dcmemory.h>
#include <wx/graphics.h>

#include <algorithm>
#include <cmath>

namespace {
// 20px logical box per selection/Checkbox.prompt.md.
constexpr int kCheckBoxPx = 20;

inline wxColour withAlpha(const wxColour &c, int a)
{
    return wxColour(c.Red(), c.Green(), c.Blue(), a);
}
} // namespace

CheckBox::CheckBox(wxWindow *parent, int id)
    : wxBitmapToggleButton(parent, id, wxNullBitmap, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
	//SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
	if (parent)
		SetBackgroundColour(parent->GetBackgroundColour());
	Bind(wxEVT_TOGGLEBUTTON, [this](auto& e) { m_half_checked = false; update(); e.Skip(); });
#ifdef __WXOSX__ // State not fully implement on MacOS
    Bind(wxEVT_SET_FOCUS, &CheckBox::updateBitmap, this);
    Bind(wxEVT_KILL_FOCUS, &CheckBox::updateBitmap, this);
    Bind(wxEVT_ENTER_WINDOW, &CheckBox::updateBitmap, this);
    Bind(wxEVT_LEAVE_WINDOW, &CheckBox::updateBitmap, this);
#endif
	SetSize(wxSize(deviceSide(), deviceSide()));
	SetMinSize(wxSize(deviceSide(), deviceSide()));
	update();
}

void CheckBox::SetValue(bool value)
{
    if (wxBitmapToggleButton::GetValue() != value) {
        wxBitmapToggleButton::SetValue(value);
        update();
    }
}

void CheckBox::SetHalfChecked(bool value)
{
	m_half_checked = value;
	update();
}

void CheckBox::SetColorScheme(MD3::ColorScheme scheme)
{
    if (m_scheme == scheme)
        return;
    m_scheme = scheme;
    update();
}

void CheckBox::Rescale()
{
    SetSize(wxSize(deviceSide(), deviceSide()));
    SetMinSize(wxSize(deviceSide(), deviceSide()));
	update();
}

int CheckBox::deviceSide() const
{
    double scale = GetDPIScaleFactor();
    if (scale <= 0.0)
        scale = 1.0;
    return std::max(1, static_cast<int>(std::ceil(kCheckBoxPx * scale)));
}

wxBitmap CheckBox::renderBitmap(bool checked, bool half, bool disabled) const
{
    double scale = GetDPIScaleFactor();
    if (scale <= 0.0)
        scale = 1.0;
    const int dev = std::max(1, static_cast<int>(std::ceil(kCheckBoxPx * scale)));

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
            gc->Scale(scale, scale); // draw in logical 0..kCheckBoxPx coordinates

            const wxColour primary    = StateColor::semantic(MD3::Role::Primary, m_scheme);
            const wxColour onPrimary   = StateColor::semantic(MD3::Role::OnPrimary, m_scheme);
            const wxColour onSurfVar    = StateColor::semantic(MD3::Role::OnSurfaceVariant);
            const wxColour onSurface    = StateColor::semantic(MD3::Role::OnSurface);
            const wxColour surface      = StateColor::semantic(MD3::Role::Surface);

            const double inset = 1.0;
            const double side  = kCheckBoxPx - 2 * inset; // 18px box, 1px breathing room
            const double radius = 2.5;

            if (!checked && !half) {
                // Unchecked: 2px rounded-square outline in OnSurfaceVariant.
                const wxColour border = disabled ? withAlpha(onSurfVar, 97) : onSurfVar;
                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                gc->SetPen(wxPen(border, 2));
                gc->DrawRoundedRectangle(inset + 1, inset + 1, side - 2, side - 2, radius);
            } else {
                // Checked / indeterminate: filled Primary square.
                const wxColour fill = disabled ? withAlpha(onSurface, 97) : primary;
                gc->SetPen(wxPen(fill));
                gc->SetBrush(wxBrush(fill));
                gc->DrawRoundedRectangle(inset, inset, side, side, radius);

                const wxColour fg = disabled ? surface : onPrimary;
                if (half) {
                    // Indeterminate: a centered horizontal bar.
                    const double bw = 10.0, bh = 2.0;
                    gc->SetPen(wxPen(fg));
                    gc->SetBrush(wxBrush(fg));
                    gc->DrawRoundedRectangle((kCheckBoxPx - bw) / 2, (kCheckBoxPx - bh) / 2, bw, bh, bh / 2);
                } else {
                    bool drawn = false;
                    if (MaterialIcon::available()) {
                        gc->SetFont(MaterialIcon::font(kCheckBoxPx), fg);
                        double tw = 0, th = 0;
                        gc->GetTextExtent(MaterialIcon::text(MaterialIcon::Check), &tw, &th);
                        gc->DrawText(MaterialIcon::text(MaterialIcon::Check),
                                     (kCheckBoxPx - tw) / 2, (kCheckBoxPx - th) / 2);
                        drawn = true;
                    }
                    if (!drawn) {
                        // Font missing: stroke a checkmark polyline as a fallback.
                        gc->SetPen(wxPen(fg, 2));
                        wxGraphicsPath path = gc->CreatePath();
                        path.MoveToPoint(5.5, 10.5);
                        path.AddLineToPoint(8.5, 13.5);
                        path.AddLineToPoint(14.5, 6.5);
                        gc->StrokePath(path);
                    }
                }
            }

            delete gc; // flush before the bitmap is read
        }
        mdc.SelectObject(wxNullBitmap);
    }
    return bmp;
}

void CheckBox::update()
{
	const bool v = GetValue();
	const bool h = m_half_checked;
	SetBitmapLabel(renderBitmap(v, h, false));
    SetBitmapDisabled(renderBitmap(v, h, true));
#ifdef __WXMSW__
    SetBitmapFocus(renderBitmap(v, h, false));
#endif
    SetBitmapCurrent(renderBitmap(v, h, false));
#ifdef __WXOSX__
    wxCommandEvent e(wxEVT_UPDATE_UI);
    updateBitmap(e);
#endif
}

#ifdef __WXMSW__

CheckBox::State CheckBox::GetNormalState() const { return State_Normal; }

#endif


#ifdef __WXOSX__

bool CheckBox::Enable(bool enable)
{
    bool result = wxBitmapToggleButton::Enable(enable);
    if (result) {
        m_disable = !enable;
        wxCommandEvent e(wxEVT_ACTIVATE);
        updateBitmap(e);
    }
    return result;
}

wxBitmap CheckBox::DoGetBitmap(State which) const
{
    if (m_disable) {
        return wxBitmapToggleButton::DoGetBitmap(State_Disabled);
    }
    if (m_focus) {
        return wxBitmapToggleButton::DoGetBitmap(State_Current);
    }
    return wxBitmapToggleButton::DoGetBitmap(which);
}

void CheckBox::updateBitmap(wxEvent & evt)
{
    evt.Skip();
    if (evt.GetEventType() == wxEVT_ENTER_WINDOW) {
        m_hover = true;
    } else if (evt.GetEventType() == wxEVT_LEAVE_WINDOW) {
        m_hover = false;
    } else {
        if (evt.GetEventType() == wxEVT_SET_FOCUS) {
            m_focus = true;
        } else if (evt.GetEventType() == wxEVT_KILL_FOCUS) {
            m_focus = false;
        }
        wxMouseEvent e;
        if (m_hover)
            OnEnterWindow(e);
        else
            OnLeaveWindow(e);
    }
}

#endif
