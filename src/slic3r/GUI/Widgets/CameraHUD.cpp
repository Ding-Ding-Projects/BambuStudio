#include "CameraHUD.hpp"

#include <algorithm>
#include <cmath>

#include <wx/dcbuffer.h>
#include <wx/dcgraph.h> // wxGCDC
#include <wx/graphics.h>

#include "../I18N.hpp" // _L
#include "Label.hpp"   // ::Label::Mono_11 (Roboto Mono 11.5/400)

namespace Slic3r { namespace GUI {

namespace {
constexpr double kPi          = 3.14159265358979323846;
constexpr int    kPulsePeriod = 1600; // ms, one full breath
constexpr int    kPulseTick   = 50;   // ms between repaints of the dot
constexpr int    kChipDIP     = 34;
constexpr int    kChipGlyphPx = 18;   // logical px; the gc scales it by DPI
constexpr int    kHudHeight   = 44;
constexpr int    kTempChipPadX  = 10; // kit temp-chip horizontal padding
constexpr int    kTempChipPillH = 22; // pill height (matches the LIVE badge)
constexpr int    kTempChipGap   = 6;  // gap between the nozzle / bed chips

// "°C" as a UTF-8 unit suffix. Kept split ("\xC2\xB0" "C") so the trailing 'C'
// is not swallowed into the \x hex escape.
inline wxString deg_c(int value) { return wxString::Format("%d", value) + wxString::FromUTF8("\xC2\xB0" "C"); }
} // namespace

// ---------------------------------------------------------------------------
// Fixed-dark palette (identical in light and dark app themes).
// ---------------------------------------------------------------------------
const wxColour &CameraHUD::CardBg()
{
    static const wxColour c(0x0c, 0x0e, 0x13);
    return c;
}
const wxColour &CameraHUD::Border()
{
    static const wxColour c(0x2a, 0x2d, 0x34);
    return c;
}
const wxColour &CameraHUD::ChipBg()
{
    static const wxColour c(0x1b, 0x1e, 0x25);
    return c;
}
const wxColour &CameraHUD::ChipHover()
{
    static const wxColour c(0x26, 0x2a, 0x33);
    return c;
}
const wxColour &CameraHUD::ChipPress()
{
    static const wxColour c(0x12, 0x14, 0x19);
    return c;
}
const wxColour &CameraHUD::Glyph()
{
    static const wxColour c(0xE6, 0xE8, 0xEC);
    return c;
}
const wxColour &CameraHUD::GlyphMuted()
{
    static const wxColour c(0x8A, 0x8F, 0x98);
    return c;
}

// ===========================================================================
// CameraHUDChip
// ===========================================================================
CameraHUD::CameraHUDChip::CameraHUDChip(wxWindow *parent, uint32_t glyph, const wxString &fallback_icon)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , m_glyph(glyph)
    , m_fallback_name(fallback_icon.ToStdString())
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    const wxSize s(FromDIP(kChipDIP), FromDIP(kChipDIP));
    SetMinSize(s);
    SetMaxSize(s);
    SetCursor(wxCursor(wxCURSOR_HAND));

    if (!m_fallback_name.empty())
        m_fallback = ScalableBitmap(this, m_fallback_name, kChipGlyphPx);

    // Only hover feedback is bound here. The click action is a handler that
    // StatusPanel Connect()s to this same window (LEFT_DOWN / LEFT_DCLICK), so
    // this chip deliberately does NOT bind the mouse-down events, avoiding any
    // handler-ordering coupling with that external Connect.
    Bind(wxEVT_PAINT, &CameraHUDChip::on_paint, this);
    Bind(wxEVT_ENTER_WINDOW, &CameraHUDChip::on_enter, this);
    Bind(wxEVT_LEAVE_WINDOW, &CameraHUDChip::on_leave, this);
}

void CameraHUD::CameraHUDChip::reset_hover()
{
    if (m_hover) {
        m_hover = false;
        Refresh();
    }
}

bool CameraHUD::CameraHUDChip::Enable(bool enable)
{
    const bool ret = wxWindow::Enable(enable);
    m_hover        = false;
    Refresh();
    return ret;
}

void CameraHUD::CameraHUDChip::msw_rescale()
{
    const wxSize s(FromDIP(kChipDIP), FromDIP(kChipDIP));
    SetMinSize(s);
    SetMaxSize(s);
    if (!m_fallback_name.empty())
        m_fallback.msw_rescale();
    Refresh();
}

void CameraHUD::CameraHUDChip::on_enter(wxMouseEvent &evt)
{
    if (!m_hover) {
        m_hover = true;
        Refresh();
    }
    evt.Skip();
}

void CameraHUD::CameraHUDChip::on_leave(wxMouseEvent &evt)
{
    if (m_hover) {
        m_hover = false;
        Refresh();
    }
    evt.Skip();
}

void CameraHUD::CameraHUDChip::on_paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC pdc(this);
    wxGCDC                dc(pdc);
    wxGraphicsContext *   gc = dc.GetGraphicsContext();
    if (!gc)
        return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    const wxSize sz = GetClientSize();

