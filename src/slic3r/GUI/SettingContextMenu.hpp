#ifndef slic3r_GUI_SettingContextMenu_hpp_
#define slic3r_GUI_SettingContextMenu_hpp_

// Right-click a setting row (its name, its value or the empty space beside
// it), or press Shift+F10 on it, for a Material menu that carries the
// setting's value as a live control (a switch, a choice, a stepper) that
// changes it in place, Back to the preset value, Back to the system value,
// Copy the setting key, Compare with other presets and Explain this (F1).
// Every change goes through the setting's own field, exactly as typing in it
// would, so undo, the modified marks and the preset's changed state follow.

#include <string>

#include <wx/gdicmn.h>

class wxWindow;

namespace Slic3r { namespace GUI {

class OptionsGroup;

namespace SettingContextMenu {

void show(wxWindow *owner, OptionsGroup *group, const std::string &opt_key, const wxPoint &screen_pos);

// The same rows under the edit menu of a setting's text field (Undo, Cut,
// Copy, Paste stay on top). Installed once; later calls do nothing.
void install();

// A card beside the row with what the setting does, its key, range and unit,
// and its current and preset values. Closes on the next click elsewhere.
void explain(wxWindow *owner, OptionsGroup *group, const std::string &opt_key, const wxPoint &screen_pos);

} // namespace SettingContextMenu
}} // namespace Slic3r::GUI

#endif // slic3r_GUI_SettingContextMenu_hpp_
