#include "StaticBox.hpp"
#include "StateColor.hpp"
#include "../GUI.hpp"
#include <algorithm>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>

// Interactive-card hover animation: step the resting -> hover border promotion
// toward its target every HOVER_TICK_MS so the OutlineVariant -> Primary swap
// eases over HOVER_ANIM_MS (~0.15s, matching the kit Card transition). Once the
// promotion is fully in, the timer drops to a cheap HOVER_WATCH_MS poll that
// notices the pointer leaving via a child window (which does not re-fire
// LEAVE on the parent card) so the border can never stick promoted.
static const int HOVER_TICK_MS  = 15;
static const int HOVER_ANIM_MS  = 150;
static const int HOVER_WATCH_MS = 100;

BEGIN_EVENT_TABLE(StaticBox, wxWindow)

// catch paint events
//EVT_ERASE_BACKGROUND(StaticBox::eraseEvent)
EVT_PAINT(StaticBox::paintEvent)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

StaticBox::StaticBox()
    : state_handler(this)
    , radius(MD3::Metrics::compact.radius)
{
    // MD3 default card border: Outline at rest, the dimmer OutlineVariant when
    // disabled (replacing the legacy Grey400/Grey300 literals). Both are stored
    // as their light role values -- keys in StateColor.cpp's gDarkColors table --
    // so colorForStates() live-remaps them on a runtime dark-mode toggle.
    border_color = StateColor(
        std::make_pair(MD3::Light::outlineVariant, (int) StateColor::Disabled),
        std::make_pair(MD3::Light::outline, (int) StateColor::Normal));
}

StaticBox::StaticBox(wxWindow* parent,
                   wxWindowID      id,
                   const wxPoint & pos,
                   const wxSize &  size, long style)
    : StaticBox()
{
    Create(parent, id, pos, size, style);
}

bool StaticBox::Create(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
{
    if (style & wxBORDER_NONE)
        border_width = 0;
    wxWindow::Create(parent, id, pos, size, style);
    RescaleDefaultCornerRadius();
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
        RescaleDefaultCornerRadius();
        Refresh(false);
        event.Skip();
    });
    state_handler.attach({&border_color, &background_color, &background_color2});
    state_handler.update_binds();
    SetBackgroundColour(GetParentBackgroundColor(parent));
    return true;
}

void StaticBox::SetCornerRadius(double radius)
{
    m_uses_default_radius = false;
    this->radius = radius;
    Refresh();
}

void StaticBox::SetDefaultCornerRadius(double radius_dip)
{
    m_default_radius_dip = radius_dip;
    if (m_uses_default_radius)
        radius = radius_dip;
}

void StaticBox::RescaleDefaultCornerRadius()
{
    if (m_uses_default_radius)
        radius = FromDIP(m_default_radius_dip);
}

void StaticBox::SetDensity(Density density)
{
    SetDefaultCornerRadius(density == Density::Comfortable ? MD3::Metrics::comfortable.radius
                                                           : MD3::Metrics::compact.radius);
    RescaleDefaultCornerRadius();
    Refresh();
}

void StaticBox::SetSchemeAccent(MD3::ColorScheme scheme)
{
    if (m_scheme == scheme)
        return;
    m_scheme = scheme;
    if (m_interactive)
        Refresh();
}

void StaticBox::SetInteractive(bool interactive)
{
    if (m_interactive == interactive)
        return;
    m_interactive = interactive;

    if (interactive) {
        // A visible border is required for the hover promotion to read; seed the
        // resting OutlineVariant so the non-hovered card matches the kit Card.
        if (border_width == 0)
            border_width = 1;
        border_color = StateColor(StateColor::semantic(m_rest_border_role, m_scheme));
        state_handler.update_binds();

        m_hover_timer.SetOwner(this);
        Bind(wxEVT_ENTER_WINDOW, &StaticBox::onHoverEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &StaticBox::onHoverLeave, this);
        Bind(wxEVT_TIMER, &StaticBox::onHoverTick, this);
        SetCursor(wxCursor(wxCURSOR_HAND));
    } else {
        m_hover_timer.Stop();
        Unbind(wxEVT_ENTER_WINDOW, &StaticBox::onHoverEnter, this);
        Unbind(wxEVT_LEAVE_WINDOW, &StaticBox::onHoverLeave, this);
        Unbind(wxEVT_TIMER, &StaticBox::onHoverTick, this);
        m_hover_active = false;
        m_hover_anim   = 0.0;
        SetCursor(wxCursor(wxCURSOR_ARROW));
    }
    Refresh();
}

