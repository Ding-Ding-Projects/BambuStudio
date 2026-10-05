#pragma once

#include "SchoolMode.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <limits>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r::GUI::PersonalModes {
inline SchoolRead parse_school_record(const std::string& bytes) {
    if (bytes.size() > 4096) return {RecordStatus::Corrupt, {}};
    try {
        std::set<std::string> keys;
        bool invalid = false;
        auto value = nlohmann::json::parse(bytes, [&keys, &invalid](int depth, nlohmann::json::parse_event_t event, nlohmann::json& item) {
            if (depth > 1) invalid = true;
            if (event == nlohmann::json::parse_event_t::key && !keys.insert(item.get<std::string>()).second) invalid = true;
            return true;
        });
        if (invalid || !value.is_object() || value.size() != 6 ||
            !value.contains("version") || value["version"] != 1 || !value["version"].is_number_integer() ||
            !value.contains("enabled") || !value["enabled"].is_boolean() ||
            !value.contains("displayName") || !value["displayName"].is_string() ||
            !value.contains("revision") || !value["revision"].is_number_unsigned() ||
            !value.contains("credentialGeneration") || !value["credentialGeneration"].is_string() ||
            !value.contains("updatedAt") || !value["updatedAt"].is_string()) return {RecordStatus::Corrupt, {}};
        SchoolRecord result{value["enabled"].get<bool>(), value["displayName"].get<std::string>(),
            value["revision"].get<std::uint64_t>(), value["credentialGeneration"].get<std::string>(), value["updatedAt"].get<std::string>()};
        if (!SchoolMode::valid_name(result.display_name) || result.credential_generation.size() > 128 ||
            (result.enabled && result.credential_generation.empty()) || result.updated_at.size() != 20 ||
            result.updated_at[4] != '-' || result.updated_at[7] != '-' || result.updated_at[10] != 'T' ||
            result.updated_at[13] != ':' || result.updated_at[16] != ':' || result.updated_at[19] != 'Z')
            return {RecordStatus::Corrupt, {}};
        for (unsigned char c : result.credential_generation) if (c < 33 || c > 126) return {RecordStatus::Corrupt, {}};
        for (std::size_t i = 0; i < result.updated_at.size(); ++i) {
            if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19) continue;
            if (result.updated_at[i] < '0' || result.updated_at[i] > '9') return {RecordStatus::Corrupt, {}};
        }
        return {RecordStatus::Ready, std::move(result)};
    } catch (...) { return {RecordStatus::Corrupt, {}}; }
}

class SchoolStore {
public:
    explicit SchoolStore(std::filesystem::path path) : m_path(std::move(path)) {}
    static std::filesystem::path default_path() {
#ifdef _WIN32
        wchar_t root[32768];
        const auto size = ::GetEnvironmentVariableW(L"LOCALAPPDATA", root, 32768);
        if (size > 0 && size < 32768) return std::filesystem::path(root) / L"DingDing/SharedPresentation/school-mode-v1.json";
#endif
        return {}; // The owner must report unsupported platform storage honestly.
    }
    SchoolRead read() const {
        if (m_path.empty()) return {};
        std::error_code ec;
        const bool exists = std::filesystem::exists(m_path, ec);
        if (ec) return {};
        if (!exists) return {RecordStatus::Missing, {}};
        std::ifstream input(m_path, std::ios::binary);
        if (!input) return {};
        std::string bytes(4097, '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        bytes.resize(static_cast<std::size_t>(input.gcount()));
        if (input.bad()) return {};
        return parse_school_record(bytes);
    }
    RecordStatus compare_exchange(SchoolRecord next, std::uint64_t expected,
                                 const std::function<bool(const std::string&)>& verify = {}) {
#ifdef _WIN32
        if (m_path.empty() || expected == std::numeric_limits<std::uint64_t>::max()) return RecordStatus::Unavailable;
        std::error_code ec;
        std::filesystem::create_directories(m_path.parent_path(), ec);
        if (ec) return RecordStatus::Unavailable;
        // Lock file lives beside the shared record; all cooperating applications
        // hold this exclusive handle until their atomic replacement completes.
        const auto lock_path = m_path.parent_path() / L"school-mode-v1.lock";
        HANDLE lock = ::CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lock == INVALID_HANDLE_VALUE) return RecordStatus::Conflict;
        struct Close { HANDLE h; ~Close() { ::CloseHandle(h); } } close{lock};
        const auto current = read();
        if (current.status != RecordStatus::Ready && current.status != RecordStatus::Missing) return current.status;
        if (current.record.revision != expected) return RecordStatus::Conflict;
        if (current.record.enabled && !next.enabled) {
            if (!verify || current.record.credential_generation != next.credential_generation ||
                !verify(current.record.credential_generation)) return RecordStatus::Unavailable;
        }
        next.revision = expected + 1;
        if (!SchoolMode::valid_name(next.display_name) || next.credential_generation.size() > 128 ||
            (next.enabled && next.credential_generation.empty())) return RecordStatus::Corrupt;
        for (unsigned char c : next.credential_generation) if (c < 33 || c > 126) return RecordStatus::Corrupt;
        SYSTEMTIME now; ::GetSystemTime(&now);
        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "%04u-%02u-%02uT%02u:%02u:%02uZ", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
        next.updated_at = stamp;
        const auto bytes = nlohmann::json{{"version", 1}, {"enabled", next.enabled}, {"displayName", next.display_name},
            {"revision", next.revision}, {"credentialGeneration", next.credential_generation}, {"updatedAt", next.updated_at}}.dump();
        if (parse_school_record(bytes).status != RecordStatus::Ready) return RecordStatus::Corrupt;
        static std::atomic<unsigned long> sequence{0};
        auto pending = m_path.parent_path() / (L"school-mode-" + std::to_wstring(::GetCurrentProcessId()) + L"-" +
            std::to_wstring(::GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1)) + L".pending");
        HANDLE file = ::CreateFileW(pending.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return RecordStatus::Unavailable;
        DWORD written = 0;
        const bool ok = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && ::FlushFileBuffers(file);
        ::CloseHandle(file);
        const bool moved = ok && ::MoveFileExW(pending.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!moved) ::DeleteFileW(pending.c_str());
        return moved ? RecordStatus::Ready : RecordStatus::Unavailable;
#else
        return RecordStatus::Unavailable;
#endif
    }
private:
    std::filesystem::path m_path;
};
}
