#ifndef slic3r_GUI_CanvasDisclosures_hpp_
#define slic3r_GUI_CanvasDisclosures_hpp_

#include <functional>
#include <string>
#include <vector>

#include <wx/gdicmn.h>
#include <wx/string.h>

#include "CollapsibleFilterState.hpp"

class wxWindow;

// Accessibility bridge for collapsible panels drawn with ImGui on a 3D canvas
// (the preview legend and statistics dock, the assembly structure panel).
//
// ImGui draws pixels only, so the canvas window gets an accessible object that
// exposes each published panel header as a child push button: named, reported
// expanded or collapsed, located on screen, activatable by assistive
// technology, and focused while the keyboard path (Shift+L on the canvas)
// owns it.
// Renderers publish their header every frame they draw it and announce each
// toggle; the bridge raises the matching MSAA events.
namespace Slic3r { namespace GUI { namespace CanvasDisclosures {

struct Entry
{
    const void *          owner { nullptr };          // the renderer that drew it
    wxString              name;                       // translated header name
    bool                  expanded { true };          // state at the last draw
    bool                  keyboard_focus { false };   // keyboard focus ring shown
    wxRect                client_rect;                // header, canvas client pixels
    std::function<void()> toggle;                     // activation by assistive technology
};

// Publish (or refresh) the header `id` drawn on `canvas`. The first publish on
// a canvas installs its accessible object.
void publish(wxWindow *canvas, const std::string &id, Entry entry);

// Forget `id` when `owner` goes away; a no-op when another owner republished it.
void withdraw(wxWindow *canvas, const std::string &id, const void *owner);

// Record the new state of `id` and raise the accessibility events of a toggle.
void announce(wxWindow *canvas, const std::string &id, bool expanded, bool keyboard_focus,
              const std::vector<CollapsibleFilters::AnnounceEvent> &events);

// The application config as the store of canvas panel states.
CollapsibleFilters::Section::Read  config_reader();
CollapsibleFilters::Section::Write config_writer();

}}} // namespace Slic3r::GUI::CanvasDisclosures

#endif // slic3r_GUI_CanvasDisclosures_hpp_
