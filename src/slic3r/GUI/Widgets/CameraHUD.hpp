#ifndef slic3r_GUI_CameraHUD_hpp_
#define slic3r_GUI_CameraHUD_hpp_

#include <cstdint>

#include <wx/colour.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "../wxExtensions.hpp" // ScalableBitmap
#include "MaterialIcon.hpp"    // MaterialIcon::Glyph + draw helpers
#include "MD3Tokens.hpp"       // MD3::Viewport::live

namespace Slic3r { namespace GUI {

// CameraHUD is the dark chrome strip that bookends the device camera video. It
// renders the MD3 design-kit "camera card" top band: a pulsing LIVE badge on the
// left (MD3::Viewport::live), the camera status indicators, and two icon-font
// control chips (settings / fullscreen) on the right.
//
// The strip stays dark in both normal app themes because the video underneath is
// always a dark surface. Windows high-contrast mode is the deliberate exception:
// the chrome switches to system colours so text, controls, and focus remain
// visible under the user's contrast palette.
//
// It never overlays a live-rendered child over the native wxMediaCtrl HWND: the
// HUD is a sibling band stacked above the video by the monitoring sizer, so
// there is no z-ordered child on the media window (no MSW flicker / clip).
class CameraHUD : public wxPanel
{
public:
    // A circular, custom-painted icon-font control chip. Behaves like the old
    // CameraItem for StatusPanel's purposes: it is the window that receives the
    // native LEFT_DOWN / LEFT_DCLICK, so StatusPanel can Connect() its handlers
    // to the chip exactly as before, and it exposes reset_hover() / Enable() /
    // msw_rescale().
    class CameraHUDChip : public wxWindow
    {
    public:
        CameraHUDChip(wxWindow *parent, uint32_t glyph, const wxString &fallback_icon);
        ~CameraHUDChip() override = default;

        void reset_hover();
        bool Enable(bool enable = true) override;
        void msw_rescale();

        bool AcceptsFocus() const override;
        bool AcceptsFocusFromKeyboard() const override;
        bool IsPressedForAccessibility() const { return m_keyboard_pressed; }
        void AccessibilityActivate();
        void SetName(const wxString &name) override;

    protected:
#ifdef __WIN32__
        WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM w_param, WXLPARAM l_param) override;
#endif

    private:
        void on_paint(wxPaintEvent &evt);
        void on_enter(wxMouseEvent &evt);
        void on_leave(wxMouseEvent &evt);
        void on_left_down(wxMouseEvent &evt);
        void on_key_down(wxKeyEvent &evt);
        void on_key_up(wxKeyEvent &evt);
        void on_focus(wxFocusEvent &evt);
        void send_activation_event();

        uint32_t       m_glyph;
        std::string    m_fallback_name;
        ScalableBitmap m_fallback;
        bool           m_hover{false};
        bool           m_keyboard_pressed{false};
    };

    // A small custom-painted temperature pill (e.g. "220°C") shown in the band.
    // Mirrors the MD3 kit's camera-card temperature chips (Device.jsx:26-29):
    // Roboto Mono 11.5, white text, translucent-black pill (r10) over the
    // fixed-dark strip. Its width auto-fits the value; StatusPanel feeds it once
    // per refresh via CameraHUD::SetTemperatures and it hides when no printer is
    // connected. Fixed-dark like the rest of the band, so it needs no re-tint.
    class CameraHUDTempChip : public wxWindow
    {
    public:
        explicit CameraHUDTempChip(wxWindow *parent);
        ~CameraHUDTempChip() override = default;

        // Set the pill text; re-fits the pill and re-lays out the band when the
        // width changes. A no-op when the text is unchanged.
        void SetText(const wxString &text);
        void msw_rescale();

    protected:
        wxSize DoGetBestSize() const override;

    private:
        void on_paint(wxPaintEvent &evt);

        wxString m_text;
    };

    explicit CameraHUD(wxWindow *parent);
    ~CameraHUD() override;

    // Start / stop the pulsing LIVE badge. Idempotent and safe to call every
    // refresh cycle: it reconciles the pulse timer with the live flag, the
    // current on-screen visibility, and the OS reduced-motion preference. Under
    // reduced motion the timer stops and the LIVE dot snaps to steady opacity.
    void SetLiveActive(bool live);

    // Nozzle / bed temperature chips (kit camera-card temp readouts). Fed once
    // per refresh from StatusPanel::update_temp_ctrl; HideTemperatures() clears
    // them when no printer is connected. Values are in whole degrees Celsius.
    void SetTemperatures(int nozzle_c, int bed_c);
    void HideTemperatures();

    bool Enable(bool enable = true) override;
    void msw_rescale();

    CameraHUDChip *setting_chip() const { return m_setting_chip; }
    CameraHUDChip *fullscreen_chip() const { return m_fullscreen_chip; }
    wxSizer *      status_slot() const { return m_status_slot; }

    // Dark camera chrome in normal themes; Windows high-contrast mode resolves
    // these through the user's current system palette.
    static bool     HighContrastActive();
    static wxColour CardBg();
    static wxColour Border();
    static wxColour ChipBg();
    static wxColour ChipHover();
    static wxColour ChipPress();
    static wxColour Glyph();
    static wxColour GlyphMuted();
    static wxColour FocusRing();

private:
    void on_paint(wxPaintEvent &evt);
    void on_pulse(wxTimerEvent &evt);

    CameraHUDChip *    m_setting_chip{nullptr};
    CameraHUDChip *    m_fullscreen_chip{nullptr};
    CameraHUDTempChip *m_nozzle_chip{nullptr};
    CameraHUDTempChip *m_bed_chip{nullptr};
    wxBoxSizer *       m_status_slot{nullptr};
    wxSizerItem *      m_badge_spacer{nullptr};

    wxTimer m_pulse_timer;
    bool    m_live{false};
    double  m_phase{0.0};
    wxRect  m_dot_rect; // bounding box of the pulsing dot, for partial refresh
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_CameraHUD_hpp_
