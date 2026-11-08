#ifndef slic3r_GUI_ElementLocks_LockService_hpp_
#define slic3r_GUI_ElementLocks_LockService_hpp_

// Element locks inside the app: "Lock this element" and "Unlock this
// element..." on the right-click menu of every adopted element (beside Edit
// appearance), an application-wide filter that stops clicks, the wheel and
// keys from reaching a locked element (its right-click menu still opens),
// a small hint saying who locked it and why, the dialogs to lock, to unlock
// with the code or the unlock ladder, and a list of every lock. Locks are kept
// in element-locks.json in the data folder. The rules live in the wx-free
// LockModel.

#include <string>

class wxMenu;
class wxWindow;

namespace Slic3r { namespace GUI { namespace ElementLocks {

void init(const std::string &data_dir);
void shutdown();
bool is_locked(const std::string &element);
// How many elements are locked now (the Local tools page).
int  lock_count();

int  lock_item_id();
// Adds "Lock this element..." or "Unlock this element..." for element_id.
void append_lock_item(wxMenu &menu, const std::string &element_id, wxWindow *anchor);
void remove_lock_item(wxMenu &menu);

bool lock_element(wxWindow *anchor, const std::string &element_id);
bool unlock_element(wxWindow *anchor, const std::string &element_id);
// Every lock, with Unlock beside each.
void open_manager(wxWindow *parent);

}}} // namespace Slic3r::GUI::ElementLocks

#endif // slic3r_GUI_ElementLocks_LockService_hpp_
