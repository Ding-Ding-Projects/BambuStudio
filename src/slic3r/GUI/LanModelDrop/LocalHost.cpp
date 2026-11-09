#include "LocalHost.hpp"
#include "LanModelDropModel.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Bcrypt.lib")
#endif
#endif

namespace Slic3r { namespace GUI { namespace LanModelDrop {
namespace {
std::atomic<bool> setup_active{false};

using json = nlohmann::json;

bool parse_result(const std::string &bytes, LocalHost::Result &result)
{
    try {
        std::set<std::string> keys;
        json document = json::parse(bytes, [&keys](int depth, json::parse_event_t event, json &value) {
            if (depth > 1) throw std::runtime_error("nested result");
            if (event == json::parse_event_t::key && !keys.insert(value.get<std::string>()).second)
                throw std::runtime_error("duplicate result field");
            return true;
        });
        if (!document.is_object() || !document.contains("schema") || !document["schema"].is_number_integer() ||
            document["schema"] != 1 || !document.contains("ok") || !document["ok"].is_boolean()) throw std::runtime_error("invalid result");
        if (!document["ok"].get<bool>()) {
            if (document.size() != 3 || !document.contains("code") || !document["code"].is_string()) throw std::runtime_error("invalid failure");
            const auto code = document["code"].get<std::string>();
            if (code.empty() || code.size() > 64 || code.find_first_not_of("abcdefghijklmnopqrstuvwxyz_") != std::string::npos)
                throw std::runtime_error("invalid failure code");
            // Never surface script-supplied diagnostics: only an internal fixed outcome.
            result.code = "setup_failed";
            return true;
        }
        if (document.size() != 4 || !document.contains("address") || !document["address"].is_string() ||
            !document.contains("lanUrl") || !document["lanUrl"].is_string()) throw std::runtime_error("invalid success");
        const auto address = parse_base_address(document["address"].get<std::string>());
        const auto lan = parse_base_address(document["lanUrl"].get<std::string>());
        if (!address || address->scheme != "http" || address->host != "127.0.0.1" || !address->explicit_port ||
            !lan || lan->scheme != "http" || !is_private_ipv4(lan->host) || lan->port != address->port)
            throw std::runtime_error("invalid pairing");
        result.address = address->base;
        result.lan_url = lan->base + "/";
        result.ok = true;
        return true;
    } catch (...) {
        result.code = "invalid_result";
        return false;
    }
}

#ifdef _WIN32
struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE handle = nullptr) : value(handle) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    bool valid() const { return value && value != INVALID_HANDLE_VALUE; }
};
struct LocalMemory {
    void *value = nullptr;
    ~LocalMemory() { if (value) LocalFree(value); }
};

bool setup(std::atomic<bool> &cancel, LocalHost::Result &result)
{
    // Resolve the installed payload beside this exact executable, never PATH or CWD.
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
    if (!length || length >= executable.size()) return false;
    executable.resize(length);
    const auto script = std::filesystem::path(executable).parent_path() / L"lan-model-drop" / L"host.ps1";
    if (!std::filesystem::is_regular_file(script)) { result.code = "payload_missing"; return false; }
    std::array<wchar_t, MAX_PATH + 1> system{};
    if (!GetSystemDirectoryW(system.data(), DWORD(system.size()))) return false;
    const auto shell = std::filesystem::path(system.data()) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";

    Handle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) return false;
    DWORD token_bytes = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &token_bytes);
    std::vector<unsigned char> user(token_bytes);
    if (!token_bytes || !GetTokenInformation(token.value, TokenUser, user.data(), token_bytes, &token_bytes)) return false;
    LocalMemory sid;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid, reinterpret_cast<LPWSTR *>(&sid.value))) return false;
    const std::wstring dacl = L"D:P(A;;GA;;;" + std::wstring(static_cast<wchar_t *>(sid.value)) + L")";
    LocalMemory descriptor;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(dacl.c_str(), SDDL_REVISION_1,
            reinterpret_cast<PSECURITY_DESCRIPTOR *>(&descriptor.value), nullptr)) return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor.value, FALSE};
    std::array<unsigned char, 16> random{};
    if (BCryptGenRandom(nullptr, random.data(), ULONG(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
    const std::string suffix = to_hex(random.data(), random.size());
    const std::wstring pipe_name = L"BambuLanDrop-" + std::wstring(suffix.begin(), suffix.end());
    const std::wstring pipe_path = L"\\\\.\\pipe\\" + pipe_name;
    Handle pipe(CreateNamedPipeW(pipe_path.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 8192, 8192, 0, &security));
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!pipe.valid() || !event.valid()) return false;
    OVERLAPPED overlap{};
    overlap.hEvent = event.value;

    // The child inherits only NUL, never unrelated application handles or the pipe server.
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle null_io(CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr));
    if (!null_io.valid()) { CancelIoEx(pipe.value, &overlap); return false; }
    SIZE_T attribute_bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_bytes);
    std::vector<unsigned char> attributes(attribute_bytes);
    auto *list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &attribute_bytes)) { CancelIoEx(pipe.value, &overlap); return false; }
    struct AttributeCleanup { LPPROC_THREAD_ATTRIBUTE_LIST list; ~AttributeCleanup() { DeleteProcThreadAttributeList(list); } } cleanup{list};
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &null_io.value, sizeof(HANDLE), nullptr, nullptr)) {
        CancelIoEx(pipe.value, &overlap); return false;
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.StartupInfo.wShowWindow = SW_HIDE;
    startup.StartupInfo.hStdInput = startup.StartupInfo.hStdOutput = startup.StartupInfo.hStdError = null_io.value;
    startup.lpAttributeList = list;
    Handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.valid() || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CancelIoEx(pipe.value, &overlap); return false;
    }
    std::wstring command = L"\"" + shell.wstring() + L"\" -NoLogo -NoProfile -NonInteractive -File \"" + script.wstring() +
        L"\" -Mode Detached -PairingPipe " + pipe_name;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(shell.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, script.parent_path().c_str(), &startup.StartupInfo, &process)) {
        CancelIoEx(pipe.value, &overlap); return false;
    }
    Handle child(process.hProcess), thread(process.hThread);
    if (!AssignProcessToJobObject(job.value, child.value)) {
        TerminateProcess(child.value, 1); CancelIoEx(pipe.value, &overlap); return false;
    }
    if (ResumeThread(thread.value) == DWORD(-1)) { CancelIoEx(pipe.value, &overlap); return false; }
    const BOOL connected = ConnectNamedPipe(pipe.value, &overlap);
    const DWORD connect_error = connected ? ERROR_SUCCESS : GetLastError();
    if (!connected && connect_error != ERROR_IO_PENDING && connect_error != ERROR_PIPE_CONNECTED) return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(660);
    auto wait = [&](bool process_exit_is_failure) {
        while (!cancel.load() && std::chrono::steady_clock::now() < deadline) {
            if (WaitForSingleObject(event.value, 40) == WAIT_OBJECT_0) return true;
            if (process_exit_is_failure && WaitForSingleObject(child.value, 0) == WAIT_OBJECT_0) return false;
        }
        result.code = cancel.load() ? "cancelled" : "timeout";
        return false;
    };
    DWORD transferred = 0;
    if (!connected && connect_error == ERROR_IO_PENDING &&
        (!wait(true) || !GetOverlappedResult(pipe.value, &overlap, &transferred, FALSE))) {
        CancelIoEx(pipe.value, &overlap);
        WaitForSingleObject(event.value, INFINITE);
        return false;
    }
    std::string bytes;
    bytes.reserve(8192);
    bool complete = false;
    while (bytes.size() < 8192 && !cancel.load()) {
        std::array<char, 1024> buffer{};
        ResetEvent(event.value);
        overlap = {};
        overlap.hEvent = event.value;
        DWORD count = 0;
        const BOOL read = ReadFile(pipe.value, buffer.data(), DWORD(std::min(buffer.size(), 8192 - bytes.size())), &count, &overlap);
        if (!read && GetLastError() != ERROR_IO_PENDING) { SecureZeroMemory(buffer.data(), buffer.size()); break; }
        if (!read && (!wait(false) || !GetOverlappedResult(pipe.value, &overlap, &count, FALSE))) {
            CancelIoEx(pipe.value, &overlap); WaitForSingleObject(event.value, INFINITE);
            SecureZeroMemory(buffer.data(), buffer.size()); break;
        }
        bytes.append(buffer.data(), count);
        SecureZeroMemory(buffer.data(), buffer.size());
        if (bytes.find('\n') != std::string::npos) { complete = true; break; }
        if (!count) break;
    }
    if (!complete || cancel.load()) return false;
    // Require a normal launcher exit as well as a valid private result.
    while (WaitForSingleObject(child.value, 40) == WAIT_TIMEOUT) {
        if (cancel.load() || std::chrono::steady_clock::now() >= deadline) return false;
    }
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(child.value, &exit_code) || exit_code != 0) return false;
    return parse_result(bytes, result);
}
#endif
} // namespace

