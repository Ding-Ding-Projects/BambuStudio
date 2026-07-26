#include "BoundedRegex.hpp"
#include "BoundedRegexProtocol.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cwctype>
#include <deque>
#include <exception>
#include <filesystem>
#include <future>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <climits>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#ifdef __APPLE__
#include <libproc.h>
#include <mach-o/dyld.h>
#endif
#endif

namespace Slic3r::GUI::BoundedRegex {
namespace {

using Clock = std::chrono::steady_clock;

Result failure(Status status, std::string diagnostic)
{
    Result result;
    result.status = status;
    result.diagnostic = std::move(diagnostic);
    return result;
}

Result preflight(const Protocol::Request &request)
{
    if (request.pattern.size() > kMaxPatternCodeUnits)
        return failure(Status::PatternTooLong, "pattern exceeds application limit");
    if (request.subject.size() > kMaxSubjectCodeUnits)
        return failure(Status::SubjectTooLong, "subject exceeds application limit");
    if (!Protocol::structurally_bounded(request.pattern))
        return failure(Status::PatternTooComplex, "pattern exceeds structural limit");
    if (request.max_matches > kMaxMatches ||
        (request.mode != Protocol::Mode::Validate && request.mode != Protocol::Mode::Ping &&
         request.max_matches == 0))
        return failure(Status::ProtocolError, "invalid match limit");
    Result ready;
    ready.status = Status::Valid;
    return ready;
}

enum class IoStatus { Ok, TimedOut, Failed };

struct ExchangeResult {
    IoStatus io = IoStatus::Failed;
    Result   result;
};

struct PatternKey {
    std::wstring pattern;
    bool case_sensitive = false;
    bool multiline = false;

    bool operator==(const PatternKey &other) const
    {
        return case_sensitive == other.case_sensitive && multiline == other.multiline &&
               pattern == other.pattern;
    }
};

struct SearchCacheEntry {
    PatternKey  key;
    std::wstring subject;
    Result       result;
};

struct RequestFailureEntry {
    PatternKey     key;
    Protocol::Mode mode = Protocol::Mode::Search;
    std::size_t    max_matches = 0;
    std::uint32_t  timeout_ms = 0;
    std::wstring   subject;
    Result         result;
};

class WorkerClient {
public:
    ~WorkerClient()
    {
        wait_for_startup();
        stop_worker();
    }

    Result execute(const Protocol::Request &request, std::uint32_t requested_timeout_ms)
    {
        const Result checked = preflight(request);
        if (checked.status != Status::Valid)
            return checked;

        // Filtering and text-input handlers must never queue behind process
        // startup or another regex RPC. A contended client fails open and lets
        // the next UI refresh use the persistent worker/cache.
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock())
            return failure(Status::WorkerUnavailable, "bounded-regex worker is busy");
        const PatternKey key{request.pattern, request.case_sensitive, request.multiline};
        const std::uint32_t timeout_ms = std::clamp<std::uint32_t>(requested_timeout_ms, 1, 250);
        if (const Result *cached_failure = find_pattern_failure(key))
            return *cached_failure;
        if (const Result *cached_failure = find_request_failure(key, request, timeout_ms))
            return *cached_failure;
        if (request.mode == Protocol::Mode::Validate) {
            if (const Result *cached_validation = find_validation(key))
                return *cached_validation;
        } else if (request.mode == Protocol::Mode::Search) {
            if (const Result *cached_search = find_search(key, request.subject))
                return *cached_search;
        }

        if (!worker_running()) {
            // CreateProcess/fork, loader/anti-malware inspection, containment,
            // and the no-regex readiness ping run only on the owned background
            // task. They are bounded separately from user-authored evaluation;
            // this UI-facing call returns fail-open without waiting for them.
            const std::string previous_failure = m_startup_failure;
            begin_startup_locked();
            return failure(Status::WorkerUnavailable,
                           !previous_failure.empty() ? previous_failure :
                           (m_starting ? "bounded-regex worker startup is pending" :
                                         "bounded-regex worker startup could not be scheduled"));
        }

        // The caller's unchanged 1..250 ms budget begins only after the worker
        // has completed its no-regex readiness handshake.
        const auto request_deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
        ExchangeResult exchange = exchange_frame(request, request_deadline);
        if (exchange.io != IoStatus::Ok) {
            stop_worker(true);
            // A successful validation/search cache says nothing about the
            // replacement worker's readiness. Discard positive results and
            // immediately schedule a contained replacement; otherwise a
            // readiness probe can return a stale Valid result while the next
            // real request only discovers that no worker exists.
            clear_positive_caches();
            begin_startup_locked();
            Result result = exchange.io == IoStatus::TimedOut
                ? failure(Status::TimedOut, "bounded-regex worker exceeded deadline")
                : failure(Status::WorkerUnavailable, "bounded-regex worker IPC failed");
            if (result.status == Status::TimedOut)
                remember_request_failure(key, request, timeout_ms, result);
            return result;
        }

