#include "RegexBuilderPopup.hpp"

#include "Button.hpp"
#include "BoundedRegex.hpp"
#include "CheckBox.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <algorithm>
#include <string>

#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/display.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/utils.h>

namespace {

// Logical (DIP) metrics + safety bounds. User patterns are evaluated by the
// bounded worker; these UI limits are the same values the worker revalidates.
constexpr int kContentW      = 344;  // inner content width
constexpr int kPad           = 12;   // card padding
constexpr int kGapY          = 8;    // vertical rhythm between rows
constexpr int kTargetH       = 44;   // minimum pointer/keyboard target
constexpr int kMaxPatternLen = static_cast<int>(Slic3r::GUI::BoundedRegex::kMaxPatternCodeUnits);
constexpr int kMaxSampleLen  = static_cast<int>(Slic3r::GUI::BoundedRegex::kMaxSubjectCodeUnits);
constexpr int kMaxMatches    = static_cast<int>(Slic3r::GUI::BoundedRegex::kMaxMatches);
constexpr int kMaxShownLen   = 60;   // clip match/group text in the results list

wxString friendlyRegexError(const Slic3r::GUI::BoundedRegex::Result &result)
{
    using Slic3r::GUI::BoundedRegex::ErrorDetail;
    using Slic3r::GUI::BoundedRegex::Status;
    if (result.status == Status::PatternTooLong)
        return wxString::Format(_L("Pattern too long (max %d characters)"), kMaxPatternLen);
    if (result.status == Status::SubjectTooLong)
        return wxString::Format(_L("Sample too long (max %d characters)"), kMaxSampleLen);
    if (result.status == Status::PatternTooComplex || result.status == Status::TimedOut)
        return _L("Pattern too complex to evaluate safely");
    if (result.status == Status::WorkerUnavailable || result.status == Status::ProtocolError)
        return _L("Regex evaluation is temporarily unavailable");
    switch (result.detail) {
    case ErrorDetail::Bracket: return _L("Unbalanced [ ] character set");
    case ErrorDetail::Parenthesis: return _L("Unbalanced ( ) group");
    case ErrorDetail::Brace: return _L("Unbalanced { } quantifier");
    case ErrorDetail::BadBrace: return _L("Invalid counts inside { }");
    case ErrorDetail::Range: return _L("Invalid character range");
    case ErrorDetail::Escape: return _L("Invalid escape sequence");
    case ErrorDetail::BackReference: return _L("Invalid backreference");
    case ErrorDetail::BadRepeat: return _L("Quantifier has nothing to repeat");
    case ErrorDetail::Collate:
    case ErrorDetail::CharacterClass: return _L("Unknown character class name");
    case ErrorDetail::Complexity:
    case ErrorDetail::Space:
    case ErrorDetail::Stack: return _L("Pattern too complex to evaluate safely");
    default: return _L("Invalid regular expression");
    }
}

// One-line preview of matched text for the results list: newlines flattened,
// clipped with an ellipsis.
wxString clipForList(const wxString &text)
{
    wxString out = text;
    out.Replace("\r", " ");
    out.Replace("\n", " ");
    if (out.length() > (size_t) kMaxShownLen)
        out = out.Left(kMaxShownLen) + wxString::FromUTF8("\xE2\x80\xA6");
    return out;
}

} // namespace

// --- ChipGroup ---------------------------------------------------------------
// A wrapped palette of real Button children. Each token is its own tab stop and
// exposes a push-button role/name/state to platform accessibility APIs.
class RegexBuilderPopup::ChipGroup : public wxPanel
{
public:
    ChipGroup(wxWindow *parent, const wxString &name, MD3::ColorScheme scheme,
              std::vector<ChipDef> defs, std::function<void(const ChipDef &)> onInsert)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                  wxTAB_TRAVERSAL | wxBORDER_NONE)
    {
        SetName(name);
        SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHigh));
        m_defs = std::move(defs);
        for (size_t i = 0; i < m_defs.size(); ++i) {
            const ChipDef &def = m_defs[i];
            auto *button = new Button(this, def.label);
            button->SetVariant(Button::Variant::Outlined);
            button->SetButtonSize(Button::Size::Large);
            button->SetColorScheme(scheme);
            button->SetName(def.label);
            button->SetToolTip(def.tip);
            button->Bind(wxEVT_BUTTON, [this, i, onInsert](wxCommandEvent &) {
                if (onInsert)
                    onInsert(m_defs[i]);
            });
            m_buttons.push_back(button);
        }
    }

    // Wrap the chips into rows for the given content width and freeze the
    // resulting min size for the hosting sizer.
    void Reflow(int width)
    {
        auto *outer = new wxBoxSizer(wxVERTICAL);
        auto *row   = new wxBoxSizer(wxHORIZONTAL);
        const int gap = FromDIP(6);
        int used = 0;
        for (Button *button : m_buttons) {
            const int button_w = button->GetBestSize().x;
            if (used > 0 && used + gap + button_w > width) {
                outer->Add(row, 0, wxBOTTOM, gap);
                row = new wxBoxSizer(wxHORIZONTAL);
                used = 0;
            }
            if (used > 0) {
                row->AddSpacer(gap);
                used += gap;
            }
            row->Add(button, 0);
            used += button_w;
        }
        outer->Add(row, 0);
        SetSizer(outer);
        SetMinSize(wxSize(width, outer->GetMinSize().y));
        Layout();
    }

