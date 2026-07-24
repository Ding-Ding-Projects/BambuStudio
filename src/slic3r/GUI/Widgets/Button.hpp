#ifndef slic3r_GUI_Button_hpp_
#define slic3r_GUI_Button_hpp_

#include "../wxExtensions.hpp"
#include "StaticBox.hpp"

#include <cstdint>

class wxTipWindow;
class Button : public StaticBox
{
public:
    // MD3 action-button variants. The default-constructed Button keeps its
    // legacy white/green toggle appearance untouched (so the ~hundreds of
    // existing call sites are unaffected); a variant is applied only when a
    // caller opts in via SetVariant() / SetIconButton(). See applyMD3Style().
    //
    // IconButton is a borderless ghost touch target (circle 50% / square r8)
    // that draws a single centered Material Symbols glyph; it is opted into via
    // SetIconButton() and its glyph is supplied through SetGlyph().
    enum class Variant { Filled, Tonal, Outlined, Text, Danger, IconButton };

    // MD3 size tiers. Map to fixed heights 36/42/44, per-side h-padding
    // 16/18/22, label sizes 12.5/13.5/14 and icon glyphs 18/20/20.
    enum class Size { Small, Medium, Large };

    // IconButton shape: circle (radius = half the container) or square (r8).
    enum class IconShape { Circle, Square };

private:
    wxRect textSize;
    wxSize minSize; // set by outer
    wxSize paddingSize;
    ScalableBitmap active_icon;
    ScalableBitmap inactive_icon;

    StateColor   text_color;
    // Optional dedicated glyph colour. Empty (count()==0) means the glyph
    // follows text_color, the historical behaviour. TabCtrl pins this to its
    // semantic tab palette so per-item label decorations (e.g. the preset
    // editor's modified-orange) never tint the leading Material Symbols glyph.
    StateColor   glyph_color;

    // MD3 variant state (inert until SetVariant() opts a Button into the
    // Material action-button styling).
    Variant           m_variant       = Variant::Filled;
    Size              m_button_size   = Size::Medium;
    MD3::ColorScheme  m_scheme        = MD3::ColorScheme::Brand;
    bool              m_md3_variant   = false;

    // Optional Material Symbols glyph content (part b). When m_has_glyph is set
    // and the icon font resolves, render() draws this codepoint through the
    // MaterialIcon font path (coloured by text_color) instead of a raster
    // ScalableBitmap; a raster icon set alongside it is the graceful fallback
    // when the Material Symbols face is unavailable.
    bool     m_has_glyph = false;
    uint32_t m_glyph_cp  = 0;
    int      m_glyph_px  = 0; // 0 => derive from the size tier / IconButton size

    // IconButton geometry (only meaningful when m_variant == Variant::IconButton).
    IconShape m_icon_shape        = IconShape::Circle;
    bool      m_icon_filled       = false;
    bool      m_icon_danger       = false;
    int       m_icon_container_px = 36; // square touch-target edge (design px)

    bool pressedDown = false;
    bool m_selected  = true;
    bool canFocus  = true;
    bool isCenter    = true;
    bool vertical    = false;

    bool m_left_corner_white = false;
    bool m_right_corner_white = false;
    bool grayed = false;
    // when true, the button can shrink and the label is truncated 
    // with an ellipsis (Chrome-style notebook tabs).
    bool m_allow_shrink = false;

    wxTipWindow* tipWindow = nullptr;

    static const int buttonWidth = 200;
    static const int buttonHeight = 50;

public:
    Button();

    Button(wxWindow* parent, wxString text, wxString icon = "", long style = 0, int iconSize = 0, wxWindowID btn_id = wxID_ANY);

    bool Create(wxWindow* parent, wxString text, wxString icon = "", long style = 0, int iconSize = 0, wxWindowID btn_id = wxID_ANY);

    void SetLabel(const wxString& label) override;

    bool SetFont(const wxFont& font) override;

    void SetIcon(const wxString& icon);

    void SetInactiveIcon(const wxString& icon);

