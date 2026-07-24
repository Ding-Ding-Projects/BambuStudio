#ifndef slic3r_GUI_RegexBuilderPopup_hpp_
#define slic3r_GUI_RegexBuilderPopup_hpp_

#include <functional>
#include <vector>

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>

#include "MD3Tokens.hpp"
#include "PopupWindow.hpp"

class Button;
class CheckBox;
class Label;

// Full guided regex-builder popover for SearchField (kit: fields/SearchField
// `tune` popover, expanded to the every-project builder spec).
//
// Anatomy — a rounded SurfaceContainerHigh card hosting real child controls
// (wxPU_CONTAINS_CONTROLS, the same transient-popup-with-inputs pattern as
// Search.cpp's SearchDialog) inside a vertical wxScrolledWindow:
//   * an engine caption naming the real matcher: std::regex / std::wregex,
//     ECMAScript grammar, icase flag, backslash escaping;
//   * a raw pattern editor (Roboto Mono), bidirectionally synced with the
//     owning SearchField's query, with a copy-to-clipboard IconButton;
//   * a live validity line — Primary "valid" / Error with the std::regex_error
//     code mapped to a friendly message;
//   * flag rows: regex mode (the ".*" toggle), case sensitive, whole word;
//   * guided sections of insertable token chips with per-token tooltips:
//     literals (auto-escaped input), character classes, anchors, groups /
//     alternation, quantifiers (greedy + lazy);
//   * a collapsible "Test pattern" section: bounded multiline sample text with
//     match highlighting plus a match / capture-group listing.
// Evaluation is local and bounded (pattern <= 2000 chars, sample <= 20000
// chars, first 200 matches) and every std::regex call is wrapped in try/catch
// so an invalid or pathological pattern can never throw out of the popover.
//
// The popover is rebuilt by SearchField on every open, so colours, fonts and
// FromDIP metrics re-derive per open (theme / DPI / density safe).
class RegexBuilderPopup : public PopupWindow
{
public:
    struct Callbacks
    {
        std::function<void(const wxString &)> onPattern;   // raw editor edit -> field query
        std::function<void(bool)>             onRegexMode; // regex-mode flag toggled
        std::function<void(bool)>             onCase;      // case-sensitive flag toggled
        std::function<void(bool)>             onWord;      // whole-word flag toggled
    };

    explicit RegexBuilderPopup(wxWindow *parent);

    // (Re)load state from the owning field. Call before Popup().
    void Configure(MD3::ColorScheme scheme, const wxString &pattern, bool regexOn,
                   bool caseOn, bool wordOn, Callbacks callbacks);

    // Field -> popover pattern sync while the popover is open (no echo back
    // through onPattern).
    void SyncPattern(const wxString &pattern);

    wxString GetPattern() const;

    // Move keyboard focus into the raw pattern editor (call after Popup()).
    void FocusPattern();

private:
    // One insertable token: chip label, inserted text, caret step-back after
    // insert (lands the caret inside "()" / "[]"), and its explanatory tooltip.
    struct ChipDef
    {
        wxString label;
        wxString insert;
        int      caret_back;
        wxString tip;
    };

    class ChipGroup;

    void build();
    void addSection(wxSizer *sizer, const wxString &title, const std::vector<ChipDef> &defs);
    void insertChip(const ChipDef &def);
    void addLiteral();
    void copyPattern();
    void toggleTest();
    void onPatternEdited();
    // Recompile + revalidate the pattern, re-run the bounded sample match, and
    // refresh status / highlights / results.
    void evaluate();
    // Size the popup to its content, capped to the display (scrolls beyond).
    void fitPopup();

    static wxString escapeLiteral(const wxString &raw);

    wxScrolledWindow *m_scroll  = nullptr;
    wxTextCtrl       *m_pattern = nullptr;
    Button           *m_copy    = nullptr;
    Label            *m_status  = nullptr;
    wxTextCtrl       *m_literal = nullptr;
    CheckBox         *m_regex_cb = nullptr;
    CheckBox         *m_case_cb  = nullptr;
    CheckBox         *m_word_cb  = nullptr;
    Button           *m_test_toggle = nullptr;
    wxPanel          *m_test_panel  = nullptr;
    wxTextCtrl       *m_sample  = nullptr;
    wxTextCtrl       *m_results = nullptr;

    MD3::ColorScheme m_scheme    = MD3::ColorScheme::Brand;
    bool             m_regex_on  = false;
    bool             m_case_on   = false;
    bool             m_word_on   = false;
    bool             m_test_open = false;
    bool             m_syncing   = false; // guards field->popover sync against echo

    Callbacks m_cb;
};

#endif // !slic3r_GUI_RegexBuilderPopup_hpp_
