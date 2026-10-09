#include "RegexBuilderPopup.hpp"
#include "MD3ScrolledWindow.hpp"

#include "Button.hpp"
#include "BoundedRegex.hpp"
#include "CheckBox.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "RegexAnalysis.hpp"
#include "StateColor.hpp"
#include "TextArea.hpp"
#include "TextInput.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <boost/version.hpp>

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

// Content updates can change wrapped diagnostic height without a popup resize.
void refreshDiagnosticLayout(wxScrolledWindow *scroll)
{
    if (!scroll || !scroll->GetSizer())
        return;
    const wxPoint view = scroll->GetViewStart();
    for (int pass = 0; pass < 2; ++pass) {
        scroll->Layout();
        scroll->FitInside();
    }
    scroll->Scroll(view.x, view.y);
}

namespace RA = Slic3r::GUI::RegexAnalysis;

// Translates an analysis message: the English source text is looked up in
// the catalogue, then its %s placeholders take the message arguments.
wxString tr(const RA::Text &message)
{
    const wxString translated = _L(message.msgid);
    return wxString(RA::substitute(translated.ToStdWstring(), message.args));
}

wxString tr_utf8(const char *msgid) { return _L(msgid); }

// The engine identity reported by the worker. It cannot change while the
// application runs, so one successful report is kept for the process.
const RA::EngineInfo &engineInfo(bool ask_worker)
{
    static RA::EngineInfo info;
    if (!info.reported && ask_worker) {
        const auto described = Slic3r::GUI::BoundedRegex::describe_engine();
        if (described.status == Slic3r::GUI::BoundedRegex::Status::Valid)
            info = RA::parse_engine_descriptor(described.diagnostic);
    }
    return info;
}

wxString engineVersionText() { return wxString(RA::version_text(BOOST_VERSION)); }

// Runs one capability probe through the worker with default flags.
RA::ProbeObservation observeProbe(const RA::Capability &row)
{
    RA::ProbeObservation seen;
    const std::wstring subject(row.subject);
    const auto result = Slic3r::GUI::BoundedRegex::search(row.probe, subject);
    using Slic3r::GUI::BoundedRegex::Status;
    seen.answered = result.definitive() || result.status == Status::InvalidPattern;
    seen.rejected = result.status == Status::InvalidPattern;
    seen.matched  = result.status == Status::Match;
    if (seen.matched && !result.matches.empty() && !result.matches.front().groups.empty()) {
        const auto &whole = result.matches.front().groups.front();
        seen.whole = subject.substr(whole.begin, whole.length);
    }
    return seen;
}

wxString offsetText(std::size_t begin, std::size_t end)
{
    return end > begin + 1 ? wxString::Format("%d-%d", static_cast<int>(begin), static_cast<int>(end))
                           : wxString::Format("%d", static_cast<int>(begin));
}

wxString compatibilitySeverity(RA::Severity severity)
{
    return severity == RA::Severity::Info ? _L("Note") : _L("Warning");
}

wxString riskSeverity(RA::Severity severity)
{
    switch (severity) {
    case RA::Severity::Info: return _L("Note");
    case RA::Severity::Warning: return tr_utf8(RA::risk_label(RA::RiskLevel::Moderate));
    case RA::Severity::Danger: return tr_utf8(RA::risk_label(RA::RiskLevel::High));
    }
    return _L("Note");
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
        SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
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
    Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
        if (event.GetEventObject() == this) {
            if (event.IsShown()) m_entrance.Show(this, MD3::Motion::short2);
            else m_entrance.Stop();
        }
        event.Skip();
    });
    Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        // Fill the full rect first so the rounded-corner triangles are clean
        // (same approach as DropDown::render), then the outlined card on top.
        dc.SetBackground(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerLow)));
        dc.Clear();
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerLow)));
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), std::max(1, FromDIP(1))));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, FromDIP(MD3::Metrics::active().radius));
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

void RegexBuilderPopup::Dismiss()
{
    m_entrance.Stop();
    PopupWindow::Dismiss();
}

void RegexBuilderPopup::OnDismiss()
{
    m_entrance.Stop();
    PopupWindow::OnDismiss();
}

