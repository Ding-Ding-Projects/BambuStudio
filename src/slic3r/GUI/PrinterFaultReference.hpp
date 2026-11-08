#ifndef slic3r_GUI_PrinterFaultReference_hpp_
#define slic3r_GUI_PrinterFaultReference_hpp_

// The reference line every printer fault surface shows under its own message:
// the error dialog, Health Management items and the Device page's print error.
//
// In a normal installation it is an opaque reference code with no published
// meaning. A personal wording file (Preferences, or a .json dropped on the
// window) may map the code to a phrase of the user's choosing; Label paints that
// phrase through the shared personal-wording path, so the application itself
// never carries the phrase. Clearing the wording file restores the code.

#include <wx/string.h>

class Label;
class wxWindow;

namespace Slic3r { namespace GUI { namespace PrinterFaultReference {

// The code, registered as a trusted display source for personal wording.
wxString code();

// A small secondary-text Label showing code(), with its accessible name set.
::Label *create_label(wxWindow *parent);

}}} // namespace Slic3r::GUI::PrinterFaultReference

#endif