    const wxColour behind = GetParent() ? GetParent()->GetBackgroundColour() : CameraHUD::CardBg();
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(behind));
    gc->DrawRectangle(0, 0, sz.x, sz.y);

    const wxColour bg = !IsEnabled() ? CameraHUD::CardBg()
                        : m_hover    ? CameraHUD::ChipHover()
                                     : CameraHUD::ChipBg();
    gc->SetBrush(wxBrush(bg));
    const double d  = std::min(sz.x, sz.y);
    const double cx = (sz.x - d) / 2.0;
    const double cy = (sz.y - d) / 2.0;
    gc->DrawEllipse(cx, cy, d, d);

    const wxColour gcol = IsEnabled() ? CameraHUD::Glyph() : CameraHUD::GlyphMuted();
    if (MaterialIcon::available()) {
        // font(px) takes LOGICAL px; the graphics context scales the point size
        // by DPI, so a fixed logical size keeps the glyph/chip ratio constant
        // across HiDPI (mirrors the AxisCtrlButton proving site).
        gc->SetFont(MaterialIcon::font(kChipGlyphPx), gcol);
        wxDouble gw = 0, gh = 0;
        gc->GetTextExtent(MaterialIcon::text(m_glyph), &gw, &gh);
        gc->DrawText(MaterialIcon::text(m_glyph), (sz.x - gw) / 2.0, (sz.y - gh) / 2.0);
    } else if (m_fallback.bmp().IsOk()) {
        const wxBitmap &b = m_fallback.bmp();
        gc->DrawBitmap(b, (sz.x - b.GetScaledWidth()) / 2.0, (sz.y - b.GetScaledHeight()) / 2.0, b.GetScaledWidth(),
                       b.GetScaledHeight());
    }
}

// ===========================================================================
// CameraHUDTempChip
// ===========================================================================
CameraHUD::CameraHUDTempChip::CameraHUDTempChip(wxWindow *parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    SetFont(::Label::Mono_11);
    Bind(wxEVT_PAINT, &CameraHUDTempChip::on_paint, this);
}

void CameraHUD::CameraHUDTempChip::SetText(const wxString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    InvalidateBestSize();
    if (wxWindow *p = GetParent())
        p->Layout();
    Refresh();
}

void CameraHUD::CameraHUDTempChip::msw_rescale()
{
    InvalidateBestSize();
    Refresh();
}

wxSize CameraHUD::CameraHUDTempChip::DoGetBestSize() const
{
    // Measure the current value (or a 3-digit placeholder so an empty chip still
    // reserves a sane width) in the mono face and pad to the kit pill geometry.
    int      tw = 0, th = 0;
    wxString probe = m_text.empty() ? wxString("000") + wxString::FromUTF8("\xC2\xB0" "C") : m_text;
    GetTextExtent(probe, &tw, &th, nullptr, nullptr, &::Label::Mono_11);
    return wxSize(tw + 2 * FromDIP(kTempChipPadX), FromDIP(kTempChipPillH));
}

void CameraHUD::CameraHUDTempChip::on_paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC pdc(this);
    wxGCDC                dc(pdc);
    wxGraphicsContext *   gc = dc.GetGraphicsContext();
    if (!gc)
        return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    const wxSize sz = GetClientSize();

    // Fill with the band colour first, then lay a translucent-black pill exactly
    // like the LIVE badge (kit rgba(0,0,0,.55)), so the chip reads over the
    // fixed-dark strip in both app themes.
    const wxColour behind = GetParent() ? GetParent()->GetBackgroundColour() : CameraHUD::CardBg();
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(behind));
    gc->DrawRectangle(0, 0, sz.x, sz.y);

    if (m_text.empty())
        return;

    const double pillH  = FromDIP(kTempChipPillH);
    const double pillY  = (sz.y - pillH) / 2.0;
    const double radius = FromDIP(10);
    gc->SetBrush(wxBrush(wxColour(0, 0, 0, 140)));
    gc->DrawRoundedRectangle(0, pillY, sz.x, pillH, radius);

    // The pill spans the whole (best-fitted) chip; centre the mono value so a
    // minor DC/gc metric difference never clips it.
    gc->SetFont(::Label::Mono_11, *wxWHITE);
    wxDouble tw = 0, thd = 0, desc = 0, lead = 0;
    gc->GetTextExtent(m_text, &tw, &thd, &desc, &lead);
    gc->DrawText(m_text, (sz.x - tw) / 2.0, pillY + (pillH - thd) / 2.0);
}

