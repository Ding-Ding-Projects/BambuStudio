#ifndef slic3r_ExternalEditor_hpp_
#define slic3r_ExternalEditor_hpp_

#include <string>
#include <vector>

namespace Slic3r {
namespace GUI {

// An external text/code editor installed on this machine.
struct FoundEditor
{
    // User-facing friendly name, also used as the identifier stored in AppConfig.
    std::string name;
    // Absolute path to the launchable executable, UTF-8 encoded.
    std::string exe_path;
};

// Enumerate the external editors installed on this machine.
// On Windows the well-known editors are looked up through their uninstall registry keys
// (HKCU + HKLM, including the WOW6432Node view) with a PATH scan as fallback, on macOS
// well-known /Applications bundles and the PATH are probed, on Linux well-known install
// locations and the PATH are probed.
// The detection result is cached for the lifetime of the process. Never throws.
std::vector<FoundEditor> get_available_editors();

// Find an installed editor by its friendly name. Falls back to the first available
// editor when the name is empty or not found, and to an empty FoundEditor (both
// members empty) when no editor is installed at all. Never throws.
FoundEditor find_editor_or_default(const std::string &name);

// Dedicated export handoff. Never substitutes an unrelated editor.
FoundEditor find_visual_studio_code();
enum class VSCodeOpenResult { Opened, NotInstalled, InvalidTarget, LaunchFailed };
VSCodeOpenResult open_export_in_visual_studio_code(const std::string &target_utf8);

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_ExternalEditor_hpp_