void StaticBox::onHoverEnter(wxMouseEvent &evt)
{
    evt.Skip();
    if (!m_interactive || !IsEnabled())
        return;
    m_hover_active = true;
    if (!m_hover_timer.IsRunning() || m_hover_timer.GetInterval() != HOVER_TICK_MS)
        m_hover_timer.Start(HOVER_TICK_MS);
}

void StaticBox::onHoverLeave(wxMouseEvent &evt)
{
    evt.Skip();
    if (!m_interactive)
        return;
    // Moving onto a child window still counts as hovering the card (CSS :hover
    // semantics); only a pointer that has left the card's bounds demotes it.
    if (GetScreenRect().Contains(wxGetMousePosition()))
        return;
    m_hover_active = false;
    if (!m_hover_timer.IsRunning() || m_hover_timer.GetInterval() != HOVER_TICK_MS)
        m_hover_timer.Start(HOVER_TICK_MS);
}

void StaticBox::onHoverTick(wxTimerEvent &)
{
    // While fully promoted, poll containment so a pointer that left via a child
    // (no parent LEAVE) still demotes the border.
    if (m_hover_active && m_hover_anim >= 1.0 && !GetScreenRect().Contains(wxGetMousePosition()))
        m_hover_active = false;

    const double target = m_hover_active ? 1.0 : 0.0;
    const double step   = static_cast<double>(HOVER_TICK_MS) / static_cast<double>(HOVER_ANIM_MS);
    if (m_hover_anim < target)
        m_hover_anim = std::min(target, m_hover_anim + step);
    else if (m_hover_anim > target)
        m_hover_anim = std::max(target, m_hover_anim - step);
    Refresh(false);

    if (m_hover_anim == target) {
        if (m_hover_active) {
            // Settled at full promotion: drop to the cheap containment watchdog.
            if (m_hover_timer.GetInterval() != HOVER_WATCH_MS)
                m_hover_timer.Start(HOVER_WATCH_MS);
        } else {
            m_hover_timer.Stop();
        }
    } else if (m_hover_timer.GetInterval() != HOVER_TICK_MS) {
        // Mid-animation: run at the smooth cadence.
        m_hover_timer.Start(HOVER_TICK_MS);
    }
}

void StaticBox::SetBorderStyle(wxPenStyle style)
{
    border_style = style;
    Refresh();
}

void StaticBox::SetBorderWidth(int width)
{
    border_width = width;
    Refresh();
}

void StaticBox::SetBorderColor(StateColor const &color)
{
    if (border_color != color) {
        border_color = color;
        state_handler.update_binds();
        Refresh();
    }
}

void StaticBox::SetBorderColorNormal(wxColor const &color)
{
    border_color.setColorForStates(color, 0);
    Refresh();
}

void StaticBox::SetBackgroundColor(StateColor const &color)
{
    background_color = color;
    state_handler.update_binds();
    Refresh();
}

void StaticBox::SetBackgroundColorNormal(wxColor const &color)
{
    background_color.setColorForStates(color, 0);
    Refresh();
}

void StaticBox::SetBackgroundColor2(StateColor const &color)
{
    background_color2 = color;
    state_handler.update_binds();
    Refresh();
}

wxColor StaticBox::GetParentBackgroundColor(wxWindow* parent)
{
    if (auto box = dynamic_cast<StaticBox*>(parent)) {
        if (box->background_color.count() > 0) {
            if (box->background_color2.count() == 0)
                return box->background_color.defaultColor();
            auto s = box->background_color.defaultColor();
            auto e = box->background_color2.defaultColor();
            int r = (s.Red() + e.Red()) / 2;
            int g = (s.Green() + e.Green()) / 2;
            int b = (s.Blue() + e.Blue()) / 2;
            return wxColor(r, g, b);
        }
    }
    if (parent)
        return parent->GetBackgroundColour();
    return ThemeColor::White;
}

void StaticBox::ShowBadge(bool show)
{
    if (show && badge.name() != "badge") {
        badge = ScalableBitmap(this, "badge", 18);
        Refresh();
    } else if (!show && !badge.name().empty()) {
        badge = ScalableBitmap {};
        Refresh();
    }
}

