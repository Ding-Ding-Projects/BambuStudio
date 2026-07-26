#include "SearchField.hpp"

#include "Label.hpp"
#include "StateColor.hpp"
#include "MaterialIcon.hpp"
#include "MD3Motion.hpp"
#include "RegexBuilderPopup.hpp"
#include "BoundedRegex.hpp"
#include "Button.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>

// Logical (DIP) anatomy — mirrors design-source/SearchField.dc.html. Every
// value is passed through FromDIP() at use so the field stays crisp on HiDPI
// and re-derives on a monitor / density change (no cached device metrics).
namespace {
constexpr int kHeight    = 44; // pill height and minimum interactive target
constexpr int kPadLeft   = 14; // leading padding to the search glyph
constexpr int kPadRight  = 5;  // trailing padding
constexpr int kGap       = 4;  // glyph <-> input, input <-> clear
constexpr int kSearchPx  = 20; // leading search glyph
constexpr int kClosePx   = 18; // clear glyph
constexpr int kActionPx  = 44; // non-overlapping clear / tune / regex targets
constexpr int kTunePx    = 20; // tune glyph
constexpr int kRadius    = 22; // stadium corner radius
constexpr int kMinWidth  = 220;
constexpr int kMaxQueryLen = static_cast<int>(Slic3r::GUI::BoundedRegex::kMaxPatternCodeUnits);
} // namespace

SearchField::SearchField() {}

SearchField::SearchField(wxWindow *parent, const wxString &placeholder, const wxPoint &pos, const wxSize &size)
{
    Create(parent, placeholder, pos, size);
}

void SearchField::Create(wxWindow *parent, const wxString &placeholder, const wxPoint &pos, const wxSize &size)
{
    m_placeholder = placeholder;

    StaticBox::Create(parent, wxID_ANY, pos, size, wxTAB_TRAVERSAL);
    SetBackgroundColour(MD3::resolve(MD3::Role::SurfaceContainerHighest,
                                     StateColor::isDarkMode()));

    m_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                            wxBORDER_NONE | wxTE_PROCESS_ENTER);
    m_text->SetFont(Label::Body_14);
    m_text->SetMaxLength(kMaxQueryLen);
    if (!m_placeholder.IsEmpty())
        m_text->SetHint(m_placeholder);
    applyTextCtrlTheme();

    m_text->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) {
        onText();
        e.Skip();
    });
    m_text->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) {
        m_focused = true;
        Refresh();
        e.Skip();
    });
    m_text->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
        m_focused = false;
        Refresh();
        e.Skip();
    });
    m_text->Bind(wxEVT_RIGHT_DOWN, [](wxMouseEvent &) {}); // suppress native context menu
    m_text->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        // Escape is the keyboard equivalent of the clear 'x' affordance (which is
        // painted, not a focusable child): it empties a non-empty query. When the
        // field is already empty the key propagates (e.g. to close a dialog).
        if (e.GetKeyCode() == WXK_ESCAPE && !GetValue().IsEmpty())
            Clear();
        else
            e.Skip();
    });

    // Real child controls provide focus, accessible names and pressed state;
    // the previous painted parent regions were invisible to keyboard and AT.
    m_regex_button = new Button(this, ".*");
    m_regex_button->SetIconButton(Button::IconShape::Circle, kActionPx);
    m_regex_button->SetFont(Label::Mono_13);
    m_regex_button->SetName(_L("Regex mode"));
    m_regex_button->SetToolTip(_L("Regex mode"));
    m_regex_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        SetRegexEnabled(!m_regex);
    });

    m_tune_button = new Button(this, wxEmptyString);
    m_tune_button->SetIconButton(Button::IconShape::Circle, kActionPx);
    if (MaterialIcon::available())
        m_tune_button->SetGlyph(MaterialIcon::Tune, kTunePx);
    else
        m_tune_button->SetLabel(wxString(wxUniChar(0x2699)));
    m_tune_button->SetName(_L("Regex builder"));
    m_tune_button->SetToolTip(_L("Regex builder"));
    m_tune_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { openBuilder(); });

    m_clear_button = new Button(this, wxEmptyString);
    m_clear_button->SetIconButton(Button::IconShape::Circle, kActionPx);
    if (MaterialIcon::available())
        m_clear_button->SetGlyph(MaterialIcon::Close, kClosePx);
    else
        m_clear_button->SetLabel(wxString(wxUniChar(0x2715)));
    m_clear_button->SetName(_L("Clear"));
    m_clear_button->SetToolTip(_L("Clear"));
    m_clear_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Clear(); });
    m_clear_button->Hide();

    Bind(wxEVT_SIZE, [this](wxSizeEvent &e) {
        layoutText();
        e.Skip();
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        if (m_text)
            m_text->SetFocus();
        e.Skip();
    });
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent &e) {
        applyTextCtrlTheme();
        Refresh();
        e.Skip();
    });

    wxSize best = size;
    if (best.x <= 0)
        best.x = FromDIP(kMinWidth);
    if (best.y <= 0)
        best.y = FromDIP(kHeight);
    best.y = FromDIP(kHeight);
    SetMinSize(wxSize(FromDIP(kMinWidth), FromDIP(kHeight)));
    SetSize(best);
    layoutText();
    applyAccessibleName();
}