private:
    std::vector<ChipDef> m_defs;
    std::vector<Button *> m_buttons;
};

// --- RegexBuilderPopup --------------------------------------------------------

RegexBuilderPopup::RegexBuilderPopup(wxWindow *parent)
    : PopupWindow(parent, wxBORDER_NONE | wxPU_CONTAINS_CONTROLS)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetName(_L("Regex builder"));
    Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        // Fill the full rect first so the rounded-corner triangles are clean
        // (same approach as DropDown::render), then the outlined card on top.
        dc.SetBackground(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHigh)));
        dc.Clear();
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHigh)));
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), std::max(1, FromDIP(1))));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, FromDIP(12));
    });
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE) {
            Dismiss();
            return;
        }
        e.Skip();
    });
    // wxPopupTransientWindow already handles WM_ACTIVATE for
    // wxPU_CONTAINS_CONTROLS. Binding the owner's deactivate event as well
    // dismisses us at the exact moment focus enters m_pattern.
    // build() is deferred to the first Configure() so every child control is
    // created with the owning field's accent scheme already installed.
}

void RegexBuilderPopup::build()
{
    const wxColour surface  = StateColor::semantic(MD3::Role::SurfaceContainerHigh);
    const wxColour field_bg = StateColor::semantic(MD3::Role::SurfaceContainerHighest);
    const wxColour on       = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var   = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    const int pad      = FromDIP(kPad);
    const int gap      = FromDIP(kGapY);
    const int contentW = FromDIP(kContentW);

    m_scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxTAB_TRAVERSAL | wxVSCROLL | wxBORDER_NONE);
    m_scroll->SetBackgroundColour(surface);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    // Title + engine identification (the popover always names the real engine
    // so the preview can never silently diverge from the search's dialect).
    auto *title = new Label(m_scroll, Label::Head_14, _L("Regex builder"));
    title->SetBackgroundColour(surface);
    title->SetForegroundColour(on);
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto *engine = new Label(m_scroll, Label::Body_11,
                              _L("Engine: Boost.Regex 1.84 wide-character ECMAScript in an isolated 50 ms worker. Case-insensitive matching uses boost::regex_constants::icase. Escape metacharacters with a backslash."));
    engine->SetBackgroundColour(surface);
    engine->SetForegroundColour(on_var);
    engine->Wrap(contentW);
    sizer->Add(engine, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto sectionLabel = [&](const wxString &text) {
        auto *lbl = new Label(m_scroll, Label::Head_12, text);
        lbl->SetBackgroundColour(surface);
        lbl->SetForegroundColour(on_var);
        sizer->Add(lbl, 0, wxLEFT | wxRIGHT | wxTOP, pad);
        return lbl;
    };

    // --- Raw pattern editor + copy ------------------------------------------
    sectionLabel(_L("Pattern"));
    wxBoxSizer *pat_row = new wxBoxSizer(wxHORIZONTAL);
    m_pattern = new wxTextCtrl(m_scroll, wxID_ANY, wxEmptyString, wxDefaultPosition,
                               wxSize(contentW - FromDIP(50), FromDIP(kTargetH)),
                               wxBORDER_NONE | wxTE_PROCESS_ENTER);
    m_pattern->SetFont(Label::Mono_13);
    m_pattern->SetBackgroundColour(field_bg);
    m_pattern->SetForegroundColour(on);
    m_pattern->SetMaxLength(kMaxPatternLen);
    m_pattern->SetName(_L("Regex pattern"));
    m_pattern->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        onPatternEdited();
        e.Skip();
    });
    pat_row->Add(m_pattern, 1, wxALIGN_CENTER_VERTICAL);

    m_copy = new Button(m_scroll, wxEmptyString);
    m_copy->SetIconButton(Button::IconShape::Circle, kTargetH);
    m_copy->SetGlyph(MaterialIcon::ContentCopy, 18);
    m_copy->SetToolTip(_L("Copy pattern"));
    m_copy->SetName(_L("Copy pattern"));
    m_copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copyPattern(); });
    pat_row->Add(m_copy, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    sizer->Add(pat_row, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    // Live validity line. Single-line (short friendly messages); the full text
    // doubles as its own tooltip in case of ellipsization at narrow scale.
    m_status = new Label(m_scroll, Label::Body_12, _L("Empty pattern matches everything"));
    m_status->SetBackgroundColour(surface);
    m_status->SetForegroundColour(on_var);
    // Full content width up front so longer validity messages never clip.
    m_status->SetMinSize(wxSize(contentW, -1));
    sizer->Add(m_status, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    // --- Flags ---------------------------------------------------------------
    sectionLabel(_L("Flags"));
    auto addFlag = [&](const wxString &text, bool value, std::function<void(bool)> onToggle) {
        wxBoxSizer *row = new wxBoxSizer(wxHORIZONTAL);
        auto *box = new CheckBox(m_scroll);
        box->SetColorScheme(m_scheme);
        box->SetValue(value);
        box->SetName(text);
        box->SetLabel(text);
        box->SetMinSize(wxSize(FromDIP(kTargetH), FromDIP(kTargetH)));
        auto *lbl = new Label(m_scroll, Label::Body_13, text);
        lbl->SetBackgroundColour(surface);
        lbl->SetForegroundColour(on);
        auto fire = [onToggle](bool v) {
            if (onToggle)
                onToggle(v);
        };
        box->Bind(wxEVT_TOGGLEBUTTON, [box, fire](wxCommandEvent &e) {
            fire(box->GetValue());
            e.Skip();
        });
        // The label is a click target too (standard checkbox affordance).
        lbl->Bind(wxEVT_LEFT_DOWN, [box, fire](wxMouseEvent &) {
            box->SetValue(!box->GetValue());
            fire(box->GetValue());
        });
        row->Add(box, 0, wxALIGN_CENTER_VERTICAL);
        lbl->Wrap(contentW - FromDIP(kTargetH + 8));
        row->Add(lbl, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        sizer->Add(row, 0, wxLEFT | wxRIGHT | wxTOP, pad);
        return box;
    };
    m_regex_cb = addFlag(_L("Regex mode"), m_regex_on, [this](bool on) {
        m_regex_on = on;
        if (m_cb.onRegexMode)
            m_cb.onRegexMode(on);
    });
    m_case_cb = addFlag(_L("Case sensitive"), m_case_on, [this](bool on) {
        m_case_on = on;
        if (m_cb.onCase)
            m_cb.onCase(on);
        evaluate(); // icase flag changes the preview matches
    });
    m_multiline_cb = addFlag(_L("Multiline anchors (^ and $ match line boundaries)"),
                             m_multiline_on, [this](bool on) {
        m_multiline_on = on;
        if (m_cb.onMultiline)
            m_cb.onMultiline(on);
        evaluate();
    });
    m_word_cb = addFlag(_L("Whole word"), m_word_on, [this](bool on) {
        m_word_on = on;
        if (m_cb.onWord)
            m_cb.onWord(on);
    });
    auto *word_note = new Label(m_scroll, Label::Body_11,
                                _L("Whole word applies to plain-text search only; in regex mode use \\b"));
    word_note->SetBackgroundColour(surface);
    word_note->SetForegroundColour(on_var);
    word_note->Wrap(contentW);
    sizer->Add(word_note, 0, wxLEFT | wxRIGHT | wxTOP, pad - gap / 2);

    // --- Literals (auto-escaped input) ----------------------------------------
    sectionLabel(_L("Literals"));
    wxBoxSizer *lit_row = new wxBoxSizer(wxHORIZONTAL);
    m_literal = new wxTextCtrl(m_scroll, wxID_ANY, wxEmptyString, wxDefaultPosition,
                               wxSize(contentW - FromDIP(92), FromDIP(kTargetH)),
                               wxBORDER_NONE | wxTE_PROCESS_ENTER);
    m_literal->SetFont(Label::Body_13);
    m_literal->SetBackgroundColour(field_bg);
    m_literal->SetForegroundColour(on);
    m_literal->SetHint(_L("Text to match literally"));
    m_literal->SetName(_L("Text to match literally"));
    m_literal->SetMaxLength(kMaxPatternLen);
    m_literal->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { addLiteral(); });
    lit_row->Add(m_literal, 1, wxALIGN_CENTER_VERTICAL);

    auto *add_btn = new Button(m_scroll, _L("Add"));
    add_btn->SetVariant(Button::Variant::Tonal);
    add_btn->SetButtonSize(Button::Size::Large);
    add_btn->SetColorScheme(m_scheme);
    add_btn->SetToolTip(_L("Insert the text with regex metacharacters escaped"));
    add_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { addLiteral(); });
    lit_row->Add(add_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    sizer->Add(lit_row, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    // --- Guided token sections -------------------------------------------------
    addSection(sizer, _L("Character classes"),
               {{".", ".", 0, _L("Any single character except newline")},
                {"[ ]", "[]", 1, _L("Character set — matches any one character listed inside")},
                {"[^ ]", "[^]", 1, _L("Negated set — matches any character not listed inside")},
                {"a-z", "a-z", 0, _L("Character range — use inside [ ], e.g. [a-z0-9]")},
                {"\\d", "\\d", 0, _L("Digit 0-9")},
                {"\\D", "\\D", 0, _L("Any character that is not a digit")},
                {"\\w", "\\w", 0, _L("Word character: letter, digit, or underscore")},
                {"\\W", "\\W", 0, _L("Any character that is not a word character")},
                {"\\s", "\\s", 0, _L("Whitespace character")},
                {"\\S", "\\S", 0, _L("Any character that is not whitespace")}});

    addSection(sizer, _L("Anchors"),
               {{"^", "^", 0, _L("Start of the text")},
                {"$", "$", 0, _L("End of the text")},
                {"\\b", "\\b", 0, _L("Word boundary")},
                {"\\B", "\\B", 0, _L("Not a word boundary")}});

    addSection(sizer, _L("Groups & alternation"),
               {{"( )", "()", 1, _L("Capturing group — remembers the matched text")},
                {"(?: )", "(?:)", 1, _L("Non-capturing group — groups without remembering")},
                {"|", "|", 0, _L("Alternation — matches either side")},
                {"\\1", "\\1", 0, _L("Backreference — matches what group 1 captured")}});

    addSection(sizer, _L("Quantifiers"),
               {{"*", "*", 0, _L("Zero or more of the previous item (greedy)")},
                {"+", "+", 0, _L("One or more of the previous item (greedy)")},
                {"?", "?", 0, _L("Zero or one of the previous item (greedy)")},
                {"{n}", "{n}", 0, _L("Exactly n repetitions — replace n with a number")},
                {"{n,}", "{n,}", 0, _L("At least n repetitions — replace n with a number")},
                {"{n,m}", "{n,m}", 0, _L("Between n and m repetitions — replace n and m with numbers")},
                {"*?", "*?", 0, _L("Zero or more (lazy — matches as little as possible)")},
                {"+?", "+?", 0, _L("One or more (lazy — matches as little as possible)")},
                {"??", "??", 0, _L("Zero or one (lazy — matches as little as possible)")}});

    // --- Collapsible test section (progressive disclosure) --------------------
    m_test_toggle = new Button(m_scroll, _L("Test pattern"));
    m_test_toggle->SetVariant(Button::Variant::Text);
    m_test_toggle->SetButtonSize(Button::Size::Large);
    m_test_toggle->SetColorScheme(m_scheme);
    m_test_toggle->SetGlyph(MaterialIcon::ExpandMore, 18);
    m_test_toggle->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { toggleTest(); });
    sizer->Add(m_test_toggle, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(4));

    m_test_panel = new wxPanel(m_scroll, wxID_ANY);
    m_test_panel->SetBackgroundColour(surface);
    wxBoxSizer *test_sizer = new wxBoxSizer(wxVERTICAL);

    auto *sample_lbl = new Label(m_test_panel, Label::Head_12, _L("Sample text"));
    sample_lbl->SetBackgroundColour(surface);
    sample_lbl->SetForegroundColour(on_var);
    test_sizer->Add(sample_lbl, 0, wxTOP, gap / 2);

    // wxTE_RICH2 for SetStyle() match highlighting (repo precedent:
    // UnsavedChangesDialog). Multiline hints are unsupported on MSW, hence the
    // label above instead of a hint.
    m_sample = new wxTextCtrl(m_test_panel, wxID_ANY, wxEmptyString, wxDefaultPosition,
                              wxSize(contentW, FromDIP(84)),
                              wxTE_MULTILINE | wxTE_RICH2 | wxBORDER_NONE);
    m_sample->SetFont(Label::Body_13);
    m_sample->SetBackgroundColour(field_bg);
    m_sample->SetForegroundColour(on);
    m_sample->SetMaxLength(kMaxSampleLen);
    m_sample->SetName(_L("Sample text"));
    m_sample->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        evaluate();
        e.Skip();
    });
    test_sizer->Add(m_sample, 0, wxTOP, gap / 2);

    auto *matches_lbl = new Label(m_test_panel, Label::Head_12, _L("Matches"));
    matches_lbl->SetBackgroundColour(surface);
    matches_lbl->SetForegroundColour(on_var);
    test_sizer->Add(matches_lbl, 0, wxTOP, gap);

    m_results = new wxTextCtrl(m_test_panel, wxID_ANY, wxEmptyString, wxDefaultPosition,
                               wxSize(contentW, FromDIP(110)),
                               wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    m_results->SetFont(Label::Mono_11);
    m_results->SetBackgroundColour(field_bg);
    m_results->SetForegroundColour(on);
    m_results->SetName(_L("Match results"));
    test_sizer->Add(m_results, 0, wxTOP, gap / 2);

    m_test_panel->SetSizer(test_sizer);
    sizer->Add(m_test_panel, 0, wxLEFT | wxRIGHT, pad);
    sizer->Show(m_test_panel, false, true);

    sizer->AddSpacer(pad);
    m_scroll->SetSizer(sizer);

    // --- Build | Reference tab header (popup children, laid out in fitPopup)
    m_tab_build = new Button(this, _L("Build"));
    m_tab_ref   = new Button(this, _L("Reference"));
    for (Button *b : {m_tab_build, m_tab_ref}) {
        b->SetButtonSize(Button::Size::Large);
        b->SetColorScheme(m_scheme);
    }
    m_tab_build->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { switchTab(0); });
    m_tab_ref->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { switchTab(1); });
    buildReference();
    switchTab(0);
}

