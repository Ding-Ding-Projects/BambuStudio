#ifndef slic3r_AssemblyExportProgressWindow_hpp_
#define slic3r_AssemblyExportProgressWindow_hpp_

#include <wx/frame.h>
#include "slic3r/GUI/Widgets/ProgressBar.hpp"
#include <wx/string.h>

class wxStaticText;
class wxWindow;

namespace Slic3r {
namespace GUI {

class AssemblyExportProgressWindow : public wxFrame
{
public:
    explicit AssemblyExportProgressWindow(wxWindow *parent);

    void update_progress(const wxString &message, int value, int maximum, wxWindow *anchor);
    void position_near_anchor(wxWindow *anchor);

private:
    wxStaticText *m_message{nullptr};
    ProgressBar      *m_gauge{nullptr};
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_AssemblyExportProgressWindow_hpp_
