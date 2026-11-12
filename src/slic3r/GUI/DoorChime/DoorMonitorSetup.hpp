#ifndef slic3r_GUI_DoorMonitorSetup_hpp_
#define slic3r_GUI_DoorMonitorSetup_hpp_

// One-click setup of the Printer Door Monitor, the small web app that keeps
// the door log and rings the door chimes even while Bambu Studio is closed.
//
// setup() runs the published image with Docker Compose on this computer, or on
// a Docker host reached over SSH (DOCKER_HOST=ssh://user@host), waits for it to
// report healthy, then hands it the selected printer and the chimes chosen in
// the Door chime card through `docker exec ... node src/provision-cli.mjs`. The
// monitor answers with a one-time sign-in link, so the page opens signed in.
//
// Both calls block, so they run on a worker thread; callers marshal the
// callbacks back to the GUI thread themselves.

#include <atomic>
#include <functional>
#include <string>

namespace Slic3r { namespace GUI { namespace DoorMonitor {

inline constexpr const char *IMAGE     = "ghcr.io/ding-ding-projects/printer-door-monitor";
inline constexpr const char *CONTAINER = "printer-door-monitor";
inline constexpr int         PORT      = 8790;

// AppConfig keys. The password is kept beside the printer access codes Bambu
// Studio already stores, so the page can still be opened by hand.
inline constexpr const char *TARGET_KEY   = "door_monitor_target"; // "local" or "ssh"
inline constexpr const char *SSH_KEY      = "door_monitor_ssh";    // user@host
inline constexpr const char *URL_KEY      = "door_monitor_url";
inline constexpr const char *PASSWORD_KEY = "door_monitor_password";

struct Request
{
    std::string ssh;          // empty: this computer
    std::string printer_host; // the printer's LAN address
    std::string serial;
    std::string access_code;
    std::string open_chime;
    std::string close_chime;
    std::string password;     // used only when the monitor has none yet
};

enum class Phase { Check, Pull, Start, Health, Configure, Reach };

enum class Error {
    None,
    BadTarget,      // the SSH target is not user@host
    DockerMissing,  // no docker command on this computer
    ComposeMissing,
    EngineStopped,  // Docker Desktop installed but not running
    SshEngine,      // Docker on the SSH host cannot be reached
    Pull,           // the image could not be pulled and none is on the target
    Start,          // compose up failed, usually a taken port
    Health,         // started but never reported healthy
    Configure,      // provisioning was refused or failed
    Reach,          // healthy in Docker but not reachable from this computer
};

struct Result
{
    Error       error = Error::None;
    std::string detail;       // the last lines of the failing command, for the log
    std::string url;          // http://<host>:8790/
    std::string login_url;    // url plus a one-time sign-in path, when there is one
    bool        password_set = false;
    std::string version;
};

bool valid_ssh_target(const std::string &target);

// A random password for a monitor that has none yet.
std::string generate_password();

Result setup(const Request &request, const std::function<void(Phase)> &on_phase, const std::atomic<bool> &cancel);

// Asks an installed monitor for a new one-time sign-in link.
Result sign_in_link(const std::string &ssh, const std::string &url);

}}} // namespace Slic3r::GUI::DoorMonitor

#endif // slic3r_GUI_DoorMonitorSetup_hpp_