void RegexBuilderPopup::buildReference()
{
    const wxColour surface  = StateColor::semantic(MD3::Role::SurfaceContainerHigh);
    const wxColour on       = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var   = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const int pad      = FromDIP(kPad);
    const int contentW = FromDIP(kContentW);

    m_ref_scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxTAB_TRAVERSAL | wxVSCROLL | wxBORDER_NONE);
    m_ref_scroll->SetBackgroundColour(surface);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    auto heading = [&](const wxString &text) {
        auto *lbl = new Label(m_ref_scroll, Label::Head_12, text);
        lbl->SetBackgroundColour(surface);
        lbl->SetForegroundColour(on_var);
        sizer->Add(lbl, 0, wxLEFT | wxRIGHT | wxTOP, pad);
    };
    auto paragraph = [&](const wxString &text) {
        auto *lbl = new Label(m_ref_scroll, Label::Body_12, text);
        lbl->SetBackgroundColour(surface);
        lbl->SetForegroundColour(on);
        lbl->Wrap(contentW);
        sizer->Add(lbl, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(6));
    };
    auto term_row = [&](const wxString &term, const wxString &meaning) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *t = new Label(m_ref_scroll, Label::Mono_11, term);
        t->SetBackgroundColour(surface);
        t->SetForegroundColour(on);
        // Wide enough for the longest documented term ("(red|blue)\b") so the
        // mono column never clips.
        t->SetMinSize(wxSize(FromDIP(92), -1));
        row->Add(t, 0, wxALIGN_TOP);
        auto *m = new Label(m_ref_scroll, Label::Body_12, meaning);
        m->SetBackgroundColour(surface);
        m->SetForegroundColour(on_var);
        m->Wrap(contentW - FromDIP(104));
        row->Add(m, 1, wxLEFT, FromDIP(8));
        sizer->Add(row, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(8));
    };

    auto *title = new Label(m_ref_scroll, Label::Head_14, wxEmptyString);
    // SetLabelText: the '&' must render literally, not become a mnemonic.
    title->SetLabelText(_L("Reference & help"));
    title->SetBackgroundColour(surface);
    title->SetForegroundColour(on);
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    heading(_L("How search works"));
    paragraph(_L("Plain text is the default in every search bar; regex mode is a deliberate opt-in via the .* toggle."));
    paragraph(_L("Engine: Boost.Regex 1.84 wide-character ECMAScript in an isolated 50 ms worker. Case-insensitive matching uses boost::regex_constants::icase. Escape metacharacters with a backslash."));
    paragraph(_L("Case sensitive refines plain-text and regex search. Multiline makes ^ and $ match line boundaries; Whole word is plain-text only, so use \\b in regex mode."));
    paragraph(_L("An invalid or half-typed pattern never hides rows: it matches everything until it compiles."));
    paragraph(_L("Evaluation is local and bounded - long patterns and samples are truncated and runaway matching stops safely."));

    // Full per-token documentation, straight from the Build tab's tables.
    for (const auto &[section_title, defs] : m_sections) {
        heading(section_title);
        for (const ChipDef &def : defs)
            term_row(def.label, def.tip);
    }

    heading(_L("Examples"));
    term_row("^PLA",          _L("Rows that start with PLA"));
    term_row("\\d+ ?mm",      _L("A number followed by mm, with an optional space"));
    term_row("(red|blue)\\b", _L("Rows containing the whole word red or blue"));

    heading(_L("OpenCode helper"));
    paragraph(_L("Let the OpenCode assistant draft the pattern: the button copies a prompt describing this engine, your current pattern and sample text to the clipboard, then opens OpenCode if it is installed."));
    auto *oc_btn = new Button(m_ref_scroll, _L("Copy prompt & open OpenCode"));
    oc_btn->SetButtonSize(Button::Size::Large);
    oc_btn->SetColorScheme(m_scheme);
    oc_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { openCodeHelp(); });
    sizer->Add(oc_btn, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(4));
    m_ref_status = new Label(m_ref_scroll, Label::Body_11, wxEmptyString);
    m_ref_status->SetBackgroundColour(surface);
    m_ref_status->SetForegroundColour(on_var);
    sizer->Add(m_ref_status, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(8));

    sizer->AddSpacer(pad);
    m_ref_scroll->SetSizer(sizer);
    m_ref_scroll->Hide();
}

