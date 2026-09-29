#ifndef slic3r_GUI_TextArea_hpp_
#define slic3r_GUI_TextArea_hpp_

#include <wx/textctrl.h>

#include "MD3ScrollBars.hpp"
#include "StaticBox.hpp"

// The multi-line editor with the kit scrollbar instead of the Windows one. The
// Windows edit control inside keeps scrolling itself (the caret, the wheel, the
// keyboard, programmatic scrolling), but is created without WS_VSCROLL and
// WS_HSCROLL, so it neither shows nor sets a bar of its own. After every
// message that can move the text, the editor reads the first visible line,
// the line count and the lines that fit, and hands them to MD3ScrollBars,
// which draws the kit strip in the editor's non-client area; a drag or a page
// on the strip scrolls the edit control by lines. It is a wxTextCtrl, so every
// caller keeps the whole wxTextCtrl interface. TextArea uses it, and so does
// any other multi-line text box.
class TextAreaEditor : public wxTextCtrl
{
public:
    TextAreaEditor(wxWindow        *parent,
                   wxWindowID       id,
                   const wxString  &value = wxEmptyString,
                   const wxPoint   &pos   = wxDefaultPosition,
                   const wxSize    &size  = wxDefaultSize,
                   long             style = wxTE_MULTILINE);
    ~TextAreaEditor() override;

    // Whether the kit bar for orient (wxVERTICAL or wxHORIZONTAL) is shown.
    bool IsBarShown(int orient) const { return m_bars.IsShown(orient); }

#ifdef __WXMSW__
    WXDWORD   MSWGetStyle(long flags, WXDWORD *exstyle = nullptr) const override;
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;

protected:
    // As the kit TextCtrl: the window's own background, never a colour passed in.
    WXHBRUSH DoMSWControlColor(WXHDC pDC, wxColour colBg, WXHWND hWnd) override;

private:
    void sync_bars();
    void scroll(int orient, wxEventType type, int pos);

    int  m_first { -1 }; // last reported first visible line, line count and lines that fit
    int  m_count { -1 };
    int  m_page { -1 };
    bool m_syncing { false };
#endif

private:
    MD3ScrollBars m_bars { this };
};

// The kit multi-line text field. Every stock multi-line wxTextCtrl view in the
// GUI (changelogs, logs, JSON dumps, diffs, scripts, comments) now lives inside
// this: an outlined MD3 container (OutlineVariant 1 px at rest, Primary 2 px
// while the editor has focus, radius_tiny corners, SurfaceContainerLowest for
// editable / SurfaceContainerLow for read-only) hosting a borderless native
// editor reached through GetTextCtrl(). It follows the shape TextInput uses
// for single-line fields, so callers keep every wxTextCtrl capability
// (SetStyle, AppendText, wxTE_RICH, scrolling) through the inner control while
// the chrome is the kit's.
class TextArea : public wxNavigationEnabled<StaticBox>
{
public:
    TextArea();
    TextArea(wxWindow *parent, const wxString &text = wxString(), const wxSize &size = wxDefaultSize,
             long style = 0);
    void Create(wxWindow *parent, const wxString &text = wxString(), const wxSize &size = wxDefaultSize,
                long style = 0);

    wxTextCtrl       *GetTextCtrl() { return m_text; }
    wxTextCtrl const *GetTextCtrl() const { return m_text; }

    wxString GetValue() const { return m_text->GetValue(); }
    void     SetValue(const wxString &value) { m_text->SetValue(value); }
    void     AppendText(const wxString &text) { m_text->AppendText(text); }
    void     Clear() { m_text->Clear(); }

    // Read-only fields use the lower-emphasis container tone and refuse edits.
    void SetReadOnly(bool read_only);
    bool IsReadOnly() const { return m_read_only; }
    // Technical text (JSON, logs, scripts) reads in Roboto Mono.
    void SetMonospace(bool monospace);
    // The height the field asks for when its parent does not size it.
    void SetMinLines(int lines);

    bool SetFont(const wxFont &font) override;
    bool Enable(bool enable = true) override;
    void Rescale();

protected:
    void   DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO) override;
    wxSize DoGetBestSize() const override;

private:
    void applyTones();
    void layoutEditor();

    wxTextCtrl *m_text { nullptr };
    bool        m_read_only { false };
    bool        m_monospace { false };
    bool        m_focused { false };
    int         m_min_lines { 4 };
};

#endif // slic3r_GUI_TextArea_hpp_
