#ifndef slic3r_GUI_ElementLocks_LockService_hpp_
#define slic3r_GUI_ElementLocks_LockService_hpp_

// Element locks inside the app: "Lock this element" and "Unlock this
// element..." on the right-click menu of every adopted element (beside Edit
// appearance), an application-wide filter that stops clicks, the wheel and
// keys from reaching a locked element (its right-click menu still opens),
// a small hint saying who locked it and why, a lock badge on the locked
// element, the dialogs to lock, to unlock with the code or the unlock ladder,
// and a list of every lock. Rows can be locked too: one ink row of the
// sidebar (a lock identity that follows the row's number), and one object of
// the object list, which then stays out of the selection in the list and in
// the 3D view, is arranged around and is not deleted by Delete all. Locks are
// kept in element-locks.json in the data folder. The rules live in the
// wx-free LockModel.

#include <functional>
#include <string>

#include <wx/gdicmn.h>
#include <wx/string.h>

class wxMenu;
class wxWindow;

namespace Slic3r { namespace GUI { namespace ElementLocks {

void init(const std::string &data_dir);
void shutdown();
bool is_locked(const std::string &element);
// How many elements are locked now (the Local tools page).
int  lock_count();

// What a lock is on, and what the dialogs and the hint call it.
struct Identity
{
    std::string id;
    wxString    name;
};
// The lock of one object (by its name) and of one ink row (1 for the first).
std::string object_lock_id(const std::string &object_name);
std::string ink_row_lock_id(int slot);
bool        object_locked(const std::string &object_name);
// A lock identity worked out when it is needed, for a window styled together
// with its siblings (one ink row of several): it follows the row's number.
void set_lock_identity(wxWindow *window, std::function<Identity()> identity);
// For a window that holds rows (the object list, the 3D view): the lock of
// the row under a point on the screen, or an empty identity. Clicks on a
// locked row are stopped with the hint; a right-click offers only Unlock.
void set_row_lookup(wxWindow *window, std::function<Identity(const wxPoint &screen)> lookup);
// What an id is called, for the dialogs and the hint.
void remember_name(const std::string &id, const wxString &name);
// Called whenever a lock is added or removed.
int  subscribe(std::function<void()> changed);
void unsubscribe(int token);
// The short note a click on a locked element shows: who locked it and why.
void show_hint(wxWindow *anchor, const std::string &id);

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
