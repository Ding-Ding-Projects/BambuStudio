#include "LiveChartsPanel.hpp"

#include "../DeviceManager.hpp"
#include "../DeviceCore/DevBed.h"
#include "../DeviceCore/DevChamber.h"
#include "../DeviceCore/DevExtruderSystem.h"
#include "../DeviceCore/DevManager.h"
#include "../DocsBrowserDialog.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Menu.hpp"
#include "../Widgets/MD3Motion.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/SwitchButton.hpp"

#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>

namespace Slic3r { namespace GUI { namespace LiveCharts {

namespace fs = std::filesystem;

namespace {

constexpr double kWindows[] = {60, 300, 900, 3600};
constexpr double kGap       = 10.0; // seconds without a reading that break the line
constexpr const char *kGuide = "docs/features/device/live-charts.md";
enum : std::uint32_t { GlyphChart = 0xF190, GlyphUp = 0xE8E5, GlyphDown = 0xE8E3, GlyphFlat = 0xE8E4, GlyphPause = 0xE034, GlyphPlay = 0xE037,
                       GlyphCopy = 0xE14D, GlyphSave = 0xE161, GlyphHelp = 0xE887 };

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

double now_seconds() { return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); }

wxString kind_name(int kind)
{
    switch (Kind(kind)) {
    case Kind::Nozzle: return _L("Nozzle");
    case Kind::Bed: return _L("Bed");
    default: return _L("Chamber");
    }
}

wxString clock_text(double t, bool seconds)
{
    const wxDateTime when(static_cast<time_t>(std::floor(t)));
    return when.Format(seconds ? "%H:%M:%S" : "%H:%M");
}

wxString degrees(double v) { return wxString::Format("%.1f ", v) + wxUniChar(0x00B0) + "C"; }

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

wxString trend_words(const TrendInfo &t, int kind)
{
    switch (t.trend) {
    case Trend::Holding: return wxString::Format(_L("Holding near %s"), degrees(t.target));
    case Trend::Rising: return wxString::Format(_L("Rising, %+.1f °C in %d s"), t.delta, int(std::lround(t.over)));
    case Trend::Falling: return wxString::Format(_L("Falling, %+.1f °C in %d s"), t.delta, int(std::lround(t.over)));
    case Trend::Flat: return wxString::Format(_L("Flat, %+.1f °C in %d s"), t.delta, int(std::lround(t.over)));
    default: (void) kind; return _L("Not enough readings for a trend yet");
    }
}

} // namespace

// ---------------------------------------------------------------- recorder

Recorder &Recorder::instance()
{
    static Recorder recorder;
    return recorder;
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
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE), m_owner(owner)
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
    }

    bool AcceptsFocus() const override { return true; }

    // The readings and settings to draw; called on every refresh tick.
    void set_data(std::vector<Reading> readings, double now, double window, double target, bool reference, bool candles, Feed feed)
    {
        m_readings  = std::move(readings);
        m_now       = now;
        m_window    = window;
        m_target    = target;
        m_reference = reference;
        m_candles   = candles;
        m_feed      = feed;
        if (m_cross >= int(m_readings.size())) m_cross = int(m_readings.size()) - 1;
        const Range want = value_range(segments(m_readings, kGap), {reference ? target : 0.0});
        if (!m_have_range || MD3::Motion::reduced()) {
            m_low        = want.low;
            m_high       = want.high;
            m_have_range = true;
        } else {
            // Ease toward each reading instead of jumping.
            m_low += (want.low - m_low) * 0.35;
            m_high += (want.high - m_high) * 0.35;
        }
        ++m_tick;
        Refresh();
    }

    int                         cross() const { return m_cross; }
    const std::vector<Reading> &readings() const { return m_readings; }
    void                        reset_range() { m_have_range = false; }

private:
    wxRect plot() const
    {
        const wxSize s = GetClientSize();
        return wxRect(FromDIP(56), FromDIP(12), std::max(10, s.x - FromDIP(56) - FromDIP(14)), std::max(10, s.y - FromDIP(12) - FromDIP(28)));
    }
    double x_of(double t, const wxRect &p) const { return p.x + (t - (m_now - m_window)) / m_window * p.width; }
    double y_of(double v, const wxRect &p) const { return p.y + p.height - (v - m_low) / std::max(1e-9, m_high - m_low) * p.height; }