void RegexBuilderPopup::build()
{
    const wxColour surface  = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour field_bg = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
    const wxColour on       = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var   = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    const int pad      = FromDIP(MD3::Metrics::active().padding);
    const int gap      = FromDIP(MD3::Metrics::active().gap);
    const int contentW = FromDIP(kContentW);

    m_scroll = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxTAB_TRAVERSAL | wxVSCROLL | wxBORDER_NONE);
    m_scroll->SetBackgroundColour(surface);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    // Title + engine identification (the popover always names the real engine
    // so the preview can never silently diverge from the search's dialect).
    auto *title = new Label(m_scroll, Label::Head_20, _L("Regex builder"));
    title->SetBackgroundColour(surface);
    title->SetForegroundColour(on);
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto *engine = new Label(m_scroll, Label::Body_13,
                              wxString::Format(_L("Engine: Boost.Regex %s wide-character ECMAScript in an isolated 50 ms worker. Case-insensitive matching uses boost::regex_constants::icase. Escape metacharacters with a backslash."),
                                               engineVersionText()));
    engine->SetBackgroundColour(surface);
    engine->SetForegroundColour(on_var);
    engine->Wrap(contentW);
    sizer->Add(engine, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto sectionLabel = [&](const wxString &text) {
        auto *lbl = new Label(m_scroll, Label::Head_14, text);
        lbl->SetBackgroundColour(surface);
        lbl->SetForegroundColour(on_var);
        sizer->Add(lbl, 0, wxLEFT | wxRIGHT | wxTOP, pad);
        return lbl;
    };

    // --- Raw pattern editor + copy ------------------------------------------
    sectionLabel(_L("Pattern"));
    wxBoxSizer *pat_row = new wxBoxSizer(wxHORIZONTAL);
    // A kit field around the editor, like every other text field; m_pattern is its inner
    // entry, so the focus call, the reads and the binds below reach the control typed into.
    auto *pattern_field = new ::TextInput(m_scroll, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition,
                                          wxSize(contentW - FromDIP(50), FromDIP(kTargetH)), wxTE_PROCESS_ENTER);
    pattern_field->SetName(_L("Regex pattern"));
    m_pattern = pattern_field->GetTextCtrl();
    m_pattern->SetFont(Label::Mono_13);
    m_pattern->SetMaxLength(kMaxPatternLen);
    m_pattern->SetName(_L("Regex pattern"));
    m_pattern->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        onPatternEdited();
        e.Skip();
    });
    pat_row->Add(pattern_field, 1, wxALIGN_CENTER_VERTICAL);

    m_copy = new Button(m_scroll, wxEmptyString);
    m_copy->SetIconButton(Button::IconShape::Circle, kTargetH);
    m_copy->SetGlyph(MaterialIcon::ContentCopy, 18);
    m_copy->SetToolTip(_L("Copy pattern"));
    m_copy->SetName(_L("Copy pattern"));
    m_copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copyPattern(); });
    pat_row->Add(m_copy, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    sizer->Add(pat_row, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    // Keep diagnostics readable independently of the editable pattern.
    m_status = new Label(m_scroll, Label::Body_13, _L("Empty pattern matches everything"), LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_status->SetBackgroundColour(surface);
    m_status->SetForegroundColour(on_var);
    m_status->SetMinSize(wxSize(0, -1));
    m_status->Wrap(contentW);
    sizer->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    // One-line structure and risk summary; the Explain tab has the detail.
    m_summary = new Label(m_scroll, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_summary->SetBackgroundColour(surface);
    m_summary->SetForegroundColour(on_var);
    m_summary->SetMinSize(wxSize(0, -1));
    m_summary->SetName(_L("Pattern analysis summary"));
    sizer->Add(m_summary, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap / 2);

    // --- Flags ---------------------------------------------------------------
    sectionLabel(_L("Flags"));
    auto addFlag = [&](const wxString &text, bool value, std::function<void(bool)> onToggle) {
        wxBoxSizer *row = new wxBoxSizer(wxHORIZONTAL);
        auto *box = new CheckBox(m_scroll);
        box->SetColorScheme(m_scheme);
        box->SetValue(value);
        // Name + tooltip carry the text for assistive tech and hover; the
        // VISIBLE text is the sibling Label below. SetLabel() must not be used
        // here: CheckBox is a wxBitmapToggleButton, i.e. a native BUTTON, and
        // MSW paints its window text inside the 44px glyph rect -- which came
        // out as clipped ghost text ("cx m", "sens") sitting underneath the
        // real label on every flag row.
        box->SetName(text);
        box->SetToolTip(text);
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
        refreshAnalysis();
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
        refreshAnalysis();
    });
    auto *word_note = new Label(m_scroll, Label::Body_13,
                                _L("Whole word applies to plain-text search only; in regex mode use \\b"));
    word_note->SetBackgroundColour(surface);
    word_note->SetForegroundColour(on_var);
    word_note->Wrap(contentW);
    sizer->Add(word_note, 0, wxLEFT | wxRIGHT | wxTOP, pad - gap / 2);

    // --- Literals (auto-escaped input) ----------------------------------------
    sectionLabel(_L("Literals"));
    wxBoxSizer *lit_row = new wxBoxSizer(wxHORIZONTAL);
    auto *literal_field = new ::TextInput(m_scroll, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition,
                                          wxSize(contentW - FromDIP(92), FromDIP(kTargetH)), wxTE_PROCESS_ENTER);
    literal_field->SetName(_L("Text to match literally"));
    m_literal = literal_field->GetTextCtrl();
    m_literal->SetFont(Label::Body_13);
    m_literal->SetHint(_L("Text to match literally"));
    m_literal->SetName(_L("Text to match literally"));
    m_literal->SetMaxLength(kMaxPatternLen);
    m_literal->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { addLiteral(); });
    lit_row->Add(literal_field, 1, wxALIGN_CENTER_VERTICAL);

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

    auto *sample_lbl = new Label(m_test_panel, Label::Head_14, _L("Sample text"));
    sample_lbl->SetBackgroundColour(surface);
    sample_lbl->SetForegroundColour(on_var);
    test_sizer->Add(sample_lbl, 0, wxTOP, gap / 2);

    // wxTE_RICH2 for SetStyle() match highlighting (repo precedent:
    // UnsavedChangesDialog). Multiline hints are unsupported on MSW, hence the
    // label above instead of a hint.
    m_sample = new TextAreaEditor(m_test_panel, wxID_ANY, wxEmptyString, wxDefaultPosition,
                              wxSize(0, FromDIP(96)),
                              wxTE_MULTILINE | wxTE_RICH2 | wxBORDER_NONE);
    m_sample->SetFont(Label::Mono_13);
    m_sample->SetBackgroundColour(field_bg);
    m_sample->SetForegroundColour(on);
    m_sample->SetMaxLength(kMaxSampleLen);
    m_sample->SetName(_L("Sample text"));
    m_sample->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        evaluate();
        e.Skip();
    });
    test_sizer->Add(m_sample, 0, wxEXPAND | wxTOP, gap / 2);

    auto *matches_lbl = new Label(m_test_panel, Label::Head_14, _L("Matches"));
    matches_lbl->SetBackgroundColour(surface);
    matches_lbl->SetForegroundColour(on_var);
    test_sizer->Add(matches_lbl, 0, wxTOP, gap);

    m_results = new TextAreaEditor(m_test_panel, wxID_ANY, wxEmptyString, wxDefaultPosition,
                               wxSize(0, FromDIP(144)),
                               wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    m_results->SetFont(Label::Mono_13);
    m_results->SetBackgroundColour(field_bg);
    m_results->SetForegroundColour(on);
    m_results->SetName(_L("Match results"));
    test_sizer->Add(m_results, 0, wxEXPAND | wxTOP, gap / 2);

    m_test_panel->SetSizer(test_sizer);
    sizer->Add(m_test_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, pad);
    sizer->Show(m_test_panel, false, true);

    sizer->AddSpacer(pad);
    m_scroll->SetSizer(sizer);

    // --- Build | Explain | Reference tab header (popup children, laid out
    // in fitPopup). Explain and Reference are built when first opened.
    m_tab_build   = new Button(this, _L("Build"));
    m_tab_explain = new Button(this, _L("Explain"));
    m_tab_ref     = new Button(this, _L("Reference"));
    for (Button *b : {m_tab_build, m_tab_explain, m_tab_ref}) {
        b->SetButtonSize(Button::Size::Large);
        b->SetColorScheme(m_scheme);
        b->SetName(b->GetLabel());
    }
    m_tab_explain->SetToolTip(_L("Engine, structure, tokens, compatibility and backtracking risk of this pattern"));
    m_tab_build->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { switchTab(TabBuild); });
    m_tab_explain->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { switchTab(TabExplain); });
    m_tab_ref->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { switchTab(TabReference); });
    switchTab(TabBuild);
}

