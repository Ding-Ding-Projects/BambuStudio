#ifndef slic3r_GUI_PreferencesHistory_hpp_
#define slic3r_GUI_PreferencesHistory_hpp_

#include <filesystem>

namespace Slic3r {

class ProjectHistoryManager;

namespace GUI { namespace PreferencesHistory {

// Automatic, local Git history for the preferences file: every successful
// AppConfig::save() schedules a debounced snapshot of `BambuStudio.conf`
// into an isolated bare repository (the same engine and storage root as
// config profiles — beside the data directory, never synced or pushed).
// Identical snapshots dedupe inside the engine, so bursty saves cost one
// commit at most.

// Install the AppConfig save observer. Call once, on the main thread, after
// the config is loaded. Safe to call again (no-op).
void install();

// Stable identity path the snapshots are recorded under.
std::filesystem::path identity();

// The shared manager (lazy; rooted at the profiles root). May return null
// when the repository cannot be initialized — callers must tolerate that.
ProjectHistoryManager *manager();

} } // namespace GUI::PreferencesHistory
} // namespace Slic3r

#endif // slic3r_GUI_PreferencesHistory_hpp_
