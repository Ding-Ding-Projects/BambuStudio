#ifndef slic3r_GUI_StateHandler_hpp_
#define slic3r_GUI_StateHandler_hpp_

#include <memory>
#include <wx/event.h>

#include "StateColor.hpp"
#include "MD3Motion.hpp"
#include <map>

wxDECLARE_EVENT(EVT_ENABLE_CHANGED, wxCommandEvent);

class StateHandler : public wxEvtHandler
{
public:
    enum State {
        Enabled = 1,
        Checked = 2,
        Focused = 4,
        Hovered = 8,
        Pressed = 16,
        Disabled = 1 << 16,
        NotChecked = 2 << 16,
        NotFocused = 4 << 16,
        NotHovered = 8 << 16,
        NotPressed = 16 << 16,
    };

public:
    StateHandler(wxWindow * owner);

    ~StateHandler();

public:
    void attach(StateColor const & color);

    void attach(std::vector<StateColor const *> const & colors);

    void attach_child(wxWindow *child);

    void remove_child(wxWindow *child);

    void update_binds();

    int states() const { return states_ | states2_; }

    void set_state(int state, int mask);

    // Paint-only view. states() and all control actions remain synchronous.
    wxColour colorFor(StateColor const &color) const;

private:
    StateHandler(StateHandler * parent, wxWindow *owner);

    void changed(wxEvent &event);

    void changed(int state2);
    void transition(int previous_states);
    wxColour colorFor(StateColor const &color, int states) const;

private:
    wxWeakRef<wxWindow> owner_;
    std::vector<StateColor const *> colors_;
    int bind_states_ = 0;
    int states_ = 0;
    int states2_ = 0; // from children
    std::vector<std::unique_ptr<StateHandler>> children_;
    StateHandler * parent_ = nullptr;
    mutable bool motion_used_ = false;
    double progress_ = 1.0;
    std::map<StateColor const *, wxColour> from_colors_;
    MD3::Motion::Anim motion_;
};

#endif // !slic3r_GUI_StateHandler_hpp_
