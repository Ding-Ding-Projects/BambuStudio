#include "Worker.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <future>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <userenv.h>
#include <sddl.h>
#endif

namespace Slic3r::LocalConverter {
#ifdef _WIN32
namespace {
struct Handle {
    HANDLE h = nullptr;
    ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    Handle() = default;
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    void close() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); h = nullptr; }
};
bool write_all(HANDLE h, const void *data, std::size_t count)
{
    const auto *p = static_cast<const unsigned char *>(data);
    while (count) { DWORD n = 0; if (!WriteFile(h,p,static_cast<DWORD>(std::min<std::size_t>(count,65536)),&n,nullptr) || !n) return false; count -= n; p += n; }
    return true;
}
bool read_all(HANDLE h, void *data, std::size_t count)
{
    auto *p = static_cast<unsigned char *>(data);
    while (count) { DWORD n = 0; if (!ReadFile(h,p,static_cast<DWORD>(std::min<std::size_t>(count,65536)),&n,nullptr) || !n) return false; count -= n; p += n; }
    return true;
}
}
#endif
Conversion isolated_transform(const PackageProof &proof, const std::string &adapter, const Bytes &source, const std::atomic<bool> &cancel)
{
    auto fail = [](const char *code) { return Conversion{Outcome::Failed,code,{}}; };
    if (source.size() > Limits::wire_bytes || adapter.size() >= 64 || adapter.find('\0') != std::string::npos) return fail("request_limit");
#ifdef _WIN32
    Handle pinned_worker;
    pinned_worker.h=CreateFileW(proof.worker.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(pinned_worker.h==INVALID_HANDLE_VALUE)return fail("bundled_worker_unavailable");
#endif
    std::string reason;
    if (!verify_package(proof,reason)) return fail("bundled_worker_unverified");
    if (cancel.load()) return {Outcome::Cancelled,"cancelled",{}};
#ifdef _WIN32
    PSID sid = nullptr;
    HRESULT hr = CreateAppContainerProfile(L"BambuStudio.LocalConverter.v1",L"Bambu Studio Local Converter",L"Offline file conversion with brokered byte streams",nullptr,0,&sid);
    if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) hr = DeriveAppContainerSidFromAppContainerName(L"BambuStudio.LocalConverter.v1",&sid);
    if (FAILED(hr)) return fail("appcontainer_unavailable");
    struct SidOwner { PSID sid; ~SidOwner() { FreeSid(sid); } } sid_owner{sid};
    SECURITY_CAPABILITIES capabilities{}; capabilities.AppContainerSid = sid;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;SY)(A;;GA;;;OW)(A;;GRGW;;;AC)S:(ML;;NW;;;LW)",SDDL_REVISION_1,&descriptor,nullptr)) return fail("pipe_security_unavailable");
    struct DescriptorOwner { PSECURITY_DESCRIPTOR p; ~DescriptorOwner(){ LocalFree(p); } } desc_owner{descriptor};
    SECURITY_ATTRIBUTES sa{sizeof(sa),descriptor,TRUE};
    Handle child_in,parent_in,parent_out,child_out;
    if (!CreatePipe(&child_in.h,&parent_in.h,&sa,65536) || !CreatePipe(&parent_out.h,&child_out.h,&sa,65536)) return fail("pipe_unavailable");
    if (!SetHandleInformation(parent_in.h,HANDLE_FLAG_INHERIT,0) || !SetHandleInformation(parent_out.h,HANDLE_FLAG_INHERIT,0)) return fail("pipe_isolation_failed");
    Handle job;
    job.h = CreateJobObjectW(nullptr,nullptr); if (!job.h) return fail("containment_unavailable");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_PROCESS_TIME;
    limits.BasicLimitInformation.ActiveProcessLimit = 1;
    limits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = std::int64_t(Limits::seconds) * 10000000;
    limits.ProcessMemoryLimit = 256 * 1024 * 1024;
    if (!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) return fail("containment_limits_failed");
    SIZE_T size = 0; InitializeProcThreadAttributeList(nullptr,2,0,&size);
    std::vector<unsigned char> buffer(size);
    auto *attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
    if (!InitializeProcThreadAttributeList(attributes,2,0,&size)) return fail("containment_attributes_failed");
    struct AttributeOwner { LPPROC_THREAD_ATTRIBUTE_LIST p; ~AttributeOwner(){ DeleteProcThreadAttributeList(p); } } attrs{attributes};
    HANDLE inherited[]{child_in.h,child_out.h};
    if (!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,&capabilities,sizeof(capabilities),nullptr,nullptr) ||
        !UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)) return fail("containment_attributes_failed");
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup); startup.lpAttributeList = attributes;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES; startup.StartupInfo.hStdInput = child_in.h; startup.StartupInfo.hStdOutput = child_out.h; startup.StartupInfo.hStdError = child_out.h;
    PROCESS_INFORMATION process{};
    auto command = L"\"" + proof.worker.wstring() + L"\"";
    // No inherited environment, working directory selected by the broker, no
    // shell, no user-authored arguments, no network capabilities.
    std::array<wchar_t,32768> windows_directory{};
    const auto directory_length = GetWindowsDirectoryW(windows_directory.data(),static_cast<UINT>(windows_directory.size()));
    if (!directory_length || directory_length >= windows_directory.size()) return fail("system_directory_unavailable");
    std::wstring environment = L"SystemRoot=" + std::wstring(windows_directory.data());
    environment.push_back(0); environment += L"windir="; environment += windows_directory.data();
    environment.push_back(0);
    // AppContainer creation expands these profile locations before lowering
    // access. Only these OS location variables are inherited, never PATH,
    // proxy variables, credentials or application environment settings.
    for (const auto *key : {L"USERPROFILE",L"LOCALAPPDATA",L"APPDATA",L"TEMP",L"TMP",L"ProgramData"}) {
        std::array<wchar_t,32768> value{};
        const auto n = GetEnvironmentVariableW(key,value.data(),static_cast<DWORD>(value.size()));
        if (n && n < value.size()) { environment += key; environment += L"="; environment += value.data(); environment.push_back(0); }
    }
    environment.push_back(0);
    if (!CreateProcessW(proof.worker.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                        environment.data(),proof.installed_directory.c_str(),&startup.StartupInfo,&process))
        return {Outcome::Failed,"isolated_worker_start_" + std::to_string(GetLastError()),{}};
    Handle proc,thread; proc.h = process.hProcess; thread.h = process.hThread;
    if (!AssignProcessToJobObject(job.h,proc.h)) { TerminateProcess(proc.h,26); return fail("worker_job_assignment_failed"); }
    child_in.close(); child_out.close();
    if (ResumeThread(thread.h) == DWORD(-1)) { TerminateJobObject(job.h,27); return fail("worker_resume_failed"); }
    std::array<char,64> name{}; std::copy(adapter.begin(),adapter.end(),name.begin()); const std::uint64_t input_size = source.size();
    auto writer = std::async(std::launch::async,[&] {
        const bool ok = write_all(parent_in.h,name.data(),name.size()) && write_all(parent_in.h,&input_size,sizeof(input_size)) && write_all(parent_in.h,source.data(),source.size());
        parent_in.close(); return ok;
    });
    auto reader = std::async(std::launch::async,[&] {
        std::uint32_t status = 0; std::uint64_t output_size = 0; std::array<char,96> code{};
        if (!read_all(parent_out.h,&status,sizeof(status)) || !read_all(parent_out.h,code.data(),code.size()) || !read_all(parent_out.h,&output_size,sizeof(output_size)) ||
            status > static_cast<unsigned>(Outcome::Failed) || output_size > Limits::output_bytes || code.back() != 0) return fail("invalid_worker_response");
        // Diagnostics are stable machine codes, never arbitrary child output.
        const std::string diagnostic(code.data());
        if (diagnostic.empty() || diagnostic.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789") != std::string::npos) return fail("invalid_worker_diagnostic");
        Bytes out(static_cast<std::size_t>(output_size));
        if (!read_all(parent_out.h,out.data(),out.size())) return fail("truncated_worker_response");
        unsigned char extra; DWORD n = 0;
        if (ReadFile(parent_out.h,&extra,1,&n,nullptr) && n) return fail("excess_worker_response");
        return Conversion{static_cast<Outcome>(status),diagnostic,std::move(out)};
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(Limits::seconds);
    bool expired = false;
    while (WaitForSingleObject(proc.h,20) == WAIT_TIMEOUT) {
        if (cancel.load() || std::chrono::steady_clock::now() >= deadline) { expired = true; TerminateJobObject(job.h,28); break; }
    }
    if (expired) { writer.wait(); reader.wait(); return cancel.load() ? Conversion{Outcome::Cancelled,"cancelled",{}} : fail("worker_deadline"); }
    const bool sent = writer.get(); auto result = reader.get(); DWORD exit = 1;
    if (!sent || !GetExitCodeProcess(proc.h,&exit) || exit != 0) return fail("worker_failed");
    return result;
#else
    return fail("sandbox_unavailable");
#endif
}
} // namespace Slic3r::LocalConverter
