#include "LiveChartsPanel.hpp"

#include "../DeviceManager.hpp"
#include "../DeviceCore/DevBed.h"
#include "../DeviceCore/DevChamber.h"
#include "../DeviceCore/DevExtruderSystem.h"
#include "../DeviceCore/DevFan.h"
#include "../DeviceCore/DevFilaSystem.h"
#include "../DeviceCore/DevManager.h"
#include "../DocsBrowserDialog.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/FanMotionMath.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Menu.hpp"
#include "../Widgets/MD3Motion.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/SwitchButton.hpp"

#include "libslic3r/AppConfig.hpp"

#include <wx/clipbrd.h>
#include <wx/control.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/graphics.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#ifdef _WIN32
#include <wx/msw/wrapwin.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iterator>

namespace Slic3r { namespace GUI { namespace LiveCharts {

namespace fs = std::filesystem;

namespace {

constexpr double kWindows[] = {60, 300, 900, 3600};
constexpr double kGap       = 10.0; // seconds without a reading that break the line
constexpr const char *kGuide = "docs/features/device/live-charts.md";
enum : std::uint32_t { GlyphChart = 0xF190, GlyphUp = 0xE8E5, GlyphDown = 0xE8E3, GlyphFlat = 0xE8E4, GlyphPause = 0xE034, GlyphPlay = 0xE037,
                       GlyphCopy = 0xE14D, GlyphSave = 0xE161, GlyphHelp = 0xE887, GlyphHumidity = 0xF87E, GlyphFan = 0xF168,
                       GlyphMotion = 0xE71C };

// How a series' values read: degrees, a percentage, or an AMS humidity level.
enum class Unit { Celsius, Percent, Level };

// A percentage is drawn inside 0..100 and a level inside 1..5.
struct Bounds
{
    bool   on      = false;
    double floor   = 0;
    double ceiling = 0;
};

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

wxColour with_alpha(const wxColour &c, double alpha)
{
    return wxColour(c.Red(), c.Green(), c.Blue(), static_cast<unsigned char>(std::clamp(int(std::lround(alpha * 255.0)), 0, 255)));
}

double now_seconds() { return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); }

wxString kind_name(int kind)
{
    switch (Kind(kind)) {
    case Kind::Nozzle: return _L("Nozzle");
    case Kind::Bed: return _L("Bed");
    case Kind::Chamber: return _L("Chamber");
    case Kind::Humidity: return _L("AMS humidity");
    default: return _L("Part cooling fan");
    }
}

Unit unit_of(Kind kind, const AmsUnit *ams)
{
    if (kind == Kind::Humidity) return ams && !ams->percent ? Unit::Level : Unit::Percent;
    if (kind == Kind::Fan) return Unit::Percent;
    return Unit::Celsius;
}

Bounds bounds_of(Unit unit)
{
    if (unit == Unit::Percent) return Bounds{true, 0, 100};
    if (unit == Unit::Level) return Bounds{true, 0.5, 5.5};
    return Bounds{};
}

wxString degrees(double v) { return wxString::Format("%.1f ", v) + wxUniChar(0x00B0) + "C"; }

wxString value_text(double v, Unit unit)
{
    switch (unit) {
    case Unit::Celsius: return degrees(v);
    case Unit::Percent: return wxString::Format("%.0f %%", v);
    default: return wxString::Format(_L("Level %.0f of 5"), v);
    }
}

// A change over the trend's minute.
wxString change_text(double delta, Unit unit)
{
    switch (unit) {
    case Unit::Celsius: return wxString::Format("%+.1f ", delta) + wxUniChar(0x00B0) + "C";
    case Unit::Percent: return wxString::Format("%+.0f %%", delta);
    default: return wxString::Format(_L("level %+.0f"), delta);
    }
}

// A change a second, for the momentum.
wxString rate_text(double rate, Unit unit)
{
    switch (unit) {
    case Unit::Celsius: return wxString::Format("%+.2f ", rate) + wxUniChar(0x00B0) + "C";
    case Unit::Percent: return wxString::Format("%+.1f %%", rate);
    default: return wxString::Format(_L("level %+.2f"), rate);
    }
}

wxString trend_words(const TrendInfo &t, Unit unit)
{
    const int over = int(std::lround(t.over));
    switch (t.trend) {
    case Trend::Holding: return wxString::Format(_L("Holding near %s"), value_text(t.target, unit));
    case Trend::Rising: return wxString::Format(_L("Rising, %s in %d s"), change_text(t.delta, unit), over);
    case Trend::Falling: return wxString::Format(_L("Falling, %s in %d s"), change_text(t.delta, unit), over);
    case Trend::Flat: return wxString::Format(_L("Flat, %s in %d s"), change_text(t.delta, unit), over);
    default: return _L("Not enough readings for a trend yet");
    }
}

// The trend in one word, for the small charts.
wxString trend_word(const TrendInfo &t)
{
    switch (t.trend) {
    case Trend::Holding: return _L("Holding");
    case Trend::Rising: return _L("Rising");
    case Trend::Falling: return _L("Falling");
    case Trend::Flat: return _L("Flat");
    default: return _L("Not enough readings yet");
    }
}

std::uint32_t trend_glyph(const TrendInfo &t)
{
    if (t.trend == Trend::Rising) return GlyphUp;
    if (t.trend == Trend::Falling) return GlyphDown;
    if (t.trend == Trend::Unknown) return 0;
    return GlyphFlat;
}

wxString momentum_words(const MomentumInfo &m, Unit unit)
{
    switch (m.direction) {
    case Momentum::Up: return wxString::Format(_L("Going up, %s a second"), rate_text(m.rate, unit));
    case Momentum::Down: return wxString::Format(_L("Going down, %s a second"), rate_text(m.rate, unit));
    case Momentum::Steady: return _L("Steady for now");
    default: return wxString();
    }
}

wxString minutes_text(int minutes)
{
    if (minutes >= 60) return wxString::Format(_L("%d h %d min"), minutes / 60, minutes % 60);
    return wxString::Format(_L("%d min"), minutes);
}

wxString drying_words(const AmsUnit &unit)
{
    if (!unit.can_dry) return wxString();
    if (!unit.drying) return _L("drying off");
    if (unit.dry_minutes_left > 0) return wxString::Format(_L("drying on, %s left"), minutes_text(unit.dry_minutes_left));
    return _L("drying on");
}

// Whether Windows shows times on a 12-hour clock, for the clock until one is chosen.
bool windows_twelve_hour()
{
#ifdef _WIN32
    wchar_t format[80] = {};
    if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, format, int(std::size(format))) > 0)
        return twelve_hour_format(into_u8(wxString(format)));
#endif
    return false;
}

Label *label(wxWindow *parent, const wxFont &font, const wxString &text, bool muted = false, bool wrap = false)
{
    auto *l = new Label(parent, font, text, wrap ? (LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0);
    if (wrap) l->SetMinSize(wxSize(parent->FromDIP(120), -1));
    l->SetBackgroundColour(parent->GetBackgroundColour());
    l->SetForegroundColour(role(muted ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    return l;
}

Button *chip(wxWindow *parent, const wxString &text, std::uint32_t glyph = 0)
{
    auto *b = new Button(parent, text);
    b->SetVariant(Button::Variant::Outlined);
    b->SetButtonSize(Button::Size::Small);
    if (glyph) b->SetGlyph(glyph);
    return b;
}

// What the large chart draws on a refresh tick.
struct ChartView
{
    std::vector<Reading> readings;
    double               now = 0, window = 300, target = 0;
    bool                 reference = true, candles = false, momentum = true, particles = false, reduced = false, paused = false;
    Feed                 feed = Feed::Waiting;
    Bounds               bounds;
    bool                 has_session_high = false;
    double               session_high     = 0;
    wxString             note; // why there is nothing to chart, instead of the feed state
    std::string          key;  // the printer and series shown: a change starts the range and the highs again
};

} // namespace

// ---------------------------------------------------------------- recorder

namespace {

// Looks at the selected printer twice a second, whichever page is open.
class Sampler : public wxTimer
{
public:
    void Notify() override
    {
        DeviceManager *dm = wxGetApp().getDeviceManager();
        if (dm == nullptr) return;
        MachineObject *obj = dm->get_selected_machine();
        if (obj && obj->is_info_ready()) Recorder::instance().record(obj);
    }
};

} // namespace

Recorder &Recorder::instance()
{
    static Recorder recorder;
    return recorder;
}

void Recorder::start_sampling()
{
    if (m_sampler) return;
    m_sampler = std::make_unique<Sampler>();
    m_sampler->Start(500);
}

void Recorder::stop_sampling()
{
    if (m_sampler) m_sampler->Stop();
    m_sampler.reset();
}

void Recorder::record(MachineObject *obj)
{
    if (!obj) return;
    const std::string id = obj->get_dev_id();
    if (id.empty()) return;
    DeviceRecord &d = m_devices[id];
    d.name          = obj->get_dev_name();
    d.online        = obj->is_online();
    const double push = std::chrono::duration<double>(obj->last_push_time.time_since_epoch()).count();
    if (push <= 0 || push <= d.last_push) return; // nothing new from the printer
    d.last_push = push;
    if (DevExtderSystem *ext = obj->GetExtderSystem()) {
        d.series[int(Kind::Nozzle)].add(push, double(ext->GetNozzleTempCurrent(MAIN_EXTRUDER_ID)));
        d.target[int(Kind::Nozzle)] = double(ext->GetNozzleTempTarget(MAIN_EXTRUDER_ID));
        ++d.total;
    }
    if (DevBed *bed = obj->GetBed()) {
        d.series[int(Kind::Bed)].add(push, double(bed->GetBedTemp()));
        d.target[int(Kind::Bed)] = double(bed->GetBedTempTarget());
        ++d.total;
    }
    if (const auto chamber = obj->GetChamber()) {
        const double v = chamber->GetChamberTemp();
        if (v > 0) d.has_chamber = true;
        if (d.has_chamber) {
            d.series[int(Kind::Chamber)].add(push, v);
            d.target[int(Kind::Chamber)] = chamber->GetChamberTempTarget();
            ++d.total;
        }
    }
    // The humidity inside each AMS unit that reports one: the percentage when
    // the unit sends it, otherwise its level from 1 (wet) to 5 (dry).
    if (const auto fila = obj->GetFilaSystem()) {
        for (const auto &[ams_id, ams] : fila->GetAmsList()) {
            if (ams == nullptr) continue;
            double value   = -1;
            bool   percent = true;
            if (ams->SupportHumidityPercent() && ams->GetHumidityPercent() >= 0 && ams->GetHumidityPercent() <= 100) {
                value = double(ams->GetHumidityPercent());
            } else if (ams->SupportHumidityLevel() && ams->GetHumidityLevel() >= 1 && ams->GetHumidityLevel() <= 5) {
                value   = double(ams->GetHumidityLevel());
                percent = false;
            }
            if (value < 0) continue;
            AmsUnit &unit = d.ams[ams_id];
            if (!unit.series.readings().empty() && unit.percent != percent) unit.series.clear(); // never mix the two scales
            unit.name             = into_u8(ams->GetDisplayName());
            unit.percent          = percent;
            unit.can_dry          = ams->SupportDrying();
            unit.drying           = ams->AmsIsDrying();
            unit.dry_minutes_left = std::max(0, ams->GetLeftDryTime());
            unit.series.add(push, value);
            ++d.total;
        }
    }
    // The part cooling fan, as the percentage the Device page shows.
    if (DevFan *fan = obj->GetFan()) {
        d.series[int(Kind::Fan)].add(push, double(FanMotionMath::percent_from_pwm(fan->GetCoolingFanSpeed())));
        d.has_fan = true;
        ++d.total;
    }
}

const DeviceRecord *Recorder::find(const std::string &dev_id) const
{
    const auto it = m_devices.find(dev_id);
    return it == m_devices.end() ? nullptr : &it->second;
}

void Recorder::set_offline(const std::string &dev_id)
{
    const auto it = m_devices.find(dev_id);
    if (it != m_devices.end()) it->second.online = false;
}

// ---------------------------------------------------------------- the chart

class ChartCanvas : public wxWindow
{
public:
    ChartCanvas(wxWindow *parent, LiveChartsPanel *owner)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE), m_owner(owner), m_fx_timer(this)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(wxSize(480, 280)));
        SetName(_L("Live chart"));
        Bind(wxEVT_PAINT, &ChartCanvas::on_paint, this);
        Bind(wxEVT_MOTION, [this](wxMouseEvent &e) { hover(e.GetPosition().x); });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &) {
            if (!HasFocus()) {
                m_cross = -1;
                Refresh();
            }
        });
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
            SetFocus();
            hover(e.GetPosition().x);
        });
        Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent &e) {
            wxPoint at = e.GetPosition();
            if (at == wxDefaultPosition) at = ClientToScreen(wxPoint(GetSize().x / 2, GetSize().y / 2));
            m_owner->show_menu(at);
        });
        Bind(wxEVT_KEY_DOWN, &ChartCanvas::on_key, this);
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) { step_particles(); }, m_fx_timer.GetId());
    }
    ~ChartCanvas() override { m_fx_timer.Stop(); }

    bool AcceptsFocus() const override { return true; }

    // The readings and settings to draw; called on every refresh tick.
    void set_data(ChartView view)
    {
        const bool restart = view.key != m_view.key;
        m_view             = std::move(view);
        if (m_cross >= int(m_view.readings.size())) m_cross = int(m_view.readings.size()) - 1;
        Range want = value_range(segments(m_view.readings, kGap), {m_view.reference ? m_view.target : 0.0});
        if (m_view.bounds.on) want = bounded(want, m_view.bounds.floor, m_view.bounds.ceiling);
        if (restart) {
            // Another printer or series: its highs so far set the mark, and nothing bursts for them.
            m_have_range = false;
            m_highs.reset();
            if (m_view.has_session_high) m_highs.prime({Reading{0, m_view.session_high}});
            m_highs.prime(m_view.readings);
            m_particles.clear();
            m_high_note_until = 0;
            m_seen            = m_view.readings.empty() ? 0 : m_view.readings.back().t;
        }
        if (!m_have_range || m_view.reduced) {
            m_low        = want.low;
            m_high       = want.high;
            m_have_range = true;
        } else {
            // Ease toward each reading instead of jumping.
            m_low += (want.low - m_low) * 0.35;
            m_high += (want.high - m_high) * 0.35;
        }
        m_momentum = momentum(m_view.readings);

        // New highs: every reading that arrived since the last tick goes past the watch, and one burst at most follows.
        bool burst = false;
        for (const Reading &r : m_view.readings)
            if (r.t > m_seen) {
                if (m_highs.see(r)) burst = true;
                m_seen = r.t;
            }
        if (!m_view.particles || m_view.reduced) m_particles.clear();
        if (burst && m_view.particles && !m_view.paused && m_view.feed == Feed::Live && !m_view.readings.empty()) {
            if (m_view.reduced) {
                m_high_note_until = now_seconds() + 3; // a still note instead of particles
            } else {
                const wxRect   p    = plot();
                const Reading &last = m_view.readings.back();
                m_particles.burst(x_of(last.t, p), y_of(last.value, p), 28, std::uint32_t(++m_bursts) * 2654435761u + 7u, FromDIP(100) / 100.0);
                if (!m_fx_timer.IsRunning()) {
                    m_fx_last = std::chrono::steady_clock::now();
                    m_fx_timer.Start(33);
                }
            }
        }
        ++m_tick;
        Refresh();
    }

    int                         cross() const { return m_cross; }
    const std::vector<Reading> &readings() const { return m_view.readings; }
    void                        reset_range() { m_have_range = false; }

