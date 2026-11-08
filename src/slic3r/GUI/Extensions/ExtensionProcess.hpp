#ifndef slic3r_GUI_Extensions_ExtensionProcess_hpp_
#define slic3r_GUI_Extensions_ExtensionProcess_hpp_

// Runs one extension program: one JSON request on standard input, one JSON
// answer read from standard output, nothing else. There is no shell. The
// program starts in its extension folder with a small environment that carries
// no tokens or passwords, and it is killed when it runs past its time limit or
// writes more than the largest answer allowed. On Windows every process it
// starts belongs to a job object that is closed with the run, so nothing it
// started outlives it.

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Extensions {

struct ProcessRequest
{
    std::filesystem::path    folder;     // the extension folder, also the working directory
    std::vector<std::string> argv;       // argv[0] is a file in the folder, or a program on the system path
    std::string              input;      // the JSON request
    int                      timeout_ms = 10000;
    std::size_t              max_output = 64 * 1024;
    const std::atomic<bool> *cancel     = nullptr; // when set, the run is stopped within a tenth of a second
};

struct ProcessResult
{
    enum class Status { Exited, TimedOut, TooLarge, StartFailed, Cancelled };
    Status      status    = Status::StartFailed;
    int         exit_code = -1;
    std::string output;     // at most max_output bytes
    std::string errors;     // the first 4 KB of standard error, for the run record
    double      seconds   = 0;
    std::string start_error; // why it could not start, in plain ASCII
};

// Resolves argv[0]: a name that exists in the folder wins (so a bundled script
// or program runs), otherwise the system directory and then PATH are searched.
// A path that leaves the folder through ".." is refused.
std::filesystem::path resolve_program(const std::filesystem::path &folder, const std::string &program, std::string &error);

ProcessResult run_process(const ProcessRequest &request);

}}} // namespace Slic3r::GUI::Extensions

#endif // slic3r_GUI_Extensions_ExtensionProcess_hpp_