        if (exchange.result.status == Status::InvalidPattern ||
            (exchange.result.status == Status::PatternTooComplex &&
             request.mode == Protocol::Mode::Validate))
            remember_pattern_failure(key, exchange.result);
        else if (exchange.result.status == Status::PatternTooComplex)
            remember_request_failure(key, request, timeout_ms, exchange.result);
        if (request.mode == Protocol::Mode::Validate)
            remember_validation(key, exchange.result);
        else if (request.mode == Protocol::Mode::Search)
            remember_search(key, request.subject, exchange.result);
        return exchange.result;
    }

    void set_worker_path(std::wstring path)
    {
        wait_for_startup();
        std::lock_guard<std::mutex> lock(m_mutex);
        stop_worker();
        m_worker_override = std::move(path);
        m_startup_failure.clear();
        m_retry_after = Clock::time_point{};
        clear_caches();
    }

    void reset()
    {
        wait_for_startup();
        std::lock_guard<std::mutex> lock(m_mutex);
        stop_worker();
        m_startup_failure.clear();
        m_retry_after = Clock::time_point{};
        clear_caches();
    }

    void prewarm()
    {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock() || worker_running())
            return;
        begin_startup_locked();
    }

private:
    // Process creation plus code-signature/anti-malware inspection can exceed
    // one second on Windows. Startup is still off the UI thread and bounded
    // independently from the 1..250 ms user-pattern evaluation deadline.
    static constexpr auto kWorkerStartupTimeout = std::chrono::seconds(2);
    static constexpr std::size_t kValidationCacheLimit = 128;
    static constexpr std::size_t kSearchCacheLimit     = 512;
    static constexpr std::size_t kFailureCacheLimit    = 64;
    static constexpr std::size_t kRequestFailureCacheLimit = 128;

    void wait_for_startup()
    {
        if (!m_startup_future.valid())
            return;
        m_startup_future.wait();
        try {
            m_startup_future.get();
        } catch (...) {
            // The task converts failures into m_startup_failure. This guard is
            // retained so teardown cannot throw if the runtime itself fails.
        }
    }