private:
    bool arrow_shown() const { return m_view.momentum && !m_view.candles && m_view.feed == Feed::Live && !m_view.readings.empty(); }

    wxRect plot() const
    {
        // Room on the right for the momentum arrow when it is on.
        const wxSize s     = GetClientSize();
        const int    right = FromDIP(m_view.momentum && !m_view.candles ? 44 : 14);
        return wxRect(FromDIP(56), FromDIP(12), std::max(10, s.x - FromDIP(56) - right), std::max(10, s.y - FromDIP(12) - FromDIP(28)));
    }
    double x_of(double t, const wxRect &p) const { return p.x + (t - (m_view.now - m_view.window)) / m_view.window * p.width; }
    double y_of(double v, const wxRect &p) const { return p.y + p.height - (v - m_low) / std::max(1e-9, m_high - m_low) * p.height; }

    void hover(int x)
    {
        const wxRect p = plot();
        const double t = (m_view.now - m_view.window) + double(x - p.x) / std::max(1, p.width) * m_view.window;
        m_cross        = nearest(m_view.readings, t);
        m_owner->refresh();
    }

    void step_particles()
    {
        const auto   at = std::chrono::steady_clock::now();
        const double dt = std::min(0.1, std::chrono::duration<double>(at - m_fx_last).count());
        m_fx_last       = at;
        m_particles.step(dt);
        if (m_particles.empty()) m_fx_timer.Stop();
        Refresh();
    }

    void on_key(wxKeyEvent &e)
    {
        const int  n    = int(m_view.readings.size());
        const bool ctrl = e.ControlDown();
        switch (e.GetKeyCode()) {
        case WXK_LEFT: m_cross = n == 0 ? -1 : m_cross < 0 ? n - 1 : std::max(0, m_cross - 1); break;
        case WXK_RIGHT: m_cross = n == 0 ? -1 : m_cross < 0 ? n - 1 : std::min(n - 1, m_cross + 1); break;
        case WXK_HOME: m_cross = n ? 0 : -1; break;
        case WXK_END: m_cross = n - 1; break;
        case WXK_SPACE: m_owner->set_paused(!m_owner->m_paused); return;
        case WXK_ESCAPE: m_cross = -1; break;
        case WXK_F1: m_owner->explain(); return;
        default:
            if (ctrl && (e.GetKeyCode() == 'C')) {
                m_owner->copy_value();
                return;
            }
            if (ctrl && (e.GetKeyCode() == 'S')) {
                m_owner->save_csv();
                return;
            }
            e.Skip();
            return;
        }
        m_owner->refresh();
    }

    // The momentum arrow beside the newest point: up in the accent colour, down
    // in the error colour, steady in the muted colour.
    void draw_arrow(wxGraphicsContext &gc, const wxRect &p)
    {
        const Reading &last  = m_view.readings.back();
        const double   x     = x_of(last.t, p), y = y_of(last.value, p);
        const double   px_s  = p.width / std::max(1.0, m_view.window);
        const double   px_u  = p.height / std::max(1e-9, m_high - m_low);
        double         angle = 0;
        if (m_momentum.direction == Momentum::Up) angle = std::min(arrow_angle(m_momentum.rate, px_s, px_u), -0.26);
        else if (m_momentum.direction == Momentum::Down) angle = std::max(arrow_angle(m_momentum.rate, px_s, px_u), 0.26);
        const MD3::Role tone = m_momentum.direction == Momentum::Up     ? MD3::Role::Primary
                             : m_momentum.direction == Momentum::Down   ? MD3::Role::Error
                                                                        : MD3::Role::OnSurfaceVariant;
        const double nudge  = m_view.reduced || m_momentum.direction == Momentum::Steady ? 0.0 : (m_tick % 6 < 3 ? FromDIP(2) : 0.0);
        const double gap    = FromDIP(9) + nudge, length = FromDIP(16), head = FromDIP(6);
        const double ca     = std::cos(angle), sa = std::sin(angle);
        const double x0     = x + ca * gap, y0 = y + sa * gap;
        const double x1     = x + ca * (gap + length), y1 = y + sa * (gap + length);
        constexpr double back = 0.61; // the head's strokes, about 35 degrees either side
        gc.SetPen(wxPen(role(tone), FromDIP(2)));
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(x0, y0);
        path.AddLineToPoint(x1, y1);
        path.MoveToPoint(x1 - head * std::cos(angle - back), y1 - head * std::sin(angle - back));
        path.AddLineToPoint(x1, y1);
        path.AddLineToPoint(x1 - head * std::cos(angle + back), y1 - head * std::sin(angle + back));
        gc.StrokePath(path);
    }

    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxColour        surface = role(MD3::Role::SurfaceContainerLowest);
        dc.SetBackground(wxBrush(surface));
        dc.Clear();
        const wxRect p = plot();
        dc.SetFont(Label::Body_12);

        // Grid and value labels: one decimal when the range is narrow.
        dc.SetTextForeground(role(MD3::Role::OnSurfaceVariant));
        const char *format = m_high - m_low < 8 ? "%.1f" : "%.0f";
        for (int i = 0; i <= 4; ++i) {
            const double v = m_low + (m_high - m_low) * i / 4.0;
            const int    y = int(std::lround(y_of(v, p)));
            dc.SetPen(wxPen(role(MD3::Role::OutlineVariant), 1, i == 0 ? wxPENSTYLE_SOLID : wxPENSTYLE_DOT));
            dc.DrawLine(p.x, y, p.GetRight(), y);
            const wxString text = wxString::Format(format, v);
            const wxSize   ts   = dc.GetTextExtent(text);
            dc.DrawText(text, p.x - ts.x - FromDIP(8), y - ts.y / 2);
        }
        // Time labels, on the clock chosen (12-hour or 24-hour).
        const bool seconds = m_view.window <= 300;
        for (int i = 0; i <= 5; ++i) {
            const double   t    = m_view.now - m_view.window + m_view.window * i / 5.0;
            const wxString text = i == 5 ? _L("now") : m_owner->time_text(t, seconds);
            const wxSize   ts   = dc.GetTextExtent(text);
            int            x    = int(std::lround(x_of(t, p))) - ts.x / 2;
            x                   = std::clamp(x, p.x - ts.x / 2, p.GetRight() - ts.x);
            dc.DrawText(text, x, p.GetBottom() + FromDIP(8));
        }

        // The target as a reference line.
        if (m_view.reference && m_view.target > 0 && m_view.target >= m_low && m_view.target <= m_high) {
            const int y = int(std::lround(y_of(m_view.target, p)));
            dc.SetPen(wxPen(role(MD3::Role::Outline), 1, wxPENSTYLE_SHORT_DASH));
            dc.DrawLine(p.x, y, p.GetRight(), y);
            const wxString text = wxString::Format(_L("Target %.0f"), m_view.target);
            dc.DrawText(text, p.GetRight() - dc.GetTextExtent(text).x, y - dc.GetTextExtent(text).y - FromDIP(2));
        }

        dc.SetClippingRegion(p);
        const wxColour line = role(MD3::Role::Primary);
        if (m_view.candles) {
            const double width = 60.0;
            const double start = std::floor((m_view.now - m_view.window) / width) * width;
            for (const Candle &c : candles(m_view.readings, start, width, m_view.now)) {
                const bool up    = c.close >= c.open;
                wxColour   color = up ? line : role(MD3::Role::Error);
                const int  x0 = int(x_of(c.start, p)), x1 = int(x_of(c.end, p));
                const int  w  = std::max(FromDIP(3), (x1 - x0) * 6 / 10);
                const int  cx = (x0 + x1) / 2;
                dc.SetPen(wxPen(color, 1));
                dc.DrawLine(cx, int(y_of(c.high, p)), cx, int(y_of(c.low, p)));
                const int top = int(y_of(std::max(c.open, c.close), p)), bottom = int(y_of(std::min(c.open, c.close), p));
                dc.SetBrush(c.forming ? *wxTRANSPARENT_BRUSH : wxBrush(color));
                dc.DrawRectangle(cx - w / 2, top, w, std::max(FromDIP(2), bottom - top));
            }
        } else {
            dc.SetPen(wxPen(line, FromDIP(2)));
            for (const auto &part : segments(m_view.readings, kGap)) {
                std::vector<wxPoint> points;
                for (const Reading &r : part) points.emplace_back(int(std::lround(x_of(r.t, p))), int(std::lround(y_of(r.value, p))));
                if (points.size() == 1) dc.DrawCircle(points.front(), FromDIP(2));
                else if (points.size() > 1) dc.DrawLines(int(points.size()), points.data());
            }
            if (!m_view.readings.empty() && m_view.feed == Feed::Live) {
                const Reading &last = m_view.readings.back();
                const int      r    = FromDIP(4) + (m_view.reduced ? 0 : int(m_tick % 6 < 3 ? FromDIP(1) : 0));
                dc.SetBrush(wxBrush(line));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawCircle(int(x_of(last.t, p)), int(y_of(last.value, p)), r);
            }
        }
        // Stalled: a dashed line where the readings stopped; nothing is drawn for the gap.
        if (m_view.feed == Feed::Stalled && !m_view.readings.empty()) {
            const int x = int(std::lround(x_of(m_view.readings.back().t, p)));
            dc.SetPen(wxPen(role(MD3::Role::Error), 1, wxPENSTYLE_SHORT_DASH));
            dc.DrawLine(x, p.y, x, p.GetBottom());
        }
        // The crosshair.
        if (m_cross >= 0 && m_cross < int(m_view.readings.size())) {
            const Reading &r = m_view.readings[std::size_t(m_cross)];
            const int      x = int(std::lround(x_of(r.t, p))), y = int(std::lround(y_of(r.value, p)));
            dc.SetPen(wxPen(role(MD3::Role::OnSurfaceVariant), 1, wxPENSTYLE_DOT));
            dc.DrawLine(x, p.y, x, p.GetBottom());
            dc.SetBrush(wxBrush(surface));
            dc.SetPen(wxPen(line, FromDIP(2)));
            dc.DrawCircle(x, y, FromDIP(5));
        }
        // With reduced motion a new high gets a still note instead of particles.
        if (m_high_note_until > now_seconds() && !m_view.readings.empty()) {
            const Reading &last = m_view.readings.back();
            dc.SetFont(Label::Head_12);
            dc.SetTextForeground(line);
            const wxString text = _L("New high");
            const wxSize   ts   = dc.GetTextExtent(text);
            const int      tx   = std::clamp(int(x_of(last.t, p)) - ts.x - FromDIP(8), p.x, std::max(p.x, p.GetRight() - ts.x));
            const int      ty   = std::clamp(int(y_of(last.value, p)) - ts.y - FromDIP(8), p.y, std::max(p.y, p.GetBottom() - ts.y));
            dc.DrawText(text, tx, ty);
        }
        dc.DestroyClippingRegion();

        // The momentum arrow and the particles, smooth and see-through.
        if (arrow_shown() || !m_particles.empty()) {
            std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
            if (gc) {
                gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
                if (arrow_shown()) draw_arrow(*gc, p);
                if (!m_particles.empty()) {
                    gc->Clip(p.x, p.y, p.width + FromDIP(30), p.height);
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    for (const Particle &particle : m_particles.live()) {
                        gc->SetBrush(wxBrush(with_alpha(line, 0.9 * Particles::fade(particle))));
                        gc->DrawEllipse(particle.x - particle.size, particle.y - particle.size, particle.size * 2, particle.size * 2);
                    }
                    gc->ResetClip();
                }
            }
        }

        // The feed state over the chart.
        wxString state;
        if (!m_view.note.IsEmpty()) state = m_view.note;
        else if (m_view.feed == Feed::Offline) state = _L("The printer is off or not connected");
        else if (m_view.feed == Feed::Waiting) state = _L("Asking the printer for its first reading");
        else if (m_view.readings.empty()) state = _L("No readings in this time window");
        if (!state.IsEmpty()) {
            dc.SetFont(Label::Body_14);
            dc.SetTextForeground(role(MD3::Role::OnSurfaceVariant));
            const wxSize ts = dc.GetTextExtent(state);
            dc.DrawText(state, p.x + (p.width - ts.x) / 2, p.y + (p.height - ts.y) / 2);
        }
        if (HasFocus()) {
            dc.SetPen(wxPen(role(MD3::Role::Primary), FromDIP(2)));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(GetClientRect().Deflate(FromDIP(1)));
        }
    }

    LiveChartsPanel                      *m_owner;
    ChartView                             m_view;
    double                                m_low = 0, m_high = 1;
    bool                                  m_have_range = false;
    int                                   m_cross      = -1;
    unsigned                              m_tick       = 0;
    MomentumInfo                          m_momentum;
    HighWatch                             m_highs;
    double                                m_seen            = 0;
    double                                m_high_note_until = 0;
    Particles                             m_particles;
    unsigned                              m_bursts = 0;
    wxTimer                               m_fx_timer;
    std::chrono::steady_clock::time_point m_fx_last;
};

