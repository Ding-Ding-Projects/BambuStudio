#ifndef slic3r_GUI_WindowsNativeVisualSmoke_hpp_
#define slic3r_GUI_WindowsNativeVisualSmoke_hpp_

namespace Slic3r::GUI {

enum class WindowsNativeVisualSmokeResult { NotRequested, Started, Rejected };

// Starts a deliberately isolated wxWidgets window when the Windows CI visual
// smoke environment is present. The normal application startup path is not
// entered for either Started or Rejected.
WindowsNativeVisualSmokeResult try_start_windows_native_visual_smoke();

} // namespace Slic3r::GUI

#endif // slic3r_GUI_WindowsNativeVisualSmoke_hpp_