// ===========================================================================
// CameraHUD
// ===========================================================================
CameraHUD::CameraHUD(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetDoubleBuffered(true);
    SetBackgroundColour(CardBg());
    SetMinSize(wxSize(-1, FromDIP(kHudHeight)));

    m_pulse_timer.SetOwner(this);

    auto *hsizer = new wxBoxSizer(wxHORIZONTAL);
    // Left: reserve room for the painted LIVE badge (drawn in on_paint).
    m_badge_spacer = hsizer->Add(FromDIP(80), FromDIP(kHudHeight), 0);

    // Nozzle / bed temperature chips, immediately right of the LIVE badge (the
    // kit groups the temp readouts with the live/status chrome). Hidden until a
    // printer is connected and StatusPanel feeds SetTemperatures().
    m_nozzle_chip = new CameraHUDTempChip(this);
    m_bed_chip    = new CameraHUDTempChip(this);
    m_nozzle_chip->Hide();
    m_bed_chip->Hide();
    hsizer->Add(m_nozzle_chip, 0, wxALIGN_CENTER_VERTICAL);
    hsizer->Add(m_bed_chip, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(kTempChipGap));

    hsizer->AddStretchSpacer();

    // Status indicators (filled by StatusPanel), then the two control chips.
    m_status_slot = new wxBoxSizer(wxHORIZONTAL);
    hsizer->Add(m_status_slot, 0, wxALIGN_CENTER_VERTICAL);

    m_setting_chip    = new CameraHUDChip(this, MaterialIcon::Settings, "camera_setting");
    m_fullscreen_chip = new CameraHUDChip(this, MaterialIcon::Fullscreen, "camera_fullscreen");
    hsizer->Add(m_setting_chip, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    hsizer->Add(m_fullscreen_chip, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    hsizer->AddSpacer(FromDIP(12));

    SetSizer(hsizer);
    Layout();

    Bind(wxEVT_PAINT, &CameraHUD::on_paint, this);
    Bind(wxEVT_TIMER, &CameraHUD::on_pulse, this);
}

CameraHUD::~CameraHUD()
{
    if (m_pulse_timer.IsRunning())
        m_pulse_timer.Stop();
}

void CameraHUD::SetLiveActive(bool live)
{
    if (m_live != live) {
        m_live = live;
        if (!m_live)
            m_phase = 0.0;
        Refresh(); // one full repaint to draw / erase the badge pill
    }

    // Reconcile the timer with the live flag AND on-screen visibility so a busy
    // 50 ms timer never survives on a hidden monitor tab. When the page is
    // shown again StatusPanel's per-second update calls this again and restarts
    // the pulse.
    if (m_live && IsShownOnScreen()) {
        if (!m_pulse_timer.IsRunning())
            m_pulse_timer.Start(kPulseTick);
    } else if (m_pulse_timer.IsRunning()) {
        m_pulse_timer.Stop();
    }
}

void CameraHUD::SetTemperatures(int nozzle_c, int bed_c)
{
    bool changed = false;
    if (m_nozzle_chip) {
        m_nozzle_chip->SetText(deg_c(nozzle_c));
        if (!m_nozzle_chip->IsShown()) {
            m_nozzle_chip->Show();
            changed = true;
        }
    }
    if (m_bed_chip) {
        m_bed_chip->SetText(deg_c(bed_c));
        if (!m_bed_chip->IsShown()) {
            m_bed_chip->Show();
            changed = true;
        }
    }
    if (changed)
        Layout();
}

void CameraHUD::HideTemperatures()
{
    bool changed = false;
    if (m_nozzle_chip && m_nozzle_chip->IsShown()) {
        m_nozzle_chip->Hide();
        changed = true;
    }
    if (m_bed_chip && m_bed_chip->IsShown()) {
        m_bed_chip->Hide();
        changed = true;
    }
    if (changed)
        Layout();
}

bool CameraHUD::Enable(bool enable)
{
    const bool ret = wxPanel::Enable(enable);
    if (!enable) {
        m_live = false;
        if (m_pulse_timer.IsRunning())
            m_pulse_timer.Stop();
    }
    if (m_setting_chip)
        m_setting_chip->Enable(enable);
    if (m_fullscreen_chip)
        m_fullscreen_chip->Enable(enable);
    Refresh();
    return ret;
}

void CameraHUD::msw_rescale()
{
    SetMinSize(wxSize(-1, FromDIP(kHudHeight)));
    if (m_badge_spacer)
        m_badge_spacer->SetMinSize(FromDIP(80), FromDIP(kHudHeight));
    if (m_setting_chip)
        m_setting_chip->msw_rescale();
    if (m_fullscreen_chip)
        m_fullscreen_chip->msw_rescale();
    if (m_nozzle_chip)
        m_nozzle_chip->msw_rescale();
    if (m_bed_chip)
        m_bed_chip->msw_rescale();
    Layout();
    Refresh();
}

void CameraHUD::on_pulse(wxTimerEvent &)
{
    if (!m_live || !IsShownOnScreen()) {
        m_pulse_timer.Stop();
        return;
    }
    m_phase += 2.0 * kPi * kPulseTick / static_cast<double>(kPulsePeriod);
    if (m_phase > 2.0 * kPi)
        m_phase -= 2.0 * kPi;

    // Repaint ONLY the dot's bounding box, never the whole strip/panel.
    if (m_dot_rect.IsEmpty())
        Refresh();
    else
        RefreshRect(m_dot_rect);
}

void CameraHUD::on_paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC pdc(this);
    const wxSize          sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0)
        return;

    wxGCDC             dc(pdc);
    wxGraphicsContext *gc = dc.GetGraphicsContext();
    if (!gc)
        return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    // Carve rounded TOP corners against the page colour: fill everything with
    // the parent background, then lay a dark rounded rectangle that extends
    // below the visible area so only the top two corners round off.
    const wxColour behind = GetParent() ? GetParent()->GetBackgroundColour() : CardBg();
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(behind));
    gc->DrawRectangle(0, 0, sz.x, sz.y);

    const double radius = FromDIP(16);
    gc->SetBrush(wxBrush(CardBg()));
    gc->DrawRoundedRectangle(0, 0, sz.x, sz.y + radius, radius);

    // Hairline that separates the strip from the video below.
    gc->SetPen(wxPen(Border(), 1));
    gc->StrokeLine(0, sz.y - 0.5, sz.x, sz.y - 0.5);

    if (m_live) {
        const wxString live = _L("LIVE");

        wxFont badge = GetFont();
        badge.SetWeight(wxFONTWEIGHT_SEMIBOLD); // kit LIVE label is weight 600
        double pt = 11.0;
#ifndef __APPLE__
        pt = pt * 4.0 / 5.0;
#endif
        badge.SetFractionalPointSize(pt);
        gc->SetFont(badge, *wxWHITE);

        wxDouble tw = 0, th = 0;
        gc->GetTextExtent(live, &tw, &th);

        const double padX  = FromDIP(9);
        const double dotR  = FromDIP(7) / 2.0; // kit dot is 7x7px
        const double gap   = FromDIP(6);
        const double pillH = FromDIP(22);
        const double pillX = FromDIP(14);
        const double pillY = (sz.y - pillH) / 2.0;
        const double pillW = padX + dotR * 2.0 + gap + tw + padX;

        // Semi-transparent black pill (alpha via graphics context, reliable on
        // MSW where raw wxDC alpha is not).
        gc->SetBrush(wxBrush(wxColour(0, 0, 0, 140)));
        gc->DrawRoundedRectangle(pillX, pillY, pillW, pillH, pillH / 2.0);

        const double dotCx = pillX + padX + dotR;
        const double dotCy = pillY + pillH / 2.0;
        const double op    = 0.45 + 0.55 * (0.5 + 0.5 * std::cos(m_phase));
        const int    alpha = std::clamp(static_cast<int>(op * 255.0 + 0.5), 0, 255);
        const wxColour &live_col = MD3::Viewport::live;
        gc->SetBrush(wxBrush(wxColour(live_col.Red(), live_col.Green(), live_col.Blue(), alpha)));
        gc->DrawEllipse(dotCx - dotR, dotCy - dotR, dotR * 2.0, dotR * 2.0);

        gc->DrawText(live, pillX + padX + dotR * 2.0 + gap, pillY + (pillH - th) / 2.0);

        const int pad = static_cast<int>(FromDIP(2));
        m_dot_rect    = wxRect(static_cast<int>(dotCx - dotR) - pad, static_cast<int>(dotCy - dotR) - pad,
                               static_cast<int>(dotR * 2.0) + 2 * pad, static_cast<int>(dotR * 2.0) + 2 * pad);
    } else {
        m_dot_rect = wxRect();
    }
}

}} // namespace Slic3r::GUI
