#include "MD3ColorPicker.hpp"

#include "Button.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "MD3DialogChrome.hpp"
#include "MD3Tokens.hpp"
#include "Slider.hpp"
#include "StateColor.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

// wx/colour.h is in scope through the header, which enables the wxColour
// adapters (from_wx / to_wx) at the bottom of ColorSpaces.hpp.
#include "ColorSpaces.hpp"

namespace {

constexpr int kFieldW = 280;
constexpr int kFieldH = 160;
constexpr int kHueH   = 22;
constexpr int kTone   = 11; // 5,10,...,95 tone quick picks
constexpr int kRowH   = 26; // one translation row
constexpr int kValueW = 250; // widest translation value at Mono_11 (xyz-d65 / oklab rows)
constexpr int kMaxAnyFormatLen = 128; // parser input bound: no notation needs more

using namespace MD3::Color;

// Display order of the translator rows. "Name" is present for every colour
// so the column never re-flows; its value is a dash when the colour has no
// CSS name.
const char *const kSpaces[] = {
    "Name", "HEX", "HEX8", "RGB", "RGBA", "HSL", "HSLA", "HSV", "HWB",
    "XYZ", "Lab", "LCH", "OKLab", "OKLCH", "CMYK",
};

void hsv_to_rgb8(double h, double s, double v, unsigned char &r, unsigned char &g, unsigned char &b)
{
    const Rgb c = hsv_to_rgb({ h, s, v });
    r = (unsigned char) to_byte(c.r);
    g = (unsigned char) to_byte(c.g);
    b = (unsigned char) to_byte(c.b);
}

void rgb_to_hsv8(const wxColour &col, double &h, double &s, double &v)
{
    const Hsv hsv = rgb_to_hsv({ col.Red() / 255.0, col.Green() / 255.0, col.Blue() / 255.0 });
    h = hsv.h; s = hsv.s; v = hsv.v;
}

// WCAG grade for a contrast ratio: AAA >= 7, AA >= 4.5, AA-large >= 3.
wxString wcag_grade(double ratio)
{
    if (ratio >= 7.0) return "AAA";
    if (ratio >= 4.5) return "AA";
    if (ratio >= 3.0) return _L("AA large text only");
    return _L("fails WCAG");
}

wxString ratio_text(double ratio)
{
    return wxString::Format("%.2f:1", ratio);
}

} // namespace

MD3ColorPickerDialog::ContrastContext MD3ColorPickerDialog::defaultContrastContext()
{
    return { StateColor::semantic(MD3::Role::OnSurface), StateColor::semantic(MD3::Role::Surface) };
}

MD3ColorPickerDialog::MD3ColorPickerDialog(wxWindow *parent, const wxColour &initial)
    : MD3ColorPickerDialog(parent, initial, defaultContrastContext())
{
}

MD3ColorPickerDialog::MD3ColorPickerDialog(wxWindow *parent, const wxColour &initial, const ContrastContext &contrast)
    : wxDialog(parent, wxID_ANY, _L("Material color picker"), wxDefaultPosition, wxDefaultSize,
               wxBORDER_NONE)
    , m_contrast(contrast)
{
    build(parent, initial);
}

