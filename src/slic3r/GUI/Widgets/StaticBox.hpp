#ifndef slic3r_GUI_StaticBox_hpp_
#define slic3r_GUI_StaticBox_hpp_

#include "../wxExtensions.hpp"
#include "MD3Tokens.hpp"
#include "StateHandler.hpp"

#include <wx/timer.h>
#include <wx/window.h>

class StaticBox : public wxWindow
{
public:
    // Corner-radius density for the base card primitive. Comfortable -> 16,
    // Compact -> 12 (ui-md3 containment/Card: r16/12 by density). The default
    // stays Compact so existing call sites are unchanged.
    enum class Density { Compact, Comfortable };

    StaticBox();

    StaticBox(wxWindow* parent,
             wxWindowID      id        = wxID_ANY,
             const wxPoint & pos       = wxDefaultPosition,
             const wxSize &  size      = wxDefaultSize,
             long style = 0);

    bool Create(wxWindow* parent,
        wxWindowID      id        = wxID_ANY,
        const wxPoint & pos       = wxDefaultPosition,
        const wxSize &  size      = wxDefaultSize,
        long style = 0);

    void SetCornerRadius(double radius);

    void SetBorderWidth(int width);

    void SetBorderColor(StateColor const & color);

    void SetBorderColorNormal(wxColor const &color);

    void SetBorderStyle(wxPenStyle style);

    void SetBackgroundColor(StateColor const &color);

    void SetBackgroundColorNormal(wxColor const &color);

    void SetBackgroundColor2(StateColor const &color);

    static wxColor GetParentBackgroundColor(wxWindow * parent);

    void ShowBadge(bool show);

    // Drive the default corner radius from the active density (comfortable 16 /
    // compact 12) instead of the fixed compact 12. No-op when a caller has
    // pinned an explicit radius via SetCornerRadius().
    void SetDensity(Density density);

    // Interactive-card mode (ui-md3 containment/Card): promotes the resting
    // OutlineVariant border to Primary on hover over ~0.15s, and shows a pointer
    // cursor. The hover accent follows the scheme set via SetSchemeAccent().
    void SetInteractive(bool interactive);

    // Scheme for the interactive hover accent (Brand / Preview / Device) so the
    // promoted border stays contextual instead of falling back to brand green.
    void SetSchemeAccent(MD3::ColorScheme scheme);

protected:
    void SetDefaultCornerRadius(double radius_dip);
    void RescaleDefaultCornerRadius();

    void eraseEvent(wxEraseEvent& evt);

    void paintEvent(wxPaintEvent& evt);

    void render(wxDC& dc);

    virtual void doRender(wxDC& dc);

    // Interactive-card hover animation.
    void onHoverEnter(wxMouseEvent &evt);
    void onHoverLeave(wxMouseEvent &evt);
    void onHoverTick(wxTimerEvent &evt);

protected:
    double radius;
    double m_default_radius_dip{MD3::Metrics::compact.radius};
    bool   m_uses_default_radius{true};
    int border_width = 1;
    wxPenStyle border_style = wxPENSTYLE_SOLID;
    StateHandler state_handler;
    StateColor   border_color;
    StateColor   background_color;
    StateColor   background_color2;
    ScalableBitmap badge;

    // Interactive-card state. m_hover_anim is the resting(0) -> hover(1) border
    // promotion factor, eased by a short timer so the OutlineVariant -> Primary
    // transition reads as ~0.15s rather than an instant swap.
    bool             m_interactive{false};
    MD3::ColorScheme m_scheme{MD3::ColorScheme::Brand};
    MD3::Role        m_rest_border_role{MD3::Role::OutlineVariant};
    MD3::Role        m_hover_border_role{MD3::Role::Primary};
    double           m_hover_anim{0.0};
    bool             m_hover_active{false};
    wxTimer          m_hover_timer;

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_StaticBox_hpp_
