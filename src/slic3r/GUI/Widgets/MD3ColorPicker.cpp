#include "MD3ColorPicker.hpp"

#include "Button.hpp"
#include "Label.hpp"
#include "MD3Tokens.hpp"
#include "SearchField.hpp"
#include "StateColor.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <algorithm>
#include <cmath>

#include <wx/dcbuffer.h>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

namespace {

constexpr int kFieldW = 280;
constexpr int kFieldH = 170;
constexpr int kHueH   = 22;
constexpr int kTone   = 11; // 5,10,...,95 tone quick picks

void hsv_to_rgb(double h, double s, double v, unsigned char &r, unsigned char &g, unsigned char &b)
{
    const double c = v * s;
    const double hp = std::fmod(std::max(0.0, h), 360.0) / 60.0;
    const double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    double rr = 0, gg = 0, bb = 0;
    switch (int(hp)) {
    case 0: rr = c; gg = x; break;
    case 1: rr = x; gg = c; break;
    case 2: gg = c; bb = x; break;
    case 3: gg = x; bb = c; break;
    case 4: rr = x; bb = c; break;
    default: rr = c; bb = x; break;
    }
    const double m = v - c;
    r = (unsigned char) std::lround((rr + m) * 255.0);
    g = (unsigned char) std::lround((gg + m) * 255.0);
    b = (unsigned char) std::lround((bb + m) * 255.0);
}

void rgb_to_hsv(const wxColour &col, double &h, double &s, double &v)
{
    const double r = col.Red() / 255.0, g = col.Green() / 255.0, b = col.Blue() / 255.0;
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    v = mx;
    s = mx <= 0.0 ? 0.0 : d / mx;
    if (d <= 0.0)      h = 0.0;
    else if (mx == r)  h = 60.0 * std::fmod((g - b) / d, 6.0);
    else if (mx == g)  h = 60.0 * ((b - r) / d + 2.0);
    else               h = 60.0 * ((r - g) / d + 4.0);
    if (h < 0.0) h += 360.0;
}

} // namespace

