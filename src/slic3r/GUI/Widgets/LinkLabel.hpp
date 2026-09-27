#ifndef slic3r_GUI_LinkLabel_hpp_
#define slic3r_GUI_LinkLabel_hpp_

#include <wx/panel.h>

#include "Label.hpp"

wxDECLARE_EVENT(EVT_LINK_LABEL_LEFT_DOWN, wxCommandEvent);

class LinkLabel : public wxWindow
{
private:
    wxString m_url;
    Label *  m_txt{nullptr};
    wxPanel* m_underline{nullptr};

public:
    LinkLabel(wxWindow *parent, wxString const &text, std::string url, long style = 0, wxSize size = wxDefaultSize);
    ~LinkLabel() {};

    void link(wxMouseEvent &evt);
    Label *getLabel(){return m_txt;};
    void setLinkUrl(wxString url);
    void setLabel(wxString label);
    bool SeLinkLabelFColour(const wxColour &colour);
    bool SeLinkLabelBColour(const wxColour &colour);

    bool AcceptsFocus() const override;
    bool AcceptsFocusFromKeyboard() const override;
    void AccessibilityActivate();
    void SetName(const wxString& name) override;

private:
    void activate();
    void keyDown(wxKeyEvent &event);
    void focusChanged(wxFocusEvent &event);
    void paintFocus(wxPaintEvent &event);
};

#endif // !slic3r_GUI_LinkLabel_hpp_
