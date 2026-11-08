#ifndef slic3r_GUI_VirtualDisks_DiskPlan_hpp_
#define slic3r_GUI_VirtualDisks_DiskPlan_hpp_

// What a virtual disk operation will do, without doing it: the limits a new
// disk is checked against before anything is written, and the exact steps of
// every operation as a PowerShell script (the VirtDisk API through Add-Type,
// and the Storage module) and, where DiskPart can do the same, as a DiskPart
// script. The app runs the PowerShell form once, with administrator
// permission; Copy as PowerShell and Copy as DiskPart hand out the same steps.
// Also the library of disks the person added and the history of operations.
// No wxWidgets; tested by tests/virtual_disks_test.cpp.

#include "DiskImage.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace VirtualDisks {

constexpr std::uint64_t kMB = 1024ull * 1024;
constexpr std::uint64_t kGB = 1024ull * kMB;
constexpr std::uint64_t kTB = 1024ull * kGB;

struct CreateRequest
{
    std::string   path;                 // UTF-8, with its extension
    Format        format          = Format::Vhdx;
    Kind          kind            = Kind::Dynamic; // Fixed, Dynamic or Differencing (a child)
    std::uint64_t size            = 64 * kGB;      // ignored for a child
    std::uint32_t block_size      = 0;             // 0 is Windows' default
    std::uint32_t logical_sector  = 512;
    std::uint32_t physical_sector = 4096;
    std::string   parent;                          // a child's parent

    // Inside the new disk.
    std::string partition_style = "GPT";  // "GPT", "MBR" or "" to leave it raw
    std::string file_system     = "NTFS"; // NTFS, ReFS, exFAT or FAT32
    std::string label;
    char        drive_letter = 0;         // 0 picks the next free letter
    bool        attach       = true;
    bool        quick_format = true;
};

struct Validation
{
    std::vector<std::string> errors; // stop: nothing is written while any is left
    std::vector<std::string> notes;  // worth knowing, nothing stops
    bool ok() const { return errors.empty(); }
};

// free_bytes: what the target drive has free; parent: the parent's info for a child.
Validation validate(const CreateRequest &r, std::uint64_t free_bytes, bool target_exists, const DiskInfo *parent = nullptr);

enum class Route { VirtDisk, Storage, DiskPart, Robocopy };
const char *route_name(Route route); // "VirtDisk API", "Storage module", "DiskPart", "Robocopy"

enum class CompactMode { Full, Quick };

struct Script
{
    std::string powershell; // what runs; empty when PowerShell cannot do it
    std::string diskpart;   // the same steps for DiskPart; empty when it cannot do them
    Route       route = Route::VirtDisk;
    std::string summary;    // one line for the history
};

std::string ps_quote(const std::string &text);       // 'it''s'
std::string diskpart_quote(const std::string &text); // "C:\a b\x.vhdx"

Script create_script(const CreateRequest &r);
Script attach_script(const std::string &path, bool read_only, bool no_drive_letter = false);
Script detach_script(const std::string &path);
// Grows the file and, when it is attached with a drive letter, the volume.
Script grow_script(const std::string &path, std::uint64_t new_size, bool attached);
Script compact_script(const std::string &path, CompactMode mode, bool attached);
Script child_script(const std::string &parent, const std::string &child);
// Merges a child into its parent. backup: a verified copy of the parent first.
Script merge_script(const std::string &child, const std::string &parent, const std::string &backup);
Script reconnect_parent_script(const std::string &child, const std::string &new_parent);
// A new partition table identity (GPT disk GUID or MBR signature), so two
// disks no longer clash. The disk is attached for the change and detached after.
Script new_disk_id_script(const std::string &path, const std::string &partition_style);

// Quick pack: a new dynamic VHDX sized to fit a folder, the folder copied in
// with its tree, times, attributes and streams, every file checked by
// SHA-256, and the disk detached unless keep_attached.
struct PackRequest
{
    std::string   source;      // folder, UTF-8
    std::string   target;      // new .vhdx, UTF-8
    std::uint64_t source_bytes = 0;
    std::uint64_t source_files = 0;
    std::string   label;
    bool          keep_attached = false;
};
std::uint64_t pack_size(std::uint64_t source_bytes, std::uint64_t source_files); // fitted tightly, whole MB
std::string   unique_target(const std::string &folder, const std::string &name, const std::vector<std::string> &taken); // "Photos (1).vhdx"
Script        pack_script(const PackRequest &r);

// Wraps a script for running: results and progress go to files the app reads,
// and the script stops at the next step once the cancel file appears.
std::string runner_script(const Script &s, const std::string &result_path, const std::string &progress_path, const std::string &cancel_path);

// ---------------------------------------------------------------- library

struct HistoryEntry
{
    std::int64_t time = 0; // seconds since 1970
    std::string  operation;
    std::string  disk;     // file name
    std::string  route;
    std::string  result;   // "Done", "Incomplete", "Failed"
    std::string  detail;
};

struct Library
{
    std::vector<std::string>  disks;   // paths the person added, UTF-8
    std::vector<std::string>  folders; // folders scanned for disks
    std::vector<HistoryEntry> history; // newest first, at most 200
    void add_disk(const std::string &path);
    void remove_disk(const std::string &path);
    void record(HistoryEntry entry);
};

std::string to_json(const Library &library);
bool        from_json(const std::string &json, Library &library);

}}} // namespace Slic3r::GUI::VirtualDisks

#endif // slic3r_GUI_VirtualDisks_DiskPlan_hpp_
