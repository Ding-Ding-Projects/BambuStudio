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

// Appearance history: the per-element styles, saved appearance presets and the
// active preset (Appearance/ElementStyle) are recorded under their own identity
// in the same local store. Every successful ElementStyle::save() schedules a
// debounced "Appearance change" snapshot; install() records the state at
// startup so the first change of a session can be undone.
std::filesystem::path appearance_identity();

// Start a named appearance action such as "Apply appearance preset: Large
// text". Any pending unnamed change is recorded first, so the state before the
// action keeps a revision of its own; the next ElementStyle::save() is then
// recorded at once with `label`. Call before changing the registry.
void begin_appearance_action(const std::string &label);
// Drop a named action that ended without changing anything (a failed import),
// so its name is not given to a later, unrelated change.
void cancel_appearance_action();

// A snapshot holds {"format": "bambu-appearance", "version": 1, "document": ...}
// where the document is the ElementStyle file schema.
bool read_appearance_snapshot(const std::filesystem::path &path, nlohmann::json &document, std::string &error);
// Replace the live appearance with a snapshot and record the restore.
bool apply_appearance_snapshot(const std::filesystem::path &path, std::string &error);

} } // namespace GUI::PreferencesHistory
} // namespace Slic3r

#endif // slic3r_GUI_PreferencesHistory_hpp_
