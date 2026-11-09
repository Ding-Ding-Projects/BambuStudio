#ifndef slic3r_GUI_ViewTools_ObjectViews_hpp_
#define slic3r_GUI_ViewTools_ObjectViews_hpp_

// How one object is drawn in the Prepare 3D view, chosen from its right-click
// menu: Solid or Wire, and its opacity. It is a way of looking, not a change
// to the object: nothing is saved in the project, and an object that is
// solid and fully opaque is forgotten. Kept by the object's id for this
// session. The rules live here without wxWidgets
// (tests/object_views_test.cpp); 3DScene draws them.

#include <cstddef>
#include <map>

namespace Slic3r::GUI::ObjectViews {

constexpr double kMinOpacity = 0.1; // never invisible: a hidden object could not be found again

struct Look
{
    double opacity = 1.0;
    bool   wire    = false;
    bool   plain() const { return !wire && opacity >= 1.0; }
};

class Views
{
public:
    Look   look(std::size_t object_id) const;
    // Kept between kMinOpacity and 1; whole percent steps.
    void   set_opacity(std::size_t object_id, double opacity);
    void   set_wire(std::size_t object_id, bool wire);
    void   reset(std::size_t object_id) { m_looks.erase(object_id); }
    void   clear() { m_looks.clear(); }
    bool   empty() const { return m_looks.empty(); }
    std::size_t size() const { return m_looks.size(); }

private:
    void                         store(std::size_t object_id, const Look &look);
    std::map<std::size_t, Look> m_looks;
};

// The one set of views the app draws.
Views &views();

} // namespace Slic3r::GUI::ObjectViews

#endif // slic3r_GUI_ViewTools_ObjectViews_hpp_