    void SetMinSize(const wxSize& size) override;
    void SetMaxSize(const wxSize& size) override;

    // BBS: allow the button to shrink below its content width (label truncated with
    // an ellipsis). Used by the notebook tab bar so the side tools stay visible.
    void SetAllowShrink(bool allow);

    void SetPaddingSize(const wxSize& size);

    void SetTextColor(StateColor const &color);

    void SetTextColorNormal(wxColor const &color);

    // MD3 opt-in styling. SetVariant() switches the Button onto the Material
    // action-button appearance (filled/tonal/outlined/text/danger) with the
    // active size tier and colour scheme; SetButtonSize()/SetColorScheme()
    // adjust an already-variant Button. All three are no-ops on the legacy
    // default until SetVariant() is first called.
    void SetVariant(Variant variant);
    void SetButtonSize(Size size);
    void SetColorScheme(MD3::ColorScheme scheme);

    // Draw a Material Symbols glyph (via the shared MaterialIcon helper) as the
    // button's icon instead of a raster ScalableBitmap. px<=0 derives the glyph
    // size from the active size tier / IconButton size; codepoint 0 clears the
    // glyph and reverts to the raster-icon path. Works on any Button — a legacy
    // default button opts in without otherwise changing its appearance.
    void SetGlyph(uint32_t codepoint, int px = 0);

    // Pin the glyph to its own StateColor instead of following text_color.
    // Pass a default-empty StateColor() to revert to the follow-text behaviour.
    void SetGlyphColor(StateColor const &color);

    // Switch the Button into borderless IconButton mode: a circle (radius =
    // half the container) or square (r8) ghost target that is transparent +
    // OnSurfaceVariant at rest, SurfaceContainerHigh on hover, optionally a
    // resting SurfaceContainerHighest disc (filled), and Error / OnError on
    // hover when danger. Supply the glyph via SetGlyph(); container_px <= 0
    // keeps the current/default 36px edge (square adds 4px of width).
    void SetIconButton(IconShape shape = IconShape::Circle, int container_px = 0,
                       bool filled = false, bool danger = false);

    void SetSelected(bool selected = true) { m_selected = selected; }

    bool Enable(bool enable = true) override;
    void EnableTooltipEvenDisabled();// The tip will be shown even if the button is disabled

    void SetCanFocus(bool canFocus) override;

    void SetValue(bool state);

    bool GetValue() const;

    void SetCenter(bool isCenter);

    void SetVertical(bool vertical = true);

    void Rescale();

    void SetLeftCornerWhite(bool white = true) { m_left_corner_white = white; }
    void SetRightCornerWhite(bool white = true) { m_right_corner_white = white; }

    bool IsGrayed() { return grayed; }
    void SetGrayed(bool gray) { grayed = gray; }

    wxRect GetTextRect() const { return textSize; }

protected:
#ifdef __WIN32__
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

    bool AcceptsFocus() const override;

private:
    void paintEvent(wxPaintEvent& evt);

    void render(wxDC& dc);
    void renderWhiteCorners(wxDC &dc);

    // Recompute background/text/border StateColors, border width, pill radius,
    // fixed height, horizontal padding, font and icon glyph size from the
    // current variant/size/scheme. Only runs when m_md3_variant is set.
    void applyMD3Style();
    // Rebuild the active/inactive icon bitmaps at a new glyph size (px).
    void rebuildIcons(int px);
    // The design-px glyph size for the current glyph/variant: respects an
    // explicit SetGlyph() px, else derives from the IconButton container
    // (17/19/22) or the pill size tier (18/20/20).
    int effectiveGlyphPx() const;

    void messureSize();

    // some useful events
    void mouseDown(wxMouseEvent& event);
    void mouseReleased(wxMouseEvent& event);
    void mouseCaptureLost(wxMouseCaptureLostEvent &event);
    void keyDownUp(wxKeyEvent &event);

    // 
    void sendButtonEvent();

    // parent motion
    void OnParentMotion(wxMouseEvent& event);
    void OnParentLeave(wxMouseEvent& event);

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_Button_hpp_
