#include "ExtensionProcess.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace Slic3r { namespace GUI { namespace Extensions {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kErrorKeep = 4096;

bool leaves_folder(const fs::path &relative)
{
    for (const fs::path &part : relative)
        if (part == "..") return true;
    return false;
}

bool is_file(const fs::path &p)
{
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

std::vector<fs::path> path_directories()
{
    std::vector<fs::path> dirs;
#ifdef _WIN32
    const wchar_t *value = _wgetenv(L"PATH");
    if (!value) return dirs;
    std::wstring all(value);
    std::size_t  start = 0;
    while (start <= all.size()) {
        const std::size_t end = all.find(L';', start);
        std::wstring      dir = all.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!dir.empty()) dirs.emplace_back(dir);
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
#else
    const char *value = std::getenv("PATH");
    if (!value) return dirs;
    std::string all(value);
    std::size_t start = 0;
    while (start <= all.size()) {
        const std::size_t end = all.find(':', start);
        std::string       dir = all.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty()) dirs.emplace_back(dir);
        if (end == std::string::npos) break;
        start = end + 1;
    }
#endif
    return dirs;
}

fs::path search_directory(const fs::path &dir, const fs::path &name)
{
    if (is_file(dir / name)) return dir / name;
#ifdef _WIN32
    if (!name.has_extension() && is_file(dir / fs::path(name.wstring() + L".exe"))) return dir / fs::path(name.wstring() + L".exe");
#endif
    return {};
}

} // namespace

fs::path resolve_program(const fs::path &folder, const std::string &program, std::string &error)
{
    error.clear();
    const fs::path name = fs::u8path(program);
    if (program.empty()) {
        error = "no program";
        return {};
    }
    if (name.is_absolute()) {
        if (is_file(name)) return name;
        error = "program not found";
        return {};
    }
    if (leaves_folder(name)) {
        error = "program path leaves the extension folder";
        return {};
    }
    if (const fs::path inside = search_directory(folder, name); !inside.empty()) return inside;
    if (name.has_parent_path()) {
        error = "program not found in the extension folder";
        return {};
    }
#ifdef _WIN32
    wchar_t system_dir[MAX_PATH];
    const UINT n = ::GetSystemDirectoryW(system_dir, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        if (const fs::path found = search_directory(fs::path(system_dir), name); !found.empty()) return found;
        // Windows PowerShell, which every supported Windows has, lives one level down.
        if (const fs::path found = search_directory(fs::path(system_dir) / L"WindowsPowerShell" / L"v1.0", name); !found.empty()) return found;
    }
#endif
    for (const fs::path &dir : path_directories())
        if (const fs::path found = search_directory(dir, name); !found.empty()) return found;
    error = "program not found";
    return {};
}

#ifdef _WIN32

