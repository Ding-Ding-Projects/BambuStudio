#ifndef slic3r_GUI_LiveCharts_LiveChartsPanel_hpp_
#define slic3r_GUI_LiveCharts_LiveChartsPanel_hpp_

// Live charts for the printer selected on the Device page: nozzle, bed and
// chamber temperatures as the printer sends them, with the target as a
// reference line, a time window, line or one-minute candles, a crosshair for
// the mouse and the keyboard, pause, CSV and the trend in words. The Recorder
// keeps this session's readings per printer: the Device page hands it each
// update and it keeps a reading only when the printer pushed a new report.
// The rules live in the wx-free LiveSeries.

#include "LiveSeries.hpp"

#include <wx/panel.h>
#include <wx/timer.h>

#include <map>
#include <string>
#include <vector>

class Button;
class Label;
class SwitchButton;

namespace Slic3r {
class MachineObject;
namespace GUI { namespace LiveCharts {

enum class Kind { Nozzle = 0, Bed, Chamber, Count };

struct DeviceRecord
{
    std::string name;
    Series      series[int(Kind::Count)];
    double      target[int(Kind::Count)] = {0, 0, 0};
    double      last_push = 0;
    bool        online    = false;
    bool        has_chamber = false;
    std::size_t total = 0; // readings this session
};

class Recorder
{
public:
    static Recorder &instance();
    // Called with every Device page update; keeps a reading only for a new push.
    void                record(MachineObject *obj);
    const DeviceRecord *find(const std::string &dev_id) const;
    void                set_offline(const std::string &dev_id);

private:
    std::map<std::string, DeviceRecord> m_devices;
};

class ChartCanvas;

class LiveChartsPanel : public wxPanel
{
public:
    explicit LiveChartsPanel(wxWindow *parent);
    ~LiveChartsPanel() override;

    // Opens it in Local tools (from the Device page, the palette or Every feature).
    static void open();

private:
    friend class ChartCanvas;
    void refresh();
    void set_kind(int kind);
    void set_window(int index);
    void set_view(bool candles);
    void set_paused(bool paused);
    void copy_value();
    void save_csv();
    void explain();
    void show_menu(const wxPoint &screen);
    void refresh_chips();

    std::string               m_dev_id;
    int                       m_kind      = int(Kind::Nozzle);
    int                       m_window    = 1; // index into kWindows
    bool                      m_candles   = false;
    bool                      m_reference = true;
    bool                      m_paused    = false;
    double                    m_paused_at = 0;
    std::size_t               m_paused_total = 0;
    double                    m_announced = 0;

    wxTimer                   m_timer;
    Label                    *m_device   = nullptr;
    Label                    *m_now[int(Kind::Count)] = {nullptr, nullptr, nullptr};
    Label                    *m_session  = nullptr;
    Label                    *m_value    = nullptr;
    Label                    *m_trend    = nullptr;
    Label                    *m_feed     = nullptr;
    Label                    *m_cross    = nullptr;
    std::vector<Button *>     m_kind_chips, m_window_chips, m_view_chips;
    SwitchButton             *m_reference_switch = nullptr;
    Button                   *m_pause    = nullptr;
    ChartCanvas              *m_canvas   = nullptr;
};

}} // namespace GUI::LiveCharts
} // namespace Slic3r

#endif // slic3r_GUI_LiveCharts_LiveChartsPanel_hpp_
