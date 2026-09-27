#include "ProcessRunner.hpp"

#include <chrono>
#include <atomic>
#include <fstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <map>

namespace Slic3r::GUI::ModelCreator {
namespace {

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};

std::wstring environment_value(const wchar_t *key)
{
    const DWORD length = GetEnvironmentVariableW(key, nullptr, 0);
    if (!length) return {};
    std::wstring value(length, L'\0');
    const DWORD written = GetEnvironmentVariableW(key, value.data(), length);
    if (!written || written >= length) return {};
    value.resize(written);
    return value;
}

struct CaseLess {
    bool operator()(const std::wstring &a, const std::wstring &b) const
    {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
            [](wchar_t x, wchar_t y) { return std::towlower(x) < std::towlower(y); });
    }
};

std::vector<wchar_t> isolated_environment(const std::filesystem::path &directory, ProcessRole role)
{
    std::map<std::wstring, std::wstring, CaseLess> vars;
    for (const wchar_t *key : {L"SystemRoot", L"WINDIR", L"PATH", L"PATHEXT", L"COMSPEC",
                               L"PROCESSOR_ARCHITECTURE", L"NUMBER_OF_PROCESSORS"}) {
        const auto value = environment_value(key);
        if (!value.empty()) vars[key] = value;
    }
    const auto original_profile = environment_value(L"USERPROFILE");
    const auto sandbox = directory / "home";
    std::filesystem::create_directories(sandbox / "AppData" / "Roaming");
    std::filesystem::create_directories(sandbox / "AppData" / "Local");
    std::filesystem::create_directories(sandbox / "Temp");
    vars[L"HOME"] = sandbox.wstring();
    vars[L"USERPROFILE"] = sandbox.wstring();
    vars[L"APPDATA"] = (sandbox / "AppData" / "Roaming").wstring();
    vars[L"LOCALAPPDATA"] = (sandbox / "AppData" / "Local").wstring();
    vars[L"TEMP"] = (sandbox / "Temp").wstring();
    vars[L"TMP"] = vars[L"TEMP"];
    vars[L"NO_COLOR"] = L"1";
    // The CLI's supported sign-in location remains readable. Its custom
    // settings are disabled by CLI flags; no credential bytes are copied.
    if (role == ProcessRole::ClaudeCli) {
        auto auth = environment_value(L"CLAUDE_CONFIG_DIR");
        if (auth.empty()) auth = original_profile + L"\\.claude";
        vars[L"CLAUDE_CONFIG_DIR"] = auth;
    } else if (role == ProcessRole::CodexCli) {
        auto auth = environment_value(L"CODEX_HOME");
        if (auth.empty()) auth = original_profile + L"\\.codex";
        vars[L"CODEX_HOME"] = auth;
    } else {
        vars[L"BLENDER_USER_CONFIG"] = (sandbox / "Blender").wstring();
    }
    std::vector<wchar_t> block;
    for (const auto &entry : vars) {
        const std::wstring row = entry.first + L"=" + entry.second;
        block.insert(block.end(), row.begin(), row.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

std::wstring quote_arg(const std::wstring &arg)
{
    if (arg.empty()) return L"\"\"";
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
    std::wstring result = L"\"";
    size_t slash_count = 0;
    for (const wchar_t ch : arg) {
        if (ch == L'\\') { ++slash_count; continue; }
        if (ch == L'\"') {
            result.append(slash_count * 2 + 1, L'\\');
            result.push_back(L'\"');
        } else {
            result.append(slash_count, L'\\');
            result.push_back(ch);
        }
        slash_count = 0;
    }
    result.append(slash_count * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

std::wstring command_line(const std::filesystem::path &program, const std::vector<std::string> &args)
{
    std::wstring line = quote_arg(program.wstring());
    for (const auto &arg : args) {
        line.push_back(L' ');
        line += quote_arg(std::filesystem::path(arg).wstring());
    }
    return line;
}

bool job_empty(HANDLE job)
{
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    return QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info,
                                     sizeof(info), nullptr) && info.ActiveProcesses == 0;
}

constexpr size_t max_cli_output = 256 * 1024;
constexpr size_t max_renderer_log = 1024 * 1024;

size_t output_limit(ProcessRole role)
{
    return role == ProcessRole::Renderer ? max_renderer_log : max_cli_output;
}

} // namespace

bool run_isolated_process(const std::filesystem::path &program,
                          const std::vector<std::string> &args,
                          const std::filesystem::path &input,
                          const std::filesystem::path &output,
                          const std::filesystem::path &directory,
                          int timeout_seconds, std::atomic_bool &cancel,
                          ProcessRole role, std::string &error)
{
    if (!std::filesystem::is_regular_file(program) || !std::filesystem::is_directory(directory)) {
        error = "Executable or revision directory not found"; return false;
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle in(CreateFileW(input.c_str(), GENERIC_READ, FILE_SHARE_READ, &security, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, nullptr));
    Handle out(CreateFileW(output.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr));
    HANDLE pipe_read = nullptr, pipe_write = nullptr;
    if (!CreatePipe(&pipe_read, &pipe_write, &security, 0)) {
        error = "Could not prepare bounded process output"; return false;
    }
    Handle read(pipe_read), write(pipe_write);
    if (!SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0)) {
        error = "Could not isolate process output"; return false;
    }
    Handle err(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (in.value == INVALID_HANDLE_VALUE || out.value == INVALID_HANDLE_VALUE || err.value == INVALID_HANDLE_VALUE) {
        error = "Could not prepare isolated process streams"; return false;
    }
    SIZE_T attribute_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
    std::vector<unsigned char> attribute_storage(attribute_size);
    auto *attributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size)) {
        error = "Could not isolate inherited handles"; return false;
    }
    HANDLE inherited[] = {in.value, write.value, err.value};
    const bool handles_ready = UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                         inherited, sizeof(inherited), nullptr, nullptr);
    if (!handles_ready) { DeleteProcThreadAttributeList(attributes); error = "Could not limit inherited handles"; return false; }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = in.value;
    startup.StartupInfo.hStdOutput = write.value;
    startup.StartupInfo.hStdError = err.value;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    auto line = command_line(program, args);
    std::vector<wchar_t> mutable_line(line.begin(), line.end());
    mutable_line.push_back(L'\0');
    auto environment = isolated_environment(directory, role);
    const DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT |
                        CREATE_UNICODE_ENVIRONMENT;
    const BOOL launched = CreateProcessW(program.c_str(), mutable_line.data(), nullptr, nullptr, TRUE,
                                         flags, environment.data(), directory.c_str(),
                                         &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(attributes);
    if (!launched) { error = "Could not start isolated process"; return false; }
    Handle thread(process.hThread);
    Handle child(process.hProcess);
    Handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                              JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.BasicLimitInformation.ActiveProcessLimit = 16;
    if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation,
                                               &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job.value, child.value)) {
        TerminateProcess(child.value, 1);
        WaitForSingleObject(child.value, 5000);
        error = "Could not contain process tree"; return false;
    }
    if (ResumeThread(thread.value) == static_cast<DWORD>(-1)) {
        TerminateJobObject(job.value, 1);
        WaitForSingleObject(child.value, 5000);
        error = "Could not resume contained process"; return false;
    }
    CloseHandle(write.value);
    write.value = nullptr;
    std::atomic_bool output_overflow{false};
    std::atomic_bool output_io_error{false};
    std::thread collector([&] {
        char buffer[8192];
        size_t written = 0;
        DWORD received = 0;
        while (ReadFile(read.value, buffer, sizeof(buffer), &received, nullptr) && received > 0) {
            const size_t allowed = std::min<size_t>(received, output_limit(role) - written);
            DWORD saved = 0;
            if (allowed && (!WriteFile(out.value, buffer, static_cast<DWORD>(allowed), &saved, nullptr) || saved != allowed)) {
                output_io_error.store(true);
                return;
            }
            written += allowed;
            if (allowed != received) {
                output_overflow.store(true);
                return;
            }
        }
    });
    const auto direct_output_overflow = [&] {
        if (role != ProcessRole::CodexCli) return false;
        std::error_code file_error;
        const auto size = std::filesystem::file_size(directory / "response.txt", file_error);
        return !file_error && size > 65536;
    };
    bool direct_overflow = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    while (!job_empty(job.value) && !cancel && !output_overflow && !output_io_error &&
           !(direct_overflow = direct_output_overflow()) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    direct_overflow |= direct_output_overflow();
    if (cancel || output_overflow || output_io_error || direct_overflow ||
        std::chrono::steady_clock::now() >= deadline) {
        TerminateJobObject(job.value, 1);
        const auto stop_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!job_empty(job.value) && std::chrono::steady_clock::now() < stop_deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (!job_empty(job.value)) CancelSynchronousIo(collector.native_handle());
        collector.join();
        error = !job_empty(job.value) ? "Process tree did not stop" :
                cancel ? "Canceled" : (output_overflow || direct_overflow) ? "Process output exceeded its limit" :
                output_io_error ? "Could not retain process output" : "Process timed out";
        return false;
    }
    collector.join();
    if (output_overflow || output_io_error) {
        error = output_overflow ? "Process output exceeded its limit" : "Could not retain process output";
        return false;
    }
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(child.value, &exit_code) || exit_code != 0) {
        error = "Process exited without a model"; return false;
    }
    return true;
}

} // namespace Slic3r::GUI::ModelCreator

#else

namespace Slic3r::GUI::ModelCreator {
bool run_isolated_process(const std::filesystem::path &, const std::vector<std::string> &,
                          const std::filesystem::path &, const std::filesystem::path &,
                          const std::filesystem::path &, int, std::atomic_bool &,
                          ProcessRole, std::string &error)
{
    error = "Isolated Model Creator processes require Windows";
    return false;
}
} // namespace Slic3r::GUI::ModelCreator
#endif