void RegexBuilderPopup::switchTab(int tab)
{
    m_active_tab = tab;
    if (m_scroll)     m_scroll->Show(tab == 0);
    if (m_ref_scroll) {
        m_ref_scroll->Show(tab == 1);
        if (tab == 1)
            m_ref_scroll->Scroll(0, 0); // always open the docs at the top
    }
    if (m_tab_build)  m_tab_build->SetVariant(tab == 0 ? Button::Variant::Tonal : Button::Variant::Text);
    if (m_tab_ref)    m_tab_ref->SetVariant(tab == 1 ? Button::Variant::Tonal : Button::Variant::Text);
    fitPopup();
    Refresh();
}

void RegexBuilderPopup::openCodeHelp()
{
    // Everything stays local: the prompt goes to the clipboard (never onto a
    // command line, which other processes could read) and OpenCode is only
    // launched, not fed data.
    wxString sample = m_sample ? m_sample->GetValue().Left(400) : wxString{};
    wxString prompt = "Help me build a regular expression. Engine: Boost.Regex 1.84 wide-character ECMAScript "
                       "in an isolated bounded worker (512-code-unit pattern, 8192-code-unit "
                       "sample, 50 ms deadline), icase when case-insensitive, explicit multiline anchors; invalid patterns must fail safe. ";
    prompt += "Current pattern: \"" + GetPattern() + "\". ";
    if (!sample.IsEmpty())
        prompt += "It should be tested against this sample text: \"" + sample + "\". ";
    prompt += "Explain the final pattern token by token.";
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(prompt));
        wxTheClipboard->Close();
    }
    wxArrayString out, err;
    const bool found = wxExecute("where opencode", out, err, wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE) == 0 && !out.IsEmpty();
    if (found) {
        wxExecute("cmd /c start \"\" opencode", wxEXEC_ASYNC);
        m_ref_status->SetLabel(_L("Prompt copied. OpenCode is starting - paste the prompt there."));
    } else {
        m_ref_status->SetLabel(_L("Prompt copied. OpenCode was not found on PATH - paste the prompt into your assistant."));
    }
    m_ref_scroll->Layout();
}

