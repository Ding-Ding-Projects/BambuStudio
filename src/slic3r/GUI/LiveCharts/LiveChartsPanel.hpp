#ifndef slic3r_GUI_LiveCharts_LiveChartsPanel_hpp_
#define slic3r_GUI_LiveCharts_LiveChartsPanel_hpp_

// Live charts for the printer selected on the Device page: the nozzle, bed and
// chamber temperatures, the humidity inside each AMS that reports one and the
// part cooling fan, as the printer sends them. The target as a reference line,
// a time window, line or one-minute candles, a crosshair for the mouse and the
// keyboard, pause, CSV, the trend in words, a momentum arrow at the newest
// point, particles on new highs, a 12-hour or 24-hour clock and a preview of
// reduced motion. The Recorder keeps this session's readings per printer: it
// looks at the selected printer twice a second whichever page is open, and
// keeps a reading only when the printer pushed a new report. The rules live in
// the wx-free LiveSeries.

#include "LiveSeries.hpp"

#include <wx/panel.h>
#include <wx/timer.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

class Button;
class Label;
class SwitchButton;
class wxScrolledWindow;
class wxSizer;
class wxWrapSizer;

namespace Slic3r {
class MachineObject;
namespace GUI { namespace LiveCharts {

enum class Kind { Nozzle = 0, Bed, Chamber, Humidity, Fan, Count };

// The humidity inside one AMS unit: a percentage when the unit reports one,
// otherwise its level from 1 (wet) to 5 (dry).
struct AmsUnit
{
    std::string name;
    Series      series;
    bool        percent          = true;
    bool        can_dry          = false;
    bool        drying           = false;
    int         dry_minutes_left = 0;
};

// AMS ids in number order: "2" before "10", and "128" after both.
struct AmsOrder
{
    bool operator()(const std::string &a, const std::string &b) const { return a.size() != b.size() ? a.size() < b.size() : a < b; }
};

struct DeviceRecord
{
    std::string                              name;
    Series                                   series[int(Kind::Count)]; // Humidity is kept per AMS unit, in ams
    double                                   target[int(Kind::Count)] = {0, 0, 0, 0, 0};
    std::map<std::string, AmsUnit, AmsOrder> ams; // only the units that report humidity
    double                                   last_push   = 0;
    bool                                     online      = false;
    bool                                     has_chamber = false;
    bool                                     has_fan     = false;
    std::size_t                              total       = 0; // readings this session
};

class Recorder
{
public:
    static Recorder &instance();
    // Keeps a reading only for a new push from the printer.
    void                record(MachineObject *obj);
    const DeviceRecord *find(const std::string &dev_id) const;
    void                set_offline(const std::string &dev_id);
    // Looks at the selected printer twice a second from startup, whichever page
    // is open, so the charts keep filling while the Device page is hidden.
    void                start_sampling();
    void                stop_sampling();

private:
    std::map<std::string, DeviceRecord> m_devices;
    std::unique_ptr<wxTimer>            m_sampler;
};

class ChartCanvas;
class MiniChart;

class LiveChartsPanel : public wxPanel
{
public:
    explicit LiveChartsPanel(wxWindow *parent);
    ~LiveChartsPanel() override;

    // Opens it in Local tools (from the Device page, the palette or Every
    // feature), on one series when `kind` names one.
    static void open(int kind = -1);
    void        set_kind(int kind);

private:
    friend class ChartCanvas;
    friend class MiniChart;
    void            refresh();
    void            set_window(int index);
    void            set_view(bool candles);
    void            set_paused(bool paused);
    void            set_ams(const std::string &id);
    void            set_option(const char *key, bool on);
    void            set_clock(bool twelve_hour);
    void            copy_value();
    void            save_csv();
    void            explain();
    void            open_motion_settings();
    void            show_menu(const wxPoint &screen);
    void            refresh_chips();
    void            refresh_ams_chips(const DeviceRecord *rec);
    bool            reduced() const;
    wxString        time_text(double t, bool seconds) const;
    const AmsUnit  *shown_ams(const DeviceRecord *rec) const;
    const Series   *shown_series(const DeviceRecord *rec) const;
    std::string     series_name() const;

    std::string               m_dev_id;
    int                       m_kind      = int(Kind::Nozzle);
    int                       m_window    = 1; // index into kWindows
    bool                      m_candles   = false;
    bool                      m_paused    = false;
    bool                      m_preview_reduced = false;
    double                    m_paused_at = 0;
    std::size_t               m_paused_total = 0;
    double                    m_announced = 0;
    std::string               m_ams;      // the AMS unit charted for humidity
    std::vector<std::string>  m_ams_ids;  // the units the chips stand for
    ChartOptions              m_options;

    wxTimer                   m_timer;
    wxScrolledWindow         *m_body     = nullptr;
    wxSizer                  *m_root     = nullptr;
    Label                    *m_device   = nullptr;
    Label                    *m_now[3]   = {nullptr, nullptr, nullptr};
    Label                    *m_session  = nullptr;
    Label                    *m_value    = nullptr;
    Label                    *m_trend    = nullptr;
    Label                    *m_momentum = nullptr;
    Label                    *m_feed     = nullptr;
    Label                    *m_cross    = nullptr;
    std::vector<Button *>     m_kind_chips, m_window_chips, m_view_chips, m_clock_chips, m_ams_chips;
    wxWrapSizer              *m_ams_row  = nullptr;
    SwitchButton             *m_reference_switch = nullptr;
    SwitchButton             *m_momentum_switch  = nullptr;
    SwitchButton             *m_particles_switch = nullptr;
    SwitchButton             *m_preview_switch   = nullptr;
    Button                   *m_pause    = nullptr;
    ChartCanvas              *m_canvas   = nullptr;
    MiniChart                *m_humidity_card = nullptr;
    MiniChart                *m_fan_card      = nullptr;
};

}} // namespace GUI::LiveCharts
} // namespace Slic3r

#endif // slic3r_GUI_LiveCharts_LiveChartsPanel_hpp_
