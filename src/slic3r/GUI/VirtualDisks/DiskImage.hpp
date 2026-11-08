#ifndef slic3r_GUI_VirtualDisks_DiskImage_hpp_
#define slic3r_GUI_VirtualDisks_DiskImage_hpp_

// Reads virtual disk files without Windows and without wxWidgets: the VHD
// footer and dynamic header, the VHDX headers, region table and metadata
// (block size, sector sizes, virtual size, the disk ID and the parent
// locator), an ISO's primary volume descriptor, and the partition table
// identity inside a disk (the MBR signature or the GPT disk GUID) that Windows
// compares to decide whether two disks clash. Chains are built from the parent
// paths and tested against the parents' identities. Tested on its own by
// tests/virtual_disks_test.cpp; nothing here writes to a disk.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace VirtualDisks {

enum class Format { Unknown, Vhd, Vhdx, VhdSet, Iso };
enum class Kind { Unknown, Fixed, Dynamic, Differencing, ReadOnly };

// A disk as its file describes it.
struct DiskInfo
{
    std::string path;          // UTF-8
    Format      format = Format::Unknown;
    Kind        kind   = Kind::Unknown;
    bool        readable = false; // the header was found and is whole
    std::string problem;          // why it is not readable, in plain words

    std::uint64_t virtual_size  = 0; // bytes the disk shows to Windows
    std::uint64_t file_size     = 0; // bytes the file takes on its drive
    std::uint32_t block_size    = 0; // dynamic and differencing disks
    std::uint32_t logical_sector  = 512;
    std::uint32_t physical_sector = 512;
    std::string   virtual_disk_id; // VHD unique id or VHDX page 83 id, "{...}"
    std::string   data_write_guid; // VHDX: what a child's parent_linkage must match
    std::string   label;           // ISO volume label

    // Differencing disks: where the parent should be.
    std::string parent_linkage;      // VHDX parent_linkage or VHD parent unique id
    std::string parent_relative;     // relative path as stored
    std::string parent_absolute;     // absolute path as stored
    std::string parent_name;         // VHD parent unicode name

    // The partition table inside the disk, when its first block is in this file.
    std::string partition_style;     // "GPT", "MBR", "Raw" or "" when unknown
    std::string partition_disk_id;   // GPT disk GUID "{...}" or MBR signature "1A2B3C4D"
};

DiskInfo read_disk(const std::string &path);
// The same, from bytes already in memory (the tests build images this way).
DiskInfo read_disk_bytes(const std::string &path, const std::vector<std::uint8_t> &bytes);

Format format_from_extension(std::string_view path);
const char *format_name(Format format);   // "VHD", "VHDX", "VHD Set", "ISO"
const char *kind_name(Kind kind);         // "fixed", "dynamic", "differencing", "read-only"

// CRC-32C (Castagnoli), as VHDX headers and tables are checked.
std::uint32_t crc32c(const std::uint8_t *data, std::size_t size);
std::string   guid_text(const std::uint8_t bytes[16], bool big_endian = false);

// ---------------------------------------------------------------- chains

struct ChainLink
{
    std::size_t disk   = 0;  // index into the disks
    int         parent = -1; // index of the parent, -1 for a base or a missing parent
    int         depth  = 0;  // 0 for a base
};

enum class ChainResult { NotAChain, Passed, ParentMissing, ParentChanged, Loop, Unreadable };

struct ChainTest
{
    ChainResult result = ChainResult::NotAChain;
    std::string detail; // plain words: which file, which identity
};

// Resolves the stored parent path of each differencing disk against the other
// disks (absolute path first, then the relative one from the child's folder,
// then the file name in the same folder).
int find_parent(const std::vector<DiskInfo> &disks, std::size_t child);
ChainTest test_chain(const std::vector<DiskInfo> &disks, std::size_t disk);
// Disks that are parents of another disk in the list: merging into or editing
// them breaks their children, so they show as protected.
std::vector<std::size_t> children_of(const std::vector<DiskInfo> &disks, std::size_t parent);
// Pairs of disks whose partition tables carry the same identity: Windows keeps
// the second one offline while the first is attached.
std::vector<std::pair<std::size_t, std::size_t>> same_disk_ids(const std::vector<DiskInfo> &disks);

std::string size_text(std::uint64_t bytes); // "256 GB", "41.2 GB", "512 MB"

}}} // namespace Slic3r::GUI::VirtualDisks

#endif // slic3r_GUI_VirtualDisks_DiskImage_hpp_
