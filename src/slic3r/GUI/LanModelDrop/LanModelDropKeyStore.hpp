#pragma once
// Protected storage for the LAN model drop station key.
//
// The key is a secret shared with the drop container. It is never written to the application
// settings, never logged and never exported in clear: on Windows it is encrypted with DPAPI
// (CryptProtectData, current user, UI forbidden) and only the protected blob is written to
// <data folder>/lan-model-drop/station-key.dpapi. Another Windows account, or a copy of the data
// folder on another computer, cannot decrypt it. Other platforms report the store as unavailable
// rather than fall back to plain text.

#include <filesystem>
#include <string>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

enum class KeyStoreResult { Ok, Missing, Unavailable, Failed };

bool                  key_store_available();
std::filesystem::path station_key_file(const std::filesystem::path &data_folder);
// `key` receives the decrypted key on Ok and is left empty otherwise.
KeyStoreResult        load_station_key(const std::filesystem::path &file, std::string &key);
// Writes a new protected blob (temporary file, then rename). The key must already pass
// is_station_key().
KeyStoreResult        save_station_key(const std::filesystem::path &file, const std::string &key);
KeyStoreResult        forget_station_key(const std::filesystem::path &file);
// Overwrites the characters before the string is released.
void                  wipe(std::string &secret);

}}} // namespace Slic3r::GUI::LanModelDrop