// ---------------------------------------------------------------- the small charts

// AMS humidity and the part cooling fan at a glance: the value, the trend in a
// word (and whether the AMS is drying) and the readings of the same time
// window. Click it, or focus it and press Enter or Space, to show the series
// in the large chart.
class MiniChart : public wxWindow
{
public:
    struct Content
    {
        wxString             title, value, words;
        std::uint32_t        trend_glyph = 0;
        MD3::Role            words_role  = MD3::Role::OnSurfaceVariant;
        std::vector<Reading> readings;
        double               now = 0, window = 300;
        Bounds               bounds;
        bool                 selected = false;
    };

    MiniChart(wxWindow *parent, LiveChartsPanel *owner, Kind kind, std::uint32_t glyph)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE), m_owner(owner), m_kind(kind), m_glyph(glyph)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(wxSize(260, 150)));
        SetCursor(wxCursor(wxCURSOR_HAND));
        Bind(wxEVT_PAINT, &MiniChart::on_paint, this);
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) { choose(); });
        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
            const int key = e.GetKeyCode();
            if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER || key == WXK_SPACE) choose();
            else e.Skip();
        });
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &) {
            m_hover = true;
            Refresh();
        });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &) {
            m_hover = false;
            Refresh();
        });
    }

    bool AcceptsFocus() const override { return true; }

    void set(Content content)
    {
        m_content = std::move(content);
        // A screen reader hears the title, the value and the words.
        SetName(m_content.title + ", " + m_content.value + ", " + m_content.words);
        const wxString tip = m_content.selected ? m_content.words : wxString::Format(_L("Show %s in the large chart"), m_content.title);
        if (tip != m_tip) {
            m_tip = tip;
            SetToolTip(tip);
        }
        Refresh();
    }