void RegexBuilderPopup::buildReference()
{
    const wxColour surface  = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour on       = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var   = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const int pad      = FromDIP(MD3::Metrics::active().padding);
    const int contentW = FromDIP(kContentW);

    m_ref_scroll = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxTAB_TRAVERSAL | wxVSCROLL | wxBORDER_NONE);
    m_ref_scroll->SetBackgroundColour(surface);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    auto heading = [&](const wxString &text) {
        auto *lbl = new Label(m_ref_scroll, Label::Head_14, text);
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
    paragraph(wxString::Format(_L("Engine: Boost.Regex %s wide-character ECMAScript in an isolated 50 ms worker. Case-insensitive matching uses boost::regex_constants::icase. Escape metacharacters with a backslash."),
                               engineVersionText()));
    paragraph(_L("Case sensitive refines plain-text and regex search. Multiline makes ^ and $ match line boundaries; Whole word is plain-text only, so use \\b in regex mode."));
    paragraph(_L("An invalid or half-typed pattern never hides rows: it matches everything until it compiles."));
    paragraph(_L("Evaluation is local and bounded - long patterns and samples are truncated and runaway matching stops safely."));

    // Syntax on its own line, explanation below: capability and escaping
    // entries are too long for the two-column term rows.
    auto entry = [&](const wxString &syntax, const wxString &meaning) {
        auto *code = new Label(m_ref_scroll, Label::Mono_11, wxEmptyString);
        code->SetLabelText(syntax);
        code->SetBackgroundColour(surface);
        code->SetForegroundColour(on);
        code->Wrap(contentW);
        sizer->Add(code, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(8));
        auto *text = new Label(m_ref_scroll, Label::Body_12, wxEmptyString);
        text->SetLabelText(meaning); // (?&name) and && must render literally
        text->SetBackgroundColour(surface);
        text->SetForegroundColour(on_var);
        text->Wrap(contentW - FromDIP(12));
        sizer->Add(text, 0, wxLEFT | wxRIGHT, pad + FromDIP(12));
    };

    // Every builder flag and the exact engine flag it maps to.
    heading(_L("Flags"));
    for (const RA::FlagRow &row : RA::flag_rows())
        entry(wxString(row.engine) + "   " + wxString(row.inline_form),
              tr_utf8(row.name) + ": " + tr_utf8(row.effect));

    heading(_L("Escaping rules"));
    for (const RA::EscapeRule &rule : RA::escape_rules())
        entry(wxString(rule.syntax), tr_utf8(rule.meaning));

    // Full per-token documentation, straight from the Build tab's tables.
    for (const auto &[section_title, defs] : m_sections) {
        heading(section_title);
        for (const ChipDef &def : defs)
            term_row(def.label, def.tip);
    }

    // Capability matrix: unsupported constructs stay listed with the reason.
    heading(_L("Capabilities"));
    paragraph(_L("What this engine supports, partly supports and rejects. Each row has a probe pattern; the button runs every probe through the worker to confirm the row."));
    auto *check = new Button(m_ref_scroll, _L("Check against the engine"));
    check->SetButtonSize(Button::Size::Large);
    check->SetColorScheme(m_scheme);
    check->SetName(_L("Check against the engine"));
    check->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { checkCapabilities(); });
    sizer->Add(check, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(4));
    m_cap_status = new Label(m_ref_scroll, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_cap_status->SetMinSize(wxSize(0, -1));
    m_cap_status->SetBackgroundColour(surface);
    m_cap_status->SetForegroundColour(on_var);
    m_cap_status->SetName(_L("Capability check result"));
    sizer->Add(m_cap_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(8));
    for (RA::Support level : {RA::Support::Supported, RA::Support::Partial, RA::Support::Unsupported}) {
        auto *level_label = new Label(m_ref_scroll, Label::Head_14, tr_utf8(RA::support_label(level)));
        level_label->SetBackgroundColour(surface);
        level_label->SetForegroundColour(on);
        sizer->Add(level_label, 0, wxLEFT | wxRIGHT | wxTOP, pad);
        for (const RA::Capability &row : RA::capabilities())
            if (row.support == level)
                entry(wxString(row.syntax), tr_utf8(row.feature) + ": " + tr_utf8(row.explanation));
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
    m_ref_status = new Label(m_ref_scroll, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_ref_status->SetMinSize(wxSize(0, -1));
    m_ref_status->SetBackgroundColour(surface);
    m_ref_status->SetForegroundColour(on_var);
    sizer->Add(m_ref_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(8));

    sizer->AddSpacer(pad);
    m_ref_scroll->SetSizer(sizer);
    m_ref_scroll->Hide();
}

void RegexBuilderPopup::switchTab(int tab)
{
    if (tab == TabReference && !m_ref_scroll)
        buildReference();
    if (tab == TabExplain && !m_explain_scroll)
        buildExplain();
    m_active_tab = tab;
    if (m_scroll)     m_scroll->Show(tab == TabBuild);
    if (m_explain_scroll) {
        m_explain_scroll->Show(tab == TabExplain);
        if (tab == TabExplain)
            refreshAnalysis(); // the engine report and live views are filled while visible
    }
    if (m_ref_scroll) {
        m_ref_scroll->Show(tab == TabReference);
        if (tab == TabReference)
            m_ref_scroll->Scroll(0, 0); // always open the docs at the top
    }
    if (m_tab_build)   m_tab_build->SetVariant(tab == TabBuild ? Button::Variant::Tonal : Button::Variant::Text);
    if (m_tab_explain) m_tab_explain->SetVariant(tab == TabExplain ? Button::Variant::Tonal : Button::Variant::Text);
    if (m_tab_ref)     m_tab_ref->SetVariant(tab == TabReference ? Button::Variant::Tonal : Button::Variant::Text);
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
    refreshDiagnosticLayout(m_ref_scroll);
}

void RegexBuilderPopup::addSection(wxSizer *sizer, const wxString &title,
                                   const std::vector<ChipDef> &defs)
{
    m_sections.emplace_back(title, defs); // the Reference tab reuses these
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const int      pad     = FromDIP(MD3::Metrics::active().padding);

    auto *lbl = new Label(m_scroll, Label::Head_14, title);
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
    refreshAnalysis();

    const wxColour ok_colour   = StateColor::semantic(MD3::Role::Primary, m_scheme);
    const wxColour err_colour  = StateColor::semantic(MD3::Role::Error);
    const wxColour info_colour = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    auto setStatus = [this](const wxString &text, const wxColour &colour) {
        m_status->SetForegroundColour(colour);
        m_status->SetLabel(text);
        m_status->SetToolTip(text);
        m_status->Refresh();
        refreshDiagnosticLayout(m_scroll);
    };

    // Reset any previous match highlighting to the field's base style.
    auto resetHighlights = [this]() {
        if (!m_sample || m_sample->GetLastPosition() <= 0)
            return;
        wxTextAttr base;
        base.SetTextColour(StateColor::semantic(MD3::Role::OnSurface));
        base.SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
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

void RegexBuilderPopup::buildExplain()
{
    const wxColour surface  = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour field_bg = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
    const wxColour on       = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var   = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const int pad      = FromDIP(MD3::Metrics::active().padding);
    const int gap      = FromDIP(MD3::Metrics::active().gap);
    const int contentW = FromDIP(kContentW);

    m_explain_scroll = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                            wxTAB_TRAVERSAL | wxVSCROLL | wxBORDER_NONE);
    m_explain_scroll->SetBackgroundColour(surface);
    m_explain_scroll->SetName(_L("Explain"));
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    // The views stretch to the card; this strut gives the card the same
    // content width as the Build and Reference tabs.
    sizer->Add(contentW, 0, 0, wxLEFT | wxRIGHT, pad);

    auto heading = [&](const wxString &text) {
        auto *lbl = new Label(m_explain_scroll, Label::Head_14, text);
        lbl->SetBackgroundColour(surface);
        lbl->SetForegroundColour(on_var);
        sizer->Add(lbl, 0, wxLEFT | wxRIGHT | wxTOP, pad);
    };
    // Text that changes with the pattern; it wraps inside the card however
    // long a translation or diagnostic is.
    auto live = [&](const wxString &name) {
        auto *lbl = new Label(m_explain_scroll, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        lbl->SetBackgroundColour(surface);
        lbl->SetForegroundColour(on);
        lbl->SetMinSize(wxSize(0, -1));
        lbl->SetName(name);
        sizer->Add(lbl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap / 2);
        return lbl;
    };
    // Read-only, keyboard-focusable views a screen reader reads line by line.
    auto view = [&](const wxString &name, int height) {
        auto *text = new TextAreaEditor(m_explain_scroll, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                        wxSize(0, FromDIP(height)), wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
        text->SetFont(Label::Mono_11);
        text->SetBackgroundColour(field_bg);
        text->SetForegroundColour(on);
        text->SetName(name);
        sizer->Add(text, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap / 2);
        return text;
    };

    auto *title = new Label(m_explain_scroll, Label::Head_14, _L("Explain this pattern"));
    title->SetBackgroundColour(surface);
    title->SetForegroundColour(on);
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, pad);
    m_mode_note = live(_L("Regex mode"));
    m_mode_note->SetForegroundColour(on_var);

    heading(_L("Engine"));
    m_engine_info = live(_L("Engine"));
    heading(_L("Flags in effect"));
    m_flags_info = live(_L("Flags in effect"));
    heading(_L("Engine verdict"));
    m_verdict = live(_L("Engine verdict"));
    m_explain_summary = live(_L("Pattern analysis summary"));
    heading(_L("Structure"));
    m_tree = view(_L("Pattern structure"), 140);
    heading(_L("Tokens"));
    m_tokens = view(_L("Token annotations"), 140);
    heading(_L("Compatibility"));
    m_compat = view(_L("Compatibility notes"), 104);
    heading(_L("Backtracking risk"));
    m_risks = view(_L("Backtracking risk findings"), 104);

    m_use_example = new Button(m_explain_scroll, _L("Use example as sample text"));
    m_use_example->SetButtonSize(Button::Size::Large);
    m_use_example->SetVariant(Button::Variant::Tonal);
    m_use_example->SetColorScheme(m_scheme);
    m_use_example->SetName(_L("Use example as sample text"));
    m_use_example->SetToolTip(_L("Load the adversarial example into Test pattern to see the worker stop it safely"));
    m_use_example->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { useRiskExample(); });
    sizer->Add(m_use_example, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(4));

    sizer->AddSpacer(pad);
    m_explain_scroll->SetSizer(sizer);
    m_explain_scroll->Hide();
}

void RegexBuilderPopup::refreshAnalysis()
{
    if (!m_pattern)
        return;
    const bool explain_visible = m_explain_scroll && m_active_tab == TabExplain;
    // Ask the worker for its engine identity only for the visible Explain tab.
    const RA::EngineInfo &engine = engineInfo(explain_visible);
    const std::wstring pattern = m_pattern->GetValue().ToStdWstring();
    RA::Options options;
    options.regex_mode     = m_regex_on;
    options.case_sensitive = m_case_on;
    options.multiline      = m_multiline_on;
    options.whole_word     = m_word_on;
    options.code_unit_bits = engine.code_unit_bits;
    const RA::Analysis analysis = RA::analyze(pattern, options);

    std::size_t warnings = 0;
    for (const RA::Finding &finding : analysis.compatibility)
        if (finding.severity != RA::Severity::Info)
            ++warnings;
    const wxString risk = tr_utf8(RA::risk_label(analysis.risk));

    if (m_summary) {
        const wxString summary = pattern.empty() ? wxString()
            : wxString::Format(_L("Tokens: %d, capture groups: %d, backtracking risk: %s, compatibility warnings: %d"),
                               static_cast<int>(analysis.tokens.size()), static_cast<int>(analysis.capture_groups),
                               risk, static_cast<int>(warnings));
        if (m_summary->GetLabel() != summary) {
            m_summary->SetForegroundColour(StateColor::semantic(
                analysis.risk == RA::RiskLevel::High ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
            m_summary->SetLabel(summary);
            m_summary->SetToolTip(summary);
            refreshDiagnosticLayout(m_scroll);
        }
    }
    if (!explain_visible)
        return;

    const wxColour ok_colour  = StateColor::semantic(MD3::Role::Primary, m_scheme);
    const wxColour err_colour = StateColor::semantic(MD3::Role::Error);
    const wxColour on         = StateColor::semantic(MD3::Role::OnSurface);

    m_mode_note->SetLabel(m_regex_on ? wxString()
        : _L("Regex mode is off, so the search field matches this text literally. The analysis below shows how the engine would read it as a pattern."));
    m_mode_note->Show(!m_regex_on);

    // Engine identity, as the worker reported it.
    if (engine.reported)
        m_engine_info->SetLabel(wxString::Format(
            _L("Boost.Regex %s as reported by the worker. Syntax: Perl-compatible, selected by the ECMAScript flag. Code units: %d-bit. Locale traits: %s. State limit: %s."),
            wxString(RA::version_text(engine.version)), static_cast<int>(engine.code_unit_bits),
            tr_utf8(RA::traits_label(engine.traits)), wxString(std::to_wstring(engine.max_states))));
    else
        m_engine_info->SetLabel(wxString::Format(
            _L("The worker has not reported its engine yet, so this shows the Boost.Regex %s this build was compiled with. Open this tab again to retry."),
            engineVersionText()));

    // Flags in effect and the exact engine flag behind each one.
    const bool states[] = {m_regex_on, m_case_on, m_multiline_on, false, false, m_word_on};
    wxString flags;
    const auto &rows = RA::flag_rows();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!flags.empty())
            flags << "\n";
        flags << wxString::Format(_L("%s: %s. Engine flag: %s. In the pattern: %s"), tr_utf8(rows[i].name),
                                  (i < sizeof(states) / sizeof(states[0]) && states[i]) ? _L("On") : _L("Off"),
                                  wxString(rows[i].engine), wxString(rows[i].inline_form));
    }
    m_flags_info->SetLabel(flags);

    // The engine's verdict, with the offset where it stopped.
    if (pattern.empty()) {
        m_verdict->SetForegroundColour(on);
        m_verdict->SetLabel(_L("Empty pattern matches everything"));
    } else {
        Slic3r::GUI::BoundedRegex::Options engine_options;
        engine_options.case_sensitive = m_case_on;
        engine_options.multiline      = m_multiline_on;
        const auto verdict = Slic3r::GUI::BoundedRegex::validate(pattern, engine_options);
        using Slic3r::GUI::BoundedRegex::Status;
        if (verdict.status == Status::Valid) {
            m_verdict->SetForegroundColour(ok_colour);
            m_verdict->SetLabel(_L("The engine accepts this pattern."));
        } else if (verdict.status == Status::InvalidPattern &&
                   verdict.error_offset != Slic3r::GUI::BoundedRegex::kNoErrorOffset) {
            m_verdict->SetForegroundColour(err_colour);
            m_verdict->SetLabel(wxString::Format(_L("The engine rejected the pattern at offset %d: %s"),
                                                 static_cast<int>(verdict.error_offset), friendlyRegexError(verdict)));
        } else {
            m_verdict->SetForegroundColour(err_colour);
            m_verdict->SetLabel(friendlyRegexError(verdict));
        }
    }
    m_explain_summary->SetLabel(wxString::Format(_L("Tokens: %d, capture groups: %d (%d named), backtracking risk: %s."),
                                                 static_cast<int>(analysis.tokens.size()),
                                                 static_cast<int>(analysis.capture_groups),
                                                 static_cast<int>(analysis.named_groups), risk));

    // Structure tree.
    wxString tree;
    for (const RA::TreeLine &line : RA::tree_lines(analysis)) {
        const RA::Node &node = analysis.nodes[line.node];
        if (!tree.empty())
            tree << "\n";
        tree << wxString(static_cast<size_t>(line.depth) * 2, ' ') << tr(node.label);
        if (node.end > node.begin)
            tree << "   " << wxString(RA::fragment(pattern, node.begin, node.end, 24));
    }
    m_tree->ChangeValue(pattern.empty() ? wxString() : tree);

    // Token-by-token annotation.
    wxString tokens;
    for (const RA::Token &token : analysis.tokens) {
        if (!tokens.empty())
            tokens << "\n";
        tokens << wxString::Format("%-7s %-10s ", offsetText(token.begin, token.end),
                                   wxString(RA::fragment(pattern, token.begin, token.end, 16)))
               << tr(token.explanation);
    }
    m_tokens->ChangeValue(tokens);

    // Compatibility and portability notes.
    wxString compat;
    for (const RA::Finding &finding : analysis.compatibility) {
        if (!compat.empty())
            compat << "\n";
        compat << "[" << compatibilitySeverity(finding.severity) << "] " << offsetText(finding.begin, finding.end)
               << "  " << wxString(RA::fragment(pattern, finding.begin, finding.end, 24)) << "\n    "
               << tr(finding.message);
    }
    m_compat->ChangeValue(pattern.empty() ? wxString() : (compat.empty() ? _L("No compatibility notes.") : compat));

    // Backtracking risk with an adversarial example input.
    wxString risks;
    m_risk_example.clear();
    for (const RA::Finding &finding : analysis.risks) {
        if (!risks.empty())
            risks << "\n";
        risks << "[" << riskSeverity(finding.severity) << "] " << offsetText(finding.begin, finding.end) << "  "
              << wxString(RA::fragment(pattern, finding.begin, finding.end, 24)) << "\n    " << tr(finding.message);
        if (!finding.example.empty()) {
            risks << "\n    " << wxString::Format(_L("Example input: %s"), wxString(finding.example));
            if (m_risk_example.empty())
                m_risk_example = wxString(finding.example);
        }
    }
    m_risks->ChangeValue(pattern.empty() ? wxString() : (risks.empty() ? _L("No backtracking risks found.") : risks));
    m_use_example->Enable(!m_risk_example.empty());

    m_explain_scroll->Layout();
    refreshDiagnosticLayout(m_explain_scroll);
}

void RegexBuilderPopup::checkCapabilities()
{
    if (!m_cap_status)
        return;
    const auto &rows = RA::capabilities();
    std::size_t confirmed = 0;
    std::size_t unanswered = 0;
    wxString different;
    for (const RA::Capability &row : rows) {
        const RA::ProbeObservation seen = observeProbe(row);
        if (!seen.answered) {
            ++unanswered;
            continue;
        }
        if (RA::confirms(row, seen)) {
            ++confirmed;
        } else {
            if (!different.empty())
                different << ", ";
            different << tr_utf8(row.feature);
        }
    }
    const RA::EngineInfo &engine = engineInfo(true);
    wxString text;
    if (confirmed == rows.size()) {
        text = wxString::Format(_L("All %d rows behaved as listed on Boost.Regex %s in the worker."),
                                static_cast<int>(rows.size()),
                                engine.reported ? wxString(RA::version_text(engine.version)) : engineVersionText());
    } else {
        text = wxString::Format(_L("%d of %d rows behaved as listed."), static_cast<int>(confirmed),
                                static_cast<int>(rows.size()));
        if (!different.empty())
            text << "\n" << wxString::Format(_L("Different on this engine: %s"), different);
        if (unanswered != 0)
            text << "\n" << wxString::Format(_L("The worker did not answer for %d rows. Try again in a moment."),
                                             static_cast<int>(unanswered));
    }
    m_cap_status->SetForegroundColour(StateColor::semantic(
        confirmed == rows.size() ? MD3::Role::Primary : MD3::Role::Error, m_scheme));
    m_cap_status->SetLabel(text);
    m_cap_status->SetToolTip(text);
    m_ref_scroll->Layout();
    refreshDiagnosticLayout(m_ref_scroll);
}

void RegexBuilderPopup::useRiskExample()
{
    if (m_risk_example.empty() || !m_sample)
        return;
    if (!m_test_open)
        toggleTest();
    switchTab(TabBuild);
    // wxEVT_TEXT runs evaluate(): the bounded worker answers or stops it.
    m_sample->SetValue(m_risk_example);
    m_sample->SetFocus();
}

void RegexBuilderPopup::fitPopup()
{
    wxScrolledWindow *active = (m_active_tab == TabReference && m_ref_scroll) ? m_ref_scroll
                             : (m_active_tab == TabExplain && m_explain_scroll) ? m_explain_scroll
                                                                                 : m_scroll;
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
    const int    max_h = std::max(FromDIP(96), std::min(FromDIP(600), area.height - FromDIP(32)));

    const int inset  = FromDIP(4); // keeps square children inside the r12 border arc
    const int tab_h  = FromDIP(52); // 44-DIP Build | Explain | Reference targets + insets
    const int view_h = std::min(content.y, max_h - tab_h);
    const int sb_w   = content.y > view_h ? MD3ScrolledWindow::BarThickness(this) : 0;

    const int view_w = std::max(1, std::min(content.x + sb_w, area.width - 2 * inset - FromDIP(16)));
    SetClientSize(view_w + 2 * inset, tab_h + view_h + 2 * inset);
    if (m_tab_build && m_tab_explain && m_tab_ref) {
        // Equal-width tabs, never wider than a third of the card.
        const int widest = std::max({m_tab_build->GetBestSize().x, m_tab_explain->GetBestSize().x,
                                     m_tab_ref->GetBestSize().x});
        const int tab_w  = std::max(FromDIP(72), std::min(std::max(FromDIP(96), widest),
                                                          (view_w - FromDIP(16)) / 3));
        int x = inset + FromDIP(8);
        for (Button *tab : {m_tab_build, m_tab_explain, m_tab_ref}) {
            tab->SetSize(x, inset + FromDIP(4), tab_w, FromDIP(kTargetH));
            x += tab_w + FromDIP(4);
        }
    }
    for (wxScrolledWindow *scroll : {m_scroll, m_explain_scroll, m_ref_scroll}) {
        if (!scroll)
            continue;
        scroll->SetSize(inset, inset + tab_h, view_w, view_h);
        scroll->SetScrollRate(FromDIP(16), FromDIP(16));
    }
    active->FitInside();
    active->Layout();
}
