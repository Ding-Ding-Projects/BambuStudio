#include "ServerSystem.hpp"

#include "libslic3r/miniz_extension.hpp"
#include "slic3r/Utils/Http.hpp"

#include <boost/asio.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/nowide/fstream.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <set>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

namespace Slic3r { namespace GUI { namespace GameServers {

namespace fs = std::filesystem;

namespace {

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::string env(const char *name)
{
#ifdef _WIN32
    std::wstring wname(name, name + std::strlen(name));
    wchar_t      buf[32768];
    const DWORD  n = GetEnvironmentVariableW(wname.c_str(), buf, DWORD(std::size(buf)));
    if (n == 0 || n >= std::size(buf)) return "";
    return fs::path(std::wstring(buf, n)).u8string();
#else
    const char *v = std::getenv(name);
    return v ? v : "";
#endif
}

std::time_t to_time_t(fs::file_time_type t)
{
    const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sys);
}

// JAVA_VERSION="21.0.5" and IMPLEMENTOR="Eclipse Adoptium" from a Java home's release file.
void read_release(const fs::path &home, JavaInstall &j)
{
    std::string text;
    if (!read_file((home / "release").u8string(), text, 64 * 1024)) return;
    std::istringstream in(text);
    std::string        line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string value = line.substr(eq + 1);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
        const std::string key = line.substr(0, eq);
        if (key == "JAVA_VERSION") j.version = value;
        else if (key == "IMPLEMENTOR") j.vendor = value;
    }
}

void consider(std::vector<JavaInstall> &out, std::set<std::string> &seen, const fs::path &java)
{
    std::error_code ec;
    if (!fs::is_regular_file(java, ec)) return;
    const fs::path real = fs::weakly_canonical(java, ec);
    const std::string key = lower((ec ? java : real).u8string());
    if (!seen.insert(key).second) return;
    JavaInstall j;
    j.path = java.u8string();
    read_release(java.parent_path().parent_path(), j);
    j.feature = java_feature_version(j.version);
    out.push_back(j);
}

// Every bin/java(.exe) up to `depth` folders below root.
void search(std::vector<JavaInstall> &out, std::set<std::string> &seen, const fs::path &root, int depth, const char *exe)
{
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return;
    consider(out, seen, root / "bin" / exe);
    if (depth <= 0) return;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (it->is_directory(e2) && !it->is_symlink(e2)) search(out, seen, it->path(), depth - 1, exe);
    }
}

} // namespace

bool read_file(const std::string &path, std::string &out, std::uint64_t limit)
{
    out.clear();
    std::error_code ec;
    const auto      size = fs::file_size(fs::u8path(path), ec);
    if (ec || size > limit) return false;
    boost::nowide::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.resize(std::size_t(size));
    if (size > 0) in.read(&out[0], std::streamsize(size));
    return bool(in) || in.eof();
}

bool write_file_atomic(const std::string &path, const std::string &text, std::string *error)
{
    const fs::path target = fs::u8path(path);
    const fs::path temp   = target.parent_path() / (target.filename().u8string() + ".writing");
    {
        boost::nowide::ofstream out(temp.u8string(), std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) *error = "Could not write " + temp.u8string();
            return false;
        }
        out.write(text.data(), std::streamsize(text.size()));
        out.flush();
        if (!out) {
            if (error) *error = "Could not finish writing " + temp.u8string();
            return false;
        }
    }
    std::error_code ec;
    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        if (error) *error = "Could not replace " + path;
        return false;
    }
    return true;
}

