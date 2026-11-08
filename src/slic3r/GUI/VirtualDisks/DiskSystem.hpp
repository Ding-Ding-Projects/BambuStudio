#ifndef slic3r_GUI_VirtualDisks_DiskSystem_hpp_
#define slic3r_GUI_VirtualDisks_DiskSystem_hpp_

// What Windows itself knows and does about virtual disks: which routes this
// computer has (the VirtDisk API, the Storage module, DiskPart, Robocopy, the
// Hyper-V module), which disks are attached and where their volumes are
// mounted (asked without administrator permission), free space, finding disk
// files in folders, and running a planned operation with administrator
// permission (one prompt) while its progress lines arrive. On other systems
// every query answers "not available" and nothing runs.

#include "DiskPlan.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace VirtualDisks {

struct Routes
{
    bool        windows = false;
    std::string windows_name; // "Windows 11 Pro, build 26100"
    bool        virtdisk = false, storage = false, diskpart = false, robocopy = false, hyperv = false;
};
Routes find_routes();

struct Attachment
{
    bool                     attached = false;
    std::vector<std::string> mount_points; // "E:\", "D:\Mounts\Projects\"
};
// Attached disks by lower-case path. available is false when Windows could not be asked.
std::map<std::string, Attachment> attached_disks(bool &available);
std::string                       path_key(const std::string &path); // the map's key for a path

std::uint64_t free_space(const std::string &path); // on the drive that would hold path

// Disk files (.vhd, .vhdx, .avhdx, .vhds, .iso) under a folder, at most 2,000.
std::vector<std::string> find_disks(const std::string &folder, int depth = 3);

struct FolderStats
{
    std::uint64_t bytes = 0, files = 0, folders = 0;
    bool          complete = true; // false when cancelled or something could not be read
};
FolderStats folder_stats(const std::string &folder, const std::atomic<bool> *cancel = nullptr);

struct RunOutcome
{
    enum class Status { Done, Failed, Cancelled, PermissionCancelled, Unavailable };
    Status      status = Status::Unavailable;
    std::string message;
    std::string extra_json; // what the script reported back
};

// Runs a planned operation as administrator (Windows asks once) and blocks
// until it ends; call it from a worker thread. progress receives each step the
// script reports; setting cancel asks the script to stop at its next step.
// owner_window: the native handle the permission prompt belongs to.
RunOutcome run_elevated(const Script &script, const std::string &work_dir, const std::function<void(const std::string &)> &progress,
                        const std::atomic<bool> *cancel, void *owner_window);

}}} // namespace Slic3r::GUI::VirtualDisks

#endif // slic3r_GUI_VirtualDisks_DiskSystem_hpp_
