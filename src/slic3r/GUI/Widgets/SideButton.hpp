#ifndef slic3r_GUI_SideButton_hpp_
#define slic3r_GUI_SideButton_hpp_

#include <cstdint>

#include <wx/stattext.h>
#include <wx/vlbox.h>
#include <wx/combo.h>
#include "../wxExtensions.hpp"
#include "StateHandler.hpp"


class SideButton : public wxWindow
{
public:

    enum EHorizontalOrientation : unsigned char
    {
        HO_Left,
        HO_Center,
        HO_Right,
        Num_Horizontal_Orientations
    };

    // Defaults to the MD3 menu-row treatment (see applyMenuRowStyle): neutral
    // container fill, no contrasting ring, OnSurface label. That is what the
    // Slice/Print dropdown rows render with, since they override nothing but
    // their corner radius. A SideButton used as a standalone action button is
    // expected to restyle itself explicitly, as MainFrame's Slice/Print pills do
    // in update_side_button_style().
    SideButton(wxWindow* parent, wxString text, wxString icon = "", long style = 0, int iconSize = 0);

    void SetCornerRadius(double radius);

    //BBS set enable array
    void SetCornerEnable(const std::vector<bool>& enable);

    void SetTextLayout(EHorizontalOrientation orient, int margin = 15);

    void SetLayoutStyle(int style);

    void SetLabel(const wxString& label);

    bool SetForegroundColour(wxColour const & colour) override;

    bool SetBackgroundColour(wxColour const & color) override;

    bool SetBottomColour(wxColour const &color);

    void SetMinSize(const wxSize& size) override;
    
    void SetBorderColor(StateColor const & color);

    void SetForegroundColor(StateColor const &color);

    void SetBackgroundColor(StateColor const &color);

    bool Enable(bool enable = true);

    void Rescale();

    void SetExtraSize(const wxSize& size);

    void SetIconOffset(const int offset);

    // Optional leading Material Symbols glyph, drawn before the label through the
    // shared MaterialIcon helper. Additive: the existing raster-icon and
    // text-only paths are unchanged, and the glyph is suppressed (a graceful
    // fallback to the label alone) whenever MaterialIcon::available() is false.
    // px<=0 derives a default size; codepoint 0 clears the glyph.
    void SetLeadingGlyph(uint32_t codepoint, int px = 0);

    // This control is custom-painted, so wxWidgets cannot infer either its
    // keyboard contract or its push-button semantics from the wxWindow base.
    bool AcceptsFocus() const override;
    bool AcceptsFocusFromKeyboard() const override;
    bool IsPressedForAccessibility() const { return pressedDown || keyboard_pressed; }
    void AccessibilityActivate();
    void SetName(const wxString& name) override;

protected:
#ifdef __WIN32__
    WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM w_param, WXLPARAM l_param) override;
#endif

private:
    wxSize textSize;
    wxSize minSize;
    ScalableBitmap icon;
    double radius;
    wxSize extra_size;
    int icon_offset;
    uint32_t leading_glyph_cp = 0;
    int      leading_glyph_px = 0;
    std::vector<bool> radius_enable;

    StateHandler    state_handler;
    StateColor      text_color;
    StateColor      border_color;
    StateColor      background_color;
    wxColour        bottom_color;

    bool pressedDown      = false;
    bool keyboard_pressed = false;
    int  layout_style     = 0;

    EHorizontalOrientation text_orientation;
    int text_margin;


    // Install the MD3 menu-row palette on border/text/background + bottom colour.
    // Assigns the StateColor members directly (no update_binds/Refresh), so it is
    // safe to run from the constructor before state_handler.attach().
    void applyMenuRowStyle();

    void paintEvent(wxPaintEvent& evt);

    void dorender(wxDC& dc, wxDC& text_dc);

    // Design-px size of the leading glyph (explicit SetLeadingGlyph px, else a
    // default that balances the Body_14 label).
    int leadingGlyphPx() const;

    void messureSize();

    void mouseDown(wxMouseEvent& event);
    void mouseReleased(wxMouseEvent& event);
    void mouseCaptureLost(wxMouseCaptureLostEvent& event);
    void keyDown(wxKeyEvent& event);
    void keyUp(wxKeyEvent& event);
    void focusChanged(wxFocusEvent& event);

    void sendButtonEvent();

	DECLARE_EVENT_TABLE()
};
#endif // !slic3r_GUI_Button_hpp_