    void hover(int x)
    {
        const wxRect p = plot();
        const double t = (m_now - m_window) + double(x - p.x) / std::max(1, p.width) * m_window;
        m_cross        = nearest(m_readings, t);
        m_owner->refresh();
    }

    void on_key(wxKeyEvent &e)
    {
        const int  n    = int(m_readings.size());
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

    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxColour        surface = role(MD3::Role::SurfaceContainerLowest);
        dc.SetBackground(wxBrush(surface));
        dc.Clear();
        const wxRect p = plot();
        dc.SetFont(Label::Body_12);

        // Grid and value labels.
        dc.SetTextForeground(role(MD3::Role::OnSurfaceVariant));
        for (int i = 0; i <= 4; ++i) {
            const double v = m_low + (m_high - m_low) * i / 4.0;
            const int    y = int(std::lround(y_of(v, p)));
            dc.SetPen(wxPen(role(MD3::Role::OutlineVariant), 1, i == 0 ? wxPENSTYLE_SOLID : wxPENSTYLE_DOT));
            dc.DrawLine(p.x, y, p.GetRight(), y);
            const wxString text = wxString::Format("%.0f", v);
            const wxSize   ts   = dc.GetTextExtent(text);
            dc.DrawText(text, p.x - ts.x - FromDIP(8), y - ts.y / 2);
        }
        // Time labels.
        const bool seconds = m_window <= 300;
        for (int i = 0; i <= 5; ++i) {
            const double   t    = m_now - m_window + m_window * i / 5.0;
            const wxString text = i == 5 ? _L("now") : clock_text(t, seconds);
            const wxSize   ts   = dc.GetTextExtent(text);
            int            x    = int(std::lround(x_of(t, p))) - ts.x / 2;
            x                   = std::clamp(x, p.x - ts.x / 2, p.GetRight() - ts.x);
            dc.DrawText(text, x, p.GetBottom() + FromDIP(8));
        }

        // The target as a reference line.
        if (m_reference && m_target > 0 && m_target >= m_low && m_target <= m_high) {
            const int y = int(std::lround(y_of(m_target, p)));
            dc.SetPen(wxPen(role(MD3::Role::Outline), 1, wxPENSTYLE_SHORT_DASH));
            dc.DrawLine(p.x, y, p.GetRight(), y);
            const wxString text = wxString::Format(_L("Target %.0f"), m_target);
            dc.DrawText(text, p.GetRight() - dc.GetTextExtent(text).x, y - dc.GetTextExtent(text).y - FromDIP(2));
        }

        dc.SetClippingRegion(p);
        const wxColour line = role(MD3::Role::Primary);
        if (m_candles) {
            const double width = 60.0;
            const double start = std::floor((m_now - m_window) / width) * width;
            for (const Candle &c : candles(m_readings, start, width, m_now)) {
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
            for (const auto &part : segments(m_readings, kGap)) {
                std::vector<wxPoint> points;
                for (const Reading &r : part) points.emplace_back(int(std::lround(x_of(r.t, p))), int(std::lround(y_of(r.value, p))));
                if (points.size() == 1) dc.DrawCircle(points.front(), FromDIP(2));
                else if (points.size() > 1) dc.DrawLines(int(points.size()), points.data());
            }
            if (!m_readings.empty() && m_feed == Feed::Live) {
                const Reading &last = m_readings.back();
                const int      r    = FromDIP(4) + (MD3::Motion::reduced() ? 0 : int(m_tick % 6 < 3 ? FromDIP(1) : 0));
                dc.SetBrush(wxBrush(line));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawCircle(int(x_of(last.t, p)), int(y_of(last.value, p)), r);
            }
        }
        // The crosshair.
        if (m_cross >= 0 && m_cross < int(m_readings.size())) {
            const Reading &r = m_readings[std::size_t(m_cross)];
            const int      x = int(std::lround(x_of(r.t, p))), y = int(std::lround(y_of(r.value, p)));
            dc.SetPen(wxPen(role(MD3::Role::OnSurfaceVariant), 1, wxPENSTYLE_DOT));
            dc.DrawLine(x, p.y, x, p.GetBottom());
            dc.SetBrush(wxBrush(surface));
            dc.SetPen(wxPen(line, FromDIP(2)));
            dc.DrawCircle(x, y, FromDIP(5));
        }
        dc.DestroyClippingRegion();

        // The feed state over the chart.
        wxString state;
        if (m_feed == Feed::Offline) state = _L("The printer is off or not connected");
        else if (m_feed == Feed::Waiting) state = _L("Asking the printer for its first reading");
        else if (m_readings.empty()) state = _L("No readings in this time window");
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

    LiveChartsPanel     *m_owner;
    std::vector<Reading> m_readings;
    double               m_now = 0, m_window = 300, m_target = 0, m_low = 0, m_high = 1;
    bool                 m_reference = true, m_candles = false, m_have_range = false;
    Feed                 m_feed  = Feed::Waiting;
    int                  m_cross = -1;
    unsigned             m_tick  = 0;
};

// ---------------------------------------------------------------- the panel

void LiveChartsPanel::open()
{
    if (MainFrame *frame = wxGetApp().mainframe)
        frame->open_service("live-charts", _L("Live charts"), [](wxWindow *parent) { return new LiveChartsPanel(parent); });
}

LiveChartsPanel::LiveChartsPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY), m_timer(this)
{
    SetName("live-charts");
    SetBackgroundColour(role(MD3::Role::Surface));
    const int pad = FromDIP(MD3::Metrics::active().padding);
    auto     *root = new wxBoxSizer(wxVERTICAL);

    auto *head = new wxBoxSizer(wxHORIZONTAL);
    auto *titles = new wxBoxSizer(wxVERTICAL);
    titles->Add(label(this, Label::Head_20, _L("Live charts")), 0);
    titles->Add(label(this, Label::Body_13,
                      _L("The temperatures of the printer selected on the Device page, as it sends them. A gap in its reports stays a gap: "
                         "nothing is drawn that the printer did not send."),
                      true, true),
                0, wxEXPAND | wxTOP, FromDIP(4));
    m_device = label(this, Label::Head_14, wxString());
    titles->Add(m_device, 0, wxTOP, FromDIP(8));
    head->Add(titles, 1, wxEXPAND | wxRIGHT, pad);

    auto *now = new wxBoxSizer(wxVERTICAL);
    now->Add(label(this, Label::Head_12, _L("Now"), true), 0, wxBOTTOM, FromDIP(4));
    for (int k = 0; k < int(Kind::Count); ++k) {
        m_now[k] = label(this, Label::Body_14, wxString());
        now->Add(m_now[k], 0, wxBOTTOM, FromDIP(2));
    }
    m_session = label(this, Label::Body_12, wxString(), true);
    now->Add(m_session, 0, wxTOP, FromDIP(4));
    head->Add(now, 0);
    root->Add(head, 0, wxEXPAND | wxALL, pad);

    auto *controls = new wxWrapSizer(wxHORIZONTAL);
    for (int k = 0; k < int(Kind::Count); ++k) {
        auto *c = chip(this, kind_name(k));
        c->Bind(wxEVT_BUTTON, [this, k](wxCommandEvent &) { set_kind(k); });
        m_kind_chips.push_back(c);
        controls->Add(c, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    controls->AddSpacer(FromDIP(16));
    const wxString windows[] = {_L("1 min"), _L("5 min"), _L("15 min"), _L("1 h")};
    for (int w = 0; w < 4; ++w) {
        auto *c = chip(this, windows[w]);
        c->Bind(wxEVT_BUTTON, [this, w](wxCommandEvent &) { set_window(w); });
        m_window_chips.push_back(c);
        controls->Add(c, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    controls->AddSpacer(FromDIP(16));
    for (int v = 0; v < 2; ++v) {
        auto *c = chip(this, v == 0 ? _L("Line") : _L("Candles"));
        c->SetToolTip(v == 0 ? _L("Every reading, joined except across gaps") : _L("One candle per minute: first, highest, lowest and last reading"));
        c->Bind(wxEVT_BUTTON, [this, v](wxCommandEvent &) { set_view(v == 1); });
        m_view_chips.push_back(c);
        controls->Add(c, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    controls->AddSpacer(FromDIP(16));
    controls->Add(label(this, Label::Body_13, _L("Target line")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(6));
    m_reference_switch = new SwitchButton(this);
    m_reference_switch->SetName(_L("Target line"));
    m_reference_switch->SetValue(true);
    m_reference_switch->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) {
        m_reference = m_reference_switch->GetValue();
        m_canvas->reset_range();
        refresh();
    });
    controls->Add(m_reference_switch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(16));
    m_pause = chip(this, _L("Pause"), GlyphPause);
    m_pause->SetToolTip(_L("Freeze the chart; readings keep arriving and are shown when you resume (Space)"));
    m_pause->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { set_paused(!m_paused); });
    controls->Add(m_pause, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    root->Add(controls, 0, wxEXPAND | wxLEFT | wxRIGHT, pad);

    auto *summary = new wxBoxSizer(wxHORIZONTAL);
    m_value = label(this, Label::Head_24, wxString());
    summary->Add(m_value, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    m_trend = label(this, Label::Body_14, wxString());
    summary->Add(m_trend, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    m_feed = label(this, Label::Head_13, wxString());
    summary->Add(m_feed, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(summary, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *box = new StaticBox(this);
    box->SetCornerRadius(FromDIP(16));
    box->SetBorderWidth(0);
    box->SetBackgroundColor(role(MD3::Role::SurfaceContainerLowest));
    box->SetBackgroundColour(role(MD3::Role::SurfaceContainerLowest));
    auto *bs = new wxBoxSizer(wxVERTICAL);
    m_canvas = new ChartCanvas(box, this);
    bs->Add(m_canvas, 1, wxEXPAND | wxALL, FromDIP(12));
    box->SetSizer(bs);
    root->Add(box, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *foot = new wxBoxSizer(wxHORIZONTAL);
    m_cross = label(this, Label::Body_13, wxString());
    m_cross->SetName("live-chart-crosshair");
    foot->Add(m_cross, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    auto *copy = chip(this, _L("Copy value"), GlyphCopy);
    copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copy_value(); });
    auto *save = chip(this, _L("Save CSV"), GlyphSave);
    save->SetToolTip(_L("Save the readings shown as CSV (Ctrl+S)"));
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save_csv(); });
    auto *help = chip(this, _L("Explain this chart"), GlyphHelp);
    help->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { explain(); });
    for (Button *b : {copy, save, help}) foot->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    root->Add(foot, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    root->Add(label(this, Label::Body_12,
                    _L("Arrow keys move the crosshair a reading at a time, Home and End jump to the ends, Space pauses, Ctrl+C copies the value "
                       "under the crosshair and Ctrl+S saves the shown readings. Right-click the chart for the same choices."),
                    true, true),
              0, wxEXPAND | wxALL, pad);
    SetSizer(root);

    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { refresh(); }, m_timer.GetId());
    m_timer.Start(500);
    refresh_chips();
    refresh();
}

LiveChartsPanel::~LiveChartsPanel() { m_timer.Stop(); }

void LiveChartsPanel::refresh_chips()
{
    for (int k = 0; k < int(m_kind_chips.size()); ++k) m_kind_chips[k]->SetVariant(k == m_kind ? Button::Variant::Tonal : Button::Variant::Outlined);
    for (int w = 0; w < int(m_window_chips.size()); ++w)
        m_window_chips[w]->SetVariant(w == m_window ? Button::Variant::Tonal : Button::Variant::Outlined);
    for (int v = 0; v < int(m_view_chips.size()); ++v)
        m_view_chips[v]->SetVariant((v == 1) == m_candles ? Button::Variant::Tonal : Button::Variant::Outlined);
    m_pause->SetLabel(m_paused ? _L("Resume") : _L("Pause"));
    m_pause->SetGlyph(m_paused ? GlyphPlay : GlyphPause);
    for (auto *list : {&m_kind_chips, &m_window_chips, &m_view_chips})
        for (Button *b : *list) b->Refresh();
    m_pause->Refresh();
}

void LiveChartsPanel::set_kind(int kind)
{
    m_kind = kind;
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
    const DeviceRecord *rec   = Recorder::instance().find(m_dev_id);
    const double        now   = m_paused ? m_paused_at : now_seconds();
    const bool          online = obj && obj->is_online();

    if (!obj) m_device->SetLabel(_L("No printer is selected on the Device page."));
    else m_device->SetLabel(wxString::FromUTF8(obj->get_dev_name()) + (online ? wxString() : wxString(" ") + wxUniChar(0x00B7) + " " + _L("offline")));

    for (int k = 0; k < int(Kind::Count); ++k) {
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

    const Series        *series = rec ? &rec->series[m_kind] : nullptr;
    std::vector<Reading> shown  = series ? series->window(now, kWindows[m_window]) : std::vector<Reading>{};
    const Feed           feed   = feed_state(online, series ? series->latest() : std::nullopt, now_seconds());
    const double         target = rec ? rec->target[m_kind] : 0;
    m_canvas->set_data(shown, now, kWindows[m_window], target, m_reference, m_candles, feed);

    const auto last = series ? series->latest() : std::nullopt;
    m_value->SetLabel(last ? degrees(last->value) : wxString("--"));
    const TrendInfo t = trend(shown, target);
    m_trend->SetLabel(kind_name(m_kind) + ": " + trend_words(t, m_kind));

    wxString feed_text;
    MD3::Role feed_role = MD3::Role::Primary;
    switch (feed) {
    case Feed::Live: feed_text = _L("Live"); break;
    case Feed::Waiting: feed_text = _L("Waiting"); feed_role = MD3::Role::OnSurfaceVariant; break;
    case Feed::Offline: feed_text = _L("Offline"); feed_role = MD3::Role::OnSurfaceVariant; break;
    case Feed::Stalled:
        feed_text = wxString::Format(_L("Stalled %d s: no reading since %s"), int(now_seconds() - last->t), clock_text(last->t, true));
        feed_role = MD3::Role::Error;
        break;
    }
    if (m_paused && rec)
        feed_text = wxString::Format(_L("Paused, %d new readings"), int(rec->total - std::min(rec->total, m_paused_total)));
    m_feed->SetLabel(feed_text);
    m_feed->SetForegroundColour(role(feed_role));

    const int cross = m_canvas->cross();
    if (cross >= 0 && cross < int(shown.size()))
        m_cross->SetLabel(clock_text(shown[std::size_t(cross)].t, true) + "  " + kind_name(m_kind) + " " + degrees(shown[std::size_t(cross)].value));
    else
        m_cross->SetLabel(_L("Point at the chart, or focus it and use the arrow keys, to read a value."));

    // A screen reader hears the latest value and its trend every 10 seconds.
    if (last && now_seconds() - m_announced >= 10) {
        m_announced = now_seconds();
        m_canvas->SetName(kind_name(m_kind) + " " + degrees(last->value) + ", " + trend_words(t, m_kind));
    }
    Layout();
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
    data[into_u8(kind_name(m_kind))] = m_canvas->readings();
    wxFileDialog dialog(this, _L("Save the shown readings"), wxString(), "live-chart.csv", "CSV (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    std::ofstream out(fs::u8path(into_u8(dialog.GetPath())), std::ios::binary | std::ios::trunc);
    out << to_csv(data);
    if (!out) MessageDialog(this, _L("The readings could not be saved there."), _L("Live charts"), wxOK | wxICON_WARNING).ShowModal();
}

void LiveChartsPanel::explain() { DocsBrowserDialog::ShowArticle(wxGetApp().mainframe, kGuide); }

void LiveChartsPanel::show_menu(const wxPoint &screen)
{
    wxMenu    menu;
    const int pause = wxID_HIGHEST + 1, copy = wxID_HIGHEST + 2, save = wxID_HIGHEST + 3, help = wxID_HIGHEST + 4;
    menu.Append(pause, (m_paused ? _L("Resume updates") : _L("Pause updates")) + "\tSpace");
    menu.Append(copy, _L("Copy the value under the crosshair") + "\tCtrl+C");
    menu.Enable(copy, m_canvas->cross() >= 0);
    menu.Append(save, _L("Save shown readings as CSV") + "\tCtrl+S");
    menu.AppendSeparator();
    menu.Append(help, _L("Explain this chart") + "\tF1");
    const int chosen = MD3::PopupMenuSelection(this, menu, screen);
    if (chosen == pause) set_paused(!m_paused);
    else if (chosen == copy) copy_value();
    else if (chosen == save) save_csv();
    else if (chosen == help) explain();
}

}}} // namespace Slic3r::GUI::LiveCharts