std::vector<JavaInstall> find_java()
{
    std::vector<JavaInstall> out;
    std::set<std::string>    seen;
#ifdef _WIN32
    const char *exe = "java.exe";
#else
    const char *exe = "java";
#endif
    if (const std::string home = env("JAVA_HOME"); !home.empty()) consider(out, seen, fs::u8path(home) / "bin" / exe);
    if (const std::string path = env("PATH"); !path.empty()) {
#ifdef _WIN32
        const char sep = ';';
#else
        const char sep = ':';
#endif
        std::istringstream in(path);
        std::string        dir;
        while (std::getline(in, dir, sep))
            if (!dir.empty()) consider(out, seen, fs::u8path(dir) / exe);
    }
#ifdef _WIN32
    for (const char *root_var : {"ProgramFiles", "ProgramW6432", "ProgramFiles(x86)"}) {
        const std::string root = env(root_var);
        if (root.empty()) continue;
        for (const char *vendor : {"Java", "Eclipse Adoptium", "Eclipse Foundation", "AdoptOpenJDK", "Microsoft", "Zulu", "BellSoft",
                                   "Amazon Corretto", "OpenJDK", "Semeru", "IBM", "Azul"})
            search(out, seen, fs::u8path(root) / vendor, 2, exe);
        // The Minecraft Launcher's own Java runtimes.
        search(out, seen, fs::u8path(root) / "Minecraft Launcher" / "runtime", 4, exe);
    }
    if (const std::string local = env("LOCALAPPDATA"); !local.empty()) {
        search(out, seen, fs::u8path(local) / "Packages" / "Microsoft.4297127D64EC6_8wekyb3d8bbwe" / "LocalCache" / "Local" / "runtime", 4, exe);
        search(out, seen, fs::u8path(local) / "Programs" / "Eclipse Adoptium", 2, exe);
    }
    if (const std::string roaming = env("APPDATA"); !roaming.empty()) search(out, seen, fs::u8path(roaming) / ".minecraft" / "runtime", 4, exe);
#else
    search(out, seen, "/usr/lib/jvm", 1, exe);
    search(out, seen, "/Library/Java/JavaVirtualMachines", 3, exe);
#endif
    std::stable_sort(out.begin(), out.end(), [](const JavaInstall &a, const JavaInstall &b) { return a.feature > b.feature; });
    return out;
}

std::string pick_java(const std::vector<JavaInstall> &installs, int feature)
{
    // The closest version that is new enough: old servers can break on a much newer Java.
    const JavaInstall *best = nullptr;
    for (const JavaInstall &j : installs)
        if (j.feature >= feature && (!best || j.feature < best->feature)) best = &j;
    return best ? best->path : "";
}

bool port_free(int port)
{
    if (port < 1 || port > 65535) return false;
    boost::asio::io_context        io;
    boost::asio::ip::tcp::acceptor acceptor(io);
    boost::system::error_code      ec;
    acceptor.open(boost::asio::ip::tcp::v4(), ec);
    if (ec) return true; // nothing to learn; the server says for itself
    acceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), static_cast<unsigned short>(port)), ec);
    return !ec;
}

int free_port_near(int start)
{
    for (int p = std::max(1024, start); p < start + 200 && p <= 65535; ++p)
        if (port_free(p)) return p;
    return 0;
}

std::uint64_t physical_memory_mb()
{
#ifdef _WIN32
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    return GlobalMemoryStatusEx(&m) ? std::uint64_t(m.ullTotalPhys / (1024 * 1024)) : 0;
#else
    const long pages = sysconf(_SC_PHYS_PAGES), size = sysconf(_SC_PAGE_SIZE);
    return pages > 0 && size > 0 ? std::uint64_t(pages) * std::uint64_t(size) / (1024 * 1024) : 0;
#endif
}

JarInfo inspect_jar(const std::string &jar_path)
{
    std::string    version_json, manifest;
    mz_zip_archive zip{};
    if (open_zip_reader(&zip, jar_path)) {
        for (const auto &[name, out] : {std::pair<const char *, std::string *>{"version.json", &version_json}, {"META-INF/MANIFEST.MF", &manifest}}) {
            size_t size = 0;
            if (void *p = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0)) {
                if (size < 1024 * 1024) out->assign(static_cast<const char *>(p), size);
                mz_free(p);
            }
        }
        close_zip_reader(&zip);
    }
    return identify_jar(fs::u8path(jar_path).filename().u8string(), version_json, manifest);
}

bool world_in_use(const std::string &world_folder)
{
#ifdef _WIN32
    const fs::path lock = fs::u8path(world_folder) / "session.lock";
    std::error_code ec;
    if (!fs::exists(lock, ec)) return false;
    // A running server keeps session.lock open; asking for it alone fails then.
    HANDLE h = CreateFileW(lock.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD e = GetLastError();
        return e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION;
    }
    CloseHandle(h);
    return false;
#else
    (void) world_folder;
    return false;
#endif
}