    void begin_startup_locked()
    {
        if (m_starting || Clock::now() < m_retry_after)
            return;
        if (m_startup_future.valid()) {
            if (m_startup_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                return;
            try {
                m_startup_future.get();
            } catch (...) {
                m_startup_failure = "worker background startup task failed";
            }
        }

        m_starting = true;
        try {
            m_startup_future = std::async(std::launch::async, [this]() {
                std::lock_guard<std::mutex> lock(m_mutex);
                bool ready = false;
                try {
                    ready = ensure_worker(Clock::now() + kWorkerStartupTimeout);
                } catch (const std::exception &error) {
                    stop_worker(true);
                    m_startup_failure = std::string("worker startup threw: ") + error.what();
                } catch (...) {
                    stop_worker(true);
                    m_startup_failure = "worker startup threw an unknown exception";
                }
                if (!ready)
                    m_retry_after = Clock::now() + kWorkerStartupTimeout;
                m_starting = false;
            });
        } catch (const std::exception &error) {
            m_starting = false;
            m_retry_after = Clock::now() + kWorkerStartupTimeout;
            m_startup_failure = std::string("worker startup task creation failed: ") + error.what();
        } catch (...) {
            m_starting = false;
            m_retry_after = Clock::now() + kWorkerStartupTimeout;
            m_startup_failure = "worker startup task creation failed";
        }
    }

    const Result *find_pattern_failure(const PatternKey &key)
    {
        for (const auto &entry : m_pattern_failures)
            if (entry.first == key)
                return &entry.second;
        return nullptr;
    }

    const Result *find_validation(const PatternKey &key)
    {
        for (const auto &entry : m_validations)
            if (entry.first == key)
                return &entry.second;
        return nullptr;
    }

    const Result *find_request_failure(const PatternKey &key, const Protocol::Request &request,
                                       std::uint32_t timeout_ms)
    {
        for (const RequestFailureEntry &entry : m_request_failures)
            if (entry.key == key && entry.mode == request.mode &&
                entry.max_matches == request.max_matches && entry.timeout_ms == timeout_ms &&
                entry.subject == request.subject)
                return &entry.result;
        return nullptr;
    }

    const Result *find_search(const PatternKey &key, const std::wstring &subject)
    {
        for (const SearchCacheEntry &entry : m_searches)
            if (entry.key == key && entry.subject == subject)
                return &entry.result;
        return nullptr;
    }

    void remember_pattern_failure(const PatternKey &key, const Result &result)
    {
        if (find_pattern_failure(key) != nullptr)
            return;
        if (m_pattern_failures.size() == kFailureCacheLimit)
            m_pattern_failures.pop_front();
        m_pattern_failures.emplace_back(key, result);
    }

    void remember_validation(const PatternKey &key, const Result &result)
    {
        if (find_validation(key) != nullptr)
            return;
        if (m_validations.size() == kValidationCacheLimit)
            m_validations.pop_front();
        m_validations.emplace_back(key, result);
    }

    void remember_request_failure(const PatternKey &key, const Protocol::Request &request,
                                  std::uint32_t timeout_ms, const Result &result)
    {
        if (find_request_failure(key, request, timeout_ms) != nullptr)
            return;
        if (m_request_failures.size() == kRequestFailureCacheLimit)
            m_request_failures.pop_front();
        m_request_failures.push_back(
            RequestFailureEntry{key, request.mode, request.max_matches, timeout_ms,
                                request.subject, result});
    }

    void remember_search(const PatternKey &key, const std::wstring &subject, const Result &result)
    {
        if (m_searches.size() == kSearchCacheLimit)
            m_searches.pop_front();
        m_searches.push_back(SearchCacheEntry{key, subject, result});
    }

    void clear_caches()
    {
        m_pattern_failures.clear();
        m_validations.clear();
        m_searches.clear();
        m_request_failures.clear();
    }

    void clear_positive_caches()
    {
        m_validations.clear();
        m_searches.clear();
    }

    bool ensure_worker(Clock::time_point deadline)
    {
        m_startup_failure.clear();
        if (worker_running())
            return true;
        stop_worker();
        if (!start_worker()) {
            if (m_startup_failure.empty())
                m_startup_failure = "worker process creation or containment setup failed";
            return false;
        }

        Protocol::Request ping;
        ping.mode = Protocol::Mode::Ping;
        ping.max_matches = 0;
        if (Clock::now() >= deadline) {
            m_startup_failure = "worker process creation exhausted the startup budget";
            stop_worker(true);
            return false;
        }
        const ExchangeResult hello = exchange_frame(ping, deadline);
        if (hello.io != IoStatus::Ok || hello.result.status != Status::Valid) {
            if (hello.io == IoStatus::TimedOut)
                m_startup_failure = "worker readiness ping exceeded the startup budget";
            else if (hello.io != IoStatus::Ok)
                m_startup_failure = "worker readiness ping IPC failed";
            else
                m_startup_failure = "worker rejected the readiness ping";
            stop_worker(true);
            return false;
        }
#ifdef __APPLE__
        // The forked child briefly shares Studio's pre-exec footprint. Start
        // monitoring only after the worker has answered its post-exec ping.
        m_memory_monitor_active = true;
        if (!worker_within_memory_limit()) {
            m_startup_failure = "worker exceeded its memory limit during startup";
            stop_worker(true);
            return false;
        }
#endif
        return true;
    }

    ExchangeResult exchange_frame(const Protocol::Request &request, Clock::time_point deadline)
    {
        const std::vector<std::uint8_t> encoded = Protocol::encode_request(request);
        const IoStatus write_status = write_all(encoded.data(), encoded.size(), deadline);
        if (write_status != IoStatus::Ok)
            return ExchangeResult{write_status, {}};

        std::vector<std::uint8_t> prefix(Protocol::kPrefixBytes);
        const IoStatus prefix_status = read_exact(prefix.data(), prefix.size(), deadline);
        if (prefix_status != IoStatus::Ok)
            return ExchangeResult{prefix_status, {}};
        std::size_t payload_size = 0;
        if (!Protocol::decode_prefix(prefix, Protocol::kResponseMagic,
                                     Protocol::kMaxResponseBytes, payload_size))
            return ExchangeResult{IoStatus::Failed, {}};
        std::vector<std::uint8_t> payload(payload_size);
        const IoStatus payload_status = read_exact(payload.data(), payload.size(), deadline);
        if (payload_status != IoStatus::Ok)
            return ExchangeResult{payload_status, {}};
        Result result;
        if (!Protocol::decode_result(payload, result))
            return ExchangeResult{IoStatus::Failed, {}};
        return ExchangeResult{IoStatus::Ok, std::move(result)};
    }

    std::filesystem::path worker_path() const
    {
        if (!m_worker_override.empty())
            return std::filesystem::path(m_worker_override);
#ifdef _WIN32
        std::wstring module_path(32768, L'\0');
        const DWORD count = GetModuleFileNameW(nullptr, module_path.data(),
                                               static_cast<DWORD>(module_path.size()));
        if (count == 0 || count >= module_path.size())
            return {};
        module_path.resize(count);
        return std::filesystem::path(module_path).parent_path() / L"bambu-regex-worker.exe";
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        (void) _NSGetExecutablePath(nullptr, &size);
        std::string module_path(size, '\0');
        if (_NSGetExecutablePath(module_path.data(), &size) != 0)
            return {};
        module_path.resize(std::strlen(module_path.c_str()));
        return std::filesystem::path(module_path).parent_path() / "bambu-regex-worker";
#else
        std::vector<char> module_path(4096);
        const ssize_t count = readlink("/proc/self/exe", module_path.data(), module_path.size() - 1);
        if (count <= 0)
            return {};
        module_path[static_cast<std::size_t>(count)] = '\0';
        return std::filesystem::path(module_path.data()).parent_path() / "bambu-regex-worker";
#endif
    }

#ifdef _WIN32
    bool start_worker()
    {
        const std::filesystem::path path = worker_path();
        if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            m_startup_failure = "worker executable is missing";
            return false;
        }

        SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
        HANDLE child_stdin_read = nullptr, parent_stdin_write = nullptr;
        HANDLE parent_stdout_read = nullptr, child_stdout_write = nullptr;
        HANDLE child_stderr = nullptr;
        if (!CreatePipe(&child_stdin_read, &parent_stdin_write, &security, 64 * 1024) ||
            !CreatePipe(&parent_stdout_read, &child_stdout_write, &security, 64 * 1024)) {
            if (child_stdin_read) CloseHandle(child_stdin_read);
            if (parent_stdin_write) CloseHandle(parent_stdin_write);
            if (parent_stdout_read) CloseHandle(parent_stdout_read);
            if (child_stdout_write) CloseHandle(child_stdout_write);
            m_startup_failure = "worker IPC pipe creation failed";
            return false;
        }
        SetHandleInformation(parent_stdin_write, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(parent_stdout_read, HANDLE_FLAG_INHERIT, 0);
        child_stderr = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (child_stderr == INVALID_HANDLE_VALUE) {
            CloseHandle(child_stdin_read);
            CloseHandle(parent_stdin_write);
            CloseHandle(parent_stdout_read);
            CloseHandle(child_stdout_write);
            m_startup_failure = "worker stderr redirection failed";
            return false;
        }

        SIZE_T attribute_bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_bytes);
        std::vector<std::uint8_t> attribute_storage(attribute_bytes);
        auto *attributes = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_bytes)) {
            CloseHandle(child_stdin_read); CloseHandle(parent_stdin_write);
            CloseHandle(parent_stdout_read); CloseHandle(child_stdout_write); CloseHandle(child_stderr);
            m_startup_failure = "worker handle isolation setup failed";
            return false;
        }
        HANDLE inherited[] = {child_stdin_read, child_stdout_write, child_stderr};
        if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       inherited, sizeof(inherited), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(attributes);
            CloseHandle(child_stdin_read); CloseHandle(parent_stdin_write);
            CloseHandle(parent_stdout_read); CloseHandle(child_stdout_write); CloseHandle(child_stderr);
            m_startup_failure = "worker inherited-handle allowlist setup failed";
            return false;
        }

        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput  = child_stdin_read;
        startup.StartupInfo.hStdOutput = child_stdout_write;
        startup.StartupInfo.hStdError  = child_stderr;
        startup.lpAttributeList = attributes;
        PROCESS_INFORMATION process{};
        std::wstring command = L"\"" + path.wstring() + L"\"";
        std::vector<wchar_t> mutable_command(command.begin(), command.end());
        mutable_command.push_back(L'\0');
        const std::wstring directory = path.parent_path().wstring();
        // Regex work is deadline- and memory-bounded already. BELOW_NORMAL
        // starved even the no-regex readiness ping behind parallel compilers,
        // making healthy cold starts fail open and freeze for the full cap.
        const DWORD creation_flags = CREATE_NO_WINDOW | CREATE_SUSPENDED |
                                     EXTENDED_STARTUPINFO_PRESENT;
        const BOOL created = CreateProcessW(path.c_str(), mutable_command.data(), nullptr, nullptr, TRUE,
                                            creation_flags, nullptr, directory.c_str(),
                                            &startup.StartupInfo, &process);
        DeleteProcThreadAttributeList(attributes);
        CloseHandle(child_stdin_read);
        CloseHandle(child_stdout_write);
        CloseHandle(child_stderr);
        if (!created) {
            const DWORD error = GetLastError();
            CloseHandle(parent_stdin_write);
            CloseHandle(parent_stdout_read);
            m_startup_failure = "worker process creation failed (Windows error " +
                                std::to_string(static_cast<unsigned long>(error)) + ")";
            return false;
        }

        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                                   JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit = 128u * 1024u * 1024u;
        const bool job_limits_ready = job &&
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        const bool job_assigned = job_limits_ready && AssignProcessToJobObject(job, process.hProcess);
        // Isolation is a prerequisite, not an optimization. Never resume a
        // worker that escaped the memory/kill-on-close Job Object.
        if (!job_assigned) {
            TerminateProcess(process.hProcess, 1);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            if (job) CloseHandle(job);
            CloseHandle(parent_stdin_write);
            CloseHandle(parent_stdout_read);
            if (!job)
                m_startup_failure = "worker Job Object creation failed";
            else if (!job_limits_ready)
                m_startup_failure = "worker Job Object memory-limit setup failed";
            else
                m_startup_failure = "worker Job Object assignment failed";
            return false;
        }
        if (ResumeThread(process.hThread) == static_cast<DWORD>(-1)) {
            TerminateProcess(process.hProcess, 1);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            if (job) CloseHandle(job);
            CloseHandle(parent_stdin_write);
            CloseHandle(parent_stdout_read);
            m_startup_failure = "worker process resume failed";
            return false;
        }
        CloseHandle(process.hThread);
        m_process = process.hProcess;
        m_job = job;
        m_write = parent_stdin_write;
        m_read = parent_stdout_read;
        return true;
    }

    bool worker_running() const
    {
        return m_process != nullptr && WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT;
    }

    IoStatus write_all(const void *data, std::size_t size, Clock::time_point deadline)
    {
        (void) deadline;
        const auto *bytes = static_cast<const std::uint8_t *>(data);
        while (size != 0) {
            DWORD written = 0;
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(size, 64 * 1024));
            if (!WriteFile(m_write, bytes, chunk, &written, nullptr) || written == 0)
                return IoStatus::Failed;
            bytes += written;
            size -= written;
        }
        return IoStatus::Ok;
    }

    IoStatus read_exact(void *data, std::size_t size, Clock::time_point deadline)
    {
        auto *bytes = static_cast<std::uint8_t *>(data);
        while (size != 0) {
            DWORD available = 0;
            if (!PeekNamedPipe(m_read, nullptr, 0, nullptr, &available, nullptr))
                return IoStatus::Failed;
            if (available != 0) {
                DWORD read = 0;
                const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(size, available));
                if (!ReadFile(m_read, bytes, chunk, &read, nullptr) || read == 0)
                    return IoStatus::Failed;
                bytes += read;
                size -= read;
                continue;
            }
            if (Clock::now() >= deadline)
                return IoStatus::TimedOut;
            if (WaitForSingleObject(m_process, 0) == WAIT_OBJECT_0)
                return IoStatus::Failed;
            Sleep(1);
        }
        return IoStatus::Ok;
    }

    void stop_worker(bool force = false)
    {
        if (m_write) {
            CloseHandle(m_write);
            m_write = nullptr;
        }
        if (m_process) {
            if (force) {
                if (m_job)
                    TerminateJobObject(m_job, 1);
                else
                    TerminateProcess(m_process, 1);
            } else if (WaitForSingleObject(m_process, 100) == WAIT_TIMEOUT) {
                if (m_job)
                    TerminateJobObject(m_job, 1);
                else
                    TerminateProcess(m_process, 1);
            }
            CloseHandle(m_process);
            m_process = nullptr;
        }
        if (m_read) {
            CloseHandle(m_read);
            m_read = nullptr;
        }
        if (m_job) {
            CloseHandle(m_job);
            m_job = nullptr;
        }
    }

    HANDLE m_process = nullptr;
    HANDLE m_job = nullptr;
    HANDLE m_write = nullptr;
    HANDLE m_read = nullptr;
