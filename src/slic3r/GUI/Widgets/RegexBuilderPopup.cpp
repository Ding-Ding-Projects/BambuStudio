#include "RegexBuilderPopup.hpp"

#include "Button.hpp"
#include "CheckBox.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <algorithm>
#include <regex>
#include <string>

#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/display.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/utils.h>

namespace {

// Logical (DIP) metrics + safety bounds. The bounds keep evaluation local and
// cheap: std::regex has no timeout, so the guard is small inputs + a match cap
// + try/catch around every compile / search (error_complexity / error_stack).
constexpr int kContentW      = 344;  // inner content width
constexpr int kPad           = 12;   // card padding
constexpr int kGapY          = 8;    // vertical rhythm between rows
constexpr int kChipH         = 26;   // chip height (matches the old popover)
constexpr int kMaxPatternLen = 2000;
constexpr int kMaxSampleLen  = 20000;
constexpr int kMaxMatches    = 200;
constexpr int kMaxShownLen   = 60;   // clip match/group text in the results list

wxString friendlyRegexError(const std::regex_error &err)
{
    switch (err.code()) {
    case std::regex_constants::error_brack: return _L("Unbalanced [ ] character set");
    case std::regex_constants::error_paren: return _L("Unbalanced ( ) group");
    case std::regex_constants::error_brace: return _L("Unbalanced { } quantifier");
    case std::regex_constants::error_badbrace: return _L("Invalid counts inside { }");
    case std::regex_constants::error_range: return _L("Invalid character range");
    case std::regex_constants::error_escape: return _L("Invalid escape sequence");
    case std::regex_constants::error_backref: return _L("Invalid backreference");
    case std::regex_constants::error_badrepeat: return _L("Quantifier has nothing to repeat");
    case std::regex_constants::error_collate:
    case std::regex_constants::error_ctype: return _L("Unknown character class name");
    case std::regex_constants::error_complexity:
    case std::regex_constants::error_space:
    case std::regex_constants::error_stack: return _L("Pattern too complex to evaluate safely");
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
// A single keyboard-reachable token palette: wrapped rows of custom-painted MD3
// chips. One tab stop per group; Left/Right/Up/Down move the active chip,
// Enter/Space insert it, hover shows the per-token tooltip.
class RegexBuilderPopup::ChipGroup : public wxWindow
{
public:
    ChipGroup(wxWindow *parent, const wxString &name, MD3::ColorScheme scheme,
              std::vector<ChipDef> defs, std::function<void(const ChipDef &)> onInsert)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE)
        , m_scheme(scheme)
        , m_defs(std::move(defs))
        , m_on_insert(std::move(onInsert))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetName(name);
        SetLabel(name); // accessible name for assistive tech
        Bind(wxEVT_PAINT, &ChipGroup::onPaint, this);
        Bind(wxEVT_MOTION, &ChipGroup::onMotion, this);
        Bind(wxEVT_LEAVE_WINDOW, &ChipGroup::onLeave, this);
        Bind(wxEVT_LEFT_DOWN, &ChipGroup::onLeftDown, this);
        Bind(wxEVT_KEY_DOWN, &ChipGroup::onKeyDown, this);
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
    }

    // Wrap the chips into rows for the given content width and freeze the
    // resulting min size for the hosting sizer.
    void Reflow(int width)
    {
        m_rects.clear();
        const int gap  = FromDIP(6);
        const int padX = FromDIP(10);
        const int h    = FromDIP(kChipH);

        wxBitmap   probe(1, 1);
        wxMemoryDC mdc(probe);
        mdc.SetFont(Label::Mono_12);

        int x = 0, y = 0;
        for (const ChipDef &d : m_defs) {
            const int w = mdc.GetTextExtent(d.label).x + 2 * padX;
            if (x > 0 && x + w > width) {
                x = 0;
                y += h + gap;
            }
            m_rects.push_back(wxRect(x, y, w, h));
            x += w + gap;
        }
        SetMinSize(wxSize(width, y + h));
    }

private:
    void onPaint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHigh)));
        dc.Clear();

        const bool focused = HasFocus();
        for (size_t i = 0; i < m_rects.size(); ++i) {
            const wxRect &rc  = m_rects[i];
            const bool    hov = (static_cast<int>(i) == m_hover);
            const bool    act = focused && (static_cast<int>(i) == m_focus_idx);
            dc.SetBrush(wxBrush(StateColor::semantic(hov ? MD3::Role::SurfaceContainerHigh
                                                         : MD3::Role::SurfaceContainerHighest)));
            // The keyboard-active chip carries a 2px Primary focus ring.
            dc.SetPen(act ? wxPen(StateColor::semantic(MD3::Role::Primary, m_scheme), std::max(2, FromDIP(2)))
                          : wxPen(StateColor::semantic(MD3::Role::OutlineVariant), std::max(1, FromDIP(1))));
            dc.DrawRoundedRectangle(rc, FromDIP(8));
            dc.SetFont(Label::Mono_12);
            dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurface));
            const wxSize te = dc.GetTextExtent(m_defs[i].label);
            dc.DrawText(m_defs[i].label, rc.x + (rc.width - te.x) / 2, rc.y + (rc.height - te.y) / 2);
        }
    }

    int hitTest(const wxPoint &p) const
    {
        for (size_t i = 0; i < m_rects.size(); ++i)
            if (m_rects[i].Contains(p))
                return static_cast<int>(i);
        return -1;
    }

    void onMotion(wxMouseEvent &e)
    {
        const int h = hitTest(e.GetPosition());
        if (h != m_hover) {
            m_hover = h;
            if (h >= 0)
                SetToolTip(m_defs[h].tip);
            else
                UnsetToolTip();
            Refresh();
        }
        SetCursor(h >= 0 ? wxCursor(wxCURSOR_HAND) : *wxSTANDARD_CURSOR);
        e.Skip();
    }

    void onLeave(wxMouseEvent &e)
    {
        if (m_hover != -1) {
            m_hover = -1;
            UnsetToolTip();
            Refresh();
        }
        e.Skip();
    }

    void onLeftDown(wxMouseEvent &e)
    {
        const int h = hitTest(e.GetPosition());
        if (h >= 0) {
            m_focus_idx = h;
            if (m_on_insert)
                m_on_insert(m_defs[h]); // popover stays open for further inserts
            Refresh();
            return;
        }
        e.Skip();
    }

    // Up/Down (dir -1/+1) land on the chip in the nearest row above/below
    // whose horizontal centre is closest to the current one.
    void moveRow(int dir)
    {
        if (m_rects.empty())
            return;
        const wxRect cur      = m_rects[m_focus_idx];
        int          best     = -1;
        long         best_key = 0;
        for (size_t i = 0; i < m_rects.size(); ++i) {
            const wxRect &rc = m_rects[i];
            if (dir > 0 ? rc.y <= cur.y : rc.y >= cur.y)
                continue;
            const long row_dist = std::abs((long) rc.y - (long) cur.y);
            const long dx  = std::abs((long) (rc.x + rc.width / 2) - (long) (cur.x + cur.width / 2));
            const long key = row_dist * 100000 + dx; // nearest row first, then nearest column
            if (best < 0 || key < best_key) {
                best     = static_cast<int>(i);
                best_key = key;
            }
        }
        if (best >= 0) {
            m_focus_idx = best;
            Refresh();
        }
    }

    void onKeyDown(wxKeyEvent &e)
    {
        const int n = static_cast<int>(m_defs.size());
        switch (e.GetKeyCode()) {
        case WXK_LEFT:
            m_focus_idx = std::max(0, m_focus_idx - 1);
            Refresh();
            return;
        case WXK_RIGHT:
            m_focus_idx = std::min(n - 1, m_focus_idx + 1);
            Refresh();
            return;
        case WXK_HOME:
            m_focus_idx = 0;
            Refresh();
            return;
        case WXK_END:
            m_focus_idx = n - 1;
            Refresh();
            return;
        case WXK_UP:
            moveRow(-1);
            return;
        case WXK_DOWN:
            moveRow(+1);
            return;
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
        case WXK_SPACE:
            if (m_focus_idx >= 0 && m_focus_idx < n && m_on_insert)
                m_on_insert(m_defs[m_focus_idx]);
            return;
        case WXK_TAB:
            Navigate(e.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
            return;
        default:
            e.Skip();
        }
    }

    MD3::ColorScheme     m_scheme;
    std::vector<ChipDef> m_defs;
    std::vector<wxRect>  m_rects;
    int                  m_hover     = -1;
    int                  m_focus_idx = 0;

    std::function<void(const ChipDef &)> m_on_insert;
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
#ifdef __WXMSW__
    BindUnfocusEvent();
#endif
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
                             _L("Engine: std::regex / std::wregex, ECMAScript grammar. Case-insensitive matching uses std::regex::icase. Escape metacharacters with a backslash."));
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
                               wxSize(contentW - FromDIP(36), -1), wxBORDER_NONE | wxTE_PROCESS_ENTER);
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
    m_copy->SetIconButton(Button::IconShape::Circle, 30);
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
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
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
                               wxSize(contentW - FromDIP(76), -1), wxBORDER_NONE | wxTE_PROCESS_ENTER);
    m_literal->SetFont(Label::Body_13);
    m_literal->SetBackgroundColour(field_bg);
    m_literal->SetForegroundColour(on);
    m_literal->SetHint(_L("Text to match literally"));
    m_literal->SetName(_L("Text to match literally"));
    m_literal->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { addLiteral(); });
    lit_row->Add(m_literal, 1, wxALIGN_CENTER_VERTICAL);

    auto *add_btn = new Button(m_scroll, _L("Add"));
    add_btn->SetVariant(Button::Variant::Tonal);
    add_btn->SetButtonSize(Button::Size::Small);
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
    m_test_toggle->SetButtonSize(Button::Size::Small);
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
        b->SetButtonSize(Button::Size::Small);
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
        t->SetMinSize(wxSize(FromDIP(64), -1));
        row->Add(t, 0, wxALIGN_TOP);
        auto *m = new Label(m_ref_scroll, Label::Body_12, meaning);
        m->SetBackgroundColour(surface);
        m->SetForegroundColour(on_var);
        m->Wrap(contentW - FromDIP(72));
        row->Add(m, 1, wxLEFT, FromDIP(8));
        sizer->Add(row, 0, wxLEFT | wxRIGHT | wxTOP, pad - FromDIP(8));
    };

    auto *title = new Label(m_ref_scroll, Label::Head_14, _L("Reference & help"));
    title->SetBackgroundColour(surface);
    title->SetForegroundColour(on);
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    heading(_L("How search works"));
    paragraph(_L("Plain text is the default in every search bar; regex mode is a deliberate opt-in via the .* toggle."));
    paragraph(_L("Engine: std::regex / std::wregex, ECMAScript grammar. Case-insensitive matching uses std::regex::icase. Escape metacharacters with a backslash."));
    paragraph(_L("Case sensitive and Whole word refine plain-text search; in regex mode author \\b yourself for word boundaries."));
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
    oc_btn->SetButtonSize(Button::Size::Small);
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
    if (m_ref_scroll) m_ref_scroll->Show(tab == 1);
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
    wxString prompt = "Help me build a regular expression. Engine: C++ std::regex/std::wregex, "
                      "ECMAScript grammar, icase when case-insensitive; invalid patterns must fail safe. ";
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
                                  bool regexOn, bool caseOn, bool wordOn, Callbacks callbacks)
{
    m_scheme   = scheme;
    m_regex_on = regexOn;
    m_case_on  = caseOn;
    m_word_on  = wordOn;
    m_cb       = std::move(callbacks);

    if (!m_scroll)
        build();

    m_syncing = true;
    m_pattern->ChangeValue(pattern);
    m_syncing = false;
    m_regex_cb->SetValue(regexOn);
    m_case_cb->SetValue(caseOn);
    m_word_cb->SetValue(wordOn);
    m_regex_cb->SetColorScheme(scheme);
    m_case_cb->SetColorScheme(scheme);
    m_word_cb->SetColorScheme(scheme);

    evaluate();
    fitPopup();
}