void RegexBuilderPopup::addSection(wxSizer *sizer, const wxString &title,
                                   const std::vector<ChipDef> &defs)
{
    m_sections.emplace_back(title, defs); // the Reference tab reuses these
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainerHigh);
    const int      pad     = FromDIP(kPad);

    auto *lbl = new Label(m_scroll, Label::Head_12, title);
    lbl->SetBackgroundColour(surface);
    lbl->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    sizer->Add(lbl, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto *group = new ChipGroup(m_scroll, title, m_scheme, defs,
                                [this](const ChipDef &d) { insertChip(d); });
    group->SetBackgroundColour(surface);
    group->Reflow(FromDIP(kContentW));
    sizer->Add(group, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(4));
}

void RegexBuilderPopup::Configure(MD3::ColorScheme scheme, const wxString &pattern,
                                  bool regexOn, bool caseOn, bool multilineOn,
                                  bool wordOn, Callbacks callbacks)
{
    m_scheme   = scheme;
    m_regex_on = regexOn;
    m_case_on  = caseOn;
    m_multiline_on = multilineOn;
    m_word_on  = wordOn;
    m_cb       = std::move(callbacks);

    if (!m_scroll)
        build();

    const wxString bounded_pattern = pattern.Left(kMaxPatternLen);
    m_syncing = true;
    m_pattern->ChangeValue(bounded_pattern);
    m_syncing = false;
    if (bounded_pattern != pattern && m_cb.onPattern)
        m_cb.onPattern(bounded_pattern);
    m_regex_cb->SetValue(regexOn);
    m_case_cb->SetValue(caseOn);
    m_multiline_cb->SetValue(multilineOn);
    m_word_cb->SetValue(wordOn);
    m_regex_cb->SetColorScheme(scheme);
    m_case_cb->SetColorScheme(scheme);
    m_multiline_cb->SetColorScheme(scheme);
    m_word_cb->SetColorScheme(scheme);

    evaluate();
    fitPopup();
}

