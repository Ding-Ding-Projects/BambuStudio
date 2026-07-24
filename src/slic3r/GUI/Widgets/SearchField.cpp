#include "SearchField.hpp"

#include "Label.hpp"
#include "StateColor.hpp"
#include "MaterialIcon.hpp"
#include "RegexBuilderPopup.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <algorithm>
#include <cwctype>
#include <regex>
#include <string>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>

// Logical (DIP) anatomy — mirrors design-source/SearchField.dc.html. Every
// value is passed through FromDIP() at use so the field stays crisp on HiDPI
// and re-derives on a monitor / density change (no cached device metrics).
namespace {
constexpr int kHeight    = 40; // pill height
constexpr int kPadLeft   = 14; // leading padding to the search glyph
constexpr int kPadRight  = 5;  // trailing padding
constexpr int kGap       = 4;  // glyph <-> input, input <-> clear
constexpr int kSearchPx  = 20; // leading search glyph
constexpr int kClosePx   = 18; // clear glyph
constexpr int kClearDiam = 30; // circular clear / tune / regex button
constexpr int kTunePx    = 20; // tune glyph
constexpr int kRadius    = 22; // stadium corner radius
constexpr int kMinWidth  = 220;
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

    m_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                            wxBORDER_NONE | wxTE_PROCESS_ENTER);
    m_text->SetFont(Label::Body_14);
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

    Bind(wxEVT_SIZE, [this](wxSizeEvent &e) {
        layoutText();
        e.Skip();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent &e) {
        const wxPoint p        = e.GetPosition();
        const bool    overTune = tuneButtonRect().Contains(p);
        const bool    overRegex = regexButtonRect().Contains(p);
        const bool    overClear = !overTune && !overRegex && !GetValue().IsEmpty() &&
                                  clearHitRect().Contains(p);
        bool changed = false;
        if (overClear != m_clear_hover) { m_clear_hover = overClear; changed = true; }
        if (overTune != m_tune_hover)   { m_tune_hover  = overTune;  changed = true; }
        if (overRegex != m_regex_hover) { m_regex_hover = overRegex; changed = true; }
        if (changed)
            Refresh();
        SetCursor((overClear || overTune || overRegex) ? wxCursor(wxCURSOR_HAND) : *wxSTANDARD_CURSOR);
        e.Skip();
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &e) {
        if (m_clear_hover || m_tune_hover || m_regex_hover) {
            m_clear_hover = m_tune_hover = m_regex_hover = false;
            Refresh();
        }
        e.Skip();
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        const wxPoint p = e.GetPosition();
        if (tuneButtonRect().Contains(p))
            openBuilder();
        else if (regexButtonRect().Contains(p))
            SetRegexEnabled(!m_regex);
        else if (!GetValue().IsEmpty() && clearHitRect().Contains(p))
            Clear();
        else if (m_text)
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
    m_text->ChangeValue(value); // no wxEVT_TEXT -> no query emit
    m_had_text = !value.IsEmpty();
    layoutText();
    Refresh();
}

void SearchField::Clear()
{
    if (m_text)
        m_text->ChangeValue(wxEmptyString);
    m_had_text = false;
    m_clear_hover = false;
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
    Refresh();
}

void SearchField::SetRegexEnabled(bool on)
{
    if (m_regex == on)
        return;
    m_regex = on;
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
            layoutText();
        }
        Refresh();
        emit(pattern);
    };
    cbs.onRegexMode = [this](bool on) { SetRegexEnabled(on); };
    cbs.onCase      = [this](bool on) {
        m_case_sensitive = on;
        if (m_on_regex_toggle)
            m_on_regex_toggle(m_regex);
    };
    cbs.onWord = [this](bool on) {
        m_whole_word = on;
        if (m_on_regex_toggle)
            m_on_regex_toggle(m_regex);
    };
    popup->Configure(m_scheme, GetValue(), m_regex, m_case_sensitive, m_whole_word, std::move(cbs));

    const wxRect  tr  = tuneButtonRect();
    const wxPoint pos = ClientToScreen(wxPoint(tr.GetLeft(), tr.GetBottom() + FromDIP(4)));
    popup->Position(pos, wxSize(0, 0));
    popup->Popup();
    popup->FocusPattern();
}

