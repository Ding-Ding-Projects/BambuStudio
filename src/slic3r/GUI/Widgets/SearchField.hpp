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
// The kit additionally specifies a ".*" regex toggle and a `tune`
// regex-builder popover, both painted into the trailing area left of the clear
// button:
//   * a ".*" toggle pill — SecondaryContainer fill / OnSecondaryContainer glyph
//     when regex mode is active, OnSurfaceVariant when idle. Clicking it flips
//     regex mode (fires SetOnRegexToggle) and swaps the entry to Roboto Mono;
//   * a `tune` IconButton that opens a small transient BUILDER POPOVER carrying
//     quick-insert token chips (. * + ? [] () | ^ $ \d \w \s) plus "case
//     sensitive" and "whole word" checkboxes. Inserting a chip writes the token
//     at the entry caret and fires the query; the checkboxes drive the shared
//     textMatches() matcher and re-fire SetOnRegexToggle so consumers re-run.
// All colours and radii are resolved live per paint from MD3 tokens, so the
// field re-themes and re-DPIs with no stale cached geometry.
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

    // --- Regex / builder controls (kit ".*" toggle + tune popover). --------
    void SetRegexEnabled(bool on);
    bool IsRegexEnabled() const { return m_regex; }
    void SetOnRegexToggle(std::function<void(bool)> cb) { m_on_regex_toggle = std::move(cb); }
    // Fired (in addition to opening the built-in popover) when the tune button
    // is clicked, so a host may layer its own behaviour.
    void SetOnBuilderRequested(std::function<void()> cb) { m_on_builder = std::move(cb); }

    // Matcher modifiers driven by the builder popover (both default false).
    // Consumers read these to configure textMatches() and re-run their filter
    // from the SetOnRegexToggle callback, which is re-fired whenever regex,
    // case-sensitivity, or whole-word changes.
    bool IsCaseSensitive() const { return m_case_sensitive; }
    bool IsWholeWord() const { return m_whole_word; }
    void SetCaseSensitive(bool on);
    void SetWholeWord(bool on);

    // Shared query matcher for every SearchField consumer. Replaces the ad-hoc
    // `candidate.Lower().Contains(query.Lower())` filters.
    //   * empty query           -> matches everything (true);
    //   * regex == false         -> substring test (respecting caseSensitive);
    //                               wholeWord constrains hits to word boundaries;
    //   * regex == true          -> std::wregex search over candidate (icase
    //                               when !caseSensitive); wholeWord is ignored
    //                               (author your own \b). An invalid / half-typed
    //                               pattern returns true so nothing is hidden.
    static bool textMatches(const wxString &query, const wxString &candidate,
                            bool regex, bool caseSensitive, bool wholeWord = false);

    virtual void Rescale();

    bool Enable(bool enable = true) override;
    void SetMinSize(const wxSize &size) override;

protected:
    void doRender(wxDC &dc) override;

private:
    // Device-pixel rect of the circular clear button for the current size (the
    // 30px visual circle used for painting the glyph + hover disc).
    wxRect clearButtonRect() const;
    // Enlarged (>=44px logical) hit/hover region centered on clearButtonRect, so
    // the clear affordance meets the a11y minimum touch target without growing
    // the visible circle.
    wxRect clearHitRect() const;
    // Trailing ".*" regex toggle and `tune` builder button, both persistent and
    // anchored to the right of the entry, left of the (conditional) clear slot.
    wxRect tuneButtonRect() const;
    wxRect regexButtonRect() const;
    // Reserved leading width (pad + search glyph + gap), device px.
    int    leadingWidth() const;
    void   layoutText();
    void   applyTextCtrlTheme();
    // Set the field's (and entry's) accessible name from the placeholder.
    void   applyAccessibleName();
    void   emit(const wxString &value);
    void   onText();
    // Open (or reuse) the transient builder popover under the tune button.
    void   openBuilder();
    // Write a builder token at the entry caret and fire the query.
    void   insertToken(const wxString &token);

    wxTextCtrl *m_text = nullptr;

    wxString         m_placeholder;
    MD3::ColorScheme m_scheme = MD3::ColorScheme::Brand;

    bool m_focused     = false; // text entry holds focus -> Primary border
    bool m_clear_hover = false; // pointer over the clear button
    bool m_tune_hover  = false; // pointer over the tune button
    bool m_regex_hover = false; // pointer over the ".*" toggle
    bool m_had_text    = false; // last known non-empty state (for relayout)
    bool m_regex       = false; // regex mode flag (".*" toggle)
    bool m_case_sensitive = false; // matcher: case-sensitive substring / regex
    bool m_whole_word     = false; // matcher: whole-word substring constraint

    // Transient builder popover (a SearchBuilderPopup, owned as a child of this
    // field and reused across opens). Kept as wxWindow* so the concrete type
    // stays private to the .cpp.
    wxWindow *m_builder_popup = nullptr;

    std::function<void(const wxString &)> m_on_query;
    std::function<void(bool)>             m_on_regex_toggle;
    std::function<void()>                 m_on_builder;
};

#endif // !slic3r_GUI_SearchField_hpp_
