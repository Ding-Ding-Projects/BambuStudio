#pragma once

#include <string_view>

namespace MD3::Motion {
// Unknown or missing settings retain the operating-system default. There is
// deliberately no preference that overrides the system's reduced-motion request.
inline bool reduce_motion(std::string_view preference, bool system_reduced)
{
    return preference == "reduced" || system_reduced;
}

enum class OwnerAction { Animate, Settle, Cancel };
inline OwnerAction owner_action(bool bound, bool alive, bool shown, bool reduce)
{
    if (bound && !alive) return OwnerAction::Cancel;
    if (reduce || (bound && !shown)) return OwnerAction::Settle;
    return OwnerAction::Animate;
}
}