MD3ColorPickerDialog::MD3ColorPickerDialog(wxWindow *parent, const wxColour &initial)
    : wxDialog(parent, wxID_ANY, _L("Material color picker"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE)
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    m_colour = initial.IsOk() ? initial : wxColour(20, 108, 46);
    rgb_to_hsv(m_colour, m_h, m_s, m_v);

    auto *root = new wxBoxSizer(wxVERTICAL);

    m_sv_field = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(kFieldW), FromDIP(kFieldH)));
    m_sv_field->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_sv_field->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_sv_field);
        const wxSize sz = m_sv_field->GetClientSize();
        wxImage img(sz.x, sz.y);
        unsigned char *data = img.GetData();
        for (int y = 0; y < sz.y; ++y)
            for (int x = 0; x < sz.x; ++x) {
                unsigned char r, g, b;
                hsv_to_rgb(m_h, double(x) / (sz.x - 1), 1.0 - double(y) / (sz.y - 1), r, g, b);
                unsigned char *px = data + 3 * (y * sz.x + x);
                px[0] = r; px[1] = g; px[2] = b;
            }
        dc.DrawBitmap(wxBitmap(img), 0, 0);
        // pick marker
        const int mx = int(m_s * (sz.x - 1)), my = int((1.0 - m_v) * (sz.y - 1));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(*wxWHITE, 2));
        dc.DrawCircle(mx, my, FromDIP(7));
        dc.SetPen(wxPen(*wxBLACK, 1));
        dc.DrawCircle(mx, my, FromDIP(8));
    });
    auto sv_pick = [this](wxMouseEvent &e) {
        if (!e.Dragging() && !e.LeftDown()) { e.Skip(); return; }
        const wxSize sz = m_sv_field->GetClientSize();
        const double s = std::clamp(double(e.GetX()) / (sz.x - 1), 0.0, 1.0);
        const double v = std::clamp(1.0 - double(e.GetY()) / (sz.y - 1), 0.0, 1.0);
        set_from_hsv(m_h, s, v);
    };
    m_sv_field->Bind(wxEVT_LEFT_DOWN, sv_pick);
    m_sv_field->Bind(wxEVT_MOTION, sv_pick);
    root->Add(m_sv_field, 0, wxALL, FromDIP(16));

    m_hue_strip = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(kFieldW), FromDIP(kHueH)));
    m_hue_strip->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_hue_strip->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_hue_strip);
        const wxSize sz = m_hue_strip->GetClientSize();
        wxImage img(sz.x, sz.y);
        unsigned char *data = img.GetData();
        for (int x = 0; x < sz.x; ++x) {
            unsigned char r, g, b;
            hsv_to_rgb(360.0 * x / (sz.x - 1), 1.0, 1.0, r, g, b);
            for (int y = 0; y < sz.y; ++y) {
                unsigned char *px = data + 3 * (y * sz.x + x);
                px[0] = r; px[1] = g; px[2] = b;
            }
        }
        dc.DrawBitmap(wxBitmap(img), 0, 0);
        const int hx = int(m_h / 360.0 * (sz.x - 1));
        dc.SetPen(wxPen(*wxWHITE, 2));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(hx - FromDIP(3), 0, FromDIP(6), sz.y);
    });
    auto hue_pick = [this](wxMouseEvent &e) {
        if (!e.Dragging() && !e.LeftDown()) { e.Skip(); return; }
        const wxSize sz = m_hue_strip->GetClientSize();
        set_from_hsv(std::clamp(360.0 * e.GetX() / (sz.x - 1), 0.0, 360.0), m_s, m_v);
    };
    m_hue_strip->Bind(wxEVT_LEFT_DOWN, hue_pick);
    m_hue_strip->Bind(wxEVT_MOTION, hue_pick);
    root->Add(m_hue_strip, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(16));

    m_tone_caption = new Label(this, Label::Head_12, _L("Material tones"));
    m_tone_caption->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    m_tone_caption->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    root->Add(m_tone_caption, 0, wxLEFT | wxRIGHT, FromDIP(16));

    m_tone_row = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(kFieldW), FromDIP(30)));
    m_tone_row->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_tone_row->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_tone_row);
        const wxSize sz = m_tone_row->GetClientSize();
        const int w = sz.x / kTone;
        for (int i = 0; i < kTone; ++i) {
            unsigned char r, g, b;
            const double tone = (5.0 + i * 9.0) / 100.0; // 0.05 .. 0.95
            hsv_to_rgb(m_h, m_s * (1.0 - tone * 0.35), tone, r, g, b);
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(wxColour(r, g, b)));
            dc.DrawRoundedRectangle(i * w + 1, 1, w - 2, sz.y - 2, FromDIP(5));
        }
    });
    m_tone_row->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        const wxSize sz = m_tone_row->GetClientSize();
        const int i = std::clamp(e.GetX() / (sz.x / kTone), 0, kTone - 1);
        const double tone = (5.0 + i * 9.0) / 100.0;
        set_from_hsv(m_h, m_s * (1.0 - tone * 0.35), tone);
    });
    root->Add(m_tone_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    auto *hex_row = new wxBoxSizer(wxHORIZONTAL);
    m_preview = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(30), FromDIP(30)));
    m_preview->SetBackgroundColour(m_colour);
    hex_row->Add(m_preview, 0, wxALIGN_CENTER_VERTICAL);
    m_hex = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(110), -1));
    m_hex->SetFont(Label::Mono_11);
    m_hex->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        wxColour c(m_hex->GetValue());
        if (c.IsOk() && m_hex->GetValue().length() == 7)
            set_colour(c, false);
        e.Skip();
    });
    hex_row->Add(m_hex, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));
    root->Add(hex_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    // Colour translator: the same colour in every notation people paste at
    // each other - rgb(), hsv(), and the nearest everyday name (the exact
    // text colour-aware search matches on).
    m_translate = new Label(this, Label::Mono_11, wxEmptyString);
    m_translate->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    m_translate->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    root->Add(m_translate, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    auto *cancel = new Button(this, _L("Cancel"), "", 0, 0, wxID_CANCEL);
    auto *ok     = new Button(this, _L("OK"), "", 0, 0, wxID_OK);
    for (Button *b : {cancel, ok})
        b->SetMinSize(wxSize(FromDIP(96), FromDIP(36)));
    ok->SetVariant(Button::Variant::Filled);
    cancel->SetVariant(Button::Variant::Outlined);
    cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    ok->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
    actions->AddStretchSpacer();
    actions->Add(cancel, 0, wxRIGHT, FromDIP(8));
    actions->Add(ok, 0);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(16));

    SetSizerAndFit(root);
    sync_hex();
    CenterOnParent();
}

void MD3ColorPickerDialog::set_from_hsv(double h, double s, double v, bool update_hex)
{
    m_h = h; m_s = s; m_v = v;
    unsigned char r, g, b;
    hsv_to_rgb(h, s, v, r, g, b);
    m_colour = wxColour(r, g, b);
    m_preview->SetBackgroundColour(m_colour);
    m_preview->Refresh();
    m_sv_field->Refresh();
    m_hue_strip->Refresh();
    m_tone_row->Refresh();
    if (update_hex)
        sync_hex();
}

void MD3ColorPickerDialog::set_colour(const wxColour &c, bool update_hex)
{
    m_colour = c;
    rgb_to_hsv(c, m_h, m_s, m_v);
    m_preview->SetBackgroundColour(c);
    m_preview->Refresh();
    m_sv_field->Refresh();
    m_hue_strip->Refresh();
    m_tone_row->Refresh();
    if (update_hex)
        sync_hex();
}

void MD3ColorPickerDialog::sync_hex()
{
    if (m_hex)
        m_hex->ChangeValue(m_colour.GetAsString(wxC2S_HTML_SYNTAX));
    if (m_translate) {
        const wxString named = SearchField::colorSearchText(m_colour).AfterFirst(' ');
        m_translate->SetLabel(wxString::Format("rgb(%d, %d, %d)  hsv(%d, %d%%, %d%%)  ~ %s",
            m_colour.Red(), m_colour.Green(), m_colour.Blue(),
            int(std::lround(m_h)), int(std::lround(m_s * 100)), int(std::lround(m_v * 100)),
            named));
        // The translation grows/shrinks per pick; without a re-layout the
        // label keeps its first width and clips the colour name.
        Layout();
    }
}

void MD3ColorPickerDialog::rebuild_bitmaps() {}
