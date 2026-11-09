#include "ObjectViews.hpp"

#include <algorithm>
#include <cmath>

namespace Slic3r::GUI::ObjectViews {

Look Views::look(std::size_t object_id) const
{
    const auto it = m_looks.find(object_id);
    return it == m_looks.end() ? Look{} : it->second;
}

void Views::set_opacity(std::size_t object_id, double opacity)
{
    Look next = look(object_id);
    if (!std::isfinite(opacity)) return;
#ifdef SLIC3R_OBJECT_VIEWS_NEGATIVE_FIXTURE
    // Test-only mutation (tests/object_views.test.mjs): lets an object turn invisible.
    next.opacity = std::clamp(opacity, 0.0, 1.0);
#else
    next.opacity = std::clamp(std::round(opacity * 100.0) / 100.0, kMinOpacity, 1.0);
#endif
    store(object_id, next);
}

void Views::set_wire(std::size_t object_id, bool wire)
{
    Look next = look(object_id);
    next.wire = wire;
    store(object_id, next);
}

void Views::store(std::size_t object_id, const Look &look)
{
    if (look.plain()) m_looks.erase(object_id);
    else m_looks[object_id] = look;
}

Views &views()
{
    static Views v;
    return v;
}

} // namespace Slic3r::GUI::ObjectViews
