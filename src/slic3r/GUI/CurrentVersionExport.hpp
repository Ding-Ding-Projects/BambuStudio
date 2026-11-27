#pragma once

#include <filesystem>

namespace Slic3r::GUI {

// Preserve the open archive and its embedded history, including through hard links.
inline bool current_version_export_destination_allowed(const std::filesystem::path &project,
                                                       const std::filesystem::path &destination)
{
    if (destination.empty()) return false;
    if (project.empty()) return true;
    if (destination.lexically_normal() == project.lexically_normal()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(destination, ec)) return !ec;
    if (!std::filesystem::exists(project, ec)) return !ec;
    const bool same = std::filesystem::equivalent(project, destination, ec);
    return !ec && !same;
}

} // namespace Slic3r::GUI
