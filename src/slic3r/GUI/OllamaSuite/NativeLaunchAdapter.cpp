#include "NativeLaunchAdapter.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "version.lib")
#endif
#endif

namespace Slic3r { namespace OllamaSuite {
namespace NativeLaunchDetail {
bool supported_kind(LaunchKind kind) { return kind == LaunchKind::LlamaServer; }
std::string profile_unavailable_reason(LaunchKind kind) {
    if (kind == LaunchKind::OllamaChat) return "Ollama terminal launch is unavailable. Use the suite Chat tab, which uses Ollama's documented local HTTP API.";
    if (!supported_kind(kind)) return "This native profile has no reviewed executable schema.";
    return {};
}
std::wstring quote_argument(const std::wstring& argument) {
    // The documented CommandLineToArgvW/CRT escaping convention. Always quote,
    // including empty arguments; never invoke cmd.exe to interpret this string.
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'\"') { out.append(slashes * 2 + 1, L'\\'); out += c; }
        else { out.append(slashes, L'\\'); out += c; }
        slashes = 0;
    }
    out.append(slashes * 2, L'\\'); out += L'\"'; return out;
}
bool safe_snapshot(const RedactedLaunchSnapshot& s) {
    auto token = [](const std::string& value, bool model) {
        return value.size() <= 200 && (value.empty() || value.front() != '-') &&
            std::all_of(value.begin(), value.end(), [model](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
                       (model && (c == '/' || c == ':'));
            });
    };
    return token(s.profile_id, false) && token(s.model_tag, true) &&
           s.context_length <= 1048576 && s.port <= 65535;
}
std::string encode_snapshot(const RedactedLaunchSnapshot& s) {
    if (!safe_snapshot(s)) return {};
    return "OLLP1\n" + s.profile_id + "\n" + s.model_tag + "\n" +
        std::to_string(s.context_length) + "\n" + std::to_string(s.port) + "\n" + (s.enabled ? "1\n" : "0\n");
}
std::optional<RedactedLaunchSnapshot> decode_snapshot(const std::string& bytes) {
    if (bytes.size() > 512) return {};
    std::istringstream in(bytes); std::string magic, context, port, enabled;
    RedactedLaunchSnapshot s;
    if (!std::getline(in, magic) || magic != "OLLP1" || !std::getline(in, s.profile_id) ||
        !std::getline(in, s.model_tag) || !std::getline(in, context) || !std::getline(in, port) ||
        !std::getline(in, enabled) || (enabled != "0" && enabled != "1") || in.peek() != EOF) return {};
    auto number = [](const std::string& v, unsigned& value) {
        if (v.empty() || v.size() > 7 || !std::all_of(v.begin(), v.end(), [](char c) { return c >= '0' && c <= '9'; })) return false;
        value = 0; for (char c : v) value = value * 10 + static_cast<unsigned>(c - '0'); return true;
    };
    if (!number(context, s.context_length) || !number(port, s.port)) return {};
    s.enabled = enabled == "1";
    if (!safe_snapshot(s) || encode_snapshot(s) != bytes) return {};
    return s;
}
}

