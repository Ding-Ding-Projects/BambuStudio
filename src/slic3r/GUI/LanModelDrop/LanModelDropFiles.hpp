#pragma once
// LAN model drop, station side: the received files on disk
// (docs/features/application-integration/lan-model-drop.md).
//
// Plain C++17 with std::filesystem and no wxWidgets, so tests/lan_model_drop/lan_model_drop_files_test.cpp
// can compile it with g++ alone. Each received model lives in <root>/<item id>/. While it waits for
// Open or Discard the folder also holds the marker file kWaitingMarker; Open removes the marker and
// the folder then belongs to the user (an opened project may point at it, and may be saved into it).

#include "LanModelDropModel.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

constexpr const char *kWaitingMarker = ".waiting";

enum class ReceivedState {
    Absent,  // no folder for this id
    Waiting, // downloaded, waiting for Open or Discard (the marker is there)
    Opened,  // opened: no marker, the file is there; never written again
    Stale    // a folder with neither, left by an interrupted write; it may be replaced
};
ReceivedState received_state(const std::filesystem::path &root, const std::string &id);

enum class WriteResult {
    Written,
    Exists, // the folder already exists (waiting or opened): nothing was written
    Failed  // a local disk problem; nothing is left behind
};
// Saves a downloaded model as <root>/<id>/<name> and marks it waiting. The folder must not exist yet
// (a stale one is removed first): a folder that is waiting or was opened is never written into. The
// bytes are written under a fixed name of the station's own and then renamed, so no name a sender
// chose is ever opened for writing. <name> is `file_name` when is_clean_file_name accepts it and it
// matches `type`, otherwise fallback_file_name(type); the fallback is also used when the full path
// would be longer than Windows accepts without long-path support. `file` receives the saved path.
WriteResult write_received(const std::filesystem::path &root, const std::string &id, const std::string &file_name, FileType type,
                           const std::string &bytes, std::filesystem::path &file);
// Open: the file's folder stops waiting (its marker is removed). False when the marker is still there.
bool mark_opened(const std::filesystem::path &file);
// Discard, a refusal, or switched off while the file was on its way: removes <root>/<id> only while it
// is waiting. An opened folder is never removed.
void remove_waiting(const std::filesystem::path &root, const std::string &id);
// At start: removes every folder still waiting (never opened) and every stale one. Opened folders stay.
void clean_unopened(const std::filesystem::path &root);

// The deletes the drop sites have not confirmed yet, kept across restarts (format_pending_deletes).
// Written to a temporary name and renamed over the file.
std::vector<PendingDelete> load_pending_deletes(const std::filesystem::path &file);
bool                       save_pending_deletes(const std::filesystem::path &file, const std::vector<PendingDelete> &pending);

}}} // namespace Slic3r::GUI::LanModelDrop
