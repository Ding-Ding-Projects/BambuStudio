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

// CameraHUD is the always-dark chrome strip that bookends the device camera
// video. It renders the MD3 design-kit "camera card" top band: a pulsing LIVE
// badge on the left (MD3::Viewport::live), the camera status indicators, and
// two icon-font control chips (settings / fullscreen) on the right.
//
// The strip is fixed-dark in BOTH app themes (the video underneath is always a
// dark surface), so it deliberately does NOT follow the Device theme roles and
// is excluded from StatusPanel's on_sys_color_changed re-tinting.
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

    private:
        void on_paint(wxPaintEvent &evt);
        void on_enter(wxMouseEvent &evt);
        void on_leave(wxMouseEvent &evt);

        uint32_t       m_glyph;
        std::string    m_fallback_name;
        ScalableBitmap m_fallback;
        bool           m_hover{false};
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
    // refresh cycle: it reconciles the pulse timer with the live flag and the
    // current on-screen visibility, so the timer never runs on a hidden page.
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

    // Fixed-dark palette (shared with StatusPanel for the status-indicator
    // window backgrounds). Never theme-dependent.
    static const wxColour &CardBg();
    static const wxColour &Border();
    static const wxColour &ChipBg();
    static const wxColour &ChipHover();
    static const wxColour &ChipPress();
    static const wxColour &Glyph();
    static const wxColour &GlyphMuted();

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