bool SearchField::textMatches(const wxString &query, const wxString &candidate, bool regex,
                              bool caseSensitive, bool wholeWord)
{
    if (query.IsEmpty())
        return true; // an empty query never filters anything out

    if (regex) {
        try {
            std::wregex::flag_type flags = std::regex_constants::ECMAScript;
            if (!caseSensitive)
                flags |= std::regex_constants::icase;
            const std::wstring pattern = query.ToStdWstring();
            const std::wstring hay     = candidate.ToStdWstring();
            const std::wregex  re(pattern, flags);
            return std::regex_search(hay, re);
        } catch (const std::regex_error &) {
            // A half-typed / invalid pattern must never hide every row.
            return true;
        }
    }

    if (!wholeWord)
        return caseSensitive ? candidate.Contains(query) : candidate.Lower().Contains(query.Lower());

    // Whole-word substring: scan for occurrences bounded by non-word characters
    // (\b semantics) without exposing the query to regex metacharacter parsing.
    const std::wstring hay = (caseSensitive ? candidate : candidate.Lower()).ToStdWstring();
    const std::wstring ndl = (caseSensitive ? query : query.Lower()).ToStdWstring();
    if (ndl.empty())
        return true;
    auto is_word = [](wchar_t c) { return std::iswalnum(static_cast<wint_t>(c)) != 0 || c == L'_'; };
    for (std::size_t pos = hay.find(ndl); pos != std::wstring::npos; pos = hay.find(ndl, pos + 1)) {
        const bool left_ok  = (pos == 0) || !is_word(hay[pos - 1]);
        const bool right_ok = (pos + ndl.size() >= hay.size()) || !is_word(hay[pos + ndl.size()]);
        if (left_ok && right_ok)
            return true;
    }
    return false;
}

void SearchField::Rescale()
{
    applyTextCtrlTheme();
    layoutText();
    Refresh();
}

bool SearchField::Enable(bool enable)
{
    bool result = wxWindow::Enable(enable);
    if (m_text)
        m_text->Enable(enable);
    Refresh();
    return result;
}

void SearchField::SetMinSize(const wxSize &size)
{
    wxSize size2 = size;
    if (size2.y < 0)
        size2.y = GetSize().y;
    wxWindow::SetMinSize(size2);
}

int SearchField::leadingWidth() const { return FromDIP(kPadLeft) + FromDIP(kSearchPx) + FromDIP(kGap); }

wxRect SearchField::clearButtonRect() const
{
    const wxSize sz = GetSize();
    const int    d  = FromDIP(kClearDiam);
    const int    x  = sz.x - FromDIP(kPadRight) - d;
    const int    y  = (sz.y - d) / 2;
    return wxRect(x, y, d, d);
}

wxRect SearchField::clearHitRect() const
{
    // >=44x44 logical hit/hover region centered on the 30px visual circle: the
    // glyph and circle keep their size, only the clickable area grows for a11y.
    const wxRect vis = clearButtonRect();
    const int    hit = std::max(FromDIP(44), vis.width);
    const int    cx  = vis.x + vis.width / 2;
    const int    cy  = vis.y + vis.height / 2;
    return wxRect(cx - hit / 2, cy - hit / 2, hit, hit);
}

wxRect SearchField::tuneButtonRect() const
{
    // One slot left of the (always-reserved) clear slot.
    const wxRect cr  = clearButtonRect();
    const int    d   = FromDIP(kClearDiam);
    const int    gap = FromDIP(kGap);
    return wxRect(cr.x - gap - d, cr.y, d, d);
}

