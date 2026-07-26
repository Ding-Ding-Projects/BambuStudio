#pragma once

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>

#include "Widgets/MD3Dialog.hpp"

class Button;

namespace Slic3r {

class MachineObject;
namespace GUI {

// Migrated onto the shared MD3Dialog shell (borderless rounded surface, header
// icon tile + title, footer flex-end). Uses the Device (teal) color scheme
// because this is a live device-monitoring flow.
class ThermalPreconditioningDialog : public MD3Dialog
{
public:
    ThermalPreconditioningDialog(wxWindow *parent, std::string dev_id, bool is_show_remain_time);
    ~ThermalPreconditioningDialog();

    void update_thermal_remaining_time();

private:
    void create_ui();
    void on_ok_clicked(wxCommandEvent &event);
    void on_timer(wxTimerEvent &event);

    std::string     m_dev_id;
    wxTimer        *m_refresh_timer;
    wxStaticText   *m_remaining_time_label;
    wxStaticText   *m_explanation_label;
    Button         *m_ok_button;
    wxStaticBitmap *m_title_bitmap;

    DECLARE_EVENT_TABLE()
};

} // namespace GUI
} // namespace Slic3r