void MD3ColorPickerDialog::build(wxWindow * /*parent*/, const wxColour &initial)
{
    const wxColour surface = StateColor::semantic(MD3::Role::Surface);
    const wxColour on_var  = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    SetBackgroundColour(surface);
    m_colour = initial.IsOk() ? initial : wxColour(20, 108, 46);
    m_alpha_percent = initial.IsOk() ? int(std::lround(initial.Alpha() * 100.0 / 255.0)) : 100;
    m_colour = wxColour(m_colour.Red(), m_colour.Green(), m_colour.Blue(), (unsigned char) std::lround(m_alpha_percent * 2.55));
    rgb_to_hsv8(m_colour, m_h, m_s, m_v);

    auto caption_label = [&](const wxString &text) {
        auto *l = new Label(this, Label::Head_12, text);
        l->SetBackgroundColour(surface);
        l->SetForegroundColour(on_var);
        return l;
    };

    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(new MD3DialogCaption(this, _L("Material color picker")), 0, wxEXPAND);

    // Two columns: the picker on the left, the translations on the right, so
    // the fifteen translator rows never push the dialog past 600 DIP tall.
    auto *columns = new wxBoxSizer(wxHORIZONTAL);
    auto *left    = new wxBoxSizer(wxVERTICAL);
    auto *right   = new wxBoxSizer(wxVERTICAL);

    // ---------------------------------------------------------- picker column
    m_sv_field = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(kFieldW), FromDIP(kFieldH)));
    m_sv_field->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_sv_field->SetName(_L("Saturation and value field"));
    m_sv_field->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_sv_field);
        const wxSize sz = m_sv_field->GetClientSize();
        wxImage img(sz.x, sz.y);
        unsigned char *data = img.GetData();
        for (int y = 0; y < sz.y; ++y)
            for (int x = 0; x < sz.x; ++x) {
                unsigned char r, g, b;
                hsv_to_rgb8(m_h, double(x) / (sz.x - 1), 1.0 - double(y) / (sz.y - 1), r, g, b);
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
    // Keyboard: arrows nudge saturation (left/right) and value (up/down) by
    // 1 %, Shift by 10 %, so the field is operable without a pointer.
    m_sv_field->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        const double step = e.ShiftDown() ? 0.10 : 0.01;
        switch (e.GetKeyCode()) {
        case WXK_LEFT:  set_from_hsv(m_h, std::clamp(m_s - step, 0.0, 1.0), m_v); break;
        case WXK_RIGHT: set_from_hsv(m_h, std::clamp(m_s + step, 0.0, 1.0), m_v); break;
        case WXK_UP:    set_from_hsv(m_h, m_s, std::clamp(m_v + step, 0.0, 1.0)); break;
        case WXK_DOWN:  set_from_hsv(m_h, m_s, std::clamp(m_v - step, 0.0, 1.0)); break;
        default: e.Skip();
        }
    });
    left->Add(m_sv_field, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    m_hue_strip = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(kFieldW), FromDIP(kHueH)));
    m_hue_strip->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_hue_strip->SetName(_L("Hue"));
    m_hue_strip->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_hue_strip);
        const wxSize sz = m_hue_strip->GetClientSize();
        wxImage img(sz.x, sz.y);
        unsigned char *data = img.GetData();
        for (int x = 0; x < sz.x; ++x) {
            unsigned char r, g, b;
            hsv_to_rgb8(360.0 * x / (sz.x - 1), 1.0, 1.0, r, g, b);
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
    m_hue_strip->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        const double step = e.ShiftDown() ? 10.0 : 1.0;
        switch (e.GetKeyCode()) {
        case WXK_LEFT:  set_from_hsv(std::clamp(m_h - step, 0.0, 360.0), m_s, m_v); break;
        case WXK_RIGHT: set_from_hsv(std::clamp(m_h + step, 0.0, 360.0), m_s, m_v); break;
        default: e.Skip();
        }
    });
    left->Add(m_hue_strip, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    m_tone_caption = caption_label(_L("Material tones"));
    left->Add(m_tone_caption, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    m_tone_row = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(kFieldW), FromDIP(28)));
    m_tone_row->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_tone_row->SetName(_L("Material tones"));
    m_tone_row->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_tone_row);
        const wxSize sz = m_tone_row->GetClientSize();
        const int w = sz.x / kTone;
        for (int i = 0; i < kTone; ++i) {
            unsigned char r, g, b;
            const double tone = (5.0 + i * 9.0) / 100.0; // 0.05 .. 0.95
            hsv_to_rgb8(m_h, m_s * (1.0 - tone * 0.35), tone, r, g, b);
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
    left->Add(m_tone_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(4));

    // Alpha: an MD3 Slider (keyboard-focusable, wxAccessible-named) with a
    // live percentage beside it.
    auto *alpha_row = new wxBoxSizer(wxHORIZONTAL);
    auto *alpha_caption = caption_label(_L("Opacity"));
    alpha_row->Add(alpha_caption, 0, wxALIGN_CENTER_VERTICAL);
    m_alpha = new Slider(this, m_alpha_percent, 0, 100, false, wxDefaultPosition, wxSize(FromDIP(kFieldW - 110), FromDIP(24)));
    m_alpha->SetName(_L("Opacity"));
    m_alpha->SetToolTip(_L("Opacity, 0 to 100 percent"));
    m_alpha->SetOnChange([this](int v) { set_alpha(v); });
    alpha_row->Add(m_alpha, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    m_alpha_value = new Label(this, Label::Mono_11, "100%");
    m_alpha_value->SetBackgroundColour(surface);
    m_alpha_value->SetForegroundColour(on_var);
    m_alpha_value->SetMinSize(wxSize(FromDIP(40), -1));
    alpha_row->Add(m_alpha_value, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    left->Add(alpha_row, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, FromDIP(12));

    auto *hex_row = new wxBoxSizer(wxHORIZONTAL);
    m_preview = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(30), FromDIP(30)));
    m_preview->SetBackgroundColour(m_colour);
    m_preview->SetName(_L("Preview"));
    hex_row->Add(m_preview, 0, wxALIGN_CENTER_VERTICAL);
    m_hex = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(110), -1));
    m_hex->SetFont(Label::Mono_11);
    m_hex->SetName(_L("HEX color"));
    m_hex->SetMaxLength(9);
    m_hex->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        if (!m_syncing) {
            wxColour c(m_hex->GetValue());
            if (c.IsOk() && m_hex->GetValue().length() == 7)
                set_colour(c, Source::HexField);
        }
        e.Skip();
    });
    hex_row->Add(m_hex, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));
    left->Add(hex_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    // "Enter any format": one field that understands every notation the
    // translator can print, plus CSS colour names. Parsing is local and
    // length-bounded; nothing leaves the process.
    left->Add(caption_label(_L("Enter any format")), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    m_any_format = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(kFieldW), -1));
    m_any_format->SetFont(Label::Mono_11);
    m_any_format->SetName(_L("Enter any color format"));
    m_any_format->SetHint("oklch(70% 0.1 200)  #rrggbbaa  hsl(...)  cmyk(...)  navy");
    m_any_format->SetToolTip(_L("Type a color in HEX, RGB, HSL, HSV, HWB, XYZ, Lab, LCH, OKLab, OKLCH, CMYK or a CSS color name"));
    m_any_format->SetMaxLength(kMaxAnyFormatLen);
    m_any_format->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        if (!m_syncing) on_any_format_edited();
        e.Skip();
    });
    left->Add(m_any_format, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(4));

    // Inline, non-blocking status: active colour space and gamut, or the
    // parse problem / clipping warning. Reserved at two lines so the column
    // never re-flows as the message changes.
    m_status = new Label(this, Label::Body_12, wxEmptyString, LB_AUTO_WRAP, wxSize(FromDIP(kFieldW), -1));
    m_status->SetBackgroundColour(surface);
    m_status->SetForegroundColour(on_var);
    m_status->SetName(_L("Color format status"));
    m_status->SetLabel(_L("Outside the sRGB gamut: the picker shows the nearest sRGB color (clipped)."));
    m_status->Wrap(FromDIP(kFieldW));
    m_status->SetMinSize(wxSize(FromDIP(kFieldW), m_status->GetBestSize().y));
    left->Add(m_status, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(4));

    m_gamut = new Label(this, Label::Body_12, wxEmptyString);
    m_gamut->SetBackgroundColour(surface);
    m_gamut->SetForegroundColour(on_var);
    m_gamut->SetName(_L("Active color space and gamut"));
    left->Add(m_gamut, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(2));

    // Contrast readout against the caller's foreground/background pair.
    m_contrast_line = new Label(this, Label::Body_12, wxEmptyString, LB_AUTO_WRAP, wxSize(FromDIP(kFieldW), -1));
    m_contrast_line->SetBackgroundColour(surface);
    m_contrast_line->SetForegroundColour(on_var);
    m_contrast_line->SetName(_L("Contrast"));
    m_contrast_line->SetLabel(_L("Text on it: 21.00:1 (AA large text only)") + wxString::FromUTF8("  ·  ") + _L("As text: 21.00:1 (AA large text only)"));
    m_contrast_line->Wrap(FromDIP(kFieldW));
    m_contrast_line->SetMinSize(wxSize(FromDIP(kFieldW), m_contrast_line->GetBestSize().y));
    left->Add(m_contrast_line, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

    // ---------------------------------------------------- translations column
    right->Add(caption_label(_L("Translations")), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    const wxColour field_bg = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour on       = StateColor::semantic(MD3::Role::OnSurface);
    for (const char *space : kSpaces) {
        TranslationRow row;
        row.space = space;
        auto *line = new wxBoxSizer(wxHORIZONTAL);
        row.caption = new Label(this, Label::Body_12, wxString::FromUTF8(space));
        row.caption->SetBackgroundColour(surface);
        row.caption->SetForegroundColour(on_var);
        row.caption->SetMinSize(wxSize(FromDIP(48), -1));
        line->Add(row.caption, 0, wxALIGN_CENTER_VERTICAL);
        row.value = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                   wxSize(FromDIP(kValueW), FromDIP(kRowH)), wxTE_READONLY | wxBORDER_NONE);
        row.value->SetFont(Label::Mono_11);
        row.value->SetBackgroundColour(field_bg);
        row.value->SetForegroundColour(on);
        row.value->SetName(wxString::FromUTF8(space));
        line->Add(row.value, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
        row.copy = new Button(this, wxEmptyString);
        row.copy->SetIconButton(Button::IconShape::Circle, FromDIP(kRowH));
        row.copy->SetGlyph(MaterialIcon::ContentCopy, 16);
        const wxString copy_name = wxString::Format(_L("Copy %s"), wxString::FromUTF8(space));
        row.copy->SetToolTip(copy_name);
        row.copy->SetName(copy_name);
        wxTextCtrl *value = row.value;
        row.copy->Bind(wxEVT_BUTTON, [this, value](wxCommandEvent &) { copy_to_clipboard(value->GetValue()); });
        line->Add(row.copy, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        right->Add(line, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, FromDIP(4));
        m_rows.push_back(row);
    }
    right->AddSpacer(FromDIP(12));

    columns->Add(left, 0);
    columns->Add(right, 0, wxLEFT | wxRIGHT, FromDIP(4));
    root->Add(columns, 0);

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
    refresh_all(Source::Picker);
    CenterOnParent();
    MD3DialogCaption::FinishChrome(this);
}

void MD3ColorPickerDialog::set_from_hsv(double h, double s, double v, Source source)
{
    m_h = h; m_s = s; m_v = v;
    unsigned char r, g, b;
    hsv_to_rgb8(h, s, v, r, g, b);
    m_colour = wxColour(r, g, b, (unsigned char) std::lround(m_alpha_percent * 2.55));
    refresh_all(source);
}

void MD3ColorPickerDialog::set_colour(const wxColour &c, Source source)
{
    m_colour = wxColour(c.Red(), c.Green(), c.Blue(), (unsigned char) std::lround(m_alpha_percent * 2.55));
    rgb_to_hsv8(c, m_h, m_s, m_v);
    refresh_all(source);
}

void MD3ColorPickerDialog::set_alpha(int percent, Source source)
{
    m_alpha_percent = std::clamp(percent, 0, 100);
    m_colour = wxColour(m_colour.Red(), m_colour.Green(), m_colour.Blue(), (unsigned char) std::lround(m_alpha_percent * 2.55));
    refresh_all(source);
}

void MD3ColorPickerDialog::refresh_all(Source source)
{
    m_syncing = true;
    if (m_preview) {
        // The chip shows the composited colour: alpha over the dialog surface.
        const Rgb seen = composite_over(from_wx(m_colour), Rgb { from_wx(GetBackgroundColour()).r,
                                                                from_wx(GetBackgroundColour()).g,
                                                                from_wx(GetBackgroundColour()).b });
        m_preview->SetBackgroundColour(to_wx({ seen.r, seen.g, seen.b, 1.0 }));
        m_preview->Refresh();
    }
    if (m_sv_field) m_sv_field->Refresh();
    if (m_hue_strip) m_hue_strip->Refresh();
    if (m_tone_row) m_tone_row->Refresh();
    if (m_alpha && m_alpha->GetValue() != m_alpha_percent) m_alpha->SetValue(m_alpha_percent);
    if (m_alpha_value) m_alpha_value->SetLabel(wxString::Format("%d%%", m_alpha_percent));
    if (m_hex && source != Source::HexField)
        m_hex->ChangeValue(m_colour.GetAsString(wxC2S_HTML_SYNTAX));
    if (source != Source::AnyFormatField) {
        // A pick from the widgets is, by definition, an in-gamut sRGB colour.
        if (m_any_format && !m_any_format->GetValue().IsEmpty())
            m_any_format->ChangeValue(wxString::FromUTF8(format_hex8(from_wx(m_colour))));
        set_status(_L("Picked in sRGB. Type any notation below to jump the picker."), false);
        if (m_gamut) m_gamut->SetLabel(wxString::Format(_L("Active space: %s"), "sRGB (HSV pick)") + wxString::FromUTF8("  ·  ") + wxString::Format(_L("Gamut: %s"), _L("sRGB, in gamut")));
    }
    refresh_translations();
    refresh_contrast();
    m_syncing = false;
}

void MD3ColorPickerDialog::refresh_translations()
{
    const Rgba c = from_wx(m_colour);
    const std::vector<Translation> all = translate_all(c);
    for (TranslationRow &row : m_rows) {
        wxString text = wxString::FromUTF8("\xE2\x80\x94"); // em dash: no value (a colour with no CSS name)
        for (const Translation &t : all)
            if (std::string(t.space) == row.space) { text = wxString::FromUTF8(t.text); break; }
        if (row.value->GetValue() != text) {
            row.value->ChangeValue(text);
            row.value->SetToolTip(text);
        }
        row.copy->Enable(text != wxString::FromUTF8("\xE2\x80\x94"));
    }
}

void MD3ColorPickerDialog::refresh_contrast()
{
    if (!m_contrast_line) return;
    const Rgba picked = from_wx(m_colour);
    wxString parts;
    if (m_contrast.foreground.IsOk()) {
        // The picked colour as a background under the caller's text.
        const Rgba bg_ctx = m_contrast.background.IsOk() ? from_wx(m_contrast.background) : Rgba { 1, 1, 1, 1 };
        const Rgb  seen   = composite_over(picked, { bg_ctx.r, bg_ctx.g, bg_ctx.b });
        const Rgba fg     = from_wx(m_contrast.foreground);
        const double ratio = contrast_ratio(seen, { fg.r, fg.g, fg.b });
        parts += wxString::Format(_L("Text on it: %s (%s)"), ratio_text(ratio), wcag_grade(ratio));
    }
    if (m_contrast.background.IsOk()) {
        // The picked colour as text on the caller's surface.
        const Rgba bg   = from_wx(m_contrast.background);
        const Rgb  seen = composite_over(picked, { bg.r, bg.g, bg.b });
        const double ratio = contrast_ratio(seen, { bg.r, bg.g, bg.b });
        if (!parts.IsEmpty()) parts += wxString::FromUTF8("  \xC2\xB7  ");
        parts += wxString::Format(_L("As text: %s (%s)"), ratio_text(ratio), wcag_grade(ratio));
    }
    if (parts.IsEmpty()) parts = _L("No contrast pair supplied");
    m_contrast_line->SetLabel(parts);
    m_contrast_line->Wrap(FromDIP(kFieldW));
    m_contrast_line->SetToolTip(parts);
    Layout();
}

void MD3ColorPickerDialog::set_status(const wxString &text, bool warning)
{
    if (!m_status) return;
    m_status->SetForegroundColour(StateColor::semantic(warning ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
    m_status->SetLabel(text);
    m_status->Wrap(FromDIP(kFieldW));
    m_status->SetToolTip(text);
    m_status->Refresh();
    Layout();
}

void MD3ColorPickerDialog::on_any_format_edited()
{
    const wxString raw = m_any_format->GetValue();
    if (wxString(raw).Trim(true).Trim(false).IsEmpty()) {
        set_status(_L("Type any notation to jump the picker."), false);
        return;
    }
    const std::string utf8 = raw.ToStdString(wxConvUTF8);
    const std::optional<Parsed> parsed = parse(utf8);
    if (!parsed) {
        set_status(_L("Not a recognised color. Try #rrggbb, rgb(), hsl(), hsv(), hwb(), lab(), lch(), oklab(), oklch(), cmyk() or a CSS name."), true);
        return;
    }
    const wxString space = wxString::FromUTF8(parsed->space).Upper();
    if (m_gamut)
        m_gamut->SetLabel(wxString::Format(_L("Active space: %s") + wxString::FromUTF8("  ·  ") + _L("Gamut: %s"), space,
                                           parsed->clipped ? _L("outside sRGB") : _L("sRGB, in gamut")));
    if (parsed->clipped)
        set_status(_L("Outside the sRGB gamut: the picker shows the nearest sRGB color (clipped)."), true);
    else
        set_status(wxString::Format(_L("Parsed as %s."), space), false);

    m_alpha_percent = int(std::lround(parsed->rgba.a * 100.0));
    const wxColour c = to_wx(parsed->rgba);
    m_colour = c;
    rgb_to_hsv8(c, m_h, m_s, m_v);
    refresh_all(Source::AnyFormatField);
}

void MD3ColorPickerDialog::copy_to_clipboard(const wxString &text)
{
    if (text.IsEmpty() || !wxTheClipboard->Open())
        return;
    wxTheClipboard->SetData(new wxTextDataObject(text));
    wxTheClipboard->Close();
    set_status(wxString::Format(_L("Copied %s"), text), false);
}

void MD3ColorPickerDialog::sync_hex() { refresh_all(Source::Picker); }

void MD3ColorPickerDialog::rebuild_bitmaps() {}