namespace {

std::wstring widen(const std::string &s)
{
    if (s.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring out(std::size_t(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n);
    return out;
}

// CommandLineToArgvW-compatible quoting.
std::wstring quote(const std::wstring &arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    std::size_t  slashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') out.append(slashes * 2 + 1, L'\\');
        else out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}

// A small environment: what programs need to start, and nothing that carries
// tokens or passwords.
std::wstring environment_block()
{
    static const wchar_t *keep[] = {L"SystemRoot", L"windir", L"SystemDrive", L"ComSpec", L"PATH", L"PATHEXT", L"TEMP", L"TMP",
                                    L"USERPROFILE", L"APPDATA", L"LOCALAPPDATA", L"ProgramData", L"ProgramFiles",
                                    L"ProgramFiles(x86)", L"NUMBER_OF_PROCESSORS", L"PROCESSOR_ARCHITECTURE", L"OS", L"PSModulePath"};
    std::vector<std::wstring> entries;
    for (const wchar_t *key : keep) {
        wchar_t     buffer[32767];
        const DWORD n = ::GetEnvironmentVariableW(key, buffer, 32767);
        if (n > 0 && n < 32767) entries.push_back(std::wstring(key) + L"=" + std::wstring(buffer, n));
    }
    entries.push_back(L"BAMBU_STUDIO_EXTENSION_API=1");
    std::sort(entries.begin(), entries.end(), [](const std::wstring &a, const std::wstring &b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    std::wstring block;
    for (const std::wstring &e : entries) {
        block += e;
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

} // namespace

ProcessResult run_process(const ProcessRequest &request)
{
    ProcessResult result;
    const auto    started = std::chrono::steady_clock::now();
    if (request.argv.empty()) {
        result.start_error = "no program";
        return result;
    }
    std::string    error;
    const fs::path program = resolve_program(request.folder, request.argv.front(), error);
    if (program.empty()) {
        result.start_error = error;
        return result;
    }
    std::wstring line = quote(program.wstring());
    for (std::size_t i = 1; i < request.argv.size(); ++i) line += L" " + quote(widen(request.argv[i]));

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE in_read = nullptr, in_write = nullptr, out_read = nullptr, out_write = nullptr, err_read = nullptr, err_write = nullptr;
    const auto close = [](HANDLE &h) {
        if (h) ::CloseHandle(h);
        h = nullptr;
    };
    if (!::CreatePipe(&in_read, &in_write, &sa, 0) || !::CreatePipe(&out_read, &out_write, &sa, 0) || !::CreatePipe(&err_read, &err_write, &sa, 0)) {
        for (HANDLE *h : {&in_read, &in_write, &out_read, &out_write, &err_read, &err_write}) close(*h);
        result.start_error = "could not create pipes";
        return result;
    }
    ::SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);

    // Only the three pipe ends are inherited, never another handle the app holds.
    HANDLE inherit[3] = {in_read, out_write, err_write};
    SIZE_T attr_size  = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    std::vector<unsigned char> attr_buffer(attr_size);
    auto *attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buffer.data());
    const bool attributes_ok = ::InitializeProcThreadAttributeList(attributes, 1, 0, &attr_size) &&
                               ::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), nullptr, nullptr);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb         = sizeof(startup);
    startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput  = in_read;
    startup.StartupInfo.hStdOutput = out_write;
    startup.StartupInfo.hStdError  = err_write;
    startup.lpAttributeList        = attributes_ok ? attributes : nullptr;

    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }

    std::wstring        environment = environment_block();
    std::wstring        directory   = request.folder.wstring();
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> mutable_line(line.begin(), line.end());
    mutable_line.push_back(L'\0');
    const DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | (attributes_ok ? EXTENDED_STARTUPINFO_PRESENT : 0);
    const BOOL  launched = ::CreateProcessW(program.c_str(), mutable_line.data(), nullptr, nullptr, TRUE, flags, environment.data(),
                                            directory.c_str(), &startup.StartupInfo, &process);
    if (attributes_ok) ::DeleteProcThreadAttributeList(attributes);
    close(in_read);
    close(out_write);
    close(err_write);
    if (!launched) {
        close(in_write);
        close(out_read);
        close(err_read);
        if (job) ::CloseHandle(job);
        result.start_error = "the program could not be started";
        return result;
    }
    if (job) ::AssignProcessToJobObject(job, process.hProcess);
    ::ResumeThread(process.hThread);
    ::CloseHandle(process.hThread);

    bool        too_large = false;
    std::string output, errors;
    std::thread writer([&in_write, &request]() {
        DWORD             written = 0;
        const char       *data    = request.input.data();
        std::size_t       left    = request.input.size();
        while (left > 0 && ::WriteFile(in_write, data, DWORD(std::min<std::size_t>(left, 1 << 16)), &written, nullptr) && written > 0) {
            data += written;
            left -= written;
        }
        ::CloseHandle(in_write);
        in_write = nullptr;
    });
    std::thread reader([&]() {
        char  buffer[8192];
        DWORD got = 0;
        while (::ReadFile(out_read, buffer, sizeof(buffer), &got, nullptr) && got > 0) {
            if (output.size() + got > request.max_output) {
                too_large = true;
                if (job) ::TerminateJobObject(job, 1);
                else ::TerminateProcess(process.hProcess, 1);
                break;
            }
            output.append(buffer, got);
        }
    });
    std::thread error_reader([&]() {
        char  buffer[4096];
        DWORD got = 0;
        while (::ReadFile(err_read, buffer, sizeof(buffer), &got, nullptr) && got > 0)
            if (errors.size() < kErrorKeep) errors.append(buffer, std::min<std::size_t>(got, kErrorKeep - errors.size()));
    });

    bool       timed_out = false, cancelled = false;
    const auto deadline  = started + std::chrono::milliseconds(std::max(1, request.timeout_ms));
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            timed_out = true;
            break;
        }
        if (::WaitForSingleObject(process.hProcess, DWORD(std::min<long long>(left, 100))) != WAIT_TIMEOUT) break;
        if (request.cancel && request.cancel->load()) {
            cancelled = true;
            break;
        }
    }
    // Whatever happened, nothing the program started may outlive the run.
    if (job) ::TerminateJobObject(job, timed_out || cancelled ? 2 : 0);
    else if (timed_out || cancelled) ::TerminateProcess(process.hProcess, 2);
    ::WaitForSingleObject(process.hProcess, 5000);
    writer.join();
    reader.join();
    error_reader.join();
    DWORD code = 0;
    ::GetExitCodeProcess(process.hProcess, &code);
    ::CloseHandle(process.hProcess);
    close(out_read);
    close(err_read);
    if (job) ::CloseHandle(job);

    result.seconds   = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    result.exit_code = int(code);
    result.output    = std::move(output);
    result.errors    = std::move(errors);
    result.status    = too_large   ? ProcessResult::Status::TooLarge :
                       cancelled   ? ProcessResult::Status::Cancelled :
                       timed_out   ? ProcessResult::Status::TimedOut : ProcessResult::Status::Exited;
    return result;
}

#else // POSIX

