#ifndef slic3r_GUI_PreferencesHistory_hpp_
#define slic3r_GUI_PreferencesHistory_hpp_

#include <filesystem>
#include <string>
#include "nlohmann/json.hpp"

namespace Slic3r {

class ProjectHistoryManager;
class AppConfig;

namespace GUI { namespace PreferencesHistory {

// Automatic, local Git history for safe preferences: every successful
// AppConfig::save() schedules a debounced, versioned allowlisted JSON snapshot
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

// Only presentation/editor settings are included, never account, device,
// command, path, or personal-vocabulary data. Legacy raw snapshots are not applied.
nlohmann::json safe_snapshot(const AppConfig &config);
bool read_snapshot(const std::filesystem::path &path, nlohmann::json &snapshot, std::string &error);
bool apply_snapshot(const std::filesystem::path &path, std::string &error);

} } // namespace GUI::PreferencesHistory
} // namespace Slic3r

#endif // slic3r_GUI_PreferencesHistory_hpp_
