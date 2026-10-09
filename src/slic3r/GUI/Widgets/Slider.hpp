#ifndef slic3r_GUI_Slider_hpp_
#define slic3r_GUI_Slider_hpp_

#include <functional>

#include <wx/window.h>

#include "MD3Tokens.hpp"
#include "MD3Motion.hpp"

// Shared Material Design 3 slider (kit: selection/Slider.prompt.md).
//
// A thin (~4px) rounded track: the active portion (min -> thumb) paints in
// Primary (scheme-aware), the inactive remainder in OutlineVariant, with a
// filled Primary circular thumb. Both orientations are supported: a horizontal
// bar (fan speed, move timeline) and a vertical 12x300 variant (Preview layer),
// where the minimum sits at the bottom and the maximum at the top.
//
// Pure wxDC drawing — no glyphs, no bitmaps. All geometry is resolved live from
// FromDIP() at paint / hit-test time so the control stays crisp and correct
// across DPI and density changes with no cached device metrics. Colours are
// resolved per paint from MD3 tokens so it re-themes (light/dark) and re-tints
// to the active accent scheme.
class Slider : public wxWindow
{
public:
    Slider();

    Slider(wxWindow *     parent,
           int            value    = 0,
           int            minValue = 0,
           int            maxValue = 100,
           bool           vertical = false,
           const wxPoint &pos      = wxDefaultPosition,
           const wxSize & size     = wxDefaultSize);

    // ~wxWindowMSW destroys the window after this class is gone. ::DestroyWindow
    // sends a visible child WM_SHOWWINDOW(FALSE) and releases a held mouse
    // capture; wx delivers those as wxEVT_SHOW and wxEVT_MOUSE_CAPTURE_LOST.
    // Unbind both first so neither can reach settleHalo() on a destroyed Slider.
    ~Slider() override
    {
        Unbind(wxEVT_SHOW, &Slider::onShow, this);
        Unbind(wxEVT_MOUSE_CAPTURE_LOST, &Slider::onCaptureLost, this);
        m_halo_motion.Stop();
    }

    bool Create(wxWindow *     parent,
                int            value    = 0,
                int            minValue = 0,
                int            maxValue = 100,
                bool           vertical = false,
                const wxPoint &pos      = wxDefaultPosition,
                const wxSize & size     = wxDefaultSize);

    void SetRange(int minValue, int maxValue);
    int  GetMin() const { return m_min; }
    int  GetMax() const { return m_max; }

    // Set the value without firing the change callback (clamped to range).
    void SetValue(int value);
    int  GetValue() const { return m_value; }

    void SetVertical(bool vertical);
    bool IsVertical() const { return m_vertical; }

    // Accent scheme for the active track + thumb (Brand / Preview / Device).
    void SetColorScheme(MD3::ColorScheme scheme);

    // Fired whenever a user interaction changes the value.
    void SetOnChange(std::function<void(int)> cb) { m_on_change = std::move(cb); }

    virtual void Rescale();

    // A custom-drawn wxWindow is not focusable from the keyboard by default, so
    // without this the slider is operable only by mouse: its arrow / Page / Home /
    // End handling can never receive a key because nothing can give it focus. A
    // reskin that costs keyboard reachability is a regression, not a redesign -
    // this is why a native wxSlider must never be replaced by this control until
    // both this and the wxAccessible below are in place.
    bool AcceptsFocusFromKeyboard() const override { return IsEnabled() && IsShown(); }

protected:
    wxSize DoGetBestSize() const override;

private:
    void paintEvent(wxPaintEvent &evt);
    void render(wxDC &dc);

    void onMouseDown(wxMouseEvent &evt);
    void onMouseUp(wxMouseEvent &evt);
    void onMotion(wxMouseEvent &evt);
    void onWheel(wxMouseEvent &evt);
    void onKey(wxKeyEvent &evt);
    void onFocus(wxFocusEvent &evt);
    void onShow(wxShowEvent &e);
    void onCaptureLost(wxMouseCaptureLostEvent &e);
    void emphasizeHalo();
    void settleHalo();

    int  thumbDiameter() const;      // device px
    int  trackThickness() const;     // device px
    int  valueFromPoint(const wxPoint &p) const;
    void setValueInternal(int value, bool notify);
    int  clampValue(int value) const;

    int              m_min      = 0;
    int              m_max      = 100;
    int              m_value    = 0;
    bool             m_vertical = false;
    bool             m_dragging = false;
    bool             m_focused  = false;
    MD3::Motion::Anim m_halo_motion;
    double           m_halo_emphasis = 0.0;
    wxColour         m_halo_color;
    wxColour         m_halo_background;
    MD3::ColorScheme m_scheme   = MD3::ColorScheme::Brand;

    std::function<void(int)> m_on_change;
};

#endif // !slic3r_GUI_Slider_hpp_
