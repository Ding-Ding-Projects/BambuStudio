#include "SearchField.hpp"

#include "Label.hpp"
#include "StateColor.hpp"
#include "MaterialIcon.hpp"

#include <algorithm>

#include <wx/dcclient.h>
#include <wx/dcgraph.h>

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
constexpr int kClearDiam = 30; // circular clear button
constexpr int kRadius    = 22; // stadium corner radius
constexpr int kMinWidth  = 200;
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

    Bind(wxEVT_SIZE, [this](wxSizeEvent &e) {
        layoutText();
        e.Skip();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent &e) {
        bool over = !GetValue().IsEmpty() && clearButtonRect().Contains(e.GetPosition());
        if (over != m_clear_hover) {
            m_clear_hover = over;
            Refresh();
        }
        SetCursor(over ? wxCursor(wxCURSOR_HAND) : *wxSTANDARD_CURSOR);
        e.Skip();
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &e) {
        if (m_clear_hover) {
            m_clear_hover = false;
            Refresh();
        }
        e.Skip();
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        if (!GetValue().IsEmpty() && clearButtonRect().Contains(e.GetPosition()))
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

void SearchField::layoutText()
{
    if (!m_text)
        return;
    const wxSize sz       = GetSize();
    const bool   hasText  = !GetValue().IsEmpty();
    const int    lead     = leadingWidth();
    const int    reserve  = FromDIP(kPadRight) + (hasText ? (FromDIP(kClearDiam) + FromDIP(kGap)) : 0);
    int          w        = sz.x - lead - reserve;
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
