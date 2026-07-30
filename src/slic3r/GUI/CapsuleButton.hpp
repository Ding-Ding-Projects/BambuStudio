#ifndef CAPSULE_BUTTON_HPP
#define CAPSULE_BUTTON_HPP

#include "wxExtensions.hpp"
#include "Widgets/Label.hpp"

namespace Slic3r { namespace GUI {
class CapsuleButton : public wxPanel
{
public:
    CapsuleButton(wxWindow *parent, wxWindowID id, const wxString &label, bool selected);
    void Select(bool selected);
    bool IsSelected() const { return m_selected; }

    bool AcceptsFocusFromKeyboard() const override { return IsEnabled() && IsShown(); }
    bool AccessibilityActivate();

protected:
    void OnPaint(wxPaintEvent &event);
#ifdef __WIN32__
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

private:
    void OnEnterWindow(wxMouseEvent &event);
    void OnLeaveWindow(wxMouseEvent &event);
    void OnKeyDown(wxKeyEvent &event);
    void OnKeyUp(wxKeyEvent &event);
    void SendButtonEvent();
    void UpdateStatus();

    wxBitmapButton *m_btn;
    Label          *m_label;

    wxBitmap tag_on_bmp;
    wxBitmap tag_off_bmp;

    bool m_hovered;
    bool m_selected;
    bool m_key_pressed{false};
};
}} // namespace Slic3r::GUI

#endif