#else
    static void close_inherited_descriptors(long max_fd)
    {
        // The helper receives its protocol only on fd 0/1. Close every other
        // descriptor in the post-fork child using async-signal-safe operations;
        // otherwise project files, sockets, and credentials opened by Studio
        // could leak through exec.
#if defined(__linux__) && defined(__NR_close_range)
        if (syscall(__NR_close_range, 3u, UINT_MAX, 0u) == 0)
            return;
#endif
        const long upper = max_fd > 3 ? max_fd : 65536;
        for (long fd = 3; fd < upper; ++fd)
            close(static_cast<int>(fd));
    }

    bool start_worker()
    {
#ifdef __APPLE__
        m_memory_monitor_active = false;
#endif
        const std::filesystem::path path = worker_path();
        // All allocation, filesystem conversion, and limit discovery happens
        // before fork. The child below calls only async-signal-safe primitives.
        const std::string native_path = path.string();
        const long max_fd = sysconf(_SC_OPEN_MAX);
        if (path.empty() || native_path.empty() || max_fd <= 3 ||
            access(native_path.c_str(), X_OK) != 0) {
            m_startup_failure = "worker executable is missing or not executable";
            return false;
        }
        int sockets[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
            m_startup_failure = "worker IPC socket creation failed";
            return false;
        }
        for (int socket_fd : sockets) {
            const int flags = fcntl(socket_fd, F_GETFD);
            if (flags == -1 || fcntl(socket_fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
                close(sockets[0]); close(sockets[1]);
                m_startup_failure = "worker IPC descriptor isolation failed";
                return false;
            }
        }
        const int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd == -1) {
            close(sockets[0]); close(sockets[1]);
            m_startup_failure = "worker stderr redirection failed";
            return false;
        }
        const pid_t pid = fork();
        if (pid == -1) {
            close(null_fd); close(sockets[0]); close(sockets[1]);
            m_startup_failure = "worker process creation failed";
            return false;
        }
        if (pid == 0) {
            close(sockets[0]);
            if (dup2(sockets[1], STDIN_FILENO) == -1 ||
                dup2(sockets[1], STDOUT_FILENO) == -1 ||
                dup2(null_fd, STDERR_FILENO) == -1)
                _exit(126);
            // dup2() clears FD_CLOEXEC only when oldfd and newfd differ. If
            // Studio was launched with stdin/stdout closed, socketpair() may
            // return fd 0/1 and either dup2 becomes a no-op. Explicitly clear
            // the flag so exec cannot silently sever the protocol channel.
            const int stdin_flags = fcntl(STDIN_FILENO, F_GETFD);
            const int stdout_flags = fcntl(STDOUT_FILENO, F_GETFD);
            if (stdin_flags == -1 || stdout_flags == -1 ||
                fcntl(STDIN_FILENO, F_SETFD, stdin_flags & ~FD_CLOEXEC) == -1 ||
                fcntl(STDOUT_FILENO, F_SETFD, stdout_flags & ~FD_CLOEXEC) == -1)
                _exit(126);
            close_inherited_descriptors(max_fd);
            execl(native_path.c_str(), native_path.c_str(), static_cast<char *>(nullptr));
            _exit(127);
        }
        close(null_fd);
        close(sockets[1]);
#ifdef SO_NOSIGPIPE
        int no_sigpipe = 1;
        (void) setsockopt(sockets[0], SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
        m_pid = pid;
        m_socket = sockets[0];
        return true;
    }

    bool worker_running() const
    {
        if (m_pid <= 0)
            return false;
        int status = 0;
        return waitpid(m_pid, &status, WNOHANG) == 0;
    }

    static int poll_timeout(Clock::time_point deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        return remaining.count() <= 0 ? 0 : static_cast<int>(std::min<long long>(remaining.count() + 1, 1000));
    }

#ifdef __APPLE__
    bool worker_within_memory_limit() const
    {
        if (!m_memory_monitor_active)
            return true;
        if (m_pid <= 0)
            return false;
        rusage_info_v4 usage{};
        if (proc_pid_rusage(m_pid, RUSAGE_INFO_V4,
                            reinterpret_cast<rusage_info_t *>(&usage)) != 0)
            return false;
        constexpr std::uint64_t footprint_limit = 128u * 1024u * 1024u;
        return usage.ri_phys_footprint <= footprint_limit;
    }
#endif

    IoStatus wait_for_socket(short events, Clock::time_point deadline)
    {
        for (;;) {
#ifdef __APPLE__
            // XNU's map-size rlimits are the allocation guard. This short-
            // cadence physical-footprint watchdog is independent defense in
            // depth and kills a worker before accepting an oversized result.
            if (!worker_within_memory_limit())
                return IoStatus::Failed;
#endif
            int timeout = poll_timeout(deadline);
#ifdef __APPLE__
            timeout = std::min(timeout, 2);
#endif
            pollfd descriptor{m_socket, events, 0};
            const int ready = poll(&descriptor, 1, timeout);
            if (ready > 0) {
#ifdef __APPLE__
                if (!worker_within_memory_limit())
                    return IoStatus::Failed;
#endif
                return IoStatus::Ok;
            }
            if (ready < 0) {
                if (errno == EINTR)
                    continue;
                return IoStatus::Failed;
            }
            if (Clock::now() >= deadline)
                return IoStatus::TimedOut;
        }
    }

    IoStatus write_all(const void *data, std::size_t size, Clock::time_point deadline)
    {
        const auto *bytes = static_cast<const std::uint8_t *>(data);
        while (size != 0) {
            const IoStatus ready = wait_for_socket(POLLOUT, deadline);
            if (ready != IoStatus::Ok)
                return ready;
#ifdef MSG_NOSIGNAL
            const ssize_t written = send(m_socket, bytes, size, MSG_NOSIGNAL);
#else
            const ssize_t written = send(m_socket, bytes, size, 0);
#endif
            if (written <= 0) {
                if (written < 0 && errno == EINTR) continue;
                return IoStatus::Failed;
            }
            bytes += written;
            size -= static_cast<std::size_t>(written);
        }
        return IoStatus::Ok;
    }

    IoStatus read_exact(void *data, std::size_t size, Clock::time_point deadline)
    {
        auto *bytes = static_cast<std::uint8_t *>(data);
        while (size != 0) {
            const IoStatus ready = wait_for_socket(POLLIN, deadline);
            if (ready != IoStatus::Ok)
                return ready;
            const ssize_t count = recv(m_socket, bytes, size, 0);
            if (count <= 0) {
                if (count < 0 && errno == EINTR) continue;
                return IoStatus::Failed;
            }
            bytes += count;
            size -= static_cast<std::size_t>(count);
        }
        return IoStatus::Ok;
    }

    void stop_worker(bool force = false)
    {
#ifdef __APPLE__
        m_memory_monitor_active = false;
#endif
        if (m_socket != -1) {
            close(m_socket);
            m_socket = -1;
        }
        if (m_pid > 0) {
            int status = 0;
            if (force) {
                kill(m_pid, SIGKILL);
                (void) waitpid(m_pid, &status, 0);
            } else {
                const auto deadline = Clock::now() + std::chrono::milliseconds(100);
                while (waitpid(m_pid, &status, WNOHANG) == 0 && Clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (waitpid(m_pid, &status, WNOHANG) == 0) {
                    kill(m_pid, SIGKILL);
                    (void) waitpid(m_pid, &status, 0);
                }
            }
            m_pid = -1;
        }
    }

    pid_t m_pid = -1;
    int   m_socket = -1;
#ifdef __APPLE__
    bool  m_memory_monitor_active = false;
#endif
#endif

    std::mutex m_mutex;
    std::future<void> m_startup_future;
    bool m_starting = false;
    Clock::time_point m_retry_after{};
    std::wstring m_worker_override;
    std::string m_startup_failure;
    std::deque<std::pair<PatternKey, Result>> m_pattern_failures;
    std::deque<std::pair<PatternKey, Result>> m_validations;
    std::deque<SearchCacheEntry> m_searches;
    std::deque<RequestFailureEntry> m_request_failures;
};

WorkerClient &client()
{
    static WorkerClient instance;
    return instance;
}

Result execute(Protocol::Mode mode, const std::wstring &pattern, const std::wstring &subject,
               std::size_t max_matches, const Options &options)
{
    Protocol::Request request;
    request.mode = mode;
    request.case_sensitive = options.case_sensitive;
    request.multiline = options.multiline;
    request.max_matches = max_matches;
    request.pattern = pattern;
    request.subject = subject;
    return client().execute(request, options.timeout_ms);
}

} // namespace