ServerFolder inspect_folder(const std::string &folder)
{
    ServerFolder    f;
    f.folder        = folder;
    const fs::path  root = fs::u8path(folder);
    std::error_code ec;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().u8string();
        f.entries.push_back(name);
        std::error_code e2;
        if (it->is_regular_file(e2) && lower(it->path().extension().u8string()) == ".jar") f.jars.push_back(name);
        if (it->is_directory(e2) && fs::exists(it->path() / "level.dat", e2)) f.worlds.push_back(name);
    }
    std::sort(f.entries.begin(), f.entries.end());
    std::sort(f.jars.begin(), f.jars.end());
    std::sort(f.worlds.begin(), f.worlds.end());
    f.jar = pick_server_jar(f.jars);
    if (!f.jar.empty()) f.info = inspect_jar((root / fs::u8path(f.jar)).u8string());
    std::string text;
    if (read_file((root / "eula.txt").u8string(), text, 64 * 1024)) f.eula = parse_eula(text);
    if (read_file((root / "server.properties").u8string(), text, 1024 * 1024)) {
        f.has_properties = true;
        f.properties     = parse_properties(text);
        if (const auto name = f.properties.get("level-name"); name && !name->empty()) f.level_name = *name;
    }
    const auto load = [&](const char *file, std::vector<Player> &list) {
        std::string json;
        if (!read_file((root / file).u8string(), json, 16 * 1024 * 1024)) return;
        if (!parse_players(json, list)) f.lists_readable = false;
    };
    load("whitelist.json", f.allowlist);
    load("ops.json", f.operators);
    load("banned-players.json", f.bans);
    f.world_locked = world_in_use((root / fs::u8path(f.level_name)).u8string());
    return f;
}