void RegexBuilderPopup::SyncPattern(const wxString &pattern)
{
    if (!m_pattern || m_pattern->GetValue() == pattern)
        return;
    m_syncing = true;
    m_pattern->ChangeValue(pattern); // no wxEVT_TEXT -> no echo through onPattern
    m_syncing = false;
    evaluate();
}

wxString RegexBuilderPopup::GetPattern() const { return m_pattern ? m_pattern->GetValue() : wxString(); }

void RegexBuilderPopup::FocusPattern()
{
    if (!m_pattern)
        return;
    m_pattern->SetFocus();
    m_pattern->SetInsertionPointEnd();
}

void RegexBuilderPopup::insertChip(const ChipDef &def)
{
    if (!m_pattern)
        return;
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
    m_pattern->WriteText(escapeLiteral(raw));
    m_literal->Clear();
}

wxString RegexBuilderPopup::escapeLiteral(const wxString &raw)
{
    // ECMAScript metacharacters that must be escaped to match literally.
    const wxString meta = "\\^$.|?*+()[]{}";
    wxString       out;
    out.reserve(raw.length() * 2);
    for (size_t i = 0; i < raw.length(); ++i) {
        const wxUniChar c = raw[i];
        if (meta.Find(c) != wxNOT_FOUND)
            out << '\\';
        out << c;
    }
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

    std::wregex re;
    try {
        auto flags = std::regex_constants::ECMAScript;
        if (!m_case_on)
            flags |= std::regex_constants::icase;
        re.assign(pattern.ToStdWstring(), flags);
    } catch (const std::regex_error &err) {
        setStatus(friendlyRegexError(err), err_colour);
        resetHighlights();
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    } catch (const std::exception &) {
        setStatus(_L("Invalid regular expression"), err_colour);
        resetHighlights();
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    }

    // --- Bounded sample evaluation --------------------------------------------
    const wxString sample_full = m_sample ? m_sample->GetValue() : wxString();
    if (sample_full.IsEmpty()) {
        setStatus(_L("Valid pattern"), ok_colour);
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    }

    const bool     truncated = sample_full.length() > (size_t) kMaxSampleLen;
    const wxString sample    = truncated ? sample_full.Left(kMaxSampleLen) : sample_full;
    const std::wstring hay   = sample.ToStdWstring();

    wxString out;
    int      count = 0;
    std::vector<std::pair<long, long>> spans;
    try {
        // std::wsregex_iterator handles zero-width advancement; the loop is
        // bounded by kMaxMatches, and error_complexity/error_stack land in the
        // catch below — the popover never propagates a regex exception.
        std::wsregex_iterator it(hay.begin(), hay.end(), re), end;
        for (; it != end && count < kMaxMatches; ++it) {
            const std::wsmatch &m = *it;
            ++count;
            const long s = static_cast<long>(m.position(0));
            const long e = s + static_cast<long>(m.length(0));
            spans.emplace_back(s, e);
            out << wxString::Format(_L("Match %d at %d-%d: %s"), count, (int) s, (int) e,
                                    clipForList(wxString(m.str(0))))
                << "\n";
            for (size_t g = 1; g < m.size(); ++g) {
                out << "    ";
                if (m[g].matched)
                    out << wxString::Format(_L("group %d: %s"), (int) g, clipForList(wxString(m.str(g))));
                else
                    out << wxString::Format(_L("group %d: (no match)"), (int) g);
                out << "\n";
            }
        }
    } catch (const std::exception &) {
        setStatus(_L("Pattern too complex to evaluate safely"), err_colour);
        resetHighlights();
        if (m_results)
            m_results->ChangeValue(wxEmptyString);
        return;
    }

    if (count == 0)
        out = _L("No matches.");
    else if (count >= kMaxMatches)
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
    const int tab_h  = FromDIP(34); // Build | Reference header strip
    const int view_h = std::min(content.y, max_h - tab_h);
    const int sb_w   = content.y > view_h ? wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, active) : 0;

    SetClientSize(content.x + sb_w + 2 * inset, tab_h + view_h + 2 * inset);
    if (m_tab_build && m_tab_ref) {
        const int tab_w = FromDIP(96);
        m_tab_build->SetSize(inset + FromDIP(8), inset + FromDIP(3), tab_w, tab_h - FromDIP(6));
        m_tab_ref->SetSize(inset + FromDIP(12) + tab_w, inset + FromDIP(3), tab_w, tab_h - FromDIP(6));
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
