#ifndef slic3r_GUI_AxisCtrlButton_hpp_
#define slic3r_GUI_AxisCtrlButton_hpp_

#include <wx/stattext.h>
#include <wx/vlbox.h>
#include <wx/combo.h>
#include "../wxExtensions.hpp"
#include "StateHandler.hpp"


// Kit Device.jsx:79-89 — the XY jog control is a 3x3 arrow grid (keyboard_arrow_*
// tiles on SurfaceContainerHighest r8 + a centered home tile on secondary-container),
// driven by mouse or keyboard arrows, replacing the legacy circular +10/+1 dial.
//
// The command contract is unchanged: a jog fires wxEVT_COMMAND_BUTTON_CLICKED with
// SetInt(position) where position is the SAME 0..8 code on_axis_ctrl_xy decodes
// (0 Y+10 / 1 X-10 / 2 Y-10 / 3 X+10 / 4 Y+1 / 5 X-1 / 6 Y-1 / 7 X+1 / 8 home). The
// magnitude (outer +10 / inner +1 ring) is now supplied by SetStep(10|1), which the
// compact 10/1 step SegmentedControl above the grid drives, so BOTH legacy jog
// magnitudes stay reachable.
class AxisCtrlButton : public wxWindow
{
    wxSize minSize;
    wxPoint center;

    int m_step = 10; // active jog magnitude in mm (10 = outer ring, 1 = inner ring)

    StateHandler    state_handler;
    StateColor      text_color;             // glyph / label colour
    StateColor      border_color;           // active-tile ring
    StateColor      background_color;        // arrow-tile fill
    StateColor      inner_background_color;  // kept for API compatibility

    ScalableBitmap m_icon; // legacy home raster (fallback when the icon face is absent)

    bool pressedDown = false;

    unsigned char current_cell;
    enum Cell {
        CELL_UP = 0,
        CELL_LEFT,
        CELL_HOME,
        CELL_RIGHT,
        CELL_DOWN,
        CELL_NONE
    };

public:
    AxisCtrlButton(wxWindow *parent, ScalableBitmap &icon, long style = 0);

    void SetMinSize(const wxSize& size) override;

    void SetTextColor(StateColor const& color);

    void SetBorderColor(StateColor const& color);

    void SetBackgroundColor(StateColor const& color);

    void SetInnerBackgroundColor(StateColor const& color);

    void SetBitmap(ScalableBitmap &bmp);

    // Active jog magnitude (mm): 10 (outer ring) or 1 (inner ring). Driven by the
    // 10/1 step SegmentedControl; any other value is clamped to 10.
    void SetStep(int mm);
    int  GetStep() const { return m_step; }

    void Rescale();

private:
    void updateParams();

    void paintEvent(wxPaintEvent& evt);

    void render(wxDC& dc);

    void mouseDown(wxMouseEvent& event);
    void mouseReleased(wxMouseEvent& event);
    void mouseMoving(wxMouseEvent& event);
    void mouseLeave(wxMouseEvent& event);
    void keyDown(wxKeyEvent& event);

    // Device-pixel geometry of the 3x3 grid, resolved live from the current size.
    void gridMetrics(int &tile, int &gap, int &ox, int &oy) const;
    wxRect cellRect(int cell) const;
    int    cellFromPoint(const wxPoint& p) const;
    // Legacy 0..8 position code for a cell at the active step (-1 = no command).
    int    positionForCell(int cell) const;

    void sendButtonEvent();

    DECLARE_EVENT_TABLE()
};
#endif // !slic3r_GUI_Button_hpp_