Result validate(const std::wstring &pattern, const Options &options)
{
    return execute(Protocol::Mode::Validate, pattern, {}, 0, options);
}

Result search(const std::wstring &pattern, const std::wstring &subject, const Options &options)
{
    const Result validation = validate(pattern, options);
    if (validation.status != Status::Valid)
        return validation;
    return execute(Protocol::Mode::Search, pattern, subject, 1, options);
}

Result find_all(const std::wstring &pattern, const std::wstring &subject,
                std::size_t max_matches, const Options &options)
{
    const Result validation = validate(pattern, options);
    if (validation.status != Status::Valid)
        return validation;
    return execute(Protocol::Mode::FindAll, pattern, subject,
                   std::min(max_matches, kMaxMatches), options);
}

void prewarm()
{
    client().prewarm();
}

SearchPass::SearchPass(std::wstring pattern, const Options &options)
    : m_pattern(std::move(pattern))
    , m_options(options)
{
    const Result checked = validate(m_pattern, m_options);
    if (checked.status != Status::Valid)
        open_circuit(checked);
    else
        // Worker readiness has its own background budget. Start the aggregate
        // candidate-evaluation budget only after validation succeeds so a
        // ready-but-new pattern receives the full requested 1..250 ms.
        m_deadline = Clock::now() + std::chrono::milliseconds(
            std::clamp<std::uint32_t>(options.timeout_ms, 1, 250));
}