wxRect SearchField::regexButtonRect() const
{
    const wxRect tr  = tuneButtonRect();
    const int    d   = FromDIP(kClearDiam);
    const int    gap = FromDIP(kGap);
    return wxRect(tr.x - gap - d, tr.y, d, d);
}

void SearchField::layoutText()
{
    if (!m_text)
        return;
    const wxSize sz   = GetSize();
    const int    lead = leadingWidth();
    // The regex toggle and tune button are persistent and the clear slot is
    // always reserved, so the trailing width is constant — the entry never
    // reflows when the query gains or loses its first character.
    const int    reserve = FromDIP(kPadRight) + 3 * FromDIP(kClearDiam) + 3 * FromDIP(kGap);
    int          w       = sz.x - lead - reserve;
    if (w < 0)
        w = 0;
    int th = m_text->GetBestSize().y;
    if (th <= 0)
        th = m_text->GetSize().y;
    const int y = (sz.y - th) / 2;
    m_text->SetSize(lead, y, w, th);
}

void SearchField::applyTextCtrlTheme()
{
    if (!m_text)
        return;
    const bool dark = StateColor::isDarkMode();
    m_text->SetBackgroundColour(MD3::resolve(MD3::Role::SurfaceContainerHighest, dark));
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
        m_had_text = hasText; // clear button appears / disappears -> reflow entry
        layoutText();
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

    // Trailing ".*" regex toggle — persistent. Active carries a SecondaryContainer
    // pill (scheme-aware) with an OnSecondaryContainer glyph; idle sits in
    // OnSurfaceVariant with a SurfaceContainerLow hover disc.
    {
        const wxRect rr = regexButtonRect();
        if (m_regex) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(MD3::resolve(MD3::Role::SecondaryContainer, dark, m_scheme)));
            dc.DrawRoundedRectangle(rr, rr.height / 2);
        } else if (m_regex_hover) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(MD3::resolve(MD3::Role::SurfaceContainerLow, dark)));
            dc.DrawCircle(rr.x + rr.width / 2, rr.y + rr.height / 2, rr.width / 2);
        }
        const wxColour fg = m_regex ? MD3::resolve(MD3::Role::OnSecondaryContainer, dark, m_scheme)
                                    : MD3::resolve(MD3::Role::OnSurfaceVariant, dark);
        // Plain (non-icon) mono text, safe on either a wxPaintDC or a wxGCDC.
        dc.SetFont(Label::Mono_13);
        dc.SetTextForeground(fg);
        const wxString lbl = ".*";
        const wxSize   te  = dc.GetTextExtent(lbl);
        dc.DrawText(lbl, rr.x + (rr.width - te.x) / 2, rr.y + (rr.height - te.y) / 2);
    }

    // Trailing `tune` builder button — persistent.
    {
        const wxRect br = tuneButtonRect();
        if (m_tune_hover) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(MD3::resolve(MD3::Role::SurfaceContainerLow, dark)));
            dc.DrawCircle(br.x + br.width / 2, br.y + br.height / 2, br.width / 2);
        }
        if (MaterialIcon::available())
            MaterialIcon::drawCentered(dc, MaterialIcon::Tune, kTunePx,
                                       MD3::resolve(MD3::Role::OnSurfaceVariant, dark), br);
    }

    // Trailing clear button — only while the field carries a query.
    if (!GetValue().IsEmpty()) {
        const wxRect cr = clearButtonRect();
        if (m_clear_hover) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(MD3::resolve(MD3::Role::SurfaceContainerLow, dark)));
            dc.DrawCircle(cr.x + cr.width / 2, cr.y + cr.height / 2, cr.width / 2);
        }
        if (MaterialIcon::available())
            MaterialIcon::drawCentered(dc, MaterialIcon::Close, kClosePx,
                                       MD3::resolve(MD3::Role::OnSurfaceVariant, dark), cr);
    }
}
