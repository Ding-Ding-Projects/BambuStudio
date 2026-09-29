#include "TextArea.hpp"

#include "Label.hpp"
#include "StateColor.hpp"
#include "TextCtrl.h"

#include <wx/dcclient.h>

#include <algorithm>

#ifdef __WXMSW__
#include <windows.h>
#endif

namespace {
constexpr int kPad = 8;          // inset from the outline to the editor, in DIP
constexpr int kRestBorder = 1;   // OutlineVariant at rest
constexpr int kFocusBorder = 2;  // Primary while the editor has focus

#ifdef __WXMSW__
// Messages after which the edit control's view cannot have moved: queries,
// painting, hit testing, and the frame's own recalculation (the strips are
// resized there, never read back from).
bool leaves_view_alone(WXUINT msg)
{
    switch (msg) {
    case WM_NCCREATE: case WM_CREATE: case WM_DESTROY: case WM_NCDESTROY:
    case WM_NCCALCSIZE: case WM_NCHITTEST: case WM_SETCURSOR: case WM_NCMOUSEMOVE: case WM_NCMOUSELEAVE:
    case WM_PAINT: case WM_NCPAINT: case WM_ERASEBKGND: case WM_GETDLGCODE: case WM_GETOBJECT:
    case WM_GETTEXT: case WM_GETTEXTLENGTH: case WM_GETFONT: case WM_WINDOWPOSCHANGING:
    case EM_GETFIRSTVISIBLELINE: case EM_GETLINECOUNT: case EM_GETRECT: case EM_GETSEL: case EM_GETLINE:
    case EM_LINEINDEX: case EM_LINELENGTH: case EM_LINEFROMCHAR: case EM_POSFROMCHAR: case EM_CHARFROMPOS:
        return true;
    default:
        return false;
    }
}
#endif
} // namespace

// ---------------------------------------------------------------------------
// TextAreaEditor

TextAreaEditor::TextAreaEditor(wxWindow *parent, wxWindowID id, const wxString &value, const wxPoint &pos,
                               const wxSize &size, long style)
{
#ifdef __WXMSW__
    m_bars.SetScrollHandler([this](int orient, wxEventType type, int position) { scroll(orient, type, position); });
#endif
    // Created here rather than through the base constructor, so that Create()
    // already runs with this class's MSWGetStyle(): the edit control never
    // gets a scrollbar of its own.
    Create(parent, id, value, pos, size, style);
#ifdef __WXMSW__
    sync_bars();
#endif
}

TextAreaEditor::~TextAreaEditor()
{
#ifdef __WXMSW__
    m_bars.Abandon();
#endif
}

#ifdef __WXMSW__
WXDWORD TextAreaEditor::MSWGetStyle(long flags, WXDWORD *exstyle) const
{
    // ES_AUTOVSCROLL stays, so the text still follows the caret.
    WXDWORD style = MD3ScrollBars::WithoutNativeBars(wxTextCtrl::MSWGetStyle(flags, exstyle));
    // wx asks a rich edit control to disable rather than hide its bar
    // (ES_DISABLENOSCROLL, 0x2000 in richedit.h), which would bring a bar back.
    // A plain edit control uses the same bit for ES_NUMBER, so only a rich one
    // loses it.
    if (flags & (wxTE_RICH | wxTE_RICH2))
        style &= ~static_cast<WXDWORD>(0x00002000);
    return style;
}

WXHBRUSH TextAreaEditor::DoMSWControlColor(WXHDC pDC, wxColour colBg, WXHWND hWnd)
{
    return wxTextCtrl::DoMSWControlColor(pDC, wxColour(), hWnd);
}

WXLRESULT TextAreaEditor::MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam)
{
    WXLRESULT result = 0;
    if (m_bars.Before(msg, wParam, lParam, result))
        return result;
    result = wxTextCtrl::MSWWindowProc(msg, wParam, lParam);
    m_bars.After(msg, wParam, lParam, result);
    if (!m_syncing && !leaves_view_alone(msg))
        sync_bars();
    return result;
}

void TextAreaEditor::sync_bars()
{
    HWND hwnd = (HWND) GetHWND();
    if (m_syncing || hwnd == nullptr)
        return;
    m_syncing = true;
    // A shown or hidden bar changes the width, which can rewrap the text and
    // change the counts again; settle within a few passes, as a native bar does.
    for (int pass = 0; pass < 3; ++pass) {
        const int first = static_cast<int>(::SendMessage(hwnd, EM_GETFIRSTVISIBLELINE, 0, 0));
        const int count = static_cast<int>(::SendMessage(hwnd, EM_GETLINECOUNT, 0, 0));
        RECT rc { 0, 0, 0, 0 };
        ::SendMessage(hwnd, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&rc));
        const int line = std::max(1, GetCharHeight());
        const int page = std::max(1, static_cast<int>(rc.bottom - rc.top) / line);
        if (first == m_first && count == m_count && page == m_page)
            break;
        m_first = first;
        m_count = count;
        m_page  = page;
        // Shown only while there is more text than fits, like the native bar.
        m_bars.SetScrollbar(wxVERTICAL, first, page, count > page ? count : 0, true);
    }
    m_syncing = false;
}

void TextAreaEditor::scroll(int orient, wxEventType type, int pos)
{
    HWND hwnd = (HWND) GetHWND();
    if (orient != wxVERTICAL || hwnd == nullptr)
        return;
    if (type == wxEVT_SCROLLWIN_THUMBTRACK || type == wxEVT_SCROLLWIN_THUMBRELEASE) {
        const int first = static_cast<int>(::SendMessage(hwnd, EM_GETFIRSTVISIBLELINE, 0, 0));
        if (pos != first)
            ::SendMessage(hwnd, EM_LINESCROLL, 0, pos - first);
    } else if (type == wxEVT_SCROLLWIN_PAGEUP || type == wxEVT_SCROLLWIN_PAGEDOWN) {
        ::SendMessage(hwnd, EM_SCROLL, type == wxEVT_SCROLLWIN_PAGEUP ? SB_PAGEUP : SB_PAGEDOWN, 0);
    } else if (type == wxEVT_SCROLLWIN_LINEUP || type == wxEVT_SCROLLWIN_LINEDOWN) {
        ::SendMessage(hwnd, EM_SCROLL, type == wxEVT_SCROLLWIN_LINEUP ? SB_LINEUP : SB_LINEDOWN, 0);
    }
    sync_bars();
}
#endif // __WXMSW__

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
    m_text = new TextAreaEditor(this, wxID_ANY, text, wxPoint(FromDIP(kPad), FromDIP(kPad)), wxDefaultSize, editor_style);
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