std::vector<std::string> find_server_folders(const std::string &root, int depth)
{
    std::vector<std::string> out;
    std::error_code          ec;
    const fs::path           base = fs::u8path(root);
    if (!fs::is_directory(base, ec)) return out;
    std::vector<std::pair<fs::path, int>> stack = {{base, 0}};
    while (!stack.empty() && out.size() < 500) {
        const auto [dir, level] = stack.back();
        stack.pop_back();
        std::vector<std::string> jars;
        bool                     marker = false;
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code e2;
            const std::string name = it->path().filename().u8string();
            if (it->is_regular_file(e2)) {
                if (lower(it->path().extension().u8string()) == ".jar") jars.push_back(name);
                if (name == "server.properties" || name == "eula.txt") marker = true;
            } else if (it->is_directory(e2) && !it->is_symlink(e2) && level < depth && name != "world" && name != "libraries" &&
                       name != "versions" && name != "logs" && name != "cache" && name != "plugins" && name != "mods" && name != "backups") {
                stack.push_back({it->path(), level + 1});
            }
        }
        ec.clear();
        if (marker && !pick_server_jar(jars).empty()) out.push_back(dir.u8string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<BackupFile> list_backups(const std::string &folder)
{
    std::vector<BackupFile> out;
    std::error_code         ec;
    for (fs::directory_iterator it(fs::u8path(folder), fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (!it->is_regular_file(e2) || lower(it->path().extension().u8string()) != ".zip") continue;
        BackupFile b;
        b.path      = it->path().u8string();
        b.name      = it->path().filename().u8string();
        b.size      = it->file_size(e2);
        b.when      = to_time_t(it->last_write_time(e2));
        b.automatic = is_automatic_backup(b.name);
        out.push_back(b);
    }
    std::sort(out.begin(), out.end(), [](const BackupFile &a, const BackupFile &b) { return a.when > b.when; });
    return out;
}

bool copy_tree(const std::string &from, const std::string &to, std::string *error)
{
    std::error_code ec;
    if (fs::exists(fs::u8path(to), ec)) {
        if (error) *error = "The target folder is already there.";
        return false;
    }
    fs::copy(fs::u8path(from), fs::u8path(to), fs::copy_options::recursive, ec);
    if (ec) {
        if (error) *error = ec.message();
        fs::remove_all(fs::u8path(to), ec);
        return false;
    }
    return true;
}

Download fetch(const std::string &url, const std::function<void(std::uint64_t, std::uint64_t)> &progress, const std::atomic<bool> *cancel)
{
    Download d;
    auto     http = Http::get(url);
    http.timeout_connect(20)
        .on_progress([&](Http::Progress p, bool &stop) {
            if (progress) progress(p.dlnow, p.dltotal);
            if (cancel && cancel->load()) stop = true;
        })
        .on_complete([&](std::string body, unsigned status) {
            d.ok = status == 200;
            if (!d.ok) d.error = "The server answered " + std::to_string(status) + ".";
            d.body  = std::move(body);
            d.bytes = d.body.size();
        })
        .on_error([&](std::string, std::string error, unsigned status) {
            d.ok    = false;
            d.error = !error.empty() ? error : "The server answered " + std::to_string(status) + ".";
        })
        .perform_sync();
    if (cancel && cancel->load()) {
        d.ok        = false;
        d.cancelled = true;
        d.body.clear();
    }
    return d;
}

Download fetch_to_file(const std::string &url, const std::string &target_u8, const std::string &sha1,
                       const std::function<void(std::uint64_t, std::uint64_t)> &progress, const std::atomic<bool> *cancel)
{
    const fs::path  target = fs::u8path(target_u8);
    std::error_code ec;
    if (fs::exists(target, ec)) {
        Download d;
        d.error = "A file with that name is already there.";
        return d;
    }
    Download d = fetch(url, progress, cancel);
    if (!d.ok) return d;
    if (!sha1.empty() && sha1_hex(d.body) != lower(sha1)) {
        d.ok    = false;
        d.error = "The download does not match its SHA-1, so it was not kept.";
        d.body.clear();
        return d;
    }
    fs::create_directories(target.parent_path(), ec);
    if (!write_file_atomic(target.u8string(), d.body, &d.error)) d.ok = false;
    d.body.clear();
    d.body.shrink_to_fit();
    return d;
}

// ---------------------------------------------------------------------------
// The server process

#ifdef _WIN32

namespace {

// One argument quoted the way CommandLineToArgvW and the C runtime read it back.
std::wstring quote_argument(const std::wstring &arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    for (std::size_t i = 0;; ++i) {
        std::size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') { ++i; ++backslashes; }
        if (i == arg.size()) {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out += L'"';
        } else {
            out.append(backslashes, L'\\');
            out += arg[i];
        }
    }
    return out + L"\"";
}

} // namespace

struct ServerProcess::Impl
{
    HANDLE              process = nullptr, in_write = nullptr, out_read = nullptr;
    DWORD               pid     = 0;
    std::atomic<bool>   alive{false}, quiet{false};
    std::mutex          write_mutex;
    LineFn              on_line;
    ExitFn              on_exit;
    std::thread         reader;

    ~Impl()
    {
        if (in_write) CloseHandle(in_write);
        if (out_read) CloseHandle(out_read);
        if (process) CloseHandle(process);
    }
};

ServerProcess::ServerProcess() : m(std::make_shared<Impl>()) {}

ServerProcess::~ServerProcess()
{
    m->quiet = true;
    if (m->reader.joinable()) {
        // A process that outlives this object keeps its reader, which holds
        // what it needs and finishes when the process ends.
        if (m->alive) m->reader.detach();
        else m->reader.join();
    }
}

bool ServerProcess::start(const std::vector<std::string> &argv, const std::string &folder, LineFn on_line, ExitFn on_exit, std::string *error)
{
    if (m->alive || argv.empty()) {
        if (error) *error = m->alive ? "The server is already running." : "Nothing to start.";
        return false;
    }
    std::wstring command;
    for (const std::string &a : argv) {
        if (!command.empty()) command += L' ';
        command += quote_argument(fs::u8path(a).wstring());
    }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE out_read = nullptr, out_write = nullptr, in_read = nullptr, in_write = nullptr;
    if (!CreatePipe(&out_read, &out_write, &sa, 0) || !CreatePipe(&in_read, &in_write, &sa, 0)) {
        if (error) *error = "Could not create the console pipes.";
        return false;
    }
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);

    // Only the two pipe ends are handed to the server, nothing else this app has open.
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<unsigned char>   attr_buffer(size);
    auto                        *attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buffer.data());
    HANDLE                       inherit[2] = {in_read, out_write};
    const bool attrs_ok = InitializeProcThreadAttributeList(attrs, 1, 0, &size) &&
                          UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), nullptr, nullptr);
    STARTUPINFOEXW si{};
    si.StartupInfo.cb         = sizeof(si);
    si.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput  = in_read;
    si.StartupInfo.hStdOutput = out_write;
    si.StartupInfo.hStdError  = out_write;
    si.lpAttributeList        = attrs_ok ? attrs : nullptr;
    PROCESS_INFORMATION pi{};
    const std::wstring  cwd = fs::u8path(folder).wstring();
    std::vector<wchar_t> line(command.begin(), command.end());
    line.push_back(0);
    const BOOL created = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | (attrs_ok ? EXTENDED_STARTUPINFO_PRESENT : 0), nullptr,
                                        cwd.c_str(), &si.StartupInfo, &pi);
    const DWORD create_error = created ? 0 : GetLastError();
    if (attrs_ok) DeleteProcThreadAttributeList(attrs);
    CloseHandle(in_read);
    CloseHandle(out_write);
    if (!created) {
        CloseHandle(out_read);
        CloseHandle(in_write);
        if (error) *error = "Windows could not start " + argv.front() + " (error " + std::to_string(create_error) + ").";
        return false;
    }
    CloseHandle(pi.hThread);
    if (m->reader.joinable()) m->reader.join();
    if (m->process) CloseHandle(m->process);
    if (m->in_write) CloseHandle(m->in_write);
    if (m->out_read) CloseHandle(m->out_read);
    m->process  = pi.hProcess;
    m->pid      = pi.dwProcessId;
    m->in_write = in_write;
    m->out_read = out_read;
    m->on_line  = std::move(on_line);
    m->on_exit  = std::move(on_exit);
    m->alive    = true;
    m->quiet    = false;
    m_started   = std::time(nullptr);
    std::shared_ptr<Impl> impl = m;
    m->reader   = std::thread([impl]() {
        std::string pending;
        char        buffer[8192];
        DWORD       got = 0;
        while (ReadFile(impl->out_read, buffer, sizeof(buffer), &got, nullptr) && got > 0)
            for (const std::string &l : take_lines(pending, std::string(buffer, got)))
                if (!impl->quiet && impl->on_line) impl->on_line(l);
        if (!pending.empty() && !impl->quiet && impl->on_line) impl->on_line(pending);
        WaitForSingleObject(impl->process, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(impl->process, &code);
        impl->alive = false;
        if (!impl->quiet && impl->on_exit) impl->on_exit(int(code));
    });
    return true;
}

