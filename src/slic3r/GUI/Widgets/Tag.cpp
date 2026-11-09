#include "Tag.hpp"

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>

#include "../PersonalVocabulary.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "MD3Tokens.hpp"
#include "StateColor.hpp"
#include "StaticBox.hpp"

namespace Slic3r::GUI {

namespace {
constexpr int kHeight   = 22;
constexpr int kPadX     = 8;
constexpr int kPadGlyph = 6; // leading padding when a glyph is drawn
constexpr int kGlyphPx  = 16;
constexpr int kGlyphGap = 4;

wxFont tag_font()
{
    wxFont font = ::Label::Body_11;
    font.SetNumericWeight(500);
    return font;
}
} // namespace

Tag::Tag(wxWindow *parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(StaticBox::GetParentBackgroundColor(parent));
    SetFont(tag_font());
    Bind(wxEVT_PAINT, &Tag::on_paint, this);
}

bool Tag::has_glyph() const { return m_glyph != 0 && MaterialIcon::available(); }

void Tag::SetText(const wxString &text)
{
    if (m_text == text) return;
    m_text = text;
    InvalidateBestSize();
    Refresh();
}

void Tag::SetKind(Kind kind)
{
    if (m_kind == kind) return;
    m_kind = kind;
    Refresh();
}

void Tag::SetGlyph(uint32_t codepoint)
{
    if (m_glyph == codepoint) return;
    m_glyph = codepoint;
    InvalidateBestSize();
    Refresh();
}

wxColour Tag::fill_colour() const
{
    switch (m_kind) {
    case Kind::Green: return StateColor::semantic(MD3::Role::PrimaryContainer);
    case Kind::Tonal: return StateColor::semantic(MD3::Role::SecondaryContainer);
    case Kind::Red:   return StateColor::semantic(MD3::Role::ErrorContainer);
    case Kind::Neutral:
    default:          return StateColor::semantic(MD3::Role::SurfaceContainerHighest);
    }
}

wxColour Tag::text_colour() const
{
    switch (m_kind) {
    case Kind::Green: return StateColor::semantic(MD3::Role::OnPrimaryContainer);
    case Kind::Tonal: return StateColor::semantic(MD3::Role::OnSecondaryContainer);
    case Kind::Red:   return StateColor::semantic(MD3::Role::OnErrorContainer);
    case Kind::Neutral:
    default:          return StateColor::semantic(MD3::Role::OnSurfaceVariant);
    }
}

wxSize Tag::DoGetBestSize() const
{
    wxClientDC dc(const_cast<Tag *>(this));
    dc.SetFont(GetFont());
    const wxString shown = PersonalVocabulary::display(m_text);
    int            width = m_text.empty() ? 0 : dc.GetTextExtent(shown).x;
    width += has_glyph() ? FromDIP(kPadGlyph + kGlyphPx + kGlyphGap + kPadX) : FromDIP(2 * kPadX);
    return wxSize(width, FromDIP(kHeight));
}

void Tag::on_paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    wxGCDC       gdc(dc);
    const wxSize size = GetClientSize();
    gdc.SetPen(*wxTRANSPARENT_PEN);
    gdc.SetBrush(wxBrush(fill_colour()));
    gdc.DrawRoundedRectangle(wxRect(0, 0, size.x, size.y), FromDIP(MD3::Metrics::radius_tag));

    int x = FromDIP(has_glyph() ? kPadGlyph : kPadX);
    if (has_glyph()) {
        const wxBitmap glyph = MaterialIcon::bitmap(this, m_glyph, kGlyphPx, text_colour());
        if (glyph.IsOk()) {
            const wxSize gs = glyph.GetScaledSize();
            gdc.DrawBitmap(glyph, x, (size.y - gs.y) / 2);
            x += gs.x;
        } else {
            x += FromDIP(kGlyphPx);
        }
        x += FromDIP(kGlyphGap);
    }
    gdc.SetFont(GetFont());
    gdc.SetTextForeground(text_colour());
    const wxString shown = PersonalVocabulary::display(m_text);
    gdc.DrawText(shown, x, (size.y - gdc.GetTextExtent(shown).y) / 2);
}

} // namespace Slic3r::GUI
