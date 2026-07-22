#ifndef slic3r_GUI_Label_hpp_
#define slic3r_GUI_Label_hpp_

#include <cstdint>

#include <wx/stattext.h>
#include <wx/window.h>

#define LB_HYPERLINK 0x0020
#define LB_PROPAGATE_MOUSE_EVENT 0x0040
#define LB_AUTO_WRAP 0x0080


class Label : public wxStaticText
{
public:
    Label(wxWindow *parent, wxString const &text = {}, long style = 0, wxSize size = wxDefaultSize);

	Label(wxWindow *parent, wxFont const &font, wxString const &text = {}, long style = 0, wxSize size = wxDefaultSize);

    void SetLabel(const wxString& label) override;

    void SetWindowStyleFlag(long style) override;

	void Wrap(int width);

protected:
	wxSize DoGetBestClientSize() const override;

private:
	void OnSize(wxSizeEvent & evt);

private:
    wxFont m_font;
    wxColour m_color;
	wxString m_text;
	bool m_skip_size_evt = false;

public:
    static wxFont Head_48;
    static wxFont Head_32;
	static wxFont Head_24;
	static wxFont Head_20;
	static wxFont Head_18;
	static wxFont Head_16;
	static wxFont Head_15;
	static wxFont Head_14;
	static wxFont Head_13;
	static wxFont Head_12;
	static wxFont Head_11;
    static wxFont Head_10;

	static wxFont Body_16;
	static wxFont Body_15;
	static wxFont Body_14;
    static wxFont Body_13;
	static wxFont Body_12;
	static wxFont Body_10;
	static wxFont Body_11;
	static wxFont Body_9;
	static wxFont Body_8;

	// Roboto Mono — the MD3 canon face for numeric/technical values
	// (temperatures, percentages, times, dimensions, commit hashes).
	static wxFont Mono_14;
	static wxFont Mono_13;
	static wxFont Mono_12;
	static wxFont Mono_11;

	static void initSysFont(std::string lang_code = "", bool load_font_resource = true);

    static wxFont sysFont(int size, bool bold = false, std::string lang_code = "");

    static wxSize split_lines(wxDC &dc, int width, const wxString &text, wxString &multiline_text, int max_count = 0);
};

// Shared MD3 section-label header (ui-md3 containment/SectionHeader): the uppercase
// micro-header that opens a sidebar / panel section. Renders 11px / weight 600,
// UPPERCASE, +0.6px letter-spacing, in OnSurfaceVariant, with an OPTIONAL 16px
// leading Material Symbol slot. The caller supplies the glyph codepoint (one of
// the MaterialIcon glyphs); this widget hardcodes none. Custom-drawn because a
// native wxStaticText cannot mix the icon font with the text run nor apply the
// letter-spacing the label style calls for. DPI-safe: gap / tracking are derived
// from the live DPI at paint, and the icon font scales with the DC — nothing is
// cached in device pixels.
class SectionHeader : public wxWindow
{
public:
    // leading_icon is a Material Symbols codepoint (e.g. MaterialIcon::Palette);
    // pass 0 for no leading glyph.
    SectionHeader(wxWindow *parent, wxString const &text = {}, uint32_t leading_icon = 0, long style = 0);

    void SetLabel(const wxString &label) override;

    // 0 clears the leading glyph.
    void SetLeadingIcon(uint32_t codepoint);

protected:
    wxSize DoGetBestClientSize() const override;

private:
    void OnPaint(wxPaintEvent &evt);

    wxString m_text;
    uint32_t m_icon{0};
};

#endif // !slic3r_GUI_Label_hpp_