bool ServerProcess::send(const std::string &command)
{
    if (!valid_console_command(command) || !m->alive || !m->in_write) return false;
    std::lock_guard<std::mutex> lock(m->write_mutex);
    const std::string           line = command + "\n";
    DWORD                       wrote = 0;
    return WriteFile(m->in_write, line.data(), DWORD(line.size()), &wrote, nullptr) && wrote == line.size();
}

bool          ServerProcess::running() const { return m && m->alive; }
unsigned long ServerProcess::pid() const { return m ? m->pid : 0; }

std::uint64_t ServerProcess::memory_bytes() const
{
    if (!m || !m->alive || !m->process) return 0;
    PROCESS_MEMORY_COUNTERS pmc{};
    return GetProcessMemoryInfo(m->process, &pmc, sizeof(pmc)) ? std::uint64_t(pmc.WorkingSetSize) : 0;
}

void ServerProcess::kill()
{
    if (m && m->alive && m->process) TerminateProcess(m->process, 1);
}

bool processes_supported() { return true; }

#else // not Windows

struct ServerProcess::Impl
{
};
ServerProcess::ServerProcess() : m(std::make_shared<Impl>()) {}
ServerProcess::~ServerProcess() = default;
bool ServerProcess::start(const std::vector<std::string> &, const std::string &, LineFn, ExitFn, std::string *error)
{
    if (error) *error = "Running a server needs Windows in this build.";
    return false;
}
bool          ServerProcess::send(const std::string &) { return false; }
bool          ServerProcess::running() const { return false; }
unsigned long ServerProcess::pid() const { return 0; }
std::uint64_t ServerProcess::memory_bytes() const { return 0; }
void          ServerProcess::kill() {}
bool          processes_supported() { return false; }

#endif

}}} // namespace Slic3r::GUI::GameServers