#ifdef _WIN32
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& h) noexcept : value(h.value) { h.value = INVALID_HANDLE_VALUE; }
    Handle& operator=(Handle&& h) noexcept { if (this != &h) { reset(); value = h.value; h.value = INVALID_HANDLE_VALUE; } return *this; }
    bool valid() const { return value != INVALID_HANDLE_VALUE && value != nullptr; }
    void reset(HANDLE h = INVALID_HANDLE_VALUE) { if (valid()) CloseHandle(value); value = h; }
};
std::wstring wide(const std::string& s) {
    if (s.empty() || s.size() > 32760 || s.find('\0') != std::string::npos) return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) return {};
    std::wstring w(static_cast<size_t>(n), 0);
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), &w[0], n) == n ? w : L"";
}
std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (!n) return {};
    std::string s(static_cast<size_t>(n), 0);
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr) == n ? s : "";
}
bool local_path(const std::wstring& s) {
    if (s.size() < 3 || s.size() > 32000 || !((s[0] >= L'A' && s[0] <= L'Z') || (s[0] >= L'a' && s[0] <= L'z')) ||
        s[1] != L':' || (s[2] != L'\\' && s[2] != L'/') || s.find(L':', 2) != std::wstring::npos) return false;
    if (s.find_first_of(L"\r\n\t\"%$`|;&<>") != std::wstring::npos) return false;
    for (const auto& part : std::filesystem::path(s)) if (part == L".." || part == L".") return false;
    return true;
}
std::wstring canonical(HANDLE file) {
    DWORD count = GetFinalPathNameByHandleW(file, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!count || count > 32760) return {};
    std::wstring path(count, 0);
    const DWORD written = GetFinalPathNameByHandleW(file, &path[0], count, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!written || written >= count) return {};
    path.resize(written);
    if (path.rfind(L"\\\\?\\", 0) == 0) path.erase(0, 4);
    return local_path(path) ? path : L"";
}
bool lock_parents(const std::wstring& path, std::vector<Handle>& locks) {
    if (!local_path(path)) return false;
    const auto parent = std::filesystem::path(path).parent_path();
    auto current = parent.root_path();
    for (const auto& part : parent.relative_path()) {
        current /= part;
        Handle directory(CreateFileW(current.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        BY_HANDLE_FILE_INFORMATION info{};
        if (!directory.valid() || !GetFileInformationByHandle(directory.value, &info) ||
            !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
        locks.push_back(std::move(directory));
    }
    return true;
}
std::string sha256(HANDLE file, const std::atomic_uint64_t& cancellation, std::uint64_t epoch) {
    LARGE_INTEGER zero{}; if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) return {};
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    unsigned char buffer[65536], digest[32]{}; DWORD count = 0;
    while (ok) {
        if (cancellation.load(std::memory_order_relaxed) != epoch) { ok = false; break; }
        if (!ReadFile(file, buffer, sizeof(buffer), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        if (BCryptHashData(hash, buffer, count, 0) < 0) ok = false;
    }
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok) return {};
    static const char digits[] = "0123456789abcdef"; std::string result;
    for (unsigned char byte : digest) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}
bool pe_image(HANDLE file) {
    LARGE_INTEGER zero{}; DWORD got = 0; IMAGE_DOS_HEADER dos{};
    if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN) || !ReadFile(file, &dos, sizeof(dos), &got, nullptr) ||
        got != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) || dos.e_lfanew > 1048576) return false;
    LARGE_INTEGER pos{}; pos.QuadPart = dos.e_lfanew; DWORD signature = 0; IMAGE_FILE_HEADER header{};
    return SetFilePointerEx(file, pos, nullptr, FILE_BEGIN) && ReadFile(file, &signature, sizeof(signature), &got, nullptr) &&
        got == sizeof(signature) && signature == IMAGE_NT_SIGNATURE && ReadFile(file, &header, sizeof(header), &got, nullptr) &&
        got == sizeof(header) && !(header.Characteristics & IMAGE_FILE_DLL) && (header.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);
}
bool signed_image(const std::wstring& path, HANDLE file) {
    WINTRUST_FILE_INFO info{}; info.cbStruct = sizeof(info); info.pcwszFilePath = path.c_str(); info.hFile = file;
    WINTRUST_DATA data{}; data.cbStruct = sizeof(data); data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE; data.dwUnionChoice = WTD_CHOICE_FILE; data.pFile = &info;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_NONE;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG result = WinVerifyTrust(nullptr, &action, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust(nullptr, &action, &data);
    return result == ERROR_SUCCESS;
}
bool declared_role(const std::wstring& path) {
    // A renamed, legitimately signed shell must not pass as an allowlisted tool.
    // Binaries without role metadata require a reviewed exact content digest.
    DWORD ignored = 0; const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size || size > 1048576) return false;
    std::vector<unsigned char> bytes(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, bytes.data())) return false;
    struct Translation { WORD language, code_page; };
    Translation* translations = nullptr; UINT length = 0;
    if (!VerQueryValueW(bytes.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &length) ||
        length < sizeof(Translation) || length % sizeof(Translation)) return false;
    auto expected = std::filesystem::path(path).filename().wstring();
    std::transform(expected.begin(), expected.end(), expected.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    if (expected != L"llama-server.exe") return false;
    for (size_t i = 0; i < length / sizeof(Translation); ++i) {
        wchar_t key[96]{};
        swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\OriginalFilename", translations[i].language, translations[i].code_page);
        wchar_t* original = nullptr; UINT count = 0;
        if (!VerQueryValueW(bytes.data(), key, reinterpret_cast<void**>(&original), &count) || !original || count < 2 || count > 260) continue;
        std::wstring value(original, count - 1);
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        if (value == expected) return true;
    }
    return false;
}
std::string receipt() {
    unsigned char bytes[32]; if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return {};
    static const char digits[] = "0123456789abcdef"; std::string result;
    for (unsigned char c : bytes) { result += digits[c >> 4]; result += digits[c & 15]; } return result;
}
struct Selection { std::wstring path; bool cancelled = false; };
Selection pick(void* owner, bool folder, bool executable) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return {};
    IFileOpenDialog* dialog = nullptr; Selection selection;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT;
        options |= folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST;
        dialog->SetOptions(options);
        dialog->SetTitle(folder ? L"Select the launch working directory" : executable ? L"Select the registered executable" : L"Select a local GGUF model");
        const COMDLG_FILTERSPEC filter = executable ? COMDLG_FILTERSPEC{L"Native executable", L"*.exe"} : COMDLG_FILTERSPEC{L"GGUF model", L"*.gguf"};
        if (!folder) dialog->SetFileTypes(1, &filter);
        const HRESULT result = dialog->Show(static_cast<HWND>(owner));
        selection.cancelled = result == HRESULT_FROM_WIN32(ERROR_CANCELLED);
        if (SUCCEEDED(result)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr; if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) { selection.path = path; CoTaskMemFree(path); }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return selection;
}
bool health(unsigned port, bool llama, unsigned budget_ms, const std::atomic_uint64_t& cancellation, std::uint64_t epoch) {
    if (cancellation.load(std::memory_order_relaxed) != epoch) return false;
    HINTERNET session = WinHttpOpen(L"BambuStudio-LocalLaunch/1", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    const int timeout = static_cast<int>(std::max(1u, std::min(250u, budget_ms / 4)));
    WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
    HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", static_cast<INTERNET_PORT>(port), 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", llama ? L"/health" : L"/api/version", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0) : nullptr;
    bool ok = false;
    if (request) {
        DWORD disable = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
        WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));
        if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
            DWORD code = 0, size = sizeof(code);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX) && code == 200) {
                std::string body; char buffer[4096]; DWORD count = 0; bool read_ok = true;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
                while (true) {
                    if (cancellation.load(std::memory_order_relaxed) != epoch || std::chrono::steady_clock::now() >= deadline ||
                        !WinHttpReadData(request, buffer, sizeof(buffer), &count)) { read_ok = false; break; }
                    if (!count) break;
                    if (body.size() + count > 65536) { read_ok = false; break; } body.append(buffer, count);
                }
                if (read_ok) try {
                    const auto json = nlohmann::json::parse(body);
                    ok = json.is_object() && (llama ? json.value("status", "") == "ok" : !json.value("version", "").empty());
                } catch (...) {}
            }
        }
        WinHttpCloseHandle(request);
    }
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session); return ok;
}
bool owns_listener(unsigned port, HANDLE job) {
    ULONG bytes = 0;
    if (GetExtendedTcpTable(nullptr, &bytes, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) != ERROR_INSUFFICIENT_BUFFER || bytes > 16 * 1024 * 1024) return false;
    std::vector<unsigned char> buffer(bytes);
    if (GetExtendedTcpTable(buffer.data(), &bytes, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) != NO_ERROR) return false;
    const auto* table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];
        if (ntohs(static_cast<u_short>(row.dwLocalPort)) != port || row.dwLocalAddr != htonl(INADDR_LOOPBACK)) continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, row.dwOwningPid)); BOOL owned = FALSE;
        if (process.valid() && IsProcessInJob(process.value, job, &owned) && owned) return true;
    }
    return false;
}
}
#endif