void RegexBuilderPopup::SyncPattern(const wxString &pattern)
{
    if (!m_pattern || m_pattern->GetValue() == pattern)
        return;
    m_syncing = true;
    m_pattern->ChangeValue(pattern.Left(kMaxPatternLen)); // no wxEVT_TEXT -> no echo through onPattern
    m_syncing = false;
    evaluate();
}

wxString RegexBuilderPopup::GetPattern() const { return m_pattern ? m_pattern->GetValue() : wxString(); }

void RegexBuilderPopup::PopupAndFocusPattern()
{
    if (!m_pattern) {
        Popup();
        return;
    }
    m_pattern->SetInsertionPointEnd();
    Popup(m_pattern);
}

void RegexBuilderPopup::insertChip(const ChipDef &def)
{
    if (!m_pattern)
        return;
    long selection_start = 0;
    long selection_end   = 0;
    m_pattern->GetSelection(&selection_start, &selection_end);
    const size_t selected = selection_end > selection_start
                                ? static_cast<size_t>(selection_end - selection_start)
                                : 0;
    if (m_pattern->GetValue().length() - selected + def.insert.length() >
        static_cast<size_t>(kMaxPatternLen)) {
        wxBell();
        return;
    }
    // Writes at the pattern editor's stored insertion point (works without
    // focus), fires wxEVT_TEXT -> onPatternEdited -> field sync + evaluate.
    m_pattern->WriteText(def.insert);
    if (def.caret_back > 0)
        m_pattern->SetInsertionPoint(std::max(0L, m_pattern->GetInsertionPoint() - def.caret_back));
}

