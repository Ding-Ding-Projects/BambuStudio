#include "TextArea.hpp"

#include "Label.hpp"
#include "StateColor.hpp"
#include "TextCtrl.h"

#include <wx/dcclient.h>

#include <algorithm>

namespace {
constexpr int kPad = 8;          // inset from the outline to the editor, in DIP
constexpr int kRestBorder = 1;   // OutlineVariant at rest
constexpr int kFocusBorder = 2;  // Primary while the editor has focus
} // namespace

TextArea::TextArea() = default;

TextArea::TextArea(wxWindow *parent, const wxString &text, const wxSize &size, long style)
    : TextArea()
{
    Create(parent, text, size, style);
}

void TextArea::Create(wxWindow *parent, const wxString &text, const wxSize &size, long style)
{
    StaticBox::Create(parent, wxID_ANY, wxDefaultPosition, size, 0);
    SetCornerRadius(FromDIP(MD3::Metrics::radius_tiny));
    m_read_only = (style & wxTE_READONLY) != 0;

    // The editor is always multi-line and borderless; the container draws the
    // outline. wxTE_RICH keeps SetStyle() working for callers that colour runs.
    long editor_style = style | wxTE_MULTILINE | wxBORDER_NONE;
    editor_style &= ~(wxBORDER_MASK & ~wxBORDER_NONE);
    m_text = new TextCtrl(this, wxID_ANY, text, wxPoint(FromDIP(kPad), FromDIP(kPad)), wxDefaultSize, editor_style);
    m_text->SetFont(Label::Body_14);
    state_handler.attach_child(m_text);

    m_text->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) { m_focused = true; applyTones(); e.Skip(); });
    m_text->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { m_focused = false; applyTones(); e.Skip(); });

    applyTones();
    layoutEditor();
}

void TextArea::applyTones()
{
    using R = MD3::Role;
    const wxColour fill = StateColor::semantic(m_read_only ? R::SurfaceContainerLow : R::SurfaceContainerLowest);
    SetBackgroundColor(StateColor(std::make_pair(fill, (int) StateColor::Normal)));
    SetBorderColor(StateColor(
        std::make_pair(StateColor::semantic(R::Outline), (int) StateColor::Disabled),
        std::make_pair(StateColor::semantic(m_focused ? R::Primary : R::OutlineVariant), (int) StateColor::Normal)));
    SetBorderWidth(FromDIP(m_focused ? kFocusBorder : kRestBorder));
    if (m_text) {
        m_text->SetBackgroundColour(fill);
        m_text->SetForegroundColour(StateColor::semantic(IsEnabled() ? R::OnSurface : R::Outline));
        m_text->Refresh();
    }
    Refresh();
}

void TextArea::layoutEditor()
{
    if (!m_text) return;
    const int pad = FromDIP(kPad);
    const wxSize client = GetClientSize();
    m_text->SetSize(pad, pad, std::max(0, client.x - 2 * pad), std::max(0, client.y - 2 * pad));
}

void TextArea::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    StaticBox::DoSetSize(x, y, width, height, sizeFlags);
    layoutEditor();
}

wxSize TextArea::DoGetBestSize() const
{
    const int pad = FromDIP(kPad);
    int line_height = FromDIP(20);
    if (m_text) {
        wxClientDC dc(const_cast<wxTextCtrl *>(m_text));
        dc.SetFont(m_text->GetFont());
        line_height = dc.GetCharHeight();
    }
    const wxSize min = GetMinSize();
    const int w = min.x > 0 ? min.x : FromDIP(320);
    const int h = min.y > 0 ? min.y : line_height * m_min_lines + 2 * pad;
    return wxSize(w, h);
}

void TextArea::SetReadOnly(bool read_only)
{
    m_read_only = read_only;
    if (m_text) m_text->SetEditable(!read_only);
    applyTones();
}

void TextArea::SetMonospace(bool monospace)
{
    m_monospace = monospace;
    if (m_text) m_text->SetFont(monospace ? Label::Mono_13 : Label::Body_14);
    InvalidateBestSize();
}

void TextArea::SetMinLines(int lines)
{
    m_min_lines = std::max(1, lines);
    InvalidateBestSize();
}

bool TextArea::SetFont(const wxFont &font)
{
    const bool ok = StaticBox::SetFont(font);
    if (m_text) m_text->SetFont(font);
    InvalidateBestSize();
    return ok;
}

bool TextArea::Enable(bool enable)
{
    const bool ok = StaticBox::Enable(enable);
    if (m_text) m_text->Enable(enable);
    applyTones();
    return ok;
}

void TextArea::Rescale()
{
    SetCornerRadius(FromDIP(MD3::Metrics::radius_tiny));
    if (m_text) m_text->SetFont(m_monospace ? Label::Mono_13 : Label::Body_14);
    applyTones();
    layoutEditor();
}