struct NativeLaunchAdapter::Impl {
    explicit Impl(NativeLaunchPolicy p) : policy(std::move(p)) {}
    NativeLaunchPolicy policy;
    std::atomic_uint64_t cancellation{0};
    std::atomic_bool stop_requested{false};
    std::string reason = "Native launch is unavailable on this platform.";
#ifdef _WIN32
    struct Registration { PickerEvidence evidence; bool executable; LaunchKind kind; };
    std::map<std::string, Registration> registrations;
    std::wstring storage;
    Handle storage_lock, process, job, executable_lock, model_lock;
    std::vector<Handle> directory_locks, launch_path_locks;
    std::optional<RedactedLaunchSnapshot> before;
    std::optional<LaunchProfile> applied;
    bool initialized = false;
    bool network = false;
    bool ready_verified = false;
    bool journal = false;
    std::uint64_t operation_epoch = 0;

    std::wstring file(const wchar_t* name) const { return storage + L"\\" + name; }
    std::optional<RedactedLaunchSnapshot> read(const wchar_t* name, bool missing_is_empty) const {
        Handle input(CreateFileW(file(name).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!input.valid()) return missing_is_empty && GetLastError() == ERROR_FILE_NOT_FOUND ? std::optional<RedactedLaunchSnapshot>(RedactedLaunchSnapshot{}) : std::nullopt;
        BY_HANDLE_FILE_INFORMATION info{}; if (!GetFileInformationByHandle(input.value, &info) || info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return {};
        LARGE_INTEGER length{}; if (!GetFileSizeEx(input.value, &length) || length.QuadPart > 512) return {};
        std::string bytes(static_cast<size_t>(length.QuadPart), 0); DWORD got = 0;
        if (!ReadFile(input.value, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr) || got != bytes.size()) return {};
        return NativeLaunchDetail::decode_snapshot(bytes);
    }
    bool write(const wchar_t* name, const RedactedLaunchSnapshot& snapshot) {
        const std::string bytes = NativeLaunchDetail::encode_snapshot(snapshot); if (bytes.empty()) return false;
        const auto temp = file(L"pending.tmp");
        Handle output(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH, nullptr));
        if (!output.valid()) return false;
        DWORD written = 0;
        bool ok = WriteFile(output.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(output.value);
        output.reset();
        if (ok) ok = MoveFileExW(temp.c_str(), file(name).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!ok) DeleteFileW(temp.c_str());
        return ok;
    }
    bool delete_journal() {
        if (DeleteFileW(file(L"rollback.state").c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) { journal = false; return true; } return false;
    }
    bool trusted(HANDLE h, const std::wstring& path, const std::string& digest) const {
        return pe_image(h) && (std::find(policy.reviewed_unsigned_sha256.begin(), policy.reviewed_unsigned_sha256.end(), digest) != policy.reviewed_unsigned_sha256.end() ||
                              (signed_image(path, h) && declared_role(path)));
    }
    Handle lock_verified(const PickerEvidence& evidence, bool executable, std::optional<LaunchKind> kind = {}) const {
        if (stop_requested.load(std::memory_order_relaxed)) return {};
        const auto epoch = cancellation.load(std::memory_order_relaxed);
        const auto entry = registrations.find(evidence.picker_receipt);
        if (entry == registrations.end() || entry->second.executable != executable || (kind && entry->second.kind != *kind) ||
            entry->second.evidence.absolute_path != evidence.absolute_path || entry->second.evidence.content_identity != evidence.content_identity) return {};
        const auto path = wide(evidence.absolute_path); if (!local_path(path)) return {};
        Handle h(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!h.valid() || canonical(h.value) != path || sha256(h.value, cancellation, epoch) != evidence.content_identity) return {};
        if (executable && !trusted(h.value, path, evidence.content_identity)) return {};
        if (cancellation.load(std::memory_order_relaxed) != epoch) return {};
        return h;
    }
#endif
};

NativeLaunchAdapter::NativeLaunchAdapter(NativeLaunchPolicy policy) : m(new Impl(std::move(policy))) {
#ifdef _WIN32
    PWSTR root = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &root))) { m->reason = "The current-user data directory is unavailable."; return; }
    std::wstring path(root); CoTaskMemFree(root);
    if (!local_path(path)) { m->reason = "The current-user data directory is not a local absolute path."; return; }
    if (!lock_parents(path + L"\\placeholder", m->directory_locks)) {
        m->reason = "The current-user data directory ancestry is not an ordinary local path."; return;
    }
    for (const auto* part : {L"BambuStudio", L"OllamaSuite", L"Launch"}) {
        path += L"\\"; path += part;
        if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) { m->reason = "Cannot create the owned launch state directory."; return; }
        Handle directory(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        BY_HANDLE_FILE_INFORMATION info{};
        if (!directory.valid() || !GetFileInformationByHandle(directory.value, &info) || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT || canonical(directory.value).empty()) {
            m->reason = "Launch state must use an ordinary local directory."; return;
        }
        m->directory_locks.push_back(std::move(directory));
    }
    m->storage = path;
    m->storage_lock.reset(CreateFileW(m->file(L"owner.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION lock_info{};
    if (!m->storage_lock.valid() || !GetFileInformationByHandle(m->storage_lock.value, &lock_info) || lock_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        m->reason = "Another launch manager owns this state, or its lock is unavailable."; return;
    }
    m->journal = GetFileAttributesW(m->file(L"rollback.state").c_str()) != INVALID_FILE_ATTRIBUTES;
    // A stale temporary file can only be removed after exclusive ownership is proven.
    const DWORD pending = GetFileAttributesW(m->file(L"pending.tmp").c_str());
    if (pending != INVALID_FILE_ATTRIBUTES && ((pending & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) || !DeleteFileW(m->file(L"pending.tmp").c_str()))) {
        m->reason = "Interrupted state write requires local recovery."; return;
    }
    WSADATA data{}; m->network = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    if (!m->network) { m->reason = "Local socket verification is unavailable."; return; }
    m->initialized = true; m->reason.clear();
#endif
}
NativeLaunchAdapter::~NativeLaunchAdapter() {
#ifdef _WIN32
    const bool stopped = stop_owned();
    if (stopped && m->initialized && m->journal && m->before) restore(*m->before);
    if (m->network) WSACleanup();
#endif
}
bool NativeLaunchAdapter::available() const {
#ifdef _WIN32
    return m->initialized;
#else
    return false;
#endif
}
std::string NativeLaunchAdapter::unavailable_reason() const { return m->reason; }
NativePickerResult NativeLaunchAdapter::pick_executable(LaunchKind kind, void* owner) {
    NativePickerResult result;
#ifdef _WIN32
    if (!available()) { result.diagnostic = m->reason; return result; }
    if (!NativeLaunchDetail::supported_kind(kind)) { result.diagnostic = NativeLaunchDetail::profile_unavailable_reason(kind); return result; }
    m->stop_requested.store(false, std::memory_order_relaxed);
    const auto epoch = m->cancellation.load(std::memory_order_relaxed);
    const auto selected = pick(owner, false, true); result.cancelled = selected.cancelled;
    if (selected.path.empty()) { result.diagnostic = selected.cancelled ? "Selection cancelled." : "Executable selection was unavailable."; return result; }
    Handle file(CreateFileW(selected.path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    const auto path = file.valid() ? canonical(file.value) : L"";
    std::vector<Handle> parents;
    if (path.empty() || !lock_parents(path, parents)) { result.diagnostic = "The selected executable ancestry could not be locked for verification."; return result; }
    auto name = std::filesystem::path(path).filename().wstring();
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    const std::wstring expected = L"llama-server.exe";
    const auto digest = path.empty() ? "" : sha256(file.value, m->cancellation, epoch);
    if (m->cancellation.load(std::memory_order_relaxed) != epoch) { result.cancelled = true; result.diagnostic = "Executable verification cancelled."; return result; }
    if (name != expected || digest.empty() || !m->trusted(file.value, path, digest)) {
        result.diagnostic = "Choose the exact native executable with a valid cached signature and matching original filename, or an application-reviewed SHA256 identity."; return result;
    }
    // The owner asks the question (Material message dialog, No by default); only Yes registers.
    bool confirmed = false;
    try { confirmed = m->policy.confirm_registration && m->policy.confirm_registration(utf8(path), digest); } catch (...) { confirmed = false; }
    if (!confirmed) {
        result.cancelled = true; result.diagnostic = "Registration cancelled."; return result;
    }
    if (m->cancellation.load(std::memory_order_relaxed) != epoch) { result.cancelled = true; result.diagnostic = "Executable registration cancelled."; return result; }
    PickerEvidence evidence{utf8(path), digest, receipt()};
    if (evidence.picker_receipt.empty()) { result.diagnostic = "Registration could not create a secure local receipt."; return result; }
    m->registrations.emplace(evidence.picker_receipt, Impl::Registration{evidence, true, kind});
    result.evidence = std::move(evidence); result.diagnostic = "Executable identity registered for this session.";
#else
    (void)kind; (void)owner; result.diagnostic = m->reason;
#endif
    return result;
}
NativePickerResult NativeLaunchAdapter::pick_model_file(void* owner) {
    NativePickerResult result;
#ifdef _WIN32
    if (!available()) { result.diagnostic = m->reason; return result; }
    const auto epoch = m->cancellation.load(std::memory_order_relaxed);
    const auto selected = pick(owner, false, false); result.cancelled = selected.cancelled;
    if (selected.path.empty()) { result.diagnostic = selected.cancelled ? "Selection cancelled." : "Model selection was unavailable."; return result; }
    Handle file(CreateFileW(selected.path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    const auto path = file.valid() ? canonical(file.value) : L"";
    char magic[4]{}; DWORD read = 0;
    if (path.empty() || !ReadFile(file.value, magic, 4, &read, nullptr) || read != 4 || std::string(magic, 4) != "GGUF") {
        result.diagnostic = "The selected local file is not a GGUF model."; return result;
    }
    PickerEvidence evidence{utf8(path), sha256(file.value, m->cancellation, epoch), receipt()};
    if (m->cancellation.load(std::memory_order_relaxed) != epoch) { result.cancelled = true; result.diagnostic = "Model verification cancelled."; return result; }
    if (evidence.content_identity.empty() || evidence.picker_receipt.empty()) { result.diagnostic = "The selected model could not be verified."; return result; }
    m->registrations.emplace(evidence.picker_receipt, Impl::Registration{evidence, false, LaunchKind::LlamaServer});
    result.evidence = std::move(evidence); result.diagnostic = "Model file identity registered for this session.";
#else
    (void)owner; result.diagnostic = m->reason;
#endif
    return result;
}
NativeDirectoryResult NativeLaunchAdapter::pick_working_directory(void* owner) {
    NativeDirectoryResult result;
#ifdef _WIN32
    if (!available()) { result.diagnostic = m->reason; return result; }
    const auto selected = pick(owner, true, false); result.cancelled = selected.cancelled;
    Handle directory(selected.path.empty() ? INVALID_HANDLE_VALUE : CreateFileW(selected.path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    const auto path = directory.valid() ? canonical(directory.value) : L"";
    if (path.empty()) { result.diagnostic = selected.cancelled ? "Selection cancelled." : "Choose an existing local working directory."; return result; }
    result.path = utf8(path); result.diagnostic = "Working directory selected.";
#else
    (void)owner; result.diagnostic = m->reason;
#endif
    return result;
}
bool NativeLaunchAdapter::verified_file(const PickerEvidence& e, bool executable) const {
#ifdef _WIN32
    return available() && m->lock_verified(e, executable).valid();
#else
    (void)e; (void)executable; return false;
#endif
}
bool NativeLaunchAdapter::directory_exists(const std::string& path) const {
#ifdef _WIN32
    const auto w = wide(path); if (!local_path(w)) return false;
    Handle directory(CreateFileW(w.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    return directory.valid() && GetFileInformationByHandle(directory.value, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && canonical(directory.value) == w;
#else
    (void)path; return false;
#endif
}
bool NativeLaunchAdapter::port_available(unsigned port) const {
#ifdef _WIN32
    if (!available() || port < 1024 || port > 65535) return false;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); if (s == INVALID_SOCKET) return false;
    BOOL exclusive = TRUE; bool ok = setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == 0;
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(static_cast<u_short>(port)); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ok = ok && bind(s, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0; closesocket(s); return ok;
#else
    (void)port; return false;
#endif
}
bool NativeLaunchAdapter::installed_model(const std::string& tag) const {
    try { return available() && m->policy.installed_model && m->policy.installed_model(tag); } catch (...) { return false; }
}
bool NativeLaunchAdapter::hardware_fit(const LaunchProfile& profile) const {
    try { return available() && NativeLaunchDetail::supported_kind(profile.kind) && m->policy.hardware_fit && m->policy.hardware_fit(profile); } catch (...) { return false; }
}
std::optional<RedactedLaunchSnapshot> NativeLaunchAdapter::snapshot() {
#ifdef _WIN32
    if (!available() || m->journal || m->process.valid()) return {};
    if (m->stop_requested.load(std::memory_order_relaxed)) return {};
    m->operation_epoch = m->cancellation.load(std::memory_order_relaxed);
    auto state = m->read(L"current.state", true);
    if (!state || !m->write(L"rollback.state", *state)) return {};
    m->before = state; m->journal = true; return state;
#else
    return {};
#endif
}
bool NativeLaunchAdapter::apply(const ValidatedLaunchPlan& plan) {
#ifdef _WIN32
    if (!available() || !m->journal || !m->before || m->process.valid()) return false;
    if (m->stop_requested.load(std::memory_order_relaxed)) return false;
    if (m->cancellation.load(std::memory_order_relaxed) != m->operation_epoch) return false;
    const auto& p = plan.profile();
    if (!NativeLaunchDetail::supported_kind(p.kind)) return false;
    RedactedLaunchSnapshot state{p.id, p.kind == LaunchKind::OllamaChat ? p.model_tag : "", p.context_length, p.port, true};
    if (!m->write(L"current.state", state)) return false;
    m->applied = p; return true;
#else
    (void)plan; return false;
#endif
}
bool NativeLaunchAdapter::start(const ValidatedLaunchPlan& plan) {
#ifdef _WIN32
    if (!available() || !m->journal || !m->before || !m->applied || m->process.valid()) return false;
    if (m->stop_requested.load(std::memory_order_relaxed)) return false;
    if (m->cancellation.load(std::memory_order_relaxed) != m->operation_epoch) return false;
    const auto& p = plan.profile(); const auto& preview = plan.preview();
    if (!NativeLaunchDetail::supported_kind(p.kind)) return false;
    if (NativeLaunchDetail::encode_snapshot({p.id, p.kind == LaunchKind::OllamaChat ? p.model_tag : "", p.context_length, p.port, true}) !=
        NativeLaunchDetail::encode_snapshot({m->applied->id, m->applied->kind == LaunchKind::OllamaChat ? m->applied->model_tag : "", m->applied->context_length, m->applied->port, true}) ||
        !p.executable || !LaunchProfileRegistry::prepare(p, *this).plan) return false;
    if (!lock_parents(wide(p.executable->absolute_path), m->launch_path_locks) ||
        !lock_parents(wide(p.working_directory) + L"\\placeholder", m->launch_path_locks)) return false;
    if (p.model_file && !lock_parents(wide(p.model_file->absolute_path), m->launch_path_locks)) return false;
    m->executable_lock = m->lock_verified(*p.executable, true, p.kind);
    if (!m->executable_lock.valid()) return false;
    if (p.model_file) { m->model_lock = m->lock_verified(*p.model_file, false); if (!m->model_lock.valid()) return false; }
    const auto executable = wide(preview.executable), directory = wide(preview.working_directory);
    Handle working(CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (!working.valid() || canonical(working.value) != directory) return false;
    std::wstring command = NativeLaunchDetail::quote_argument(executable);
    for (const auto& arg : preview.arguments) { const auto w = wide(arg); if (w.empty()) return false; command += L" "; command += NativeLaunchDetail::quote_argument(w); }
    if (command.size() > 32760) return false;
    // No PATH or caller environment is inherited. The review is the complete
    // environment: missing runtime requirements fail readiness rather than
    // silently receiving unrelated settings or credentials from this process.
    std::map<std::wstring, std::wstring> environment;
    for (const auto& item : preview.environment) {
        if (item.first != "OLLAMA_HOST" || item.second != "127.0.0.1:11434") return false;
        environment.emplace(L"OLLAMA_HOST", L"127.0.0.1:11434");
    }
    std::wstring block; for (const auto& item : environment) { block += item.first + L"=" + item.second; block += L'\0'; } block += L'\0';
    if (block.size() == 1) block += L'\0';
    m->job.reset(CreateJobObjectW(nullptr, nullptr)); if (!m->job.valid()) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    if (!SetInformationJobObject(m->job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return false;
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    DWORD flags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW;
    if (m->cancellation.load(std::memory_order_relaxed) != m->operation_epoch) return false;
    if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE, flags, &block[0], directory.c_str(), &startup, &process)) return false;
    m->process.reset(process.hProcess); Handle thread(process.hThread);
    if (!AssignProcessToJobObject(m->job.value, m->process.value)) {
        TerminateProcess(m->process.value, 1); WaitForSingleObject(m->process.value, 5000); return false;
    }
    if (m->cancellation.load(std::memory_order_relaxed) != m->operation_epoch) return false;
    if (ResumeThread(thread.value) == static_cast<DWORD>(-1)) { TerminateJobObject(m->job.value, 1); return false; }
    m->launch_path_locks.push_back(std::move(working)); return true;
#else
    (void)plan; return false;
#endif
}
ReadinessResult NativeLaunchAdapter::ready(unsigned timeout_ms) {
#ifdef _WIN32
    if (!m->process.valid() || !m->applied || timeout_ms == 0 || timeout_ms > 300000) return ReadinessResult::Unhealthy;
    if (!NativeLaunchDetail::supported_kind(m->applied->kind)) return ReadinessResult::Unhealthy;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const bool llama = m->applied->kind == LaunchKind::LlamaServer;
    while (std::chrono::steady_clock::now() < deadline) {
        if (m->cancellation.load(std::memory_order_relaxed) != m->operation_epoch) return ReadinessResult::Unhealthy;
        if (WaitForSingleObject(m->process.value, 0) != WAIT_TIMEOUT) return ReadinessResult::Exited;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) break;
        if ((!llama || owns_listener(m->applied->port, m->job.value)) &&
            health(llama ? m->applied->port : 11434, llama, static_cast<unsigned>(std::min<long long>(remaining, 1000)), m->cancellation, m->operation_epoch)) {
            if (m->cancellation.load(std::memory_order_relaxed) != m->operation_epoch) return ReadinessResult::Unhealthy;
            if (WaitForSingleObject(m->process.value, 0) != WAIT_TIMEOUT) return ReadinessResult::Exited;
            if (llama && !owns_listener(m->applied->port, m->job.value)) continue;
            // Keep the rollback journal until explicit stop/destruction, so a
            // controller crash is recoverable even after readiness succeeded.
            m->ready_verified = true; return ReadinessResult::Ready;
        }
        if (WaitForSingleObject(m->process.value, 25) != WAIT_TIMEOUT) return ReadinessResult::Exited;
    }
    return ReadinessResult::TimedOut;
#else
    (void)timeout_ms; return ReadinessResult::Unhealthy;
#endif
}
bool NativeLaunchAdapter::stop_owned() {
#ifdef _WIN32
    if (m->job.valid() && !TerminateJobObject(m->job.value, 0)) return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    if (m->process.valid()) {
        if (WaitForSingleObject(m->process.value, 0) == WAIT_TIMEOUT && !TerminateProcess(m->process.value, 0)) return false;
        if (WaitForSingleObject(m->process.value, 5000) != WAIT_OBJECT_0) return false;
        m->process.reset();
    }
    if (m->job.valid()) {
        while (true) {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
            if (!QueryInformationJobObject(m->job.value, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr)) return false;
            if (accounting.ActiveProcesses == 0) break;
            if (std::chrono::steady_clock::now() >= deadline) return false;
            Sleep(10);
        }
    }
    m->job.reset(); m->executable_lock.reset(); m->model_lock.reset(); m->ready_verified = false;
    m->launch_path_locks.clear();
    return true;
#else
    return true;
#endif
}
bool NativeLaunchAdapter::restore(const RedactedLaunchSnapshot& state) {
#ifdef _WIN32
    if (!available() || m->process.valid() || !m->journal || !NativeLaunchDetail::safe_snapshot(state) || !m->write(L"current.state", state)) return false;
    if (!m->delete_journal()) return false;
    m->before.reset(); m->applied.reset(); return true;
#else
    (void)state; return false;
#endif
}
bool NativeLaunchAdapter::recovery_pending() const {
#ifdef _WIN32
    return m->journal;
#else
    return false;
#endif
}
bool NativeLaunchAdapter::recover_interrupted() {
#ifdef _WIN32
    if (!available() || !m->journal) return false;
    auto previous = m->read(L"rollback.state", false);
    return previous && stop_owned() && restore(*previous);
#else
    return false;
#endif
}
void NativeLaunchAdapter::request_cancel() noexcept {
    m->stop_requested.store(true, std::memory_order_relaxed);
    m->cancellation.fetch_add(1, std::memory_order_relaxed);
}
}} // namespace Slic3r::OllamaSuite