wxString SearchField::GetValue() const { return m_text ? m_text->GetValue() : wxString(); }

void SearchField::SetValue(const wxString &value)
{
    if (!m_text)
        return;
    const wxString bounded_value = value.Left(kMaxQueryLen);
    m_text->ChangeValue(bounded_value); // no wxEVT_TEXT -> no query emit
    m_had_text = !bounded_value.IsEmpty();
    if (m_clear_button)
        m_clear_button->Show(m_had_text);
    layoutText();
    Refresh();
}

void SearchField::Clear()
{
    if (m_text)
        m_text->ChangeValue(wxEmptyString);
    m_had_text = false;
    if (m_clear_button)
        m_clear_button->Hide();
    layoutText();
    Refresh();
    if (m_text)
        m_text->SetFocus();
    emit(wxEmptyString);
}

void SearchField::SetPlaceholder(const wxString &placeholder)
{
    m_placeholder = placeholder;
    if (m_text)
        m_text->SetHint(placeholder);
    applyAccessibleName();
}

void SearchField::applyAccessibleName()
{
    // The field and its entry carry an accessible name (the placeholder, or a
    // generic fallback) so assistive tech announces the search control.
    const wxString name = m_placeholder.IsEmpty() ? _L("Search") : m_placeholder;
    SetName(name);
    if (m_text)
        m_text->SetName(name);
}

void SearchField::SetColorScheme(MD3::ColorScheme scheme)
{
    m_scheme = scheme;
    if (m_regex_button)
        m_regex_button->SetColorScheme(scheme);
    if (m_tune_button)
        m_tune_button->SetColorScheme(scheme);
    if (m_clear_button)
        m_clear_button->SetColorScheme(scheme);
    Refresh();
}

void SearchField::SetRegexEnabled(bool on)
{
    if (m_regex == on)
        return;
    m_regex = on;
    if (m_regex_button)
        m_regex_button->SetValue(on);
    applyTextCtrlTheme(); // regex mode types in Roboto Mono per the kit
    layoutText();         // font swap can change the entry height
    if (m_on_regex_toggle)
        m_on_regex_toggle(on);
    Refresh();
}

void SearchField::SetCaseSensitive(bool on)
{
    if (m_case_sensitive == on)
        return;
    m_case_sensitive = on;
    // Consumers re-run their current filter through the shared regex-toggle hook.
    if (m_on_regex_toggle)
        m_on_regex_toggle(m_regex);
}

void SearchField::SetWholeWord(bool on)
{
    if (m_whole_word == on)
        return;
    m_whole_word = on;
    if (m_on_regex_toggle)
        m_on_regex_toggle(m_regex);
}

void SearchField::SetMultiline(bool on)
{
    if (m_multiline == on)
        return;
    m_multiline = on;
    if (m_on_regex_toggle)
        m_on_regex_toggle(m_regex);
}

void SearchField::openBuilder()
{
    if (m_on_builder)
        m_on_builder(); // host hook, in addition to the built-in popover

    // Rebuilt on every open so the popover re-derives its fonts, colours and
    // FromDIP metrics (theme / DPI / density safe). Destroy() is deferred by
    // wx, so allocating the replacement immediately is safe.
    if (m_builder_popup) {
        m_builder_popup->Destroy();
        m_builder_popup = nullptr;
    }
    auto *popup     = new RegexBuilderPopup(this);
    m_builder_popup = popup;

    RegexBuilderPopup::Callbacks cbs;
    cbs.onPattern = [this](const wxString &pattern) {
        if (!m_text)
            return;
        m_text->ChangeValue(pattern); // no wxEVT_TEXT -> no echo back into the popover
        const bool hasText = !pattern.IsEmpty();
        if (hasText != m_had_text) {
            m_had_text = hasText;
            if (m_clear_button)
                m_clear_button->Show(hasText);
        }
        Refresh();
        emit(pattern);
    };
    cbs.onRegexMode = [this](bool on) { SetRegexEnabled(on); };
    cbs.onCase = [this](bool on) { SetCaseSensitive(on); };
    cbs.onMultiline = [this](bool on) { SetMultiline(on); };
    cbs.onWord = [this](bool on) { SetWholeWord(on); };
    popup->Configure(m_scheme, GetValue(), m_regex, m_case_sensitive,
                     m_multiline, m_whole_word, std::move(cbs));

    const wxPoint pos = m_tune_button
                            ? m_tune_button->ClientToScreen(
                                  wxPoint(0, m_tune_button->GetSize().y + FromDIP(4)))
                            : ClientToScreen(wxPoint(0, GetSize().y + FromDIP(4)));
    popup->Position(pos, wxSize(0, 0));
    popup->PopupAndFocusPattern();
    // M3 entrance: quick fade (jumps to opaque under OS reduced motion).
    MD3::Motion::FadeIn(popup, MD3::Motion::short2);
}