void SearchPass::open_circuit(Result result)
{
    m_circuit_open = true;
    m_circuit_result = std::move(result);
}

Result SearchPass::evaluate(const std::wstring &subject)
{
    if (m_circuit_open)
        return m_circuit_result;

    Options request_options;
    if (!remaining_options(request_options))
        return m_circuit_result;

    Result result = search(m_pattern, subject, request_options);
    if (!result.definitive())
        open_circuit(result);
    return result;
}

Result SearchPass::find_all(const std::wstring &subject, std::size_t max_matches)
{
    if (m_circuit_open)
        return m_circuit_result;
    Options request_options;
    if (!remaining_options(request_options))
        return m_circuit_result;

    Result result = BoundedRegex::find_all(m_pattern, subject, max_matches, request_options);
    if (!result.definitive())
        open_circuit(result);
    return result;
}

bool SearchPass::remaining_options(Options &options)
{
    const auto now = Clock::now();
    if (now >= m_deadline) {
        open_circuit(failure(Status::TimedOut, "filter pass exhausted aggregate deadline"));
        return false;
    }
    const auto remaining_us = std::chrono::duration_cast<std::chrono::microseconds>(m_deadline - now).count();
    options = m_options;
    const std::uint32_t remaining_ms = static_cast<std::uint32_t>(
        std::max<std::int64_t>(1, (remaining_us + 999) / 1000));
    options.timeout_ms = std::min(options.timeout_ms, remaining_ms);
    return true;
}

