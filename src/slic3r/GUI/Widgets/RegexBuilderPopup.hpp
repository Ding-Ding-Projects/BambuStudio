#ifndef slic3r_GUI_RegexBuilderPopup_hpp_
#define slic3r_GUI_RegexBuilderPopup_hpp_

#include <functional>
#include <cstddef>
#include <utility>
#include <vector>

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>

#include "MD3Tokens.hpp"
#include "PopupWindow.hpp"
#include "MD3DialogChrome.hpp"

class Button;
class CheckBox;
class Label;

// Full guided regex-builder popover for SearchField (kit: fields/SearchField
// `tune` popover, expanded to the every-project builder spec).
//
// Anatomy — a rounded SurfaceContainerHigh card hosting real child controls
// (wxPU_CONTAINS_CONTROLS, the same transient-popup-with-inputs pattern as
// Search.cpp's SearchDialog) inside a vertical wxScrolledWindow:
//   * an engine caption naming the real matcher: Boost.Regex 1.84
//     wide-character ECMAScript in the isolated bounded worker, icase flag,
//     backslash escaping;
//   * a raw pattern editor (Roboto Mono), bidirectionally synced with the
//     owning SearchField's query, with a copy-to-clipboard IconButton;
//   * a live validity line — Primary "valid" / Error with the worker's stable
//     regex error code mapped to a friendly message;
//   * flag rows: regex mode (the ".*" toggle), case sensitive, multiline,
//     whole word;
//   * guided sections of insertable token chips with per-token tooltips:
//     literals (auto-escaped input), character classes, anchors, groups /
//     alternation, quantifiers (greedy + lazy);
//   * a collapsible "Test pattern" section: bounded multiline sample text with
//     match highlighting plus a match / capture-group listing;
//   * an Explain tab (RegexAnalysis.hpp): the engine identity the worker
//     reports (Boost version, syntax, code-unit width, locale traits, state
//     limit), the flags in effect and their exact engine flags, the engine's
//     verdict with its error offset, the structure tree, token-by-token
//     annotation, compatibility warnings and backtracking-risk findings with
//     an adversarial example that can be loaded as sample text;
//   * the Reference tab also lists the flag table, escaping rules and the
//     capability matrix, whose probes can be checked against the worker.
// Evaluation is local and bounded (pattern <= 512 code units, sample <= 8192,
// first 200 matches, 50 ms worker deadline). A pathological evaluator process
// is terminated; no user regex executes on the UI thread.
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
        std::function<void(bool)>             onMultiline; // ^/$ match line boundaries
        std::function<void(bool)>             onWord;      // whole-word flag toggled
    };

    explicit RegexBuilderPopup(wxWindow *parent);
    ~RegexBuilderPopup() override { m_entrance.Stop(); }
    void Dismiss() override;
    void OnDismiss() override;

    // (Re)load state from the owning field. Call before Popup().
    void Configure(MD3::ColorScheme scheme, const wxString &pattern, bool regexOn,
                   bool caseOn, bool multilineOn, bool wordOn, Callbacks callbacks);

    // Field -> popover pattern sync while the popover is open (no echo back
    // through onPattern).
    void SyncPattern(const wxString &pattern);

    wxString GetPattern() const;

    // Show the transient popup and move keyboard focus into the raw pattern
    // editor as one operation. wxPopupTransientWindow must install its focus
    // tracking around the actual child control; calling Popup() and SetFocus()
    // separately makes the owner-deactivation path dismiss the popup again.
    void PopupAndFocusPattern();

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

    enum Tab : int { TabBuild = 0, TabExplain = 1, TabReference = 2 };

    void build();
    // Reference tab: per-token descriptions, engine mini-documentation,
    // flag and escaping tables, the capability matrix, worked examples, and
    // the OpenCode search helper. Built on first use.
    void buildReference();
    // Explain tab: live analysis of the current pattern. Built on first use.
    void buildExplain();
    // Re-analyse the pattern: refresh the Build tab summary and, while the
    // Explain tab is showing, every Explain view.
    void refreshAnalysis();
    // Run every capability probe through the worker and report the outcome.
    void checkCapabilities();
    // Load the current backtracking-risk example into the sample text.
    void useRiskExample();
    void switchTab(int tab);
    void openCodeHelp();
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

    // Escape one complete literal only when its encoded form fits the caller's
    // remaining protocol budget. This prevents the usual 2x metacharacter
    // expansion from allocating or inserting past the worker's hard cap.
    static wxString escapeLiteral(const wxString &raw, std::size_t maxOutput, bool *complete);

    wxScrolledWindow *m_scroll  = nullptr;
    wxScrolledWindow *m_ref_scroll = nullptr;
    wxScrolledWindow *m_explain_scroll = nullptr;
    Button           *m_tab_build  = nullptr;
    Button           *m_tab_explain = nullptr;
    Button           *m_tab_ref    = nullptr;
    Label            *m_ref_status = nullptr;
    // Build tab: one-line structure / risk summary under the validity line.
    Label            *m_summary = nullptr;
    // Explain tab views.
    Label            *m_engine_info  = nullptr;
    Label            *m_flags_info   = nullptr;
    Label            *m_mode_note    = nullptr;
    Label            *m_verdict      = nullptr;
    Label            *m_explain_summary = nullptr;
    wxTextCtrl       *m_tree    = nullptr;
    wxTextCtrl       *m_tokens  = nullptr;
    wxTextCtrl       *m_compat  = nullptr;
    wxTextCtrl       *m_risks   = nullptr;
    Button           *m_use_example = nullptr;
    wxString          m_risk_example;
    // Reference tab: capability check outcome.
    Label            *m_cap_status = nullptr;
    int               m_active_tab = 0;
    // Guided sections recorded while building the chips, reused verbatim by
    // the Reference tab so docs can never drift from the palette.
    std::vector<std::pair<wxString, std::vector<ChipDef>>> m_sections;
    wxTextCtrl       *m_pattern = nullptr;
    Button           *m_copy    = nullptr;
    Label            *m_status  = nullptr;
    wxTextCtrl       *m_literal = nullptr;
    CheckBox         *m_regex_cb = nullptr;
    CheckBox         *m_case_cb  = nullptr;
    CheckBox         *m_multiline_cb = nullptr;
    CheckBox         *m_word_cb  = nullptr;
    Button           *m_test_toggle = nullptr;
    wxPanel          *m_test_panel  = nullptr;
    wxTextCtrl       *m_sample  = nullptr;
    wxTextCtrl       *m_results = nullptr;

    MD3::ColorScheme m_scheme    = MD3::ColorScheme::Brand;
    bool             m_regex_on  = false;
    bool             m_case_on   = false;
    bool             m_multiline_on = false;
    bool             m_word_on   = false;
    bool             m_test_open = false;
    bool             m_syncing   = false; // guards field->popover sync against echo

    MD3TransientEntrance m_entrance;
    Callbacks m_cb;
};

#endif // !slic3r_GUI_RegexBuilderPopup_hpp_
