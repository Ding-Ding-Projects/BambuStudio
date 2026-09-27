#ifndef slic3r_GUI_SafetyOptionsDialog_hpp_
#define slic3r_GUI_SafetyOptionsDialog_hpp_

#include <wx/wx.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/popupwin.h>
#include <wx/tipwin.h>

#include "GUI_Utils.hpp"
#include "wxExtensions.hpp"
#include "DeviceManager.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/StaticLine.hpp"
#include "Widgets/ComboBox.hpp"

// Previous definitions
class SwitchBoard;

namespace Slic3r { namespace GUI {

// MD3 snackbar (ui-md3 containment/Snackbar): an InverseSurface card carrying an
// InversePrimary leading mark, InverseOn body text, 12px corners and the kit's
// 16/12/10 insets, every one of them FromDIP so the card keeps its proportions
// at 150/200% display scale.
//
// It is a dialog-local popup rather than a NotificationManager toast because
// Safety Options runs MODAL over the Device tab: the plater's GL-canvas snackbar
// stack is neither rendered nor reachable from there, so a notification pushed to
// it would be queued and never seen.
class SafetyOptionToast : public wxPopupWindow
{
public:
    SafetyOptionToast(wxWindow *parent, const wxString &text);

private:
    void OnPaint(wxPaintEvent &evt);
    void ApplyShape();

    // A real wxStaticText, not painted text, so the message keeps its node in
    // the accessibility tree and can be read out.
    Label *m_label{nullptr};
};

class SafetyOptionsDialog : public DPIDialog
{
protected:
    // settings
    wxScrolledWindow* m_scrollwindow;

    CheckBox*    m_cb_open_door;
    CheckBox*    m_cb_idel_heating_protection;
    Label*       m_text_open_door;
    Label*       m_text_idel_heating_protection;
    Label*       m_text_idel_heating_protection_caption;
    SwitchBoard* m_open_door_switch_board;
    wxPanel*    m_idel_heating_container { nullptr };

    // toast for idle heating unavailable
    SafetyOptionToast *m_idel_heating_toast{nullptr};
    wxTimer      m_idel_heating_toast_timer;
    bool         m_idel_protect_unavailable { false };

    wxBoxSizer* create_settings_group(wxWindow* parent);
    bool print_halt = false;

public:
    SafetyOptionsDialog(wxWindow* parent);
    ~SafetyOptionsDialog();
    void on_dpi_changed(const wxRect &suggested_rect) override;

    MachineObject *obj { nullptr };

    void             update_options(MachineObject *obj_);
    void             update_machine_obj(MachineObject *obj_);
    bool             Show(bool show) override;

private:
    void updateOpenDoorCheck(MachineObject *obj);
    void updateIdelHeatingProtect(MachineObject *obj);
    void show_idel_heating_toast(const wxString &text);
    void dismiss_idel_heating_toast();
};

}} // namespace Slic3r::GUI

#endif