private:
    void choose() { m_owner->set_kind(int(m_kind)); }

    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();
        const wxRect r      = GetClientRect();
        const int    pad    = FromDIP(14);
        const double radius = FromDIP(16);
        const bool   chosen = m_content.selected;
        const wxColour card = role(chosen ? MD3::Role::SecondaryContainer : m_hover ? MD3::Role::SurfaceContainer : MD3::Role::SurfaceContainerLow);
        const wxColour on    = role(chosen ? MD3::Role::OnSecondaryContainer : MD3::Role::OnSurface);
        const wxColour muted = role(chosen ? MD3::Role::OnSecondaryContainer : MD3::Role::OnSurfaceVariant);
        {
            std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
            if (gc) {
                gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
                gc->SetBrush(wxBrush(card));
                gc->SetPen(chosen ? wxPen(role(MD3::Role::Primary), FromDIP(2)) : *wxTRANSPARENT_PEN);
                gc->DrawRoundedRectangle(r.x + 1, r.y + 1, r.width - 2, r.height - 2, radius);
            }
        }

        // The icon and title, then the value with the trend beside it.
        const int icon = FromDIP(18);
        MaterialIcon::draw(dc, m_glyph, icon, muted, wxPoint(pad, pad));
        dc.SetFont(Label::Body_13);
        dc.SetTextForeground(muted);
        const int title_x = pad + icon + FromDIP(6);
        dc.DrawText(wxControl::Ellipsize(m_content.title, dc, wxELLIPSIZE_END, std::max(10, r.width - title_x - pad)), title_x, pad);
        dc.SetFont(Label::Head_24);
        dc.SetTextForeground(on);
        const int    value_y = pad + FromDIP(24);
        const wxSize vs      = dc.GetTextExtent(m_content.value);
        dc.DrawText(m_content.value, pad, value_y);
        int x = pad + vs.x + FromDIP(10);
        const wxColour words = chosen ? on : role(m_content.words_role);
        if (m_content.trend_glyph) {
            MaterialIcon::draw(dc, m_content.trend_glyph, FromDIP(16), words, wxPoint(x, value_y + (vs.y - FromDIP(16)) / 2));
            x += FromDIP(20);
        }
        dc.SetFont(Label::Body_12);
        dc.SetTextForeground(words);
        const wxString shown_words = wxControl::Ellipsize(m_content.words, dc, wxELLIPSIZE_END, std::max(10, r.width - x - pad));
        dc.DrawText(shown_words, x, value_y + (vs.y - dc.GetTextExtent(shown_words).y) / 2);

        // The readings of the same window as the large chart, breaking at gaps.
        const int    top   = value_y + vs.y + FromDIP(8);
        const wxRect spark(pad, top, std::max(10, r.width - 2 * pad), std::max(0, r.GetBottom() - pad - top));
        if (spark.height >= FromDIP(16)) {
            if (m_content.readings.empty()) {
                dc.SetFont(Label::Body_12);
                dc.SetTextForeground(muted);
                const wxString none = _L("No readings in this time window");
                const wxSize   ns   = dc.GetTextExtent(none);
                dc.DrawText(none, spark.x + (spark.width - ns.x) / 2, spark.y + (spark.height - ns.y) / 2);
            } else {
                Range range = value_range(segments(m_content.readings, kGap), {});
                if (m_content.bounds.on) range = bounded(range, m_content.bounds.floor, m_content.bounds.ceiling);
                const double from = m_content.now - m_content.window;
                const auto   sx   = [&](double t) { return spark.x + (t - from) / std::max(1.0, m_content.window) * spark.width; };
                const auto   sy   = [&](double v) { return spark.y + spark.height - (v - range.low) / std::max(1e-9, range.high - range.low) * spark.height; };
                std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
                if (gc) {
                    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
                    gc->Clip(spark.x, spark.y - FromDIP(2), spark.width, spark.height + FromDIP(4));
                    gc->SetPen(wxPen(role(MD3::Role::Primary), FromDIP(2)));
                    for (const auto &part : segments(m_content.readings, kGap)) {
                        wxGraphicsPath path = gc->CreatePath();
                        path.MoveToPoint(sx(part.front().t), sy(part.front().value));
                        for (std::size_t i = 1; i < part.size(); ++i) path.AddLineToPoint(sx(part[i].t), sy(part[i].value));
                        if (part.size() == 1) path.AddCircle(sx(part.front().t), sy(part.front().value), FromDIP(1));
                        gc->StrokePath(path);
                    }
                }
            }
        }
        if (HasFocus()) {
            std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
            if (gc) {
                gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                gc->SetPen(wxPen(role(MD3::Role::Primary), FromDIP(2)));
                gc->DrawRoundedRectangle(r.x + FromDIP(3), r.y + FromDIP(3), r.width - FromDIP(6), r.height - FromDIP(6), radius - FromDIP(2));
            }
        }
    }

    LiveChartsPanel *m_owner;
    Kind             m_kind;
    std::uint32_t    m_glyph;
    Content          m_content;
    wxString         m_tip;
    bool             m_hover = false;
};

