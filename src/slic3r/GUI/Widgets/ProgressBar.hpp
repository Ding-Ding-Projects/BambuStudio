#ifndef slic3r_GUI_ProgressBar_hpp_
#define slic3r_GUI_ProgressBar_hpp_

#include <optional>
#include <vector>

#include <wx/window.h>
#include <wx/timer.h>
#include "../wxExtensions.hpp"
#include "StateColor.hpp"

wxDECLARE_EVENT(EVT_PROGRESS_BAR_HEIGHT_CHANGED, wxCommandEvent);

class ProgressBar : public wxWindow
{
public: 
    struct Marker
    {
        int      m_position = 0;
        wxString m_label;
    };

    ProgressBar();
    ProgressBar(wxWindow *         parent,
                wxWindowID         id        = wxID_ANY,
                int                max       = 100,
                const wxPoint &    pos       = wxDefaultPosition, 
                const wxSize &     size      = wxDefaultSize,
                bool               shown     = false);


    void create(wxWindow *parent, wxWindowID id,  const wxPoint &pos, wxSize &size);

    ~ProgressBar();

public:
    bool     m_shownumber                 = {false};
    int      m_disable                    = {false};
    int      m_max                        = {100};
    int      m_step                       = {0};
    int      m_miniHeight                 = {0};
    // Kit ProgressBar geometry (ui-md3 containment/ProgressBar.jsx): 8px track,
    // soft-rounded r6 corners (a fixed radius, not a height/2 stadium pill).
    const int      miniHeight             = {8};
    const double   defaultRadius          = {6};
    double   m_radius                     = {6};
    double   m_proportion                 = {0};
    wxColour m_progress_background_colour = StateColor::semantic(MD3::Role::SurfaceContainerHighest);
    wxColour m_progress_colour            = StateColor::semantic(MD3::Role::Primary);
    // Blocked/disabled progress state resolved through the Error role (the kit
    // has no separate Warning role); replaces the raw ThemeColor::Warning literal.
    wxColour m_progress_colour_disable    = StateColor::semantic(MD3::Role::Error);
    wxString m_disable_text;
    

public:
    void         ShowNumber(bool shown);
    void         Disable(wxString text);
    void         SetValue(int  step);
    void         Reset();
    void         SetProgress(int step);
    void         SetRadius(double radius);
    void         SetProgressForedColour(wxColour colour);
    void         SetProgressBackgroundColour(wxColour colour);
    void         SetMarkers(const std::vector<Marker> &markers);
    void         ClearMarkers() { SetMarkers({}); }
    void         Rescale();

    // wxGauge-compatible surface, so the stock gauges in the status bars and
    // progress windows can become this kit ProgressBar without touching their
    // callers: range accessors plus an indeterminate Pulse() that sweeps a
    // Primary segment along the track until the next SetValue().
    int          GetValue() const { return m_step; }
    int          GetRange() const { return m_max; }
    void         SetRange(int range);
    void         Pulse();
    void         SetHeight(int height);
    virtual void SetMinSize(const wxSize &size) override;

protected:
    bool         m_indeterminate = false;
    double       m_pulse_phase   = 0.0;
    wxTimer      m_pulse_timer;
    void         onPulseTick(wxTimerEvent &evt);
    void         paintEvent(wxPaintEvent &evt);
    void         mouseMove(wxMouseEvent &evt);
    void         mouseLeave(wxMouseEvent &evt);
    void         render(wxDC &dc);
    void         doRender(wxDC &dc);
    void         renderMarkers(wxDC &dc, const wxSize &size, int barHeight);
    virtual void DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO);

private:
    int  findHoveredMarker(const wxPoint &position) const;
    void updateControlHeight();

    int                 m_barHeight = miniHeight;
    int                 m_hoveredMarker = -1;
    std::optional<wxPoint> m_lastMousePosition;
    std::vector<Marker> m_markers;


    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_ProgressBar_hpp_
