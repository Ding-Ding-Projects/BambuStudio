#ifndef slic3r_GUI_SearchField_hpp_
#define slic3r_GUI_SearchField_hpp_

#include <functional>

#include <wx/textctrl.h>

#include "StaticBox.hpp"
#include "MD3Tokens.hpp"

// Shared Material Design 3 search field (kit: fields/SearchField.prompt.md,
// design-source/SearchField.dc.html).
//
// Anatomy — the core pill only:
//   * 40px tall stadium pill, SurfaceContainerHighest fill, corner radius 22;
//   * 1px border in Outline at rest, promoted to Primary (scheme-aware) while
//     the input is focused;
//   * a leading 20px `search` Material Symbol in OnSurfaceVariant;
//   * a single-line text entry (Roboto 13.5) that blends into the pill fill;
//   * a 30px circular clear button carrying the 18px `close` glyph, shown only
//     while the field is non-empty and tinted OnSurfaceVariant (hover fills
//     SurfaceContainerLow).
//
// The kit additionally specifies an optional ".*" regex toggle and a `tune`
// regex-builder popover. Those are DEFERRED: the `tune` glyph is not yet part
// of the MaterialIcon set, so the toggle chrome is intentionally not drawn.
// The behavioural hooks (SetRegexEnabled / SetOnRegexToggle /
// SetOnBuilderRequested) are exposed now so a later wave can wire the builder
// without changing this widget's ABI. All colours and radii are resolved live
// per paint from MD3 tokens, so the field re-themes and re-DPIs with no stale
// cached geometry.
class SearchField : public wxNavigationEnabled<StaticBox>
{
public:
    SearchField();

    SearchField(wxWindow *      parent,
                const wxString &placeholder = wxEmptyString,
                const wxPoint & pos         = wxDefaultPosition,
                const wxSize &  size        = wxDefaultSize);

    ~SearchField() override = default;

    void Create(wxWindow *      parent,
                const wxString &placeholder = wxEmptyString,
                const wxPoint & pos         = wxDefaultPosition,
                const wxSize &  size        = wxDefaultSize);

    wxTextCtrl *      GetTextCtrl() { return m_text; }
    wxTextCtrl const *GetTextCtrl() const { return m_text; }

    // Current query text.
    wxString GetValue() const;
    // Replace the text without firing the query callback.
    void SetValue(const wxString &value);
    // Empty the field, focus it, and fire the query callback with "".
    void Clear();

    void SetPlaceholder(const wxString &placeholder);

    // Fired on every user edit and on Clear(), with the current query string.
    void SetOnQuery(std::function<void(const wxString &)> cb) { m_on_query = std::move(cb); }

    // Accent scheme for the focus border (Brand / Preview / Device).
    void SetColorScheme(MD3::ColorScheme scheme);

    // --- Deferred regex / builder hooks (kit ".*" + tune toggles). ---------
    // No chrome is drawn today (the `tune` glyph is unavailable); these keep
    // the regex state and callbacks addressable for a future wave.
    void SetRegexEnabled(bool on);
    bool IsRegexEnabled() const { return m_regex; }
    void SetOnRegexToggle(std::function<void(bool)> cb) { m_on_regex_toggle = std::move(cb); }
    void SetOnBuilderRequested(std::function<void()> cb) { m_on_builder = std::move(cb); }

    virtual void Rescale();

    bool Enable(bool enable = true) override;
    void SetMinSize(const wxSize &size) override;

protected:
    void doRender(wxDC &dc) override;

private:
    // Device-pixel rect of the circular clear button for the current size.
    wxRect clearButtonRect() const;
    // Reserved leading width (pad + search glyph + gap), device px.
    int    leadingWidth() const;
    void   layoutText();
    void   applyTextCtrlTheme();
    void   emit(const wxString &value);
    void   onText();

    wxTextCtrl *m_text = nullptr;

    wxString         m_placeholder;
    MD3::ColorScheme m_scheme = MD3::ColorScheme::Brand;

    bool m_focused     = false; // text entry holds focus -> Primary border
    bool m_clear_hover = false; // pointer over the clear button
    bool m_had_text    = false; // last known non-empty state (for relayout)
    bool m_regex       = false; // deferred regex mode flag

    std::function<void(const wxString &)> m_on_query;
    std::function<void(bool)>             m_on_regex_toggle;
    std::function<void()>                 m_on_builder;
};

#endif // !slic3r_GUI_SearchField_hpp_