void RegexBuilderPopup::addLiteral()
{
    if (!m_literal || !m_pattern)
        return;
    const wxString raw = m_literal->GetValue();
    if (raw.IsEmpty())
        return;

    long selection_start = 0;
    long selection_end   = 0;
    m_pattern->GetSelection(&selection_start, &selection_end);
    const size_t selected = selection_end > selection_start
                                ? static_cast<size_t>(selection_end - selection_start)
                                : 0;
    const size_t base_len = m_pattern->GetValue().length() - selected;
    const size_t available = base_len < static_cast<size_t>(kMaxPatternLen)
                                 ? static_cast<size_t>(kMaxPatternLen) - base_len
                                 : 0;
    bool complete = false;
    const wxString escaped = escapeLiteral(raw, available, &complete);
    if (!complete) {
        wxBell();
        return;
    }
    m_pattern->WriteText(escaped);
    m_literal->Clear();
}

wxString RegexBuilderPopup::escapeLiteral(const wxString &raw, std::size_t maxOutput, bool *complete)
{
    // ECMAScript metacharacters that must be escaped to match literally.
    const wxString meta = "\\^$.|?*+()[]{}";
    wxString       out;
    out.reserve(std::min(raw.length(), maxOutput));
    if (complete)
        *complete = false;
    for (size_t i = 0; i < raw.length(); ++i) {
        const wxUniChar c = raw[i];
        const size_t needed = meta.Find(c) != wxNOT_FOUND ? 2 : 1;
        if (out.length() + needed > maxOutput)
            return out;
        if (needed == 2)
            out << '\\';
        out << c;
    }
    if (complete)
        *complete = true;
    return out;
}

void RegexBuilderPopup::copyPattern()
{
    if (!m_pattern || !wxTheClipboard->Open())
        return;
    wxTheClipboard->SetData(new wxTextDataObject(m_pattern->GetValue()));
    wxTheClipboard->Flush();
    wxTheClipboard->Close();
}

void RegexBuilderPopup::toggleTest()
{
    m_test_open = !m_test_open;
    m_test_toggle->SetGlyph(m_test_open ? MaterialIcon::ExpandLess : MaterialIcon::ExpandMore, 18);
    m_scroll->GetSizer()->Show(m_test_panel, m_test_open, true);
    if (m_test_open)
        evaluate();
    fitPopup();
}

void RegexBuilderPopup::onPatternEdited()
{
    if (!m_syncing && m_cb.onPattern)
        m_cb.onPattern(m_pattern->GetValue());
    evaluate();
}

