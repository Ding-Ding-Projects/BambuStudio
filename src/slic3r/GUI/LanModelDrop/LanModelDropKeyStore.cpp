#include "LanModelDropKeyStore.hpp"
#include "LanModelDropModel.hpp"

#include <openssl/crypto.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "Crypt32.lib")
#endif
#endif

namespace Slic3r { namespace GUI { namespace LanModelDrop {

namespace {

// Ties the blob to this purpose: another DPAPI user in the same account cannot be handed this blob
// as its own secret, and this code cannot be handed theirs.
constexpr char kEntropy[] = "Bambu Studio LAN model drop station key, protocol 1";
// A DPAPI blob of a 512-character key is well under this; anything larger is not ours.
constexpr std::uintmax_t kMaxBlobBytes = 16 * 1024;

} // namespace

void wipe(std::string &secret)
{
    if (!secret.empty()) OPENSSL_cleanse(&secret[0], secret.size());
    secret.clear();
}

std::filesystem::path station_key_file(const std::filesystem::path &data_folder)
{
    return data_folder / "lan-model-drop" / "station-key.dpapi";
}

#ifdef _WIN32

bool key_store_available() { return true; }

KeyStoreResult load_station_key(const std::filesystem::path &file, std::string &key)
{
    key.clear();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) return KeyStoreResult::Missing;
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec || size == 0 || size > kMaxBlobBytes) return KeyStoreResult::Failed;
    std::ifstream in(file, std::ios::binary);
    std::vector<char> blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) return KeyStoreResult::Failed;
    if (blob.empty()) return KeyStoreResult::Failed;

    DATA_BLOB input{static_cast<DWORD>(blob.size()), reinterpret_cast<BYTE *>(blob.data())};
    DATA_BLOB entropy{static_cast<DWORD>(sizeof(kEntropy) - 1), reinterpret_cast<BYTE *>(const_cast<char *>(kEntropy))};
    DATA_BLOB output{0, nullptr};
    if (!::CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        return KeyStoreResult::Failed;
    std::string plain(reinterpret_cast<const char *>(output.pbData), static_cast<std::size_t>(output.cbData));
    ::SecureZeroMemory(output.pbData, output.cbData);
    ::LocalFree(output.pbData);
    if (!is_station_key(plain)) {
        wipe(plain);
        return KeyStoreResult::Failed;
    }
    key.swap(plain);
    return KeyStoreResult::Ok;
}

KeyStoreResult save_station_key(const std::filesystem::path &file, const std::string &key)
{
    if (!is_station_key(key)) return KeyStoreResult::Failed;
    DATA_BLOB input{static_cast<DWORD>(key.size()), reinterpret_cast<BYTE *>(const_cast<char *>(key.data()))};
    DATA_BLOB entropy{static_cast<DWORD>(sizeof(kEntropy) - 1), reinterpret_cast<BYTE *>(const_cast<char *>(kEntropy))};
    DATA_BLOB output{0, nullptr};
    if (!::CryptProtectData(&input, L"LAN model drop station key", &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        return KeyStoreResult::Failed;
    std::vector<char> blob(reinterpret_cast<const char *>(output.pbData),
                           reinterpret_cast<const char *>(output.pbData) + output.cbData);
    ::LocalFree(output.pbData);

    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) return KeyStoreResult::Failed;
    std::filesystem::path temporary = file;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(blob.data(), static_cast<std::streamsize>(blob.size()));
        out.close();
        if (!out) {
            std::filesystem::remove(temporary, ec);
            return KeyStoreResult::Failed;
        }
    }
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return KeyStoreResult::Failed;
    }
    return KeyStoreResult::Ok;
}

KeyStoreResult forget_station_key(const std::filesystem::path &file)
{
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return KeyStoreResult::Missing;
    return std::filesystem::remove(file, ec) && !ec ? KeyStoreResult::Ok : KeyStoreResult::Failed;
}

#else

// Windows is the only native target. Elsewhere there is no protected store, and the key is never
// kept in plain text instead.
bool           key_store_available() { return false; }
KeyStoreResult load_station_key(const std::filesystem::path &, std::string &key)
{
    key.clear();
    return KeyStoreResult::Unavailable;
}
KeyStoreResult save_station_key(const std::filesystem::path &, const std::string &) { return KeyStoreResult::Unavailable; }
KeyStoreResult forget_station_key(const std::filesystem::path &file)
{
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return KeyStoreResult::Missing;
    return std::filesystem::remove(file, ec) && !ec ? KeyStoreResult::Ok : KeyStoreResult::Failed;
}

#endif

}}} // namespace Slic3r::GUI::LanModelDrop