void StaticBox::eraseEvent(wxEraseEvent& evt)
{
    // for transparent background, but not work
#ifdef __WXMSW__
    wxDC *dc = evt.GetDC();
    wxSize size = GetSize();
    wxClientDC dc2(GetParent());
    dc->Blit({0, 0}, size, &dc2, GetPosition());
#endif
}

void StaticBox::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
    render(dc);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void StaticBox::render(wxDC& dc)
{
#ifdef __WXMSW__
    if (radius == 0) {
        doRender(dc);
        return;
    }

	wxSize size = GetSize();
    if (size.x <= 0 || size.y <= 0)
        return;
    wxMemoryDC memdc(&dc);
    if (!memdc.IsOk()) {
        doRender(dc);
        return;
    }
    wxBitmap bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    //memdc.Blit({0, 0}, size, &dc, {0, 0});
    memdc.SetBackground(wxBrush(GetBackgroundColour()));
    memdc.Clear();
    {
        wxGCDC dc2(memdc);
        doRender(dc2);
    }

    memdc.SelectObject(wxNullBitmap);
	dc.DrawBitmap(bmp, 0, 0);
#else
    doRender(dc);
#endif
}

void StaticBox::doRender(wxDC& dc)
{
    wxSize size = GetSize();
    int states = state_handler.states();
    if (background_color2.count() == 0) {
        if ((border_width && border_color.count() > 0) || background_color.count() > 0) {
            wxRect rc(0, 0, size.x, size.y);
            if (border_width && border_color.count() > 0) {
                if (dc.GetContentScaleFactor() == 1.0) {
                    int d  = floor(border_width / 2.0);
                    int d2 = floor(border_width - 1);
                    rc.x += d;
                    rc.width -= d2;
                    rc.y += d;
                    rc.height -= d2;
                } else {
                    int d  = 1;
                    rc.x += d;
                    rc.width -= d;
                    rc.y += d;
                    rc.height -= d;
                }
                wxColour border_draw = border_color.colorForStates(states);
                if (m_interactive) {
                    // Ease the resting OutlineVariant toward the hover Primary by
                    // the animation factor. Both endpoints are resolved live so
                    // the promotion tracks the current theme + scheme.
                    const wxColour rest = StateColor::semantic(m_rest_border_role, m_scheme);
                    const wxColour hov  = StateColor::semantic(m_hover_border_role, m_scheme);
                    const double   t    = m_hover_anim;
                    border_draw = wxColour(
                        rest.Red()   + static_cast<int>((hov.Red()   - rest.Red())   * t + 0.5),
                        rest.Green() + static_cast<int>((hov.Green() - rest.Green()) * t + 0.5),
                        rest.Blue()  + static_cast<int>((hov.Blue()  - rest.Blue())  * t + 0.5));
                }
                dc.SetPen(wxPen(border_draw, border_width, border_style));
            } else {
                dc.SetPen(wxPen(background_color.colorForStates(states)));
            }
            if (background_color.count() > 0)
                dc.SetBrush(wxBrush(background_color.colorForStates(states)));
            else
                dc.SetBrush(wxBrush(GetBackgroundColour()));
            if (radius == 0) {
                dc.DrawRectangle(rc);
            }
            else {
                dc.DrawRoundedRectangle(rc, radius - border_width);
            }
        }
    }
    else {
        wxColor start = background_color.colorForStates(states);
        wxColor stop = background_color2.colorForStates(states);
        int r = start.Red(), g = start.Green(), b = start.Blue();
        int dr = (int) stop.Red() - r, dg = (int) stop.Green() - g, db = (int) stop.Blue() - b;
        int lr = 0, lg = 0, lb = 0;
        for (int y = 0; y < size.y; ++y) {
            dc.SetPen(wxPen(wxColor(r, g, b)));
            dc.DrawLine(0, y, size.x, y);
            lr += dr; while (lr >= size.y) { ++r, lr -= size.y; } while (lr <= -size.y) { --r, lr += size.y; }
            lg += dg; while (lg >= size.y) { ++g, lg -= size.y; } while (lg <= -size.y) { --g, lg += size.y; }
            lb += db; while (lb >= size.y) { ++b, lb -= size.y; } while (lb <= -size.y) { --b, lb += size.y; }
        }
    }

    if (badge.bmp().IsOk()) {
        auto s = badge.bmp().GetScaledSize();
        dc.DrawBitmap(badge.bmp(), size.x - s.x, 0);
    }
}