// ---------------------------------------------------------------- the panel

void LiveChartsPanel::open(int kind)
{
    MainFrame *frame = wxGetApp().mainframe;
    if (frame == nullptr) return;
    wxPanel *page = frame->open_service("live-charts", _L("Live charts"), [](wxWindow *parent) { return new LiveChartsPanel(parent); });
    if (auto *charts = dynamic_cast<LiveChartsPanel *>(page); charts && kind >= 0 && kind < int(Kind::Count)) charts->set_kind(kind);
}

LiveChartsPanel::LiveChartsPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY), m_timer(this)
{
    SetName("live-charts");
    SetBackgroundColour(role(MD3::Role::Surface));
    Recorder::instance().start_sampling(); // already running from startup; this is a safety net
    if (AppConfig *cfg = wxGetApp().app_config)
        m_options = options_from(cfg->get(kReferenceKey), cfg->get(kMomentumKey), cfg->get(kParticlesKey), cfg->get(kClockKey), windows_twelve_hour());
    else
        m_options = options_from("", "", "", "", windows_twelve_hour());

    // The page scrolls when the window is shorter than it.
    auto *outer = new wxBoxSizer(wxVERTICAL);
    auto *body  = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    body->SetBackgroundColour(role(MD3::Role::Surface));
    body->SetScrollRate(0, FromDIP(24));
    m_body = body;
    outer->Add(body, 1, wxEXPAND);
    SetSizer(outer);

    wxWindow *page = body;
    const int pad  = FromDIP(MD3::Metrics::active().padding);
    auto     *root = new wxBoxSizer(wxVERTICAL);
    m_root         = root;

    auto *head   = new wxBoxSizer(wxHORIZONTAL);
    auto *titles = new wxBoxSizer(wxVERTICAL);
    titles->Add(label(page, Label::Head_20, _L("Live charts")), 0);
    titles->Add(label(page, Label::Body_13,
                      _L("The temperatures, the AMS humidity and the part cooling fan of the printer selected on the Device page, as it "
                         "sends them. A gap in its reports stays a gap: nothing is drawn that the printer did not send."),
                      true, true),
                0, wxEXPAND | wxTOP, FromDIP(4));
    m_device = label(page, Label::Head_14, wxString());
    titles->Add(m_device, 0, wxTOP, FromDIP(8));
    head->Add(titles, 1, wxEXPAND | wxRIGHT, pad);

    auto *now = new wxBoxSizer(wxVERTICAL);
    now->Add(label(page, Label::Head_12, _L("Now"), true), 0, wxBOTTOM, FromDIP(4));
    for (int k = 0; k < 3; ++k) {
        m_now[k] = label(page, Label::Body_14, wxString());
        now->Add(m_now[k], 0, wxBOTTOM, FromDIP(2));
    }
    m_session = label(page, Label::Body_12, wxString(), true);
    now->Add(m_session, 0, wxTOP, FromDIP(4));
    head->Add(now, 0);
    root->Add(head, 0, wxEXPAND | wxALL, pad);

    // Series, time window, view and pause.
    auto *controls = new wxWrapSizer(wxHORIZONTAL);
    for (int k = 0; k < int(Kind::Count); ++k) {
        auto *c = chip(page, kind_name(k), k == int(Kind::Humidity) ? GlyphHumidity : k == int(Kind::Fan) ? GlyphFan : 0);
        if (k == int(Kind::Humidity)) c->SetToolTip(_L("The humidity inside each AMS that reports it: a percentage, or a level from 1 (wet) to 5 (dry)"));
        if (k == int(Kind::Fan)) c->SetToolTip(_L("The part cooling fan speed, as the Device page shows it"));
        c->Bind(wxEVT_BUTTON, [this, k](wxCommandEvent &) { set_kind(k); });
        m_kind_chips.push_back(c);
        controls->Add(c, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    controls->AddSpacer(FromDIP(16));
    const wxString windows[] = {_L("1 min"), _L("5 min"), _L("15 min"), _L("1 h")};
    for (int w = 0; w < 4; ++w) {
        auto *c = chip(page, windows[w]);
        c->Bind(wxEVT_BUTTON, [this, w](wxCommandEvent &) { set_window(w); });
        m_window_chips.push_back(c);
        controls->Add(c, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    controls->AddSpacer(FromDIP(16));
    for (int v = 0; v < 2; ++v) {
        auto *c = chip(page, v == 0 ? _L("Line") : _L("Candles"));
        c->SetToolTip(v == 0 ? _L("Every reading, joined except across gaps") : _L("One candle per minute: first, highest, lowest and last reading"));
        c->Bind(wxEVT_BUTTON, [this, v](wxCommandEvent &) { set_view(v == 1); });
        m_view_chips.push_back(c);
        controls->Add(c, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    controls->AddSpacer(FromDIP(16));
    m_pause = chip(page, _L("Pause"), GlyphPause);
    m_pause->SetToolTip(_L("Freeze the chart; readings keep arriving and are shown when you resume (Space)"));
    m_pause->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { set_paused(!m_paused); });
    controls->Add(m_pause, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    root->Add(controls, 0, wxEXPAND | wxLEFT | wxRIGHT, pad);

    // Which AMS, when more than one reports humidity.
    m_ams_row = new wxWrapSizer(wxHORIZONTAL);
    m_ams_row->Add(label(page, Label::Body_13, _L("AMS unit")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(8));
    root->Add(m_ams_row, 0, wxEXPAND | wxLEFT | wxRIGHT, pad);
    root->Show(m_ams_row, false);

    // Chart options, saved for next time (the preview of reduced motion is not).
    auto *options = new wxWrapSizer(wxHORIZONTAL);
    const auto add_switch = [&](const wxString &text, const wxString &tip, bool on, const std::function<void(bool)> &changed) {
        auto *name = label(page, Label::Body_13, text);
        name->SetToolTip(tip);
        options->Add(name, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(6));
        auto *sw = new SwitchButton(page);
        sw->SetName(text);
        sw->SetToolTip(tip);
        sw->SetValue(on);
        sw->Bind(wxEVT_TOGGLEBUTTON, [sw, changed](wxCommandEvent &) { changed(sw->GetValue()); });
        options->Add(sw, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(16));
        return sw;
    };
    m_reference_switch = add_switch(_L("Target line"), _L("The target temperature as a dashed reference line"), m_options.reference,
                                    [this](bool on) { set_option(kReferenceKey, on); });
    m_momentum_switch  = add_switch(_L("Momentum arrow"),
                                    _L("An arrow beside the newest point shows which way the last ten seconds went: up, down or steady"),
                                    m_options.momentum, [this](bool on) { set_option(kMomentumKey, on); });
    m_particles_switch = add_switch(_L("Particles on new highs"),
                                    _L("A short burst of particles when a reading beats every earlier one this session. With reduced motion a "
                                       "still New high note shows instead."),
                                    m_options.particles, [this](bool on) { set_option(kParticlesKey, on); });
    options->Add(label(page, Label::Body_13, _L("Clock")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(6));
    for (int c = 0; c < 2; ++c) {
        auto *b = chip(page, c == 0 ? _L("24 h") : _L("12 h"));
        b->SetToolTip(c == 0 ? _L("Times such as 13:05") : _L("Times such as 1:05 PM"));
        b->Bind(wxEVT_BUTTON, [this, c](wxCommandEvent &) { set_clock(c == 1); });
        m_clock_chips.push_back(b);
        options->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    options->AddSpacer(FromDIP(12));
    m_preview_switch = add_switch(_L("Preview reduced motion"),
                                  _L("See this page as it is with reduced motion: the line snaps to each reading, the dot stops pulsing and no "
                                     "particles fly. Every value, trend and state stays the same."),
                                  false, [this](bool on) {
                                      m_preview_reduced = on;
                                      refresh();
                                  });
    auto *motion = new Button(page, _L("Interface motion"));
    motion->SetVariant(Button::Variant::Text);
    motion->SetButtonSize(Button::Size::Small);
    motion->SetGlyph(GlyphMotion);
    motion->SetToolTip(_L("Animations here follow Preferences, Interface motion, or the Windows setting Show animations"));
    motion->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { open_motion_settings(); });
    options->Add(motion, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    root->Add(options, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *summary = new wxBoxSizer(wxHORIZONTAL);
    m_value = label(page, Label::Head_24, wxString());
    summary->Add(m_value, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    m_trend = label(page, Label::Body_14, wxString());
    summary->Add(m_trend, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    m_momentum = label(page, Label::Body_13, wxString());
    m_momentum->SetName("live-chart-momentum");
    summary->Add(m_momentum, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    m_feed = label(page, Label::Head_13, wxString());
    summary->Add(m_feed, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(summary, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *box = new StaticBox(page);
    box->SetCornerRadius(FromDIP(16));
    box->SetBorderWidth(0);
    box->SetBackgroundColor(role(MD3::Role::SurfaceContainerLowest));
    box->SetBackgroundColour(role(MD3::Role::SurfaceContainerLowest));
    auto *bs = new wxBoxSizer(wxVERTICAL);
    m_canvas = new ChartCanvas(box, this);
    bs->Add(m_canvas, 1, wxEXPAND | wxALL, FromDIP(12));
    box->SetSizer(bs);
    root->Add(box, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    // AMS humidity and the part cooling fan at a glance; each opens in the large chart.
    auto *cards = new wxBoxSizer(wxHORIZONTAL);
    m_humidity_card = new MiniChart(page, this, Kind::Humidity, GlyphHumidity);
    m_fan_card      = new MiniChart(page, this, Kind::Fan, GlyphFan);
    cards->Add(m_humidity_card, 1, wxEXPAND | wxRIGHT, FromDIP(12));
    cards->Add(m_fan_card, 1, wxEXPAND);
    root->Add(cards, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *foot = new wxBoxSizer(wxHORIZONTAL);
    m_cross = label(page, Label::Body_13, wxString());
    m_cross->SetName("live-chart-crosshair");
    foot->Add(m_cross, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    auto *copy = chip(page, _L("Copy value"), GlyphCopy);
    copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copy_value(); });
    auto *save = chip(page, _L("Save CSV"), GlyphSave);
    save->SetToolTip(_L("Save the readings shown as CSV (Ctrl+S)"));
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save_csv(); });
    auto *help = chip(page, _L("Explain this chart"), GlyphHelp);
    help->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { explain(); });
    for (Button *b : {copy, save, help}) foot->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    root->Add(foot, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    root->Add(label(page, Label::Body_12,
                    _L("Arrow keys move the crosshair a reading at a time, Home and End jump to the ends, Space pauses, Ctrl+C copies the value "
                       "under the crosshair and Ctrl+S saves the shown readings. Right-click the chart for the same choices. Click a small "
                       "chart, or focus it and press Enter, to show it in the large one."),
                    true, true),
              0, wxEXPAND | wxALL, pad);
    body->SetSizer(root);
    body->FitInside();

    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { refresh(); }, m_timer.GetId());
    m_timer.Start(500);
    refresh_chips();
    refresh();
}

LiveChartsPanel::~LiveChartsPanel() { m_timer.Stop(); }

bool LiveChartsPanel::reduced() const { return m_preview_reduced || MD3::Motion::reduced(); }

wxString LiveChartsPanel::time_text(double t, bool seconds) const
{
    const wxDateTime when(static_cast<time_t>(std::floor(t)));
    const ClockFace  face   = clock_face(when.GetHour(), when.GetMinute(), when.GetSecond(), m_options.twelve_hour, seconds);
    const wxString   digits = wxString::FromUTF8(face.digits);
    if (!face.twelve_hour) return digits;
    return wxString::Format(face.pm ? _L("%s PM") : _L("%s AM"), digits);
}

const AmsUnit *LiveChartsPanel::shown_ams(const DeviceRecord *rec) const
{
    if (rec == nullptr) return nullptr;
    const auto it = rec->ams.find(m_ams);
    return it == rec->ams.end() ? nullptr : &it->second;
}

const Series *LiveChartsPanel::shown_series(const DeviceRecord *rec) const
{
    if (rec == nullptr) return nullptr;
    if (Kind(m_kind) == Kind::Humidity) {
        const AmsUnit *unit = shown_ams(rec);
        return unit ? &unit->series : nullptr;
    }
    return &rec->series[m_kind];
}

std::string LiveChartsPanel::series_name() const
{
    wxString name = kind_name(m_kind);
    if (Kind(m_kind) == Kind::Humidity) {
        if (const AmsUnit *unit = shown_ams(Recorder::instance().find(m_dev_id)))
            name += " " + wxString::FromUTF8(unit->name) + (unit->percent ? wxString(" (%)") : " (" + _L("level 1 to 5") + ")");
    } else if (Kind(m_kind) == Kind::Fan) {
        name += " (%)";
    }
    return into_u8(name);
}

void LiveChartsPanel::refresh_chips()
{
    for (int k = 0; k < int(m_kind_chips.size()); ++k) m_kind_chips[k]->SetVariant(k == m_kind ? Button::Variant::Tonal : Button::Variant::Outlined);
    for (int w = 0; w < int(m_window_chips.size()); ++w)
        m_window_chips[w]->SetVariant(w == m_window ? Button::Variant::Tonal : Button::Variant::Outlined);
    for (int v = 0; v < int(m_view_chips.size()); ++v)
        m_view_chips[v]->SetVariant((v == 1) == m_candles ? Button::Variant::Tonal : Button::Variant::Outlined);
    for (int c = 0; c < int(m_clock_chips.size()); ++c)
        m_clock_chips[c]->SetVariant((c == 1) == m_options.twelve_hour ? Button::Variant::Tonal : Button::Variant::Outlined);
    for (std::size_t a = 0; a < m_ams_chips.size() && a < m_ams_ids.size(); ++a)
        m_ams_chips[a]->SetVariant(m_ams_ids[a] == m_ams ? Button::Variant::Tonal : Button::Variant::Outlined);
    m_pause->SetLabel(m_paused ? _L("Resume") : _L("Pause"));
    m_pause->SetGlyph(m_paused ? GlyphPlay : GlyphPause);
    for (auto *list : {&m_kind_chips, &m_window_chips, &m_view_chips, &m_clock_chips, &m_ams_chips})
        for (Button *b : *list) b->Refresh();
    m_pause->Refresh();
}

void LiveChartsPanel::refresh_ams_chips(const DeviceRecord *rec)
{
    std::vector<std::string> ids;
    if (rec)
        for (const auto &[id, unit] : rec->ams) ids.push_back(id);
    if (!ids.empty() && std::find(ids.begin(), ids.end(), m_ams) == ids.end()) m_ams = ids.front();
    bool changed = false;
    if (ids != m_ams_ids) {
        for (Button *b : m_ams_chips) b->Destroy(); // leaves the row as it goes
        m_ams_chips.clear();
        m_ams_ids = ids;
        for (const std::string &id : m_ams_ids) {
            auto *c = chip(m_body, wxString::FromUTF8(rec->ams.at(id).name));
            // After the click: choosing can rebuild these chips, and a button must not go inside its own handler.
            c->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { CallAfter([this, id]() { set_ams(id); }); });
            m_ams_chips.push_back(c);
            m_ams_row->Add(c, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
        }
        changed = true;
    }
    const bool show = Kind(m_kind) == Kind::Humidity && m_ams_ids.size() >= 2;
    if (m_root->IsShown(m_ams_row) != show) {
        m_root->Show(m_ams_row, show);
        changed = true;
    }
    if (changed) {
        refresh_chips();
        m_body->Layout();
        m_body->FitInside();
    }
}

void LiveChartsPanel::set_kind(int kind)
{
    m_kind = std::clamp(kind, 0, int(Kind::Count) - 1);
    m_canvas->reset_range();
    refresh_chips();
    refresh();
}

void LiveChartsPanel::set_window(int index)
{
    m_window = std::clamp(index, 0, 3);
    m_canvas->reset_range();
    refresh_chips();
    refresh();
}

void LiveChartsPanel::set_view(bool candles)
{
    m_candles = candles;
    refresh_chips();
    refresh();
}

void LiveChartsPanel::set_ams(const std::string &id)
{
    m_ams = id;
    m_canvas->reset_range();
    refresh_chips();
    refresh();
}

void LiveChartsPanel::set_option(const char *key, bool on)
{
    const std::string which = key;
    if (which == kReferenceKey) {
        m_options.reference = on;
        m_canvas->reset_range();
    } else if (which == kMomentumKey) {
        m_options.momentum = on;
    } else if (which == kParticlesKey) {
        m_options.particles = on;
    }
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(key, on ? "1" : "0");
    refresh();
}

void LiveChartsPanel::set_clock(bool twelve_hour)
{
    m_options.twelve_hour  = twelve_hour;
    m_options.clock_chosen = true;
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(kClockKey, twelve_hour ? "12" : "24");
    refresh_chips();
    refresh();
}

void LiveChartsPanel::set_paused(bool paused)
{
    m_paused = paused;
    if (paused) {
        m_paused_at = now_seconds();
        const DeviceRecord *rec = Recorder::instance().find(m_dev_id);
        m_paused_total          = rec ? rec->total : 0;
    }
    refresh_chips();
    refresh();
}

void LiveChartsPanel::refresh()
{
    MachineObject *obj = nullptr;
    if (DeviceManager *dm = wxGetApp().getDeviceManager()) obj = dm->get_selected_machine();
    const std::string id = obj ? obj->get_dev_id() : std::string();
    if (id != m_dev_id) {
        m_dev_id = id;
        m_canvas->reset_range();
    }
    const DeviceRecord *rec = Recorder::instance().find(m_dev_id);
    refresh_ams_chips(rec);
    const double now    = m_paused ? m_paused_at : now_seconds();
    const double wall   = now_seconds();
    const bool   online = obj && obj->is_online();

    if (!obj) m_device->SetLabel(_L("No printer is selected on the Device page."));
    else m_device->SetLabel(wxString::FromUTF8(obj->get_dev_name()) + (online ? wxString() : wxString(" ") + wxUniChar(0x00B7) + " " + _L("offline")));

    for (int k = 0; k < 3; ++k) {
        wxString text = kind_name(k) + "  ";
        const auto last = rec ? rec->series[k].latest() : std::nullopt;
        if (k == int(Kind::Chamber) && rec && !rec->has_chamber) text += _L("no sensor reported");
        else if (!last) text += "--";
        else {
            text += degrees(last->value);
            if (rec->target[k] > 0) text += " " + wxString::Format(_L("of %.0f °C"), rec->target[k]);
        }
        m_now[k]->SetLabel(text);
    }
    m_session->SetLabel(rec ? wxString::Format(_L("%d readings this session, every one from the printer."), int(rec->total))
                            : _L("No readings yet this session."));

    // The large chart.
    const Kind           kind   = Kind(m_kind);
    const AmsUnit       *ams    = kind == Kind::Humidity ? shown_ams(rec) : nullptr;
    const Unit           unit   = unit_of(kind, ams);
    const Series        *series = shown_series(rec);
    const double         window = kWindows[m_window];
    std::vector<Reading> shown  = series ? series->window(now, window) : std::vector<Reading>{};
    const Feed           feed   = feed_state(online, series ? series->latest() : std::nullopt, wall);
    const double         target = rec && unit == Unit::Celsius ? rec->target[m_kind] : 0;

    ChartView view;
    view.readings  = shown;
    view.now       = now;
    view.window    = window;
    view.target    = target;
    view.reference = m_options.reference;
    view.candles   = m_candles;
    view.momentum  = m_options.momentum;
    view.particles = m_options.particles;
    view.reduced   = reduced();
    view.paused    = m_paused;
    view.feed      = feed;
    view.bounds    = bounds_of(unit);
    view.key       = m_dev_id + "|" + std::to_string(m_kind) + "|" + (kind == Kind::Humidity ? m_ams : std::string());
    // Nothing to wait for: no AMS reports humidity, or no chamber temperature came.
    const bool no_ams     = kind == Kind::Humidity && rec && rec->ams.empty();
    const bool no_chamber = kind == Kind::Chamber && rec && !rec->has_chamber;
    if (no_ams) view.note = _L("No AMS on this printer reports humidity");
    else if (no_chamber) view.note = _L("This printer has not reported a chamber temperature");
    if (series)
        for (const Reading &r : series->readings())
            if (!view.has_session_high || r.value > view.session_high) {
                view.session_high     = r.value;
                view.has_session_high = true;
            }
    m_canvas->set_data(std::move(view));

    const auto last = series ? series->latest() : std::nullopt;
    m_value->SetLabel(last ? value_text(last->value, unit) : wxString("--"));
    const TrendInfo t     = trend(shown, target);
    wxString        words = kind_name(m_kind) + ": " + trend_words(t, unit);
    if (ams) {
        const wxString dry = drying_words(*ams);
        if (!dry.IsEmpty()) words += ", " + dry;
    } else if (kind == Kind::Humidity) {
        words = kind_name(m_kind) + ": " + (rec ? _L("No AMS on this printer reports humidity") : _L("No readings yet this session."));
    }
    m_trend->SetLabel(words);
    const MomentumInfo going = momentum(shown);
    m_momentum->SetLabel(m_options.momentum && feed == Feed::Live ? momentum_words(going, unit) : wxString());
    m_momentum->SetForegroundColour(role(going.direction == Momentum::Up     ? MD3::Role::Primary
                                         : going.direction == Momentum::Down ? MD3::Role::Error
                                                                             : MD3::Role::OnSurfaceVariant));

    wxString  feed_text;
    MD3::Role feed_role = MD3::Role::Primary;
    switch (feed) {
    case Feed::Live: feed_text = _L("Live"); break;
    case Feed::Waiting: feed_text = _L("Waiting"); feed_role = MD3::Role::OnSurfaceVariant; break;
    case Feed::Offline: feed_text = _L("Offline"); feed_role = MD3::Role::OnSurfaceVariant; break;
    case Feed::Stalled:
        feed_text = wxString::Format(_L("Stalled %d s: no reading since %s"), int(wall - last->t), time_text(last->t, true));
        feed_role = MD3::Role::Error;
        break;
    }
    if (no_ams || no_chamber) {
        feed_text = _L("Nothing to chart");
        feed_role = MD3::Role::OnSurfaceVariant;
    }
    if (m_paused && rec)
        feed_text = wxString::Format(_L("Paused, %d new readings"), int(rec->total - std::min(rec->total, m_paused_total)));
    m_feed->SetLabel(feed_text);
    m_feed->SetForegroundColour(role(feed_role));

    const int cross = m_canvas->cross();
    if (cross >= 0 && cross < int(shown.size()))
        m_cross->SetLabel(time_text(shown[std::size_t(cross)].t, true) + "  " + kind_name(m_kind) + " " + value_text(shown[std::size_t(cross)].value, unit));
    else
        m_cross->SetLabel(_L("Point at the chart, or focus it and use the arrow keys, to read a value."));

    // A screen reader hears the latest value, its trend and its momentum every 10 seconds.
    if (last && wall - m_announced >= 10) {
        m_announced   = wall;
        wxString name = kind_name(m_kind) + " " + value_text(last->value, unit) + ", " + trend_words(t, unit);
        if (!m_momentum->GetLabel().IsEmpty()) name += ", " + m_momentum->GetLabel();
        m_canvas->SetName(name);
    }

    // The small charts, over the same time window.
    {
        MiniChart::Content c;
        c.title          = kind_name(int(Kind::Humidity));
        const AmsUnit *u = shown_ams(rec);
        if (u && m_ams_ids.size() >= 2) c.title += wxString(" ") + wxUniChar(0x00B7) + " " + wxString::FromUTF8(u->name);
        if (u) {
            const Unit      hu     = u->percent ? Unit::Percent : Unit::Level;
            const auto      latest = u->series.latest();
            c.readings             = u->series.window(now, window);
            c.value                = latest ? value_text(latest->value, hu) : wxString("--");
            const TrendInfo ti     = trend(c.readings, 0);
            c.words                = trend_word(ti);
            const wxString dry     = drying_words(*u);
            if (!dry.IsEmpty()) c.words += ", " + dry;
            c.trend_glyph = trend_glyph(ti);
            c.words_role  = u->drying ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant;
            c.bounds      = bounds_of(hu);
        } else {
            c.value = "--";
            c.words = rec ? _L("No AMS on this printer reports humidity") : _L("No readings yet this session.");
        }
        c.now      = now;
        c.window   = window;
        c.selected = kind == Kind::Humidity;
        m_humidity_card->set(std::move(c));
    }
    {
        MiniChart::Content c;
        c.title           = kind_name(int(Kind::Fan));
        const Series *fan = rec && rec->has_fan ? &rec->series[int(Kind::Fan)] : nullptr;
        if (fan) {
            const auto      latest = fan->latest();
            c.readings             = fan->window(now, window);
            c.value                = latest ? value_text(latest->value, Unit::Percent) : wxString("--");
            const TrendInfo ti     = trend(c.readings, 0);
            c.words                = trend_word(ti);
            c.trend_glyph          = trend_glyph(ti);
            c.bounds               = bounds_of(Unit::Percent);
        } else {
            c.value = "--";
            c.words = _L("No readings yet this session.");
        }
        c.now      = now;
        c.window   = window;
        c.selected = kind == Kind::Fan;
        m_fan_card->set(std::move(c));
    }
    m_body->Layout();
}

void LiveChartsPanel::copy_value()
{
    const int   cross = m_canvas->cross();
    const auto &r     = m_canvas->readings();
    if (cross < 0 || cross >= int(r.size())) return;
    const wxString text = wxString::Format("%.1f", r[std::size_t(cross)].value);
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(text));
        wxTheClipboard->Close();
    }
}

void LiveChartsPanel::save_csv()
{
    const DeviceRecord *rec = Recorder::instance().find(m_dev_id);
    if (!rec) return;
    std::map<std::string, std::vector<Reading>> data;
    data[series_name()] = m_canvas->readings();
    wxFileDialog dialog(this, _L("Save the shown readings"), wxString(), "live-chart.csv", "CSV (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    std::ofstream out(fs::u8path(into_u8(dialog.GetPath())), std::ios::binary | std::ios::trunc);
    out << to_csv(data);
    if (!out) MessageDialog(this, _L("The readings could not be saved there."), _L("Live charts"), wxOK | wxICON_WARNING).ShowModal();
}

void LiveChartsPanel::explain() { DocsBrowserDialog::ShowArticle(wxGetApp().mainframe, kGuide); }

void LiveChartsPanel::open_motion_settings() { wxGetApp().open_preferences("motion_preference"); }

void LiveChartsPanel::show_menu(const wxPoint &screen)
{
    wxMenu    menu;
    const int pause = wxID_HIGHEST + 1, copy = wxID_HIGHEST + 2, save = wxID_HIGHEST + 3, motion = wxID_HIGHEST + 4, help = wxID_HIGHEST + 5;
    menu.Append(pause, (m_paused ? _L("Resume updates") : _L("Pause updates")) + "\tSpace");
    menu.Append(copy, _L("Copy the value under the crosshair") + "\tCtrl+C");
    menu.Enable(copy, m_canvas->cross() >= 0);
    menu.Append(save, _L("Save shown readings as CSV") + "\tCtrl+S");
    menu.AppendSeparator();
    menu.Append(motion, _L("Edit appearance, including Animation and transition"));
    menu.Append(help, _L("Explain this chart") + "\tF1");
    const int chosen = MD3::PopupMenuSelection(this, menu, screen);
    if (chosen == pause) set_paused(!m_paused);
    else if (chosen == copy) copy_value();
    else if (chosen == save) save_csv();
    else if (chosen == motion) open_motion_settings();
    else if (chosen == help) explain();
}

}}} // namespace Slic3r::GUI::LiveCharts
