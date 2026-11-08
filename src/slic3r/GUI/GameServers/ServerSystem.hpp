#ifndef slic3r_GUI_GameServers_ServerSystem_hpp_
#define slic3r_GUI_GameServers_ServerSystem_hpp_

// What File > Minecraft servers asks of this PC and does on it: the Java
// installations it has, whether a port is free, how much memory there is,
// what a server folder holds (its jar, EULA, settings, worlds, player lists
// and backups), a running server process with its console, backups written
// and checked as zip files, restoring one, and downloads checked against
// their SHA-1. Running a server needs Windows in this build; everything that
// only reads files works everywhere.

#include "ServerModel.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Slic3r { namespace GUI { namespace GameServers {

struct JavaInstall
{
    std::string path;    // java.exe
    std::string version; // "21.0.5"
    int         feature = 0;
    std::string vendor;  // from the release file when it says
};
// Every Java this PC has: JAVA_HOME, PATH, the usual install folders and the
// Minecraft Launcher's own runtimes. Read from each installation's release
// file, without running anything.
std::vector<JavaInstall> find_java();
// The newest installation of at least `feature` ("" when none is).
std::string pick_java(const std::vector<JavaInstall> &installs, int feature);

bool          port_free(int port);
int           free_port_near(int start); // the first free port from start, 0 when none in 200
std::uint64_t physical_memory_mb();      // 0 when it cannot be read

struct BackupFile
{
    std::string   path, name;
    std::uint64_t size = 0;
    std::time_t   when = 0;
    bool          automatic = false;
};

struct ServerFolder
{
    std::string              folder;
    std::vector<std::string> jars;
    std::string              jar;           // the one that starts the server
    JarInfo                  info;
    Eula                     eula;
    bool                     has_properties = false;
    PropertiesFile           properties;
    std::string              level_name = "world";
    std::vector<std::string> entries;       // names at the top of the folder
    std::vector<std::string> worlds;        // folders with a level.dat
    std::vector<Player>      allowlist, operators, bans;
    bool                     lists_readable = true; // false when a player list is not JSON
    bool                     world_locked = false;  // another process holds the world open
};
ServerFolder inspect_folder(const std::string &folder);
// Folders under root (three levels deep) that look like servers: a jar and a
// server.properties or eula.txt.
std::vector<std::string> find_server_folders(const std::string &root, int depth = 3);
// version.json and META-INF/MANIFEST.MF from inside a jar.
JarInfo inspect_jar(const std::string &jar_path);
bool    world_in_use(const std::string &world_folder); // session.lock held by a running server

// Whole-file writes that never leave half a file: a temporary name, then a rename.
bool write_file_atomic(const std::string &path, const std::string &text, std::string *error = nullptr);
bool read_file(const std::string &path, std::string &out, std::uint64_t limit = 64ull * 1024 * 1024);

// Backups -------------------------------------------------------------------
std::vector<BackupFile> list_backups(const std::string &folder);
struct ZipOutcome
{
    bool          ok = false, cancelled = false;
    std::string   error;
    std::uint64_t bytes = 0;
    int           files = 0;
};
// Writes the entries (files and folders, relative to `root`) into a new zip:
// under a temporary name, renamed when complete, then every file's CRC is
// checked. session.lock is left out. progress gets (done, total) bytes.
ZipOutcome write_zip(const std::string &root, const std::vector<std::string> &entries, const std::string &zip_path,
                     const std::function<void(std::uint64_t, std::uint64_t)> &progress = {}, const std::atomic<bool> *cancel = nullptr);
// Extracts a backup into `target` (which must not exist yet). Names that would
// land outside it are refused.
ZipOutcome extract_zip(const std::string &zip_path, const std::string &target, const std::atomic<bool> *cancel = nullptr);
std::vector<std::string> zip_top_entries(const std::string &zip_path);

// Copies a folder tree (for writing a generated world as a folder).
bool copy_tree(const std::string &from, const std::string &to, std::string *error = nullptr);

// Downloads -----------------------------------------------------------------
struct Download
{
    bool          ok = false, cancelled = false;
    std::string   error, body;
    std::uint64_t bytes = 0;
};
// Blocking; call it from a worker thread.
Download fetch(const std::string &url, const std::function<void(std::uint64_t, std::uint64_t)> &progress = {}, const std::atomic<bool> *cancel = nullptr);
// Fetches into `target` (never over an existing file) when the SHA-1 matches.
Download fetch_to_file(const std::string &url, const std::string &target, const std::string &sha1,
                       const std::function<void(std::uint64_t, std::uint64_t)> &progress = {}, const std::atomic<bool> *cancel = nullptr);
constexpr const char *kManifestUrl = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";

// A running server ------------------------------------------------------------
class ServerProcess
{
public:
    using LineFn = std::function<void(const std::string &)>;
    using ExitFn = std::function<void(int /* exit code */)>;
    ServerProcess();
    ~ServerProcess(); // asks nothing: whoever owns it stops the server first
    // argv[0] is the Java executable. Lines and the exit arrive on a worker
    // thread; hand them to the UI thread before touching windows.
    bool start(const std::vector<std::string> &argv, const std::string &folder, LineFn on_line, ExitFn on_exit, std::string *error = nullptr);
    bool send(const std::string &command); // one line, without the line break
    bool running() const;
    unsigned long pid() const;
    std::uint64_t memory_bytes() const; // the process's working set, 0 when unknown
    void          kill();               // ends it at once; the world may lose its last changes
    std::time_t   started() const { return m_started; }

private:
    struct Impl;
    std::shared_ptr<Impl> m; // shared with the console reader, which may outlive this object
    std::time_t           m_started = 0;
};
bool processes_supported(); // false outside Windows in this build

}}} // namespace Slic3r::GUI::GameServers

#endif // slic3r_GUI_GameServers_ServerSystem_hpp_
