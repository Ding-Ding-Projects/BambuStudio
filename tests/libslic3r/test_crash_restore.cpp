// Regression tests for the crash-recovery lock check.
//
// After a crash, BambuStudio finds a backup directory containing a `.3mf` snapshot and a
// `lock.txt` naming the pid that owned it. Slic3r::has_restore_data() decides from those
// whether to offer the user their unsaved work back. Three defects lived in that decision:
//
//  1. get_process_name() tested OpenProcess's failure value against INVALID_HANDLE_VALUE.
//     OpenProcess signals failure with NULL, so a dead pid - the normal input here - fell
//     through to GetModuleFileNameEx(NULL, ...) and then CloseHandle(NULL).
//
//     Scope note, because an earlier version of this comment overstated it: that is a real
//     defect (wrong sentinel, two API calls on a null handle, an invalid-handle close) but it
//     is NOT observably fatal. Both the old and the new code return an empty name for a dead
//     pid, so has_restore_data() behaves identically either way. An attempt to catch it by
//     enabling ProcessStrictHandleCheckPolicy in a child process was withdrawn: a control that
//     closed a garbage non-null handle under the same policy also survived, proving the probe
//     was never armed and the test could not fail whether the bug was present or not. Only
//     defects 2 and 3 below have discriminating regression tests here - and they do fail
//     without their fixes.
//  2. Windows reuses freed pids. Relaunching straight after a crash can hand the new
//     instance the crashed instance's pid, at which point the lock check compared the
//     process against itself, concluded a live owner held the backup, and silently
//     declined to offer recovery.
//  3. load_string_file() sat outside the try block, so an unreadable or racily-deleted
//     lock file threw out of has_restore_data() into the EVT_RESTORE_PROJECT handler,
//     where an unhandled exception takes the app down at startup.

#include <catch_main.hpp>

#include "libslic3r/Format/bbs_3mf.hpp"
#include "libslic3r/Utils.hpp"

#include <boost/filesystem/operations.hpp>

#include <cstdlib>
#include <fstream>
#include <string>

#ifdef WIN32
#include <windows.h>
#endif

using namespace Slic3r;

namespace {

struct ScopedBackupDirectory
{
    explicit ScopedBackupDirectory(const std::string &tag)
        : path((boost::filesystem::temp_directory_path() /
                boost::filesystem::unique_path("bambu-restore-" + tag + "-%%%%-%%%%"))
                   .string())
    {
        boost::filesystem::create_directories(path);
    }

    ~ScopedBackupDirectory()
    {
        boost::system::error_code ignored;
        boost::filesystem::remove_all(path, ignored);
    }

    // The crash snapshot is literally named ".3mf" - it has no stem, by path rules.
    void write_snapshot() const { write_file(path + "/.3mf", "snapshot"); }
    void write_lock(const std::string &body) const { write_file(path + "/lock.txt", body); }
    void write_origin(const std::string &body) const { write_file(path + "/origin.txt", body); }

    static void write_file(const std::string &file, const std::string &body)
    {
        std::ofstream stream(file, std::ios::binary);
        stream << body;
    }

    std::string path;
};

#ifdef WIN32
// A pid no live process owns, which is what a fully reaped crashed instance leaves behind.
int find_free_pid()
{
    for (int candidate = 60000; candidate < 65536; candidate += 4) {
        HANDLE probe = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, candidate);
        if (probe == NULL)
            return candidate;
        ::CloseHandle(probe);
    }
    return 0;
}
#endif

} // namespace

#ifdef WIN32

TEST_CASE("OpenProcess reports failure with NULL, not INVALID_HANDLE_VALUE", "[crash_restore]")
{
    // This is the invariant get_process_name()'s guard depends on. If Windows ever changed
    // it, the guard below would be the thing to revisit.
    const int free_pid = find_free_pid();
    REQUIRE(free_pid != 0);

    HANDLE handle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, free_pid);
    CHECK(handle == NULL);
    CHECK(handle != INVALID_HANDLE_VALUE);
}

