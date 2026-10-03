#include <wx/window.h>
#include "StateHandler.hpp"
#include "StateMotionMath.hpp"

wxDEFINE_EVENT(EVT_ENABLE_CHANGED, wxCommandEvent);

StateHandler::StateHandler(wxWindow * owner)
    : owner_(owner)
{
    owner_->PushEventHandler(this);
    if (owner->IsEnabled())
        states_ |= Enabled;
    if (owner->HasFocus())
        states_ |= Focused;
}

StateHandler::~StateHandler()
{
    motion_.Stop();
    if (owner_) owner_->RemoveEventHandler(this);
}

wxColour StateHandler::colorFor(StateColor const &color, int states) const
{
    const wxColour target = color.colorForStates(states);
    const auto found = from_colors_.find(&color);
    if (found == from_colors_.end() || progress_ >= 1.0 || MD3::Motion::reduced() ||
        !owner_ || !owner_->IsShownOnScreen() || !target.IsOk() || !found->second.IsOk()) return target;
    const auto &from = found->second;
    return wxColour(MD3::Motion::color_channel(from.Red(), target.Red(), progress_),
                    MD3::Motion::color_channel(from.Green(), target.Green(), progress_),
                    MD3::Motion::color_channel(from.Blue(), target.Blue(), progress_),
                    MD3::Motion::color_channel(from.Alpha(), target.Alpha(), progress_));
}

wxColour StateHandler::colorFor(StateColor const &color) const
{
    motion_used_ = true;
    return colorFor(color, states());
}

void StateHandler::transition(int previous_states)
{
    if (!owner_) return;
    if (!motion_used_) { owner_->Refresh(); return; }
    // Snapshot the currently painted colors before replacing the transition.
    // Mid-flight reversals therefore start at the visible value, not an endpoint.
    std::map<StateColor const *, wxColour> starts;
    for (const auto *color : colors_) starts[color] = colorFor(*color, previous_states);
    from_colors_ = std::move(starts);
    progress_ = 0.0;
    motion_.Play(MD3::Motion::short2, [this](double t) {
        progress_ = t;
        if (owner_) owner_->Refresh(false);
    }, nullptr, &MD3::Motion::easeStandard, owner_.get());
}

void StateHandler::attach(StateColor const &color)
{
    colors_.push_back(&color);
}

void StateHandler::attach(std::vector<StateColor const *> const & colors)
{
    colors_.insert(colors_.end(), colors.begin(), colors.end());
}

void StateHandler::attach_child(wxWindow *child)
{
    auto ch = new StateHandler(this, child);
    children_.emplace_back(ch);
    ch->update_binds();
    states2_ |= ch->states();
}

void StateHandler::remove_child(wxWindow *child)
{
    children_.erase(std::remove_if(children_.begin(), children_.end(),
            [child](auto &c) { return c->owner_ == child; }), children_.end());
    states2_ = 0;
    for (auto & c : children_) states2_ |= c->states();
}

void StateHandler::update_binds()
{
    int bind_states = parent_ ? (parent_->bind_states_ & ~Enabled) : 0;
    for (auto c : colors_) {
        bind_states |= c->states();
    }
    bind_states = bind_states | (bind_states >> 16);
    int diff = bind_states ^ bind_states_;
    State       states[] = {Enabled, Checked, Focused, Hovered, Pressed};
    wxEventType events[] = {EVT_ENABLE_CHANGED, wxEVT_CHECKBOX, wxEVT_SET_FOCUS, wxEVT_ENTER_WINDOW, wxEVT_LEFT_DOWN};
    wxEventType events2[] = {{0}, {0}, wxEVT_KILL_FOCUS, wxEVT_LEAVE_WINDOW, wxEVT_LEFT_UP};
    for (int i = 0; i < 5; ++i) {
        int s = states[i];
        if (diff & s) {
            if (bind_states & s) {
                Bind(events[i], &StateHandler::changed, this);
                if (events2[i])
                    Bind(events2[i], &StateHandler::changed, this);
            } else {
                Unbind(events[i], &StateHandler::changed, this);
                if (events2[i])
                    owner_->Unbind(events2[i], &StateHandler::changed, this);
            }
        }
    }
    bind_states_ = bind_states;
    for (auto &c : children_) c->update_binds();
}

void StateHandler::set_state(int state, int mask)
{
    if ((states_ & mask) == (state & mask)) return;
    int old = states_;
    states_ = states_ & ~mask | state & mask;
    if (old != states_ && (old | states2_) != (states_ | states2_)) {
        if (parent_)
            parent_->changed(states_ | states2_);
        else
            transition(old | states2_);
    }
}

StateHandler::StateHandler(StateHandler *parent, wxWindow *owner)
    : StateHandler(owner)
{
    states_ &= ~Enabled;
    parent_ = parent;
}

void StateHandler::changed(wxEvent &event)
{
    event.Skip();
    wxEventType events[] = {EVT_ENABLE_CHANGED, wxEVT_CHECKBOX, wxEVT_SET_FOCUS, wxEVT_ENTER_WINDOW, wxEVT_LEFT_DOWN};
    wxEventType events2[] = {{0}, {0}, wxEVT_KILL_FOCUS, wxEVT_LEAVE_WINDOW, wxEVT_LEFT_UP};
    int old = states_;
    // some events are from another window (ex: text_ctrl of TextInput), save state in states2_ to avoid conflicts
    for (int i = 0; i < 5; ++i) {
        if (events2[i]) {
            if (event.GetEventType() == events[i]) {
                states_ |= 1 << i;
                break;
            } else if (event.GetEventType() == events2[i]) {
                states_ &= ~(1 << i);
                break;
            }
        }
        else {
            if (event.GetEventType() == events[i]) {
                states_ ^= (1 << i);
                break;
            }
        }
    }
    if (old != states_ && (old | states2_) != (states_ | states2_)) {
        if (parent_)
            parent_->changed(states_ | states2_);
        else
            transition(old | states2_);
    }
}

void StateHandler::changed(int)
{
    int old = states2_;
    states2_ = 0;
    for (auto &c : children_) states2_ |= c->states();
    if (old != states2_ && (old | states_) != (states_ | states2_)) {
        if (parent_)
            parent_->changed(states_ | states2_);
        else
            transition(old | states_);
    }
}
