#ifndef slic3r_GUI_DoorChimeCard_hpp_
#define slic3r_GUI_DoorChimeCard_hpp_

// The Door chime card on the Device page: a switch for the feature, a quick
// pick of the chime for the door opening and for it closing, a Preview button
// beside each that plays it on the selected printer, and a line saying whether
// a chime can play right now.

#include "../Widgets/StaticBox.hpp"

#include <wx/timer.h>

class Button;
class ComboBox;
class Label;
class SwitchButton;

namespace Slic3r { namespace GUI {

class DoorChimeCard : public StaticBox
{
public:
    explicit DoorChimeCard(wxWindow *parent);

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

    SwitchButton *m_switch = nullptr;
    Row           m_open;
    Row           m_close;
    Label        *m_note = nullptr;
    wxTimer       m_refresh;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_DoorChimeCard_hpp_
