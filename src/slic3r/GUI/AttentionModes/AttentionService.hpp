#ifndef slic3r_GUI_AttentionModes_AttentionService_hpp_
#define slic3r_GUI_AttentionModes_AttentionService_hpp_

// Attention modes inside the app (Preferences > Attention modes), each off by
// default and independent:
//   Focus            while something is selected in Prepare, the rest of the
//                    plate steps back (drawn muted, never hidden);
//   Low stimulation  less motion (on top of the Windows setting), no action
//                    sounds, no dim sum surprise at start-up;
//   Time awareness   the strip under the title bar states how long this
//                    session has been open and how long since the last change;
//   One thing at a time  the strip shows the one next step you chose, kept
//                    across restarts until you mark it done;
//   Momentum         a quiet notification when nothing has changed for a set
//                    time, once per stretch, with "Not now" that lasts the
//                    stated period.
// The rules live in the wx-free AttentionModel.

class wxWindow;

namespace Slic3r { namespace GUI { namespace AttentionModes {

// Starts the session clock and the strip's updates. Call once the main frame exists.
void init();
void shutdown();
// A change to the project (every undo snapshot).
void note_change();
// Read by the 3D view on every frame.
bool focus_on();
// Turns Focus on or off as Preferences would (the 3D view's menu offers it).
void set_focus(bool on);
// The strip under the title bar; it shows itself only while a mode needs it.
wxWindow *create_strip(wxWindow *parent);
// Preferences on the Attention modes section.
void open_preferences();

}}} // namespace Slic3r::GUI::AttentionModes

#endif // slic3r_GUI_AttentionModes_AttentionService_hpp_