void RegexBuilderPopup::evaluate()
{
    if (!m_pattern || !m_status)
        return;

    const wxColour ok_colour   = StateColor::semantic(MD3::Role::Primary, m_scheme);
    const wxColour err_colour  = StateColor::semantic(MD3::Role::Error);
    const wxColour info_colour = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    auto setStatus = [this](const wxString &text, const wxColour &colour) {
        m_status->SetForegroundColour(colour);
        m_status->SetLabel(text);
        m_status->SetToolTip(text);
        m_status->Refresh();
    };

    // Reset any previous match highlighting to the field's base style.
    auto resetHighlights = [this]() {
        if (!m_sample || m_sample->GetLastPosition() <= 0)
            return;
        wxTextAttr base;
        base.SetTextColour(StateColor::semantic(MD3::Role::OnSurface));
        base.SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHighest));
        m_sample->SetStyle(0, m_sample->GetLastPosition(), base);
    };

    const wxString pattern = m_pattern->GetValue();

    if (pattern.IsEmpty()) {
        setStatus(_L("Empty pattern matches everything"), info_colour);
        resetHighlights();
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    }
    if (pattern.length() > (size_t) kMaxPatternLen) {
        // ChangeValue()/SyncPattern can exceed the typed-input cap; re-check.
        setStatus(wxString::Format(_L("Pattern too long (max %d characters)"), kMaxPatternLen), err_colour);
        resetHighlights();
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    }

    // --- Bounded sample evaluation --------------------------------------------
    const wxString sample_full = m_sample ? m_sample->GetValue() : wxString();
    Slic3r::GUI::BoundedRegex::Options options;
    options.case_sensitive = m_case_on;
    options.multiline = m_multiline_on;
    if (sample_full.IsEmpty()) {
        const auto result = Slic3r::GUI::BoundedRegex::validate(pattern.ToStdWstring(), options);
        const bool valid = result.status == Slic3r::GUI::BoundedRegex::Status::Valid;
        setStatus(valid ? _L("Valid pattern") : friendlyRegexError(result),
                  valid ? ok_colour : err_colour);
        resetHighlights();
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    }

    const bool     truncated = sample_full.length() > (size_t) kMaxSampleLen;
    const wxString sample    = truncated ? sample_full.Left(kMaxSampleLen) : sample_full;
    const auto result = Slic3r::GUI::BoundedRegex::find_all(
        pattern.ToStdWstring(), sample.ToStdWstring(), kMaxMatches, options);
    if (result.status != Slic3r::GUI::BoundedRegex::Status::Match &&
        result.status != Slic3r::GUI::BoundedRegex::Status::NoMatch) {
        setStatus(friendlyRegexError(result), err_colour);
        resetHighlights();
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    }

    wxString out;
    const int count = static_cast<int>(result.matches.size());
    std::vector<std::pair<long, long>> spans;
    for (std::size_t match_index = 0; match_index < result.matches.size(); ++match_index) {
        const auto &match = result.matches[match_index];
        if (match.groups.empty())
            continue;
        const auto &whole = match.groups.front();
        const long s = static_cast<long>(whole.begin);
        const long e = s + static_cast<long>(whole.length);
        spans.emplace_back(s, e);
        out << wxString::Format(_L("Match %d at %d-%d: %s"), static_cast<int>(match_index + 1),
                                static_cast<int>(s), static_cast<int>(e),
                                clipForList(sample.Mid(whole.begin, whole.length)))
            << "\n";
        for (std::size_t group_index = 1; group_index < match.groups.size(); ++group_index) {
            const auto &group = match.groups[group_index];
            out << "    ";
            if (group.matched)
                out << wxString::Format(_L("group %d: %s"), static_cast<int>(group_index),
                                        clipForList(sample.Mid(group.begin, group.length)));
            else
                out << wxString::Format(_L("group %d: (no match)"), static_cast<int>(group_index));
            out << "\n";
        }
    }

    if (count == 0)
        out = _L("No matches.");
    else if (result.match_limit_reached)
        out << wxString::Format(_L("Showing first %d matches only."), kMaxMatches) << "\n";
    if (truncated)
        out << wxString::Format(_L("Sample truncated to %d characters."), kMaxSampleLen) << "\n";
    if (m_results)
        m_results->ChangeValue(out);

    setStatus(count == 1 ? _L("Valid pattern — 1 match")
                         : wxString::Format(_L("Valid pattern — %d matches"), count),
              ok_colour);

    // Highlight the match spans in the sample (SecondaryContainer tonal pane,
    // readable in light and dark). Zero-width spans have nothing to paint.
    resetHighlights();
    if (m_sample && !spans.empty()) {
        wxTextAttr hl;
        hl.SetBackgroundColour(StateColor::semantic(MD3::Role::SecondaryContainer, m_scheme));
        hl.SetTextColour(StateColor::semantic(MD3::Role::OnSecondaryContainer, m_scheme));
        for (const auto &span : spans)
            if (span.second > span.first)
                m_sample->SetStyle(span.first, span.second, hl);
    }
}

void RegexBuilderPopup::fitPopup()
{
    wxScrolledWindow *active = (m_active_tab == 1 && m_ref_scroll) ? m_ref_scroll : m_scroll;
    wxSizer *sizer = active ? active->GetSizer() : nullptr;
    if (!sizer)
        return;

    sizer->Layout();
    const wxSize content = sizer->GetMinSize();

    // Cap to the display so the popover always fits an 800px-tall screen at
    // any supported scale; overflow scrolls inside m_scroll.
    int disp_idx = wxDisplay::GetFromWindow(GetParent() ? GetParent() : this);
    if (disp_idx == wxNOT_FOUND)
        disp_idx = 0;
    const wxRect area  = wxDisplay((unsigned) disp_idx).GetClientArea();
    const int    max_h = std::min(FromDIP(600), area.height - FromDIP(96));

    const int inset  = FromDIP(4); // keeps square children inside the r12 border arc
    const int tab_h  = FromDIP(52); // 44-DIP Build | Reference targets + insets
    const int view_h = std::min(content.y, max_h - tab_h);
    const int sb_w   = content.y > view_h ? wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, active) : 0;

    SetClientSize(content.x + sb_w + 2 * inset, tab_h + view_h + 2 * inset);
    if (m_tab_build && m_tab_ref) {
        const int tab_w = FromDIP(96);
        m_tab_build->SetSize(inset + FromDIP(8), inset + FromDIP(4), tab_w, FromDIP(kTargetH));
        m_tab_ref->SetSize(inset + FromDIP(12) + tab_w, inset + FromDIP(4), tab_w, FromDIP(kTargetH));
    }
    for (wxScrolledWindow *scroll : {m_scroll, m_ref_scroll}) {
        if (!scroll)
            continue;
        scroll->SetSize(inset, inset + tab_h, content.x + sb_w, view_h);
        scroll->SetScrollRate(0, FromDIP(16));
    }
    active->FitInside();
    active->Layout();
}
