#ifndef slic3r_GUI_DoorChimeCard_hpp_
#define slic3r_GUI_DoorChimeCard_hpp_

// The Door chime card on the Device page: a switch for the feature, a quick
// pick of the chime for the door opening and for it closing, a Preview button
// beside each that plays it on the selected printer, and a line saying whether
// a chime can play right now. Below them, the Door monitor section sets up the
// Printer Door Monitor web app with one click (see DoorMonitorSetup.hpp), which
// keeps the door log and rings the chimes while Bambu Studio is closed.

#include "../Widgets/StaticBox.hpp"

#include <wx/timer.h>

#include <atomic>
#include <memory>

class Button;
class ComboBox;
class Label;
class SwitchButton;
class TextInput;

namespace Slic3r { namespace GUI {

class DoorChimeCard : public StaticBox
{
public:
    explicit DoorChimeCard(wxWindow *parent);
    ~DoorChimeCard() override;

private:
    struct Row
    {
        ComboBox *choice  = nullptr;
        Button   *preview = nullptr;
    };

    Row  add_row(wxSizer *sizer, bool open, const wxString &label);
    void on_choice(bool open);
    void on_preview(bool open);
    void refresh_state();

    wxSizer *build_monitor_section();
    void     on_monitor_target();
    void     on_monitor_setup();
    void     on_monitor_open();
    void     on_monitor_copy_password();
    void     refresh_monitor_buttons();
    void     set_monitor_status(const wxString &status);

    SwitchButton *m_switch = nullptr;
    Row           m_open;
    Row           m_close;
    Label        *m_note = nullptr;
    wxTimer       m_refresh;

    ComboBox *m_monitor_target   = nullptr;
    TextInput *m_monitor_ssh     = nullptr;
    Button   *m_monitor_setup    = nullptr;
    Button   *m_monitor_open     = nullptr;
    Button   *m_monitor_password = nullptr;
    Label    *m_monitor_status   = nullptr;
    bool      m_monitor_busy     = false;
    // Shared with the worker thread, which outlives the card if the window closes mid-setup.
    std::shared_ptr<std::atomic<bool>> m_monitor_cancel = std::make_shared<std::atomic<bool>>(false);
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_DoorChimeCard_hpp_
