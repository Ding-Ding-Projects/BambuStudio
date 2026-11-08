#include "DiskSystem.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <virtdisk.h>
#ifndef ERROR_VIRTDISK_NOT_VIRTUAL_DISK
#define ERROR_VIRTDISK_NOT_VIRTUAL_DISK 0xC03A0015L
#endif
#endif

namespace Slic3r { namespace GUI { namespace VirtualDisks {

namespace fs = std::filesystem;

namespace {

std::string lower_text(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

bool disk_extension(const fs::path &p)
{
    const std::string ext = lower_text(p.extension().u8string());
    return ext == ".vhd" || ext == ".vhdx" || ext == ".avhdx" || ext == ".avhd" || ext == ".vhds" || ext == ".iso";
}

#ifdef _WIN32
std::string narrow(const std::wstring &w)
{
    if (w.empty()) return std::string();
    const int   n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(std::size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::string registry_text(const wchar_t *name)
{
    wchar_t buffer[256] = {};
    DWORD   size        = sizeof(buffer);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", name, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS)
        return std::string();
    return narrow(buffer);
}

fs::path system_folder()
{
    wchar_t buffer[MAX_PATH] = {};
    GetSystemDirectoryW(buffer, MAX_PATH);
    return fs::path(buffer);
}

// The mount points of a volume ("E:\", "D:\Mounts\Projects\").
std::vector<std::string> mount_points_of(const std::wstring &volume_guid_path)
{
    std::vector<std::string> out;
    DWORD                    size = 0;
    GetVolumePathNamesForVolumeNameW(volume_guid_path.c_str(), nullptr, 0, &size);
    if (size == 0) return out;
    std::wstring names(size, L'\0');
    if (!GetVolumePathNamesForVolumeNameW(volume_guid_path.c_str(), names.data(), size, &size)) return out;
    for (const wchar_t *p = names.c_str(); *p; p += wcslen(p) + 1) out.push_back(narrow(p));
    return out;
}
#endif

} // namespace

std::string path_key(const std::string &path)
{
    std::string p = path;
    std::replace(p.begin(), p.end(), '/', '\\');
    // Windows may answer with a device-style prefix: \\?\D:\x.vhdx or \??\D:\x.vhdx.
    for (const char *prefix : {"\\\\?\\", "\\??\\"})
        if (p.rfind(prefix, 0) == 0) p = p.substr(std::string(prefix).size());
    return lower_text(p);
}

Routes find_routes()
{
    Routes r;
#ifdef _WIN32
    r.windows               = true;
    std::string product     = registry_text(L"ProductName");
    const std::string build = registry_text(L"CurrentBuildNumber");
    // Windows 11 still names itself Windows 10 in this key; the build tells them apart.
    if (!build.empty() && std::atoi(build.c_str()) >= 22000 && product.rfind("Windows 10", 0) == 0) product.replace(0, 10, "Windows 11");
    r.windows_name = product.empty() ? std::string("Windows") : product;
    if (!build.empty()) r.windows_name += ", build " + build;
    if (HMODULE m = LoadLibraryW(L"virtdisk.dll")) {
        r.virtdisk = true;
        FreeLibrary(m);
    }
    const fs::path sys = system_folder();
    std::error_code ec;
    r.storage  = fs::exists(sys / "WindowsPowerShell" / "v1.0" / "Modules" / "Storage", ec);
    r.diskpart = fs::exists(sys / "diskpart.exe", ec);
    r.robocopy = fs::exists(sys / "Robocopy.exe", ec);
    r.hyperv   = fs::exists(sys / "WindowsPowerShell" / "v1.0" / "Modules" / "Hyper-V", ec);
#endif
    return r;
}

std::map<std::string, Attachment> attached_disks(bool &available, std::string *diagnostic)
{
    std::map<std::string, Attachment> out;
    available = false;
    const auto note = [diagnostic](const std::string &text) {
        if (diagnostic) *diagnostic += (diagnostic->empty() ? "" : "; ") + text;
    };
#ifdef _WIN32
    HMODULE virtdisk = LoadLibraryW(L"virtdisk.dll");
    if (!virtdisk) {
        note("virtdisk.dll is missing");
        return out;
    }
    // Every attached disk's file, without administrator permission (Windows 8 and later).
    using AllAttached = DWORD(WINAPI *)(PULONG, PWSTR);
    if (auto all = reinterpret_cast<AllAttached>(reinterpret_cast<void *>(GetProcAddress(virtdisk, "GetAllAttachedVirtualDiskPhysicalPaths")))) {
        std::vector<wchar_t> buffer(32 * 1024, L'\0');
        ULONG                size   = ULONG(buffer.size() * sizeof(wchar_t));
        DWORD                result = all(&size, buffer.data());
        if (result == ERROR_INSUFFICIENT_BUFFER && size > 0) {
            buffer.assign(size / sizeof(wchar_t) + 2, L'\0');
            size   = ULONG(buffer.size() * sizeof(wchar_t));
            result = all(&size, buffer.data());
        }
        note("attached list " + std::to_string(result));
        if (result == ERROR_SUCCESS) {
            available = true;
            for (const wchar_t *p = buffer.data(); !buffer.empty() && *p; p += wcslen(p) + 1) out[path_key(narrow(p))].attached = true;
        }
    }
    // Where each attached disk's volumes are mounted: every volume on this
    // computer is asked which file it lives in.
    using Dependency = DWORD(WINAPI *)(HANDLE, GET_STORAGE_DEPENDENCY_FLAG, ULONG, PSTORAGE_DEPENDENCY_INFO, PULONG);
    if (auto depend = reinterpret_cast<Dependency>(reinterpret_cast<void *>(GetProcAddress(virtdisk, "GetStorageDependencyInformation")))) {
        wchar_t volume[MAX_PATH] = {};
        HANDLE  find             = FindFirstVolumeW(volume, MAX_PATH);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                std::wstring device = volume;
                if (!device.empty() && device.back() == L'\\') device.pop_back();
                std::vector<std::uint8_t> info(64 * 1024);
                auto *dep  = reinterpret_cast<PSTORAGE_DEPENDENCY_INFO>(info.data());
                ULONG used = 0;
                DWORD got  = ERROR_INVALID_HANDLE;
                HANDLE h   = INVALID_HANDLE_VALUE;
                // Some systems answer only on a handle opened for reading.
                for (DWORD access : {DWORD(0), DWORD(GENERIC_READ)}) {
                    h = CreateFileW(device.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                    if (h == INVALID_HANDLE_VALUE) continue;
                    std::fill(info.begin(), info.end(), std::uint8_t(0));
                    dep->Version = STORAGE_DEPENDENCY_INFO_VERSION_2;
                    got          = depend(h, GET_STORAGE_DEPENDENCY_FLAG_HOST_VOLUMES, ULONG(info.size()), dep, &used);
                    if (got == ERROR_SUCCESS) break;
                    CloseHandle(h);
                    h = INVALID_HANDLE_VALUE;
                }
                if (got != ERROR_SUCCESS && got != DWORD(ERROR_VIRTDISK_NOT_VIRTUAL_DISK) && got != ERROR_NOT_SUPPORTED && got != ERROR_INVALID_FUNCTION)
                    note("volume " + narrow(volume) + " " + std::to_string(got));
                if (h == INVALID_HANDLE_VALUE) continue;
                if (got == ERROR_SUCCESS) {
                    for (ULONG i = 0; i < dep->NumberEntries; ++i) {
                        const STORAGE_DEPENDENCY_INFO_TYPE_2 &e = dep->Version2Entries[i];
                        // The nearest host is the file this volume lives in; deeper levels are its parents.
                        if (e.AncestorLevel > 1 || !e.HostVolumeName || !e.DependentVolumeRelativePath) continue;
                        const std::vector<std::string> hosts = mount_points_of(e.HostVolumeName);
                        if (hosts.empty()) continue;
                        std::wstring relative = e.DependentVolumeRelativePath;
                        while (!relative.empty() && relative.front() == L'\\') relative.erase(relative.begin());
                        Attachment &a = out[path_key(hosts.front() + narrow(relative))];
                        a.attached    = true;
                        for (const std::string &m : mount_points_of(volume)) a.mount_points.push_back(m);
                        available = true;
                    }
                }
                CloseHandle(h);
            } while (FindNextVolumeW(find, volume, MAX_PATH));
            FindVolumeClose(find);
        }
    }
    // Attached disks still without a mount point: their disk number, and the
    // volumes whose extents lie on that disk (needs the disk to open for its info).
    using Open     = DWORD(WINAPI *)(PVIRTUAL_STORAGE_TYPE, PCWSTR, VIRTUAL_DISK_ACCESS_MASK, OPEN_VIRTUAL_DISK_FLAG, POPEN_VIRTUAL_DISK_PARAMETERS, PHANDLE);
    using Physical = DWORD(WINAPI *)(HANDLE, PULONG, PWSTR);
    auto open_disk = reinterpret_cast<Open>(reinterpret_cast<void *>(GetProcAddress(virtdisk, "OpenVirtualDisk")));
    auto physical  = reinterpret_cast<Physical>(reinterpret_cast<void *>(GetProcAddress(virtdisk, "GetVirtualDiskPhysicalPath")));
    std::map<DWORD, std::vector<std::string>> by_disk;
    bool volumes_read = false;
    for (auto &entry : out) {
        if (!entry.second.mount_points.empty() || !open_disk || !physical) continue;
        if (!volumes_read) {
            volumes_read            = true;
            wchar_t volume[MAX_PATH] = {};
            HANDLE  find             = FindFirstVolumeW(volume, MAX_PATH);
            if (find != INVALID_HANDLE_VALUE) {
                do {
                    std::wstring device = volume;
                    if (!device.empty() && device.back() == L'\\') device.pop_back();
                    HANDLE h = CreateFileW(device.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                    if (h == INVALID_HANDLE_VALUE) continue;
                    std::vector<std::uint8_t> extents(4096);
                    DWORD                     got = 0;
                    if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, extents.data(), DWORD(extents.size()), &got, nullptr)) {
                        const auto *e = reinterpret_cast<const VOLUME_DISK_EXTENTS *>(extents.data());
                        for (DWORD k = 0; k < e->NumberOfDiskExtents; ++k) {
                            const std::vector<std::string> mounts = mount_points_of(volume);
                            for (const std::string &m : mounts) by_disk[e->Extents[k].DiskNumber].push_back(m);
                            if (diagnostic) note("disk " + std::to_string(e->Extents[k].DiskNumber) + (mounts.empty() ? std::string(" unmounted") : " at " + mounts.front()));
                        }
                    }
                    CloseHandle(h);
                } while (FindNextVolumeW(find, volume, MAX_PATH));
                FindVolumeClose(find);
            }
        }
        // The map holds lower-case keys; Windows matches file names without regard to case.
        const std::string key  = entry.first;
        const int         n    = MultiByteToWideChar(CP_UTF8, 0, key.data(), int(key.size()), nullptr, 0);
        std::wstring      wkey(std::size_t(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, key.data(), int(key.size()), wkey.data(), n);
        VIRTUAL_STORAGE_TYPE type{};
        HANDLE               h = nullptr;
        const DWORD opened = open_disk(&type, wkey.c_str(), VIRTUAL_DISK_ACCESS_GET_INFO, OPEN_VIRTUAL_DISK_FLAG_NONE, nullptr, &h);
        if (opened != ERROR_SUCCESS) {
            note("open " + key + " " + std::to_string(opened));
            continue;
        }
        wchar_t     path[MAX_PATH] = {};
        ULONG       size           = sizeof(path);
        const DWORD named          = physical(h, &size, path);
        note(key + " is " + (named == ERROR_SUCCESS ? narrow(path) : "error " + std::to_string(named)));
        if (named == ERROR_SUCCESS) {
            const wchar_t *digits = path + wcslen(path);
            while (digits > path && iswdigit(digits[-1])) --digits;
            if (*digits) {
                const DWORD number = DWORD(wcstoul(digits, nullptr, 10));
                for (const std::string &m : by_disk[number]) entry.second.mount_points.push_back(m);
            }
        }
        CloseHandle(h);
    }
    FreeLibrary(virtdisk);
#endif
    return out;
}

std::uint64_t free_space(const std::string &path)
{
    std::error_code ec;
    fs::path        p = fs::u8path(path);
    while (!p.empty() && !fs::exists(p, ec)) {
        const fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    const fs::space_info s = fs::space(p.empty() ? fs::current_path(ec) : p, ec);
    return ec ? 0 : std::uint64_t(s.available);
}

std::vector<std::string> find_disks(const std::string &folder, int depth)
{
    std::vector<std::string> out;
    std::error_code          ec;
    fs::recursive_directory_iterator it(fs::u8path(folder), fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end && out.size() < 2000; it.increment(ec)) {
        if (it.depth() >= depth) it.disable_recursion_pending();
        std::error_code e2;
        if (it->is_regular_file(e2) && disk_extension(it->path())) out.push_back(it->path().u8string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

FolderStats folder_stats(const std::string &folder, const std::atomic<bool> *cancel)
{
    FolderStats     s;
    std::error_code ec;
    fs::recursive_directory_iterator it(fs::u8path(folder), fs::directory_options::skip_permission_denied, ec), end;
    if (ec) s.complete = false;
    for (; !ec && it != end; it.increment(ec)) {
        if (cancel && cancel->load()) {
            s.complete = false;
            break;
        }
        std::error_code e2;
        if (it->is_symlink(e2)) {
            it.disable_recursion_pending(); // junctions and links are not followed, as Robocopy /XJ does
            continue;
        }
        if (it->is_directory(e2)) ++s.folders;
        else if (it->is_regular_file(e2)) {
            ++s.files;
            s.bytes += it->file_size(e2);
        }
    }
    if (ec) s.complete = false;
    return s;
}

RunOutcome run_elevated(const Script &script, const std::string &work_dir, const std::function<void(const std::string &)> &progress,
                        const std::atomic<bool> *cancel, void *owner_window)
{
    RunOutcome out;
#ifdef _WIN32
    std::error_code ec;
    const fs::path  dir = fs::u8path(work_dir);
    fs::create_directories(dir, ec);
    const auto     stamp  = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const fs::path ps1    = dir / ("run-" + stamp + ".ps1");
    const fs::path result = dir / ("run-" + stamp + ".result.json");
    const fs::path steps  = dir / ("run-" + stamp + ".progress.txt");
    const fs::path stop   = dir / ("run-" + stamp + ".cancel");
    {
        // A byte order mark, so Windows PowerShell reads the paths as UTF-8.
        std::ofstream f(ps1, std::ios::binary | std::ios::trunc);
        f << "\xEF\xBB\xBF" << runner_script(script, result.u8string(), steps.u8string(), stop.u8string());
        if (!f) {
            out.status  = RunOutcome::Status::Failed;
            out.message = "The script could not be written to " + dir.u8string() + ".";
            return out;
        }
    }
    const HRESULT      com    = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const std::wstring params = L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + ps1.wstring() + L"\"";
    SHELLEXECUTEINFOW  sei{};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.hwnd         = static_cast<HWND>(owner_window);
    sei.lpVerb       = L"runas";
    sei.lpFile       = L"powershell.exe";
    sei.lpParameters = params.c_str();
    sei.nShow        = SW_HIDE;
    const BOOL started = ShellExecuteExW(&sei);
    const DWORD error  = started ? 0 : GetLastError();
    if (!started) {
        fs::remove(ps1, ec);
        if (SUCCEEDED(com)) CoUninitialize();
        out.status  = error == ERROR_CANCELLED ? RunOutcome::Status::PermissionCancelled : RunOutcome::Status::Failed;
        out.message = error == ERROR_CANCELLED ? "Permission cancelled: nothing changed." : "Windows could not start PowerShell (error " + std::to_string(error) + ").";
        return out;
    }
    std::uint64_t read_to   = 0;
    bool          asked     = false;
    const auto    read_more = [&]() {
        std::ifstream in(steps, std::ios::binary);
        if (!in) return;
        in.seekg(std::streamoff(read_to));
        std::string line;
        while (std::getline(in, line)) {
            if (in.eof()) break; // a line still being written
            read_to += line.size() + 1;
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line = line.substr(3);
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (!line.empty() && progress) progress(line);
        }
    };
    while (sei.hProcess && WaitForSingleObject(sei.hProcess, 250) == WAIT_TIMEOUT) {
        read_more();
        if (cancel && cancel->load() && !asked) {
            std::ofstream(stop).put('1');
            asked = true;
        }
    }
    read_more();
    if (sei.hProcess) CloseHandle(sei.hProcess);
    if (SUCCEEDED(com)) CoUninitialize();
    std::ifstream in(result, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    in.close();
    const nlohmann::json j = nlohmann::json::parse(text.str(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        out.status  = RunOutcome::Status::Failed;
        out.message = "PowerShell ended without reporting a result.";
    } else {
        const bool        ok      = j.value("ok", false);
        const std::string message = j.value("message", std::string());
        out.status  = ok ? RunOutcome::Status::Done : message == "Cancelled" ? RunOutcome::Status::Cancelled : RunOutcome::Status::Failed;
        out.message = message;
        if (j.contains("extra") && !j["extra"].is_null()) out.extra_json = j["extra"].dump();
    }
    for (const fs::path &p : {ps1, result, steps, stop}) fs::remove(p, ec);
#else
    (void) script, (void) work_dir, (void) progress, (void) cancel, (void) owner_window;
    out.status  = RunOutcome::Status::Unavailable;
    out.message = "Virtual disks need Windows.";
#endif
    return out;
}

}}} // namespace Slic3r::GUI::VirtualDisks
