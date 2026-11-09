#pragma once

#include <cstdint>

#include <wx/colour.h>
#include <wx/string.h>
#include <wx/window.h>

namespace Slic3r::GUI {

/**
 * \brief A small status tag: a soft rounded rectangle holding an optional leading Material glyph and
 * one line of text (board `.tag`).
 *
 * The geometry is fixed by the design: 22 high, 8 horizontal padding (6 before a glyph), a 16 px
 * glyph followed by a 4 px gap, corner radius MD3::Metrics::radius_tag, 11 px label at weight 500.
 * Colours are resolved from Material roles at paint time, so a theme change is followed without a
 * rebuild. A tag is not interactive and never takes keyboard focus. It paints its rounded corners
 * over the window background colour, so a host whose fill changes (a hovered row) keeps the
 * background colour of the tag in step.
 */
class Tag : public wxWindow
{
public:
    /// Colour meaning of the tag.
    enum class Kind {
        Neutral, ///< SurfaceContainerHighest / OnSurfaceVariant
        Green,   ///< PrimaryContainer / OnPrimaryContainer
        Tonal,   ///< SecondaryContainer / OnSecondaryContainer
        Red,     ///< ErrorContainer / OnErrorContainer
    };

    explicit Tag(wxWindow *parent);

    void SetText(const wxString &text);
    const wxString &GetText() const { return m_text; }
    void SetKind(Kind kind);
    /// Leading Material glyph codepoint; 0 draws no glyph.
    void SetGlyph(uint32_t codepoint);

    bool AcceptsFocus() const override { return false; }

protected:
    wxSize DoGetBestSize() const override;

private:
    void on_paint(wxPaintEvent &);
    wxColour fill_colour() const;
    wxColour text_colour() const;
    bool     has_glyph() const;

    wxString m_text;
    Kind     m_kind{Kind::Neutral};
    uint32_t m_glyph{0};
};

} // namespace Slic3r::GUI