ProcessResult run_process(const ProcessRequest &request)
{
    ProcessResult result;
    const auto    started = std::chrono::steady_clock::now();
    if (request.argv.empty()) {
        result.start_error = "no program";
        return result;
    }
    std::string    error;
    const fs::path program = resolve_program(request.folder, request.argv.front(), error);
    if (program.empty()) {
        result.start_error = error;
        return result;
    }
    int in_pipe[2], out_pipe[2], err_pipe[2];
    if (::pipe(in_pipe) != 0 || ::pipe(out_pipe) != 0 || ::pipe(err_pipe) != 0) {
        result.start_error = "could not create pipes";
        return result;
    }
    std::vector<std::string> args = request.argv;
    args.front()                  = program.string();
    std::vector<char *> argv;
    for (std::string &a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    std::vector<std::string> env_strings;
    for (const char *key : {"PATH", "HOME", "TMPDIR", "LANG"})
        if (const char *v = std::getenv(key)) env_strings.push_back(std::string(key) + "=" + v);
    env_strings.push_back("BAMBU_STUDIO_EXTENSION_API=1");
    std::vector<char *> envp;
    for (std::string &e : env_strings) envp.push_back(e.data());
    envp.push_back(nullptr);
    const std::string directory = request.folder.string();

    const pid_t pid = ::fork();
    if (pid < 0) {
        for (int fd : {in_pipe[0], in_pipe[1], out_pipe[0], out_pipe[1], err_pipe[0], err_pipe[1]}) ::close(fd);
        result.start_error = "could not start a process";
        return result;
    }
    if (pid == 0) {
        ::setpgid(0, 0);
        ::dup2(in_pipe[0], 0);
        ::dup2(out_pipe[1], 1);
        ::dup2(err_pipe[1], 2);
        for (int fd : {in_pipe[0], in_pipe[1], out_pipe[0], out_pipe[1], err_pipe[0], err_pipe[1]}) ::close(fd);
        if (::chdir(directory.c_str()) != 0) ::_exit(126);
        ::execve(argv[0], argv.data(), envp.data());
        ::_exit(127);
    }
    ::setpgid(pid, pid);
    ::close(in_pipe[0]);
    ::close(out_pipe[1]);
    ::close(err_pipe[1]);

    std::thread writer([fd = in_pipe[1], &request]() {
        // A program that stops reading must not kill the app with SIGPIPE.
        sigset_t block, previous;
        sigemptyset(&block);
        sigaddset(&block, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &block, &previous);
        const char *data = request.input.data();
        std::size_t left = request.input.size();
        while (left > 0) {
            const ssize_t n = ::write(fd, data, left);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) continue;
                break;
            }
            data += n;
            left -= std::size_t(n);
        }
        ::close(fd);
        struct timespec zero {};
        sigtimedwait(&block, nullptr, &zero);
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    });

    std::string output, errors;
    bool        too_large = false, timed_out = false, cancelled = false;
    const auto  deadline  = started + std::chrono::milliseconds(std::max(1, request.timeout_ms));
    pollfd      fds[2]    = {{out_pipe[0], POLLIN, 0}, {err_pipe[0], POLLIN, 0}};
    int         open_fds  = 2;
    while (open_fds > 0) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            timed_out = true;
            break;
        }
        if (request.cancel && request.cancel->load()) {
            cancelled = true;
            break;
        }
        const int ready = ::poll(fds, 2, int(std::min<long long>(left, 100)));
        if (ready < 0 && errno != EINTR) break;
        for (pollfd &p : fds) {
            if (p.fd < 0 || !(p.revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char          buffer[8192];
            const ssize_t n = ::read(p.fd, buffer, sizeof(buffer));
            if (n <= 0) {
                ::close(p.fd);
                p.fd = -1;
                --open_fds;
                continue;
            }
            if (p.fd == out_pipe[0]) {
                if (output.size() + std::size_t(n) > request.max_output) {
                    too_large = true;
                    break;
                }
                output.append(buffer, std::size_t(n));
            } else if (errors.size() < kErrorKeep) {
                errors.append(buffer, std::min<std::size_t>(std::size_t(n), kErrorKeep - errors.size()));
            }
        }
        if (too_large) break;
    }
    // Whatever happened, nothing the program started may outlive the run.
    ::kill(-pid, SIGKILL);
    for (pollfd &p : fds)
        if (p.fd >= 0) ::close(p.fd);
    int status = 0;
    ::waitpid(pid, &status, 0);
    writer.join();

    result.seconds   = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127 && output.empty()) result.start_error = "the program could not be started";
    result.output = std::move(output);
    result.errors = std::move(errors);
    result.status = too_large ? ProcessResult::Status::TooLarge :
                    cancelled ? ProcessResult::Status::Cancelled :
                    timed_out ? ProcessResult::Status::TimedOut : ProcessResult::Status::Exited;
    if (!result.start_error.empty() && !cancelled) result.status = ProcessResult::Status::StartFailed;
    return result;
}

#endif

}}} // namespace Slic3r::GUI::Extensions