TEST_CASE("get_process_name reports the running executable and nothing for a dead pid",
          "[crash_restore]")
{
    const std::string self = get_process_name(0);
    CHECK_FALSE(self.empty());

    const int free_pid = find_free_pid();
    REQUIRE(free_pid != 0);
    // Must be empty rather than stack garbage: has_restore_data() compares this against the
    // running executable's name, and a garbage match would suppress recovery.
    CHECK(get_process_name(free_pid).empty());
    CHECK(get_process_name(free_pid) != self);
}

#endif // WIN32

TEST_CASE("has_restore_data declines when there is nothing to restore", "[crash_restore]")
{
    SECTION("no backup directory was recorded") {
        std::string path;
        std::string origin;
        CHECK_FALSE(has_restore_data(path, origin));
        CHECK(origin == "<lock>");
    }

    SECTION("the directory holds no snapshot") {
        ScopedBackupDirectory backup("empty");
        std::string           path   = backup.path;
        std::string           origin;
        CHECK_FALSE(has_restore_data(path, origin));
    }
}

TEST_CASE("has_restore_data offers an unlocked snapshot and reports its origin",
          "[crash_restore]")
{
    ScopedBackupDirectory backup("unlocked");
    backup.write_snapshot();
    backup.write_origin("C:\\projects\\widget.3mf");

    std::string path   = backup.path;
    std::string origin;
    REQUIRE(has_restore_data(path, origin));
    CHECK(path == backup.path + "/.3mf");
    CHECK(origin == "C:\\projects\\widget.3mf");
}

TEST_CASE("has_restore_data offers a snapshot whose owning process is gone", "[crash_restore]")
{
    // The defining case: the app crashed, so the pid in lock.txt names nothing.
    ScopedBackupDirectory backup("dead-owner");
    backup.write_snapshot();
#ifdef WIN32
    const int free_pid = find_free_pid();
    REQUIRE(free_pid != 0);
    backup.write_lock(std::to_string(free_pid));
#else
    backup.write_lock("999999");
#endif

    std::string path   = backup.path;
    std::string origin;
    CHECK(has_restore_data(path, origin));
    CHECK(path == backup.path + "/.3mf");
}

TEST_CASE("has_restore_data treats its own pid in the lock as reuse, not a live owner",
          "[crash_restore]")
{
    // Windows hands out freed pids again. A relaunch right after a crash can land on the
    // crashed instance's pid, and the process asking the question is definitionally not the
    // instance that wrote the lock.
    ScopedBackupDirectory backup("pid-reuse");
    backup.write_snapshot();
    backup.write_lock(std::to_string(get_current_pid()));

    std::string path   = backup.path;
    std::string origin;
    CHECK(has_restore_data(path, origin));
    CHECK(path == backup.path + "/.3mf");
}

TEST_CASE("has_restore_data declines without throwing when the lock cannot be read",
          "[crash_restore]")
{
    SECTION("the lock body is not a pid") {
        ScopedBackupDirectory backup("corrupt-lock");
        backup.write_snapshot();
        backup.write_lock("not-a-pid");

        std::string path   = backup.path;
        std::string origin;
        CHECK_NOTHROW(has_restore_data(path, origin));
        std::string retry_path = backup.path;
        std::string retry_origin;
        CHECK_FALSE(has_restore_data(retry_path, retry_origin));
    }

    SECTION("the lock body is empty because the write was interrupted") {
        ScopedBackupDirectory backup("truncated-lock");
        backup.write_snapshot();
        backup.write_lock("");

        std::string path   = backup.path;
        std::string origin;
        CHECK_NOTHROW(has_restore_data(path, origin));
    }

    SECTION("the lock path exists but is not a readable file") {
        // exists() succeeds and the read then fails - the race that used to throw straight
        // through has_restore_data() and out of the startup event handler.
        ScopedBackupDirectory backup("unreadable-lock");
        backup.write_snapshot();
        boost::filesystem::create_directories(backup.path + "/lock.txt");

        std::string path   = backup.path;
        std::string origin;
        CHECK_NOTHROW(has_restore_data(path, origin));
    }
}