bool SearchField::textMatches(const wxString &query, const wxString &candidate, bool regex,
                               bool caseSensitive, bool wholeWord, bool multiline)
{
    MatchPass pass(query, regex, caseSensitive, wholeWord, multiline);
    return pass.matches(candidate);
}

SearchField::MatchPass::MatchPass(const wxString &query, bool regex, bool caseSensitive,
                                   bool wholeWord, bool multiline)
    : m_query(query)
    , m_regex(regex)
    , m_case_sensitive(caseSensitive)
    , m_whole_word(wholeWord)
    , m_multiline(multiline)
{
    if (m_regex && !m_query.IsEmpty()) {
        Slic3r::GUI::BoundedRegex::Options options;
        options.case_sensitive = m_case_sensitive;
        options.multiline = m_multiline;
        m_regex_pass = std::make_unique<Slic3r::GUI::BoundedRegex::SearchPass>(
            m_query.ToStdWstring(), options);
    }
}

SearchField::MatchPass::~MatchPass() = default;
SearchField::MatchPass::MatchPass(MatchPass &&) noexcept = default;
SearchField::MatchPass &SearchField::MatchPass::operator=(MatchPass &&) noexcept = default;

bool SearchField::MatchPass::matches(const wxString &candidate)
{
    if (m_query.IsEmpty())
        return true;

    if (m_regex) {
        // Invalid, oversized, timed-out, unavailable, or aggregate-budget
        // exhaustion fails open for the remainder of this collection pass.
        return m_regex_pass->allows_candidate(candidate.ToStdWstring());
    }

    return Slic3r::GUI::BoundedRegex::plain_search(
        m_query.ToStdWstring(), candidate.ToStdWstring(), m_case_sensitive, m_whole_word);
}

wxString SearchField::colorSearchText(const wxColour &colour)
{
    if (!colour.IsOk())
        return wxEmptyString;
    // Nearest common colour name by RGB distance. The table deliberately keeps
    // to everyday names (both grey spellings) so "green" or "#00FF00" both hit
    // a green filament row; precision work stays with the hex form.
    struct Named { const wchar_t *name; unsigned char r, g, b; };
    static const Named names[] = {
        {L"black", 0, 0, 0},       {L"white", 255, 255, 255}, {L"gray grey", 128, 128, 128},
        {L"silver", 192, 192, 192},{L"red", 220, 40, 40},     {L"dark red maroon", 128, 0, 0},
        {L"orange", 255, 140, 0},  {L"brown", 139, 90, 43},   {L"yellow", 250, 220, 40},
        {L"gold", 212, 175, 55},   {L"lime", 160, 230, 50},   {L"green", 40, 160, 60},
        {L"dark green", 0, 100, 0},{L"teal", 0, 150, 136},    {L"cyan", 60, 200, 220},
        {L"sky blue", 120, 190, 240}, {L"blue", 40, 90, 220}, {L"navy", 0, 0, 128},
        {L"indigo", 75, 60, 180},  {L"purple violet", 140, 70, 190}, {L"magenta", 220, 60, 180},
        {L"pink", 240, 140, 180},  {L"beige natural", 235, 220, 190},
    };
    const int r = colour.Red(), g = colour.Green(), b = colour.Blue();
    const Named *best = &names[0];
    long best_d = LONG_MAX;
    for (const Named &n : names) {
        const long d = long(r - n.r) * (r - n.r) + long(g - n.g) * (g - n.g) + long(b - n.b) * (b - n.b);
        if (d < best_d) { best_d = d; best = &n; }
    }
    return wxString::Format("#%02X%02X%02X ", r, g, b) + wxString(best->name);
}

void SearchField::Rescale()
{
    applyTextCtrlTheme();
    if (m_regex_button)
        m_regex_button->Rescale();
    if (m_tune_button)
        m_tune_button->Rescale();
    if (m_clear_button)
        m_clear_button->Rescale();
    layoutText();
    Refresh();
}