bool persisted_station_matches(const std::filesystem::path &path, const std::string &address, const std::string &enabled)
{
    try {
        std::error_code error;
        const auto bytes = std::filesystem::file_size(path, error);
        if (error || bytes == 0 || bytes > 32 * 1024 * 1024) return false;
        std::ifstream input(path, std::ios::binary);
        json document;
        input >> document; // AppConfig appends a checksum line after the JSON object.
        if (!input || !document.is_object() || !document.contains("app") || !document["app"].is_object()) return false;
        const auto &app = document["app"];
        auto matches = [&app](const char *key, const std::string &expected) {
            if (!app.contains(key)) return expected.empty();
            const auto &value = app[key];
            if (value.is_boolean()) return expected == (value.get<bool>() ? "true" : "false");
            return value.is_string() && value.get<std::string>() == expected;
        };
        return matches("lan_drop_address", address) && matches("lan_drop_enabled", enabled);
    } catch (...) { return false; }
}

LocalHost::~LocalHost() { cancel(); }
void LocalHost::cancel()
{
    m_cancel.store(true);
    if (m_worker.joinable()) m_worker.join();
}
bool LocalHost::start()
{
    bool expected = false;
    if (!setup_active.compare_exchange_strong(expected, true)) return false;
    if (m_worker.joinable()) m_worker.join();
    m_cancel.store(false);
    m_running.store(true);
    { std::lock_guard<std::mutex> lock(m_mutex); m_result.reset(); }
    try { m_worker = std::thread(&LocalHost::run, this); }
    catch (...) { m_running.store(false); setup_active.store(false); return false; }
    return true;
}
void LocalHost::run()
{
    auto result = std::make_unique<Result>();
    try {
#ifdef _WIN32
        if (!setup(m_cancel, *result) && result->code.empty()) result->code = "setup_failed";
#else
        result->code = "unsupported";
#endif
    } catch (...) { result->ok = false; result->code = "setup_failed"; }
    { std::lock_guard<std::mutex> lock(m_mutex); m_running.store(false); m_result = std::move(result); }
    setup_active.store(false);
}
std::unique_ptr<LocalHost::Result> LocalHost::take_result()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::move(m_result);
}
}}}
