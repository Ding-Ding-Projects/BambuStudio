#include "LocalBedrockRecovery.hpp"
#include "libslic3r/miniz_extension.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
std::string owner(const fs::path &path)
{
    std::error_code ec;
    auto            p = fs::canonical(path, ec);
    return ec ? std::string() : sha1_hex(p.u8string());
}
constexpr const char *marker = "BAMBU-BEDROCK-BACKUP.json";
} // namespace
bool local_bedrock_archive(const std::string &path, const std::string &folder)
{
    mz_zip_archive zip{};
    if (!open_zip_reader(&zip, path)) return false;
    mz_zip_archive_file_stat st{};
    auto                     idx = mz_zip_reader_locate_file(&zip, marker, nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
    std::string              data;
    if (idx >= 0 && mz_zip_reader_file_stat(&zip, mz_uint(idx), &st) && st.m_uncomp_size > 0 && st.m_uncomp_size < 1024) {
        data.resize(std::size_t(st.m_uncomp_size));
        if (!mz_zip_reader_extract_to_mem(&zip, mz_uint(idx), data.data(), data.size(), 0)) data.clear();
    }
    close_zip_reader(&zip);
    auto doc = nlohmann::json::parse(data, nullptr, false);
    auto id  = owner(fs::u8path(folder));
    return !id.empty() && doc.is_object() && doc.contains("owner") && doc["owner"] == id && doc.contains("edition") && doc["edition"] == "bedrock";
}
ZipOutcome backup_local_bedrock(const std::string &folder, const std::string &target)
{
    ZipOutcome      result;
    auto            root = fs::u8path(folder);
    std::error_code ec;
    auto            id = owner(root);
    if (id.empty() || !fs::is_regular_file(root / "bedrock_server.exe", ec) || !fs::is_directory(root / "worlds", ec)) {
        result.error = "Select a Bedrock installation with generated worlds.";
        return result;
    }
#ifdef _WIN32
    struct Locks
    {
        std::vector<HANDLE> files;
        ~Locks()
        {
            for (auto f : files) CloseHandle(f);
        }
    } locks;
    std::size_t worlds = 0;
    for (fs::directory_iterator it(root / "worlds", ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_symlink(ec)) {
            result.error = "Linked worlds require a separate backup decision.";
            return result;
        }
        if (!it->is_directory(ec) || !fs::is_regular_file(it->path() / "level.dat", ec)) continue;
        ++worlds;
        auto path = it->path() / "db" / "LOCK";
        if (fs::exists(path, ec)) {
            auto f = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f == INVALID_HANDLE_VALUE) {
                result.error = "A Bedrock world database is open. Stop the server before backup.";
                return result;
            }
            locks.files.push_back(f);
        }
    }
    if (ec || !worlds) {
        result.error = "No readable generated Bedrock worlds were found.";
        return result;
    }
    std::string properties;
    if (!read_file((root / "server.properties").u8string(), properties, 1024 * 1024)) {
        result.error = "Could not read the server configuration.";
        return result;
    }
    std::string sanitized;
    std::size_t start = 0;
    while (start < properties.size()) {
        auto nl   = properties.find('\n', start);
        auto end  = nl == std::string::npos ? properties.size() : nl + 1;
        auto line = properties.substr(start, end - start);
        auto key  = line.substr(0, line.find('='));
        if (key != "script-debugger-passcode" && key != "rcon.password") sanitized += line;
        start = end;
    }
    std::vector<std::string> entries = {"worlds"};
    for (auto name : {"behavior_packs", "resource_packs", "allowlist.json", "permissions.json"})
        if (fs::exists(root / name, ec)) entries.push_back(name);
    std::map<std::string, std::string> generated = {{"server.properties", sanitized}, {marker, nlohmann::json({{"owner", id}, {"edition", "bedrock"}, {"schema", 1}}).dump()}};
    return write_zip(folder, entries, target, {}, nullptr, generated);
#else
    result.error = "Native Bedrock database-lock verification is unavailable on this host.";
    return result;
#endif
}
}}} // namespace Slic3r::GUI::GameServers