bool SearchField::Enable(bool enable)
{
    bool result = wxWindow::Enable(enable);
    if (m_text)
        m_text->Enable(enable);
    if (m_regex_button)
        m_regex_button->Enable(enable);
    if (m_tune_button)
        m_tune_button->Enable(enable);
    if (m_clear_button)
        m_clear_button->Enable(enable);
    Refresh();
    return result;
}

void SearchField::SetMinSize(const wxSize &size)
{
    wxSize size2 = size;
    if (size2.y < 0)
        size2.y = std::max(GetSize().y, FromDIP(kHeight));
    else
        size2.y = std::max(size2.y, FromDIP(kHeight));
    wxWindow::SetMinSize(size2);
}

int SearchField::leadingWidth() const { return FromDIP(kPadLeft) + FromDIP(kSearchPx) + FromDIP(kGap); }

void SearchField::layoutText()
{
    if (!m_text)
        return;
    const wxSize sz   = GetSize();
    const int    lead = leadingWidth();
    // The regex toggle and tune button are persistent and the clear slot is
    // always reserved, so the trailing width is constant — the entry never
    // reflows when the query gains or loses its first character.
    const int    action  = FromDIP(kActionPx);
    const int    gap     = FromDIP(kGap);
    const int    right   = FromDIP(kPadRight);
    const int    reserve = right + 3 * action + 3 * gap;
    int          w       = sz.x - lead - reserve;
    if (w < 0)
        w = 0;
    int th = m_text->GetBestSize().y;
    if (th <= 0)
        th = m_text->GetSize().y;
    const int y = (sz.y - th) / 2;
    m_text->SetSize(lead, y, w, th);

    const int action_y = (sz.y - action) / 2;
    int       action_x = sz.x - right - action;
    if (m_clear_button)
        m_clear_button->SetSize(action_x, action_y, action, action);
    action_x -= gap + action;
    if (m_tune_button)
        m_tune_button->SetSize(action_x, action_y, action, action);
    action_x -= gap + action;
    if (m_regex_button)
        m_regex_button->SetSize(action_x, action_y, action, action);
}

void SearchField::applyTextCtrlTheme()
{
    if (!m_text)
        return;
    const bool dark = StateColor::isDarkMode();
    const wxColour background = MD3::resolve(MD3::Role::SurfaceContainerHighest, dark);
    SetBackgroundColour(background);
    m_text->SetBackgroundColour(background);
    m_text->SetForegroundColour(MD3::resolve(MD3::Role::OnSurface, dark));
    m_text->SetFont(m_regex ? Label::Mono_13 : Label::Body_14);
    m_text->Refresh();
}

void SearchField::emit(const wxString &value)
{
    if (m_on_query)
        m_on_query(value);
}

void SearchField::onText()
{
    const bool hasText = !GetValue().IsEmpty();
    if (hasText != m_had_text) {
        m_had_text = hasText;
        if (m_clear_button)
            m_clear_button->Show(hasText);
    }
    Refresh();
    emit(GetValue());
    // Field -> builder pattern sync while the popover is open (the popover's
    // own edits arrive via ChangeValue, which never re-enters here).
    if (m_builder_popup && m_builder_popup->IsShown())
        static_cast<RegexBuilderPopup *>(m_builder_popup)->SyncPattern(GetValue());
}

void SearchField::doRender(wxDC &dc)
{
    const wxSize sz = GetSize();
    if (sz.x <= 0 || sz.y <= 0)
        return;

    const bool dark = StateColor::isDarkMode();

    // Pill: SurfaceContainerHighest fill, Outline border promoted to Primary
    // (scheme-aware) while focused. Radius + border width resolved live.
    const wxColour bg     = MD3::resolve(MD3::Role::SurfaceContainerHighest, dark);
    const wxColour border = m_focused ? MD3::resolve(MD3::Role::Primary, dark, m_scheme)
                                      : MD3::resolve(MD3::Role::Outline, dark);
    const int      bw     = std::max(1, FromDIP(1));
    double         radius = FromDIP(kRadius) - bw;
    if (radius < 0)
        radius = 0;

    wxRect rc(0, 0, sz.x, sz.y);
    rc.Deflate(bw);
    dc.SetPen(wxPen(border, bw));
    dc.SetBrush(wxBrush(bg));
    dc.DrawRoundedRectangle(rc, radius);

    // Leading search glyph.
    if (MaterialIcon::available()) {
        const wxRect gr(FromDIP(kPadLeft), 0, FromDIP(kSearchPx), sz.y);
        MaterialIcon::drawCentered(dc, MaterialIcon::Search, kSearchPx,
                                   MD3::resolve(MD3::Role::OnSurfaceVariant, dark), gr);
    }

}