bool plain_search(const std::wstring &needle, const std::wstring &subject,
                  bool case_sensitive, bool whole_word)
{
    if (needle.empty())
        return true;
    auto fold = [](std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return value;
    };
    const std::wstring haystack = case_sensitive ? subject : fold(subject);
    const std::wstring query    = case_sensitive ? needle : fold(needle);
    if (!whole_word)
        return haystack.find(query) != std::wstring::npos;
    auto is_word = [](wchar_t ch) {
        return std::iswalnum(static_cast<wint_t>(ch)) != 0 || ch == L'_';
    };
    for (std::size_t position = haystack.find(query); position != std::wstring::npos;
         position = haystack.find(query, position + 1)) {
        const bool left_ok = position == 0 || !is_word(haystack[position - 1]);
        const bool right_ok = position + query.size() >= haystack.size() ||
                              !is_word(haystack[position + query.size()]);
        if (left_ok && right_ok)
            return true;
    }
    return false;
}

const char *status_name(Status status)
{
    switch (status) {
    case Status::Valid: return "valid";
    case Status::Match: return "match";
    case Status::NoMatch: return "no-match";
    case Status::InvalidPattern: return "invalid-pattern";
    case Status::PatternTooLong: return "pattern-too-long";
    case Status::SubjectTooLong: return "subject-too-long";
    case Status::PatternTooComplex: return "pattern-too-complex";
    case Status::TimedOut: return "timed-out";
    case Status::WorkerUnavailable: return "worker-unavailable";
    case Status::ProtocolError: return "protocol-error";
    }
    return "unknown";
}

namespace Testing {
void set_worker_path(const std::wstring &path) { client().set_worker_path(path); }
void reset_worker() { client().reset(); }
#ifndef _WIN32
bool execute_probe(Protocol::Request request)
{
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    do {
        const Result result = client().execute(request, kDefaultTimeoutMs);
        if (result.status != Status::WorkerUnavailable)
            return result.status == Status::Valid;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (Clock::now() < deadline);
    return false;
}
#endif
bool worker_fd_is_closed(int fd)
{
#ifdef _WIN32
    (void) fd;
    return false;
#else
    Protocol::Request request;
    request.mode = Protocol::Mode::Ping;
    request.max_matches = 0;
    request.pattern = L"__bambu_audit_inherited_fd__";
    request.subject = std::to_wstring(fd);
    return execute_probe(std::move(request));
#endif
}
bool worker_resource_limits_active()
{
#ifdef _WIN32
    return false;
#else
    Protocol::Request request;
    request.mode = Protocol::Mode::Ping;
    request.max_matches = 0;
    request.pattern = L"__bambu_audit_resource_limits__";
    return execute_probe(std::move(request));
#endif
}
} // namespace Testing

} // namespace Slic3r::GUI::BoundedRegex
