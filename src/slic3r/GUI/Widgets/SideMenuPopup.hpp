#ifndef slic3r_GUI_SideMenuPopup_hpp_
#define slic3r_GUI_SideMenuPopup_hpp_

#include <wx/stattext.h>
#include <wx/vlbox.h>
#include <wx/combo.h>
#include <wx/htmllbox.h>
#include <wx/frame.h>
#include <wx/weakref.h>
#include "../wxExtensions.hpp"
#include "StateHandler.hpp"
#include "StateColor.hpp"
#include "SideButton.hpp"
#include "PopupWindow.hpp"

class SidePopup : public PopupWindow
{
private:
	std::vector<SideButton*> btn_list;
    wxWeakRef<wxWindow>     invoker;
    bool                    restoring_focus = false;
    bool                    dismissing = false;

    // MD3 floating surface, same idiom as the ComboBox popup in DropDown.cpp:
    // a SurfaceContainer fill inside a 1px OutlineVariant frame at the kit
    // popover radius. border_color holds the raw LIGHT role value because that
    // hex is a gDarkColors key -- darkModeColorFor() remaps it on every paint,
    // so the menu follows a runtime dark-mode toggle instead of freezing at the
    // theme that was in force when the popup was constructed.
    double   radius = 0.0;
    wxColour border_color;
public:
    SidePopup(wxWindow* parent);
    ~SidePopup();

    void Create();

    virtual void Popup(wxWindow *focus = NULL) wxOVERRIDE;
    virtual void OnDismiss() wxOVERRIDE;
    virtual bool ProcessLeftDown(wxMouseEvent& event) wxOVERRIDE;
    virtual bool Show(bool show = true) wxOVERRIDE;

    void append_button(SideButton* btn);

    void paintEvent(wxPaintEvent& evt);
    void keyDown(wxKeyEvent& event);

private:
    void focusBoundaryButton(bool first);
    void focusRelativeButton(SideButton* current, int direction);
    void restoreInvokerFocus();

	DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_Button_hpp_
