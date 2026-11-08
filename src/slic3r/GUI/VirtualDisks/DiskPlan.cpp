#include "DiskPlan.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace Slic3r { namespace GUI { namespace VirtualDisks {

namespace {

std::string lowered(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::string extension_of(const std::string &path)
{
    const auto dot = path.find_last_of('.');
    const auto sep = path.find_last_of("/\\");
    if (dot == std::string::npos || (sep != std::string::npos && dot < sep)) return std::string();
    return lowered(path.substr(dot + 1));
}

std::string leaf_name(const std::string &path)
{
    const auto sep = path.find_last_of("/\\");
    return sep == std::string::npos ? path : path.substr(sep + 1);
}

std::string mb(std::uint64_t bytes) { return std::to_string((bytes + kMB - 1) / kMB); }

bool power_of_two(std::uint64_t v) { return v != 0 && (v & (v - 1)) == 0; }

// The VirtDisk API from PowerShell, with no Hyper-V module: CreateVirtualDisk,
// OpenVirtualDisk, ResizeVirtualDisk, CompactVirtualDisk, MergeVirtualDisk and
// SetVirtualDiskInformation, with the x64 layouts of their version 1 and 2
// parameter structures.
const char *kVirtDiskType = R"CS(
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class BambuVirtDisk {
    [StructLayout(LayoutKind.Sequential)]
    public struct StorageType { public uint DeviceId; public Guid VendorId; }
    static readonly Guid Microsoft = new Guid("EC984AEC-A0F9-47E9-901F-71415A66345B");

    [StructLayout(LayoutKind.Explicit, Size = 192)]
    public struct CreateV2 {
        [FieldOffset(0)] public int Version;
        [FieldOffset(8)] public Guid UniqueId;
        [FieldOffset(24)] public ulong MaximumSize;
        [FieldOffset(32)] public uint BlockSizeInBytes;
        [FieldOffset(36)] public uint SectorSizeInBytes;
        [FieldOffset(40)] public uint PhysicalSectorSizeInBytes;
        [FieldOffset(48)] public IntPtr ParentPath;
        [FieldOffset(56)] public IntPtr SourcePath;
        [FieldOffset(64)] public int OpenFlags;
        [FieldOffset(68)] public uint ParentDeviceId;
        [FieldOffset(72)] public Guid ParentVendorId;
        [FieldOffset(88)] public uint SourceDeviceId;
        [FieldOffset(92)] public Guid SourceVendorId;
        [FieldOffset(108)] public Guid ResiliencyGuid;
    }
    [StructLayout(LayoutKind.Explicit, Size = 48)]
    public struct OpenV2 {
        [FieldOffset(0)] public int Version;
        [FieldOffset(4)] public int GetInfoOnly;
        [FieldOffset(8)] public int ReadOnly;
        [FieldOffset(12)] public Guid ResiliencyGuid;
    }
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct ResizeV1 { [FieldOffset(0)] public int Version; [FieldOffset(8)] public ulong NewSize; }
    [StructLayout(LayoutKind.Sequential)]
    public struct CompactV1 { public int Version; public uint Reserved; }
    [StructLayout(LayoutKind.Sequential)]
    public struct MergeV2 { public int Version; public uint MergeSourceDepth; public uint MergeTargetDepth; }
    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct SetParent { [FieldOffset(0)] public int Version; [FieldOffset(8)] public IntPtr ParentFilePath; }

    [DllImport("virtdisk.dll", CharSet = CharSet.Unicode)]
    static extern uint CreateVirtualDisk(ref StorageType type, string path, uint access, IntPtr security, uint flags, uint providerFlags, ref CreateV2 parameters, IntPtr overlapped, out IntPtr handle);
    [DllImport("virtdisk.dll", CharSet = CharSet.Unicode)]
    static extern uint OpenVirtualDisk(ref StorageType type, string path, uint access, uint flags, ref OpenV2 parameters, out IntPtr handle);
    [DllImport("virtdisk.dll")]
    static extern uint ResizeVirtualDisk(IntPtr handle, uint flags, ref ResizeV1 parameters, IntPtr overlapped);
    [DllImport("virtdisk.dll")]
    static extern uint CompactVirtualDisk(IntPtr handle, uint flags, ref CompactV1 parameters, IntPtr overlapped);
    [DllImport("virtdisk.dll")]
    static extern uint MergeVirtualDisk(IntPtr handle, uint flags, ref MergeV2 parameters, IntPtr overlapped);
    [DllImport("virtdisk.dll")]
    static extern uint SetVirtualDiskInformation(IntPtr handle, ref SetParent info);
    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr handle);

    static void Check(uint result, string what) {
        if (result != 0) throw new Win32Exception((int)result, what + ": " + new Win32Exception((int)result).Message);
    }
    static IntPtr Open(string path, uint flags) {
        var type = new StorageType();
        var p = new OpenV2(); p.Version = 2;
        IntPtr h;
        Check(OpenVirtualDisk(ref type, path, 0, flags, ref p, out h), "OpenVirtualDisk");
        return h;
    }
    public static void Create(string path, uint deviceId, ulong size, uint block, uint logical, uint physical, string parent, bool full) {
        var type = new StorageType(); type.DeviceId = deviceId; type.VendorId = Microsoft;
        var p = new CreateV2(); p.Version = 2; p.UniqueId = Guid.NewGuid();
        IntPtr parentText = IntPtr.Zero;
        if (!String.IsNullOrEmpty(parent)) {
            parentText = Marshal.StringToHGlobalUni(parent);
            p.ParentPath = parentText;
        } else {
            p.MaximumSize = size; p.BlockSizeInBytes = block; p.SectorSizeInBytes = logical; p.PhysicalSectorSizeInBytes = physical;
        }
        IntPtr h;
        try { Check(CreateVirtualDisk(ref type, path, 0, IntPtr.Zero, full ? 1u : 0u, 0, ref p, IntPtr.Zero, out h), "CreateVirtualDisk"); CloseHandle(h); }
        finally { if (parentText != IntPtr.Zero) Marshal.FreeHGlobal(parentText); }
    }
    public static void Resize(string path, ulong size) {
        IntPtr h = Open(path, 0);
        try { var p = new ResizeV1(); p.Version = 1; p.NewSize = size; Check(ResizeVirtualDisk(h, 0, ref p, IntPtr.Zero), "ResizeVirtualDisk"); }
        finally { CloseHandle(h); }
    }
    public static void Compact(string path, bool quick) {
        IntPtr h = Open(path, 0);
        try { var p = new CompactV1(); p.Version = 1; Check(CompactVirtualDisk(h, quick ? 1u : 0u, ref p, IntPtr.Zero), "CompactVirtualDisk"); }
        finally { CloseHandle(h); }
    }
    public static void Merge(string child) {
        IntPtr h = Open(child, 0);
        try { var p = new MergeV2(); p.Version = 2; p.MergeSourceDepth = 1; p.MergeTargetDepth = 2; Check(MergeVirtualDisk(h, 0, ref p, IntPtr.Zero), "MergeVirtualDisk"); }
        finally { CloseHandle(h); }
    }
    public static void SetParentPath(string child, string parent) {
        IntPtr h = Open(child, 1);
        IntPtr text = Marshal.StringToHGlobalUni(parent);
        try { var p = new SetParent(); p.Version = 1; p.ParentFilePath = text; Check(SetVirtualDiskInformation(h, ref p), "SetVirtualDiskInformation"); }
        finally { Marshal.FreeHGlobal(text); CloseHandle(h); }
    }
}
)CS";

std::string virtdisk_block()
{
    // The type is added once per PowerShell session.
    return std::string("if (-not ('BambuVirtDisk' -as [type])) {\nAdd-Type -TypeDefinition @'") + kVirtDiskType + "'@\n}\n";
}

std::string device_id(Format f) { return f == Format::Vhd ? "2" : "3"; }

// The partition and file system steps after a disk is attached as $disk.
std::string prepare_volume_ps(const std::string &style, const std::string &fs, const std::string &label, char letter, bool quick, bool dev_drive = false)
{
    std::ostringstream ps;
    if (style.empty()) return std::string();
    ps << "Initialize-Disk -Number $disk.Number -PartitionStyle " << style << "\n";
    ps << "$part = New-Partition -DiskNumber $disk.Number -UseMaximumSize " << (letter ? std::string("-DriveLetter ") + letter : std::string("-AssignDriveLetter")) << "\n";
    ps << "Format-Volume -Partition $part -FileSystem " << fs;
    if (!label.empty()) ps << " -NewFileSystemLabel " << ps_quote(label);
    if (!quick) ps << " -Full";
    if (dev_drive) ps << " -DevDrive";
    ps << " -Confirm:$false | Out-Null\n";
    return ps.str();
}

std::string prepare_volume_diskpart(const std::string &style, const std::string &fs, const std::string &label, char letter, bool quick)
{
    if (style.empty()) return std::string();
    std::ostringstream dp;
    dp << "convert " << lowered(style) << "\n";
    dp << "create partition primary\n";
    dp << "format fs=" << lowered(fs);
    if (!label.empty()) dp << " label=" << diskpart_quote(label);
    if (quick) dp << " quick";
    dp << "\n";
    if (letter) dp << "assign letter=" << letter << "\n";
    else dp << "assign\n";
    return dp.str();
}

} // namespace

const char *route_name(Route route)
{
    switch (route) {
    case Route::VirtDisk: return "VirtDisk API";
    case Route::Storage: return "Storage module";
    case Route::DiskPart: return "DiskPart";
    case Route::Robocopy: return "VirtDisk API, Storage module and Robocopy";
    }
    return "";
}

std::string ps_quote(const std::string &text)
{
    std::string out = "'";
    for (char c : text) out += c == '\'' ? std::string("''") : std::string(1, c);
    return out + "'";
}

std::string diskpart_quote(const std::string &text)
{
    std::string out;
    for (char c : text)
        if (c != '"') out += c;
    return "\"" + out + "\"";
}

Validation validate(const CreateRequest &r, std::uint64_t free_bytes, bool target_exists, const DiskInfo *parent)
{
    Validation v;
    const std::string ext = extension_of(r.path);
    const bool        vhd = r.format == Format::Vhd;
    if (r.format != Format::Vhd && r.format != Format::Vhdx) v.errors.push_back("A new disk is a VHD or a VHDX.");
    if (r.path.empty()) v.errors.push_back("Choose where the disk goes.");
    else if (vhd && ext != "vhd" && ext != "avhd") v.errors.push_back("A VHD file ends in .vhd.");
    else if (!vhd && ext != "vhdx" && ext != "avhdx") v.errors.push_back("A VHDX file ends in .vhdx (or .avhdx for a child).");
    if (target_exists) v.errors.push_back(leaf_name(r.path) + " already exists. Nothing is overwritten: choose another name.");

    if (r.kind == Kind::Differencing) {
        if (r.parent.empty()) v.errors.push_back("A child needs a parent disk.");
        else if (!parent || !parent->readable) v.errors.push_back("The parent disk cannot be read.");
        else if (parent->format != r.format) v.errors.push_back(std::string("A ") + format_name(r.format) + " child needs a " + format_name(r.format) + " parent.");
        else v.notes.push_back("The child starts empty and reads everything else from " + leaf_name(r.parent) + ", which must not change while the child exists.");
        return v; // size, sectors and the volume all come from the parent
    }

#ifndef SLIC3R_VIRTUAL_DISKS_NEGATIVE_FIXTURE
    const std::uint64_t most = vhd ? 2040 * kGB : 64 * kTB;
    if (r.size < 3 * kMB || r.size > most)
        v.errors.push_back(vhd ? "A VHD holds 3 MB to 2,040 GB." : "A VHDX holds 3 MB to 64 TB.");
#endif
    if (r.size % kMB != 0) v.errors.push_back("The size is a whole number of megabytes.");

    if (r.block_size != 0) {
        if (vhd && r.block_size != 512 * 1024 && r.block_size != 2 * kMB) v.errors.push_back("A VHD's block is 512 KB or 2 MB.");
        if (!vhd && (r.block_size < kMB || r.block_size > 256 * kMB || !power_of_two(r.block_size)))
            v.errors.push_back("A VHDX block is 1, 2, 4, 8, 16, 32, 64, 128 or 256 MB.");
        if (r.kind == Kind::Fixed) v.notes.push_back("A fixed disk has every block from the start, so the block size only matters to its children.");
    }
    if (vhd && r.logical_sector != 512) v.errors.push_back("A VHD has 512-byte logical sectors.");
    if (!vhd && r.logical_sector != 512 && r.logical_sector != 4096) v.errors.push_back("Logical sectors are 512 or 4096 bytes.");
    if (r.physical_sector != 512 && r.physical_sector != 4096) v.errors.push_back("Physical sectors are 512 or 4096 bytes.");
    if (r.logical_sector > r.physical_sector) v.errors.push_back("Logical sectors cannot be larger than physical ones.");

    if (r.kind == Kind::Fixed) {
        if (free_bytes < r.size + 64 * kMB) v.errors.push_back("The drive has room for " + size_text(free_bytes) + ", and a fixed disk takes all " + size_text(r.size) + " at once.");
        else v.notes.push_back("A fixed disk takes all " + size_text(r.size) + " now and is written out in full, which takes a while.");
    } else if (r.kind == Kind::Dynamic) {
        v.notes.push_back("The drive has " + size_text(free_bytes) + " free. A dynamic disk only takes what it holds and does not reserve " + size_text(r.size) + ".");
    }
    if (!vhd && r.block_size != 0) v.notes.push_back("Block size " + size_text(r.block_size) + " runs through the VirtDisk API. DiskPart would use its default.");

    if (!r.partition_style.empty()) {
        if (r.partition_style != "GPT" && r.partition_style != "MBR") v.errors.push_back("The partition table is GPT, MBR or none.");
        if (r.partition_style == "MBR" && r.size > 2 * kTB) v.errors.push_back("An MBR disk uses at most 2 TB; choose GPT for this size.");
        const std::string fs = r.file_system;
        if (fs != "NTFS" && fs != "ReFS" && fs != "exFAT" && fs != "FAT32") v.errors.push_back("The file system is NTFS, ReFS, exFAT or FAT32.");
        if (fs == "FAT32" && r.size > 32 * kGB) v.errors.push_back("Windows formats FAT32 up to 32 GB; choose exFAT or NTFS.");
        if (fs == "ReFS") v.notes.push_back("Windows formats ReFS on Pro for Workstations, Enterprise and Server editions, and on a Dev Drive.");
        const std::size_t limit = fs == "FAT32" ? 11 : fs == "exFAT" ? 15 : 32;
        if (r.label.size() > limit) v.errors.push_back("A " + fs + " label has at most " + std::to_string(limit) + " characters.");
        if (r.drive_letter && !(r.drive_letter >= 'D' && r.drive_letter <= 'Z')) v.errors.push_back("The drive letter is D to Z.");
        if (!r.attach) v.errors.push_back("The disk is attached to make its partition and file system; choose to attach it, or leave it raw.");
        if (r.dev_drive) {
            if (fs != "ReFS") v.errors.push_back("A Dev Drive is formatted ReFS.");
            if (r.size < 50 * kGB) v.errors.push_back("A Dev Drive is at least 50 GB.");
            v.notes.push_back("A Dev Drive needs Windows 11 build 22621.2338 or later; Windows marks it trusted, and antivirus stays on for it.");
        }
    }
    return v;
}

Script create_script(const CreateRequest &r)
{
    Script s;
    s.route   = Route::VirtDisk;
    s.summary = "Create " + leaf_name(r.path);
    const bool child = r.kind == Kind::Differencing;
    std::ostringstream ps;
    ps << virtdisk_block();
    ps << "Step 'Creating " << leaf_name(r.path) << "'\n";
    ps << "[BambuVirtDisk]::Create(" << ps_quote(r.path) << ", " << device_id(r.format) << ", " << (child ? 0 : r.size) << ", "
       << (child ? 0 : r.block_size) << ", " << (child ? 0 : r.logical_sector) << ", " << (child ? 0 : r.physical_sector) << ", "
       << ps_quote(child ? r.parent : std::string()) << ", $" << (r.kind == Kind::Fixed ? "true" : "false") << ")\n";
    if (r.attach) {
        ps << "Step 'Attaching'\n";
        ps << "$image = Mount-DiskImage -ImagePath " << ps_quote(r.path) << " -PassThru" << (r.partition_style.empty() || child ? "" : " -NoDriveLetter") << "\n";
        if (!child && !r.partition_style.empty()) {
            ps << "$disk = $image | Get-Disk\n";
            ps << "Step 'Making the partition and the file system'\n";
            ps << prepare_volume_ps(r.partition_style, r.file_system, r.label, r.drive_letter, r.quick_format, r.dev_drive);
        }
    }
    s.powershell = ps.str();

    // DiskPart makes the same disk with its own block and sector defaults.
    std::ostringstream dp;
    dp << "create vdisk file=" << diskpart_quote(r.path);
    if (child) dp << " parent=" << diskpart_quote(r.parent) << "\n";
    else dp << " maximum=" << mb(r.size) << " type=" << (r.kind == Kind::Fixed ? "fixed" : "expandable") << "\n";
    if (r.attach) {
        dp << "select vdisk file=" << diskpart_quote(r.path) << "\nattach vdisk\n";
        if (!child) dp << prepare_volume_diskpart(r.partition_style, r.file_system, r.label, r.drive_letter, r.quick_format);
    }
    s.diskpart = dp.str();
    return s;
}

Script attach_script(const std::string &path, bool read_only, bool no_drive_letter)
{
    Script s;
    s.route   = Route::Storage;
    s.summary = std::string(read_only ? "Attach read-only " : "Attach ") + leaf_name(path);
    s.powershell = "Step 'Attaching'\nMount-DiskImage -ImagePath " + ps_quote(path) + (read_only ? " -Access ReadOnly" : "") +
                   (no_drive_letter ? " -NoDriveLetter" : "") + " | Out-Null\n";
    s.diskpart = "select vdisk file=" + diskpart_quote(path) + "\nattach vdisk" + (read_only ? " readonly" : "") + "\n";
    return s;
}

Script detach_script(const std::string &path)
{
    Script s;
    s.route      = Route::Storage;
    s.summary    = "Detach " + leaf_name(path);
    s.powershell = "Step 'Detaching'\nDismount-DiskImage -ImagePath " + ps_quote(path) + " | Out-Null\n";
    s.diskpart   = "select vdisk file=" + diskpart_quote(path) + "\ndetach vdisk\n";
    return s;
}

Script grow_script(const std::string &path, std::uint64_t new_size, bool attached)
{
    Script s;
    s.route   = Route::VirtDisk;
    s.summary = "Grow " + leaf_name(path) + " to " + size_text(new_size);
    std::ostringstream ps;
    ps << virtdisk_block();
    ps << "Step 'Growing the file to " << size_text(new_size) << "'\n";
    ps << "[BambuVirtDisk]::Resize(" << ps_quote(path) << ", " << new_size << ")\n";
    if (attached) {
        // Extends the last volume into the new space, in the same run.
        ps << "Step 'Extending the volume'\n";
        ps << "$disk = Get-DiskImage -ImagePath " << ps_quote(path) << " | Get-Disk\n";
        ps << "Update-Disk -Number $disk.Number\n";
        ps << "$part = Get-Partition -DiskNumber $disk.Number | Where-Object { $_.Type -ne 'Reserved' } | Sort-Object Offset | Select-Object -Last 1\n";
        ps << "if ($part) { $max = (Get-PartitionSupportedSize -DiskNumber $disk.Number -PartitionNumber $part.PartitionNumber).SizeMax\n";
        ps << "  if ($max -gt $part.Size) { Resize-Partition -DiskNumber $disk.Number -PartitionNumber $part.PartitionNumber -Size $max } }\n";
    }
    s.powershell = ps.str();
    std::ostringstream dp;
    dp << "select vdisk file=" << diskpart_quote(path) << "\n";
    if (attached) dp << "detach vdisk\n";
    dp << "expand vdisk maximum=" << mb(new_size) << "\n";
    if (attached) dp << "attach vdisk\nrem Then: select volume <letter>, extend\n";
    s.diskpart = dp.str();
    return s;
}

Script compact_script(const std::string &path, CompactMode mode, bool attached)
{
    Script s;
    s.route   = Route::VirtDisk;
    s.summary = std::string(mode == CompactMode::Quick ? "Compact (quick) " : "Compact (full) ") + leaf_name(path);
    std::ostringstream ps;
    ps << virtdisk_block();
    // Compacting needs the disk detached or attached read-only; a read-write
    // disk is reattached read-only for the run and put back as it was.
    if (attached) {
        ps << "Step 'Reattaching read-only for the run'\n";
        ps << "Dismount-DiskImage -ImagePath " << ps_quote(path) << " | Out-Null\n";
        ps << "Mount-DiskImage -ImagePath " << ps_quote(path) << " -Access ReadOnly | Out-Null\n";
    }
    ps << "$before = (Get-Item -LiteralPath " << ps_quote(path) << ").Length\n";
    ps << "Step 'Compacting'\n";
    ps << "try { [BambuVirtDisk]::Compact(" << ps_quote(path) << ", $" << (mode == CompactMode::Quick ? "true" : "false") << ") }\n";
    if (attached) {
        ps << "finally {\n  Step 'Putting it back read-write'\n  Dismount-DiskImage -ImagePath " << ps_quote(path) << " | Out-Null\n"
           << "  Mount-DiskImage -ImagePath " << ps_quote(path) << " | Out-Null\n}\n";
    } else {
        ps << "finally { }\n";
    }
    ps << "$after = (Get-Item -LiteralPath " << ps_quote(path) << ").Length\n";
    ps << "$Extra = @{ before = $before; after = $after }\n";
    s.powershell = ps.str();
    std::ostringstream dp;
    dp << "select vdisk file=" << diskpart_quote(path) << "\n";
    if (attached) dp << "detach vdisk\n";
    dp << "attach vdisk readonly\ncompact vdisk\ndetach vdisk\n";
    if (attached) dp << "attach vdisk\n";
    s.diskpart = dp.str();
    return s;
}

Script child_script(const std::string &parent, const std::string &child)
{
    CreateRequest r;
    r.path   = child;
    r.parent = parent;
    r.kind   = Kind::Differencing;
    r.format = format_from_extension(parent) == Format::Vhd ? Format::Vhd : Format::Vhdx;
    r.attach = false;
    Script s  = create_script(r);
    s.summary = "Make child " + leaf_name(child) + " of " + leaf_name(parent);
    return s;
}

Script merge_script(const std::string &child, const std::string &parent, const std::string &backup)
{
    Script s;
    s.route   = Route::VirtDisk;
    s.summary = "Merge " + leaf_name(child) + " into " + leaf_name(parent);
    std::ostringstream ps;
    ps << virtdisk_block();
    if (!backup.empty()) {
        // A verified copy of the parent before it changes for good.
        ps << "Step 'Copying " << leaf_name(parent) << " first'\n";
        ps << "Copy-Item -LiteralPath " << ps_quote(parent) << " -Destination " << ps_quote(backup) << "\n";
        ps << "Step 'Checking the copy'\n";
        ps << "if ((Sha256 " << ps_quote(parent) << ") -ne (Sha256 " << ps_quote(backup) << ")) { throw 'The copy of the parent does not match it, so nothing was merged.' }\n";
    }
    ps << "Step 'Merging'\n";
    ps << "[BambuVirtDisk]::Merge(" << ps_quote(child) << ")\n";
    s.powershell = ps.str();
    s.diskpart   = "select vdisk file=" + diskpart_quote(child) + "\nmerge vdisk depth=1\n";
    return s;
}

Script reconnect_parent_script(const std::string &child, const std::string &new_parent)
{
    Script s;
    s.route      = Route::VirtDisk;
    s.summary    = "Reconnect " + leaf_name(child) + " to " + leaf_name(new_parent);
    s.powershell = virtdisk_block() + "Step 'Pointing the child at its parent'\n[BambuVirtDisk]::SetParentPath(" + ps_quote(child) + ", " +
                   ps_quote(new_parent) + ")\n";
    return s; // DiskPart cannot do this
}

Script new_disk_id_script(const std::string &path, const std::string &partition_style)
{
    Script s;
    s.route   = Route::Storage;
    s.summary = "New disk ID for " + leaf_name(path);
    std::ostringstream ps;
    ps << "Step 'Attaching without a drive letter'\n";
    ps << "$disk = Mount-DiskImage -ImagePath " << ps_quote(path) << " -NoDriveLetter -PassThru | Get-Disk\n";
    ps << "try {\n";
    ps << "  if ($disk.IsOffline) { Set-Disk -Number $disk.Number -IsOffline $false }\n";
    if (partition_style == "MBR") {
        ps << "  Step 'Writing a new MBR signature'\n";
        ps << "  $signature = Get-Random -Minimum 1 -Maximum 2147483647\n  Set-Disk -Number $disk.Number -Signature $signature\n";
        ps << "  $Extra = @{ id = ('{0:X8}' -f $signature) }\n";
    } else {
        ps << "  Step 'Writing a new GPT disk GUID'\n";
        ps << "  $guid = [guid]::NewGuid().ToString('B')\n  Set-Disk -Number $disk.Number -Guid $guid\n";
        ps << "  $Extra = @{ id = $guid.ToUpper() }\n";
    }
    ps << "} finally {\n  Step 'Detaching'\n  Dismount-DiskImage -ImagePath " << ps_quote(path) << " | Out-Null\n}\n";
    s.powershell = ps.str();
    s.diskpart   = "select vdisk file=" + diskpart_quote(path) + "\nattach vdisk\nrem select disk <number>\nuniqueid disk id=" +
                 (partition_style == "MBR" ? std::string("<8 hex digits>") : std::string("<new GUID>")) + "\ndetach vdisk\n";
    return s;
}

std::uint64_t pack_size(std::uint64_t source_bytes, std::uint64_t source_files)
{
    // NTFS needs room for its own records (about 4 KB per file and folder),
    // cluster slack (half a 4 KB cluster per file on average), the log and the
    // partition table: plus 2 % and 64 MB, then whole megabytes.
    const std::uint64_t overhead = source_files * (4096 + 2048) + source_bytes / 50 + 64 * kMB;
    const std::uint64_t total    = std::max<std::uint64_t>(source_bytes + overhead, 64 * kMB);
    return ((total + kMB - 1) / kMB) * kMB;
}

std::string unique_target(const std::string &folder, const std::string &name, const std::vector<std::string> &taken)
{
    const auto exists = [&taken](const std::string &candidate) {
        const std::string l = lowered(candidate);
        return std::any_of(taken.begin(), taken.end(), [&l](const std::string &t) { return lowered(t) == l; });
    };
    const std::string sep  = folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\";
    std::string       path = folder + sep + name + ".vhdx";
    for (int n = 1; exists(path) && n < 1000; ++n) path = folder + sep + name + " (" + std::to_string(n) + ").vhdx";
    return path;
}

Script pack_script(const PackRequest &r)
{
    Script s;
    s.route   = Route::Robocopy;
    s.summary = "Quick pack " + leaf_name(r.source) + " into " + leaf_name(r.target);
    const std::uint64_t size = pack_size(r.source_bytes, r.source_files);
    std::ostringstream  ps;
    ps << virtdisk_block();
    ps << "$source = " << ps_quote(r.source) << "\n$target = " << ps_quote(r.target) << "\n";
    ps << "$log = [IO.Path]::Combine([IO.Path]::GetTempPath(), 'bambu-pack-' + [guid]::NewGuid().ToString('N') + '.log')\n";
    ps << "$done = $false\n";
    ps << "try {\n";
    ps << "  Step 'Creating " << leaf_name(r.target) << " (" << size_text(size) << ")'\n";
    ps << "  [BambuVirtDisk]::Create($target, 3, " << size << ", 0, 512, 4096, '', $false)\n";
    ps << "  $disk = Mount-DiskImage -ImagePath $target -NoDriveLetter -PassThru | Get-Disk\n";
    ps << "  Initialize-Disk -Number $disk.Number -PartitionStyle GPT\n";
    ps << "  $part = New-Partition -DiskNumber $disk.Number -UseMaximumSize -AssignDriveLetter\n";
    ps << "  Format-Volume -Partition $part -FileSystem NTFS -NewFileSystemLabel " << ps_quote(r.label.substr(0, 32)) << " -Confirm:$false | Out-Null\n";
    ps << "  $letter = (Get-Partition -DiskNumber $disk.Number -PartitionNumber $part.PartitionNumber).DriveLetter\n";
    ps << "  $dest = \"$($letter):\\\" + (Split-Path -Leaf $source)\n";
    ps << "  $grew = @()\n";
    ps << "  for ($pass = 1; $pass -le 3; $pass++) {\n";
    ps << "    Cancelled\n";
    ps << "    Step \"Copying, pass $pass\"\n";
    // The tree, times, attributes and alternate data streams (web marks included); junctions are not followed.
    ps << "    $p = Start-Process -FilePath robocopy.exe -ArgumentList @(\"`\"$source`\"\", \"`\"$dest`\"\", '/E', '/COPY:DAT', '/DCOPY:DAT', '/XJ', '/R:1', '/W:1', '/NP', '/BYTES', \"/UNILOG:`\"$log`\"\") -NoNewWindow -PassThru\n";
    ps << "    while (-not $p.HasExited) { Start-Sleep -Milliseconds 400; if (Test-Path -LiteralPath $CancelPath) { $p.Kill(); throw 'Cancelled' }\n";
    ps << "      $copied = (Select-String -LiteralPath $log -Pattern '^\\s+(New File|Newer|Older|Changed)' -ErrorAction SilentlyContinue | Measure-Object).Count; Step \"Copied $copied of "
       << r.source_files << " files\" }\n";
    ps << "    if ($p.ExitCode -lt 8) { break }\n";
    // Out of room: a file grew while it was copied. Grow the disk and copy again; robocopy skips what is already there.
    ps << "    if (-not (Select-String -LiteralPath $log -Pattern 'not enough space' -Quiet)) { throw \"Robocopy stopped with code $($p.ExitCode); see $log\" }\n";
    ps << "    $old = (Get-DiskImage -ImagePath $target).Size; $new = [uint64]([math]::Ceiling($old * 1.25 / 1MB) * 1MB)\n";
    ps << "    Step \"The files grew while they were copied: growing the disk to $([math]::Round($new / 1GB, 1)) GB\"\n";
    ps << "    [BambuVirtDisk]::Resize($target, $new); Update-Disk -Number $disk.Number\n";
    ps << "    Resize-Partition -DiskNumber $disk.Number -PartitionNumber $part.PartitionNumber -Size (Get-PartitionSupportedSize -DiskNumber $disk.Number -PartitionNumber $part.PartitionNumber).SizeMax\n";
    ps << "    $grew += \"from $([math]::Round($old / 1GB, 1)) GB to $([math]::Round($new / 1GB, 1)) GB\"\n";
    ps << "  }\n";
    ps << "  Step 'Checking every file by SHA-256'\n";
    ps << "  $files = Get-ChildItem -LiteralPath $source -Recurse -File -Force -Attributes !ReparsePoint\n";
    ps << "  $checked = 0; $bad = @()\n";
    ps << "  foreach ($f in $files) {\n";
    ps << "    Cancelled\n";
    ps << "    $rel = $f.FullName.Substring($source.TrimEnd('\\').Length)\n";
    ps << "    $copy = $dest + $rel\n";
    ps << "    if (-not (Test-Path -LiteralPath $copy) -or (Sha256 $f.FullName) -ne (Sha256 $copy)) { $bad += $rel }\n";
    ps << "    $checked++; if ($checked % 25 -eq 0) { Step \"Checked $checked of $($files.Count) files\" }\n";
    ps << "  }\n";
    ps << "  if ($bad.Count -gt 0) { throw (\"$($bad.Count) files do not match their source: \" + ($bad | Select-Object -First 5) -join ', ') }\n";
    ps << "  $folders = (Get-ChildItem -LiteralPath $source -Recurse -Directory -Force | Measure-Object).Count\n";
    ps << "  $Extra = @{ target = $target; files = $checked; folders = $folders; grew = $grew; letter = \"$letter\"; size = (Get-DiskImage -ImagePath $target).Size; on_drive = (Get-Item -LiteralPath $target).Length }\n";
    if (!r.keep_attached) ps << "  Step 'Detaching'\n  Dismount-DiskImage -ImagePath $target | Out-Null\n";
    ps << "  $done = $true\n";
    ps << "} finally {\n";
    // Cancel or failure: the new disk goes, the folder is never touched.
    ps << "  if (-not $done -and (Test-Path -LiteralPath $target)) { Dismount-DiskImage -ImagePath $target -ErrorAction SilentlyContinue | Out-Null; Remove-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue }\n";
    ps << "  Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue\n";
    ps << "}\n";
    s.powershell = ps.str();
    return s; // DiskPart cannot copy files
}

std::string clean_script(const Script &s)
{
    if (s.powershell.empty()) return std::string();
    std::ostringstream ps;
    ps << "# " << s.summary << "\n# Run in Windows PowerShell as administrator.\n";
    ps << "$ErrorActionPreference = 'Stop'\n$CancelPath = ''\n";
    ps << "function Step([string]$text) { Write-Host $text }\nfunction Cancelled { }\n";
    ps << "function Sha256([string]$path) { $s = [IO.File]::OpenRead($path); try { [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($s)) } finally { $s.Dispose() } }\n";
    ps << s.powershell;
    return ps.str();
}

std::string runner_script(const Script &s, const std::string &result_path, const std::string &progress_path, const std::string &cancel_path)
{
    std::ostringstream ps;
    ps << "$ErrorActionPreference = 'Stop'\n";
    // Windows PowerShell started from PowerShell 7 inherits its module path; take the machine's own.
    ps << "$env:PSModulePath = [Environment]::GetEnvironmentVariable('PSModulePath', 'Machine')\n";
    ps << "function Sha256([string]$path) { if ($path.Length -gt 250 -and -not $path.StartsWith('\\\\?\\')) { $path = '\\\\?\\' + $path }\n";
    ps << "  $s = [IO.File]::OpenRead($path); try { [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($s)) } finally { $s.Dispose() } }\n";
    ps << "$ResultPath = " << ps_quote(result_path) << "\n$ProgressPath = " << ps_quote(progress_path) << "\n$CancelPath = " << ps_quote(cancel_path) << "\n";
    ps << "$Extra = $null\n";
    ps << "function Step([string]$text) { Add-Content -LiteralPath $ProgressPath -Value $text -Encoding UTF8 }\n";
    ps << "function Cancelled { if (Test-Path -LiteralPath $CancelPath) { throw 'Cancelled' } }\n";
    ps << "function Finish([bool]$ok, [string]$message) {\n";
    ps << "  $json = @{ ok = $ok; message = $message; extra = $Extra } | ConvertTo-Json -Compress -Depth 5\n";
    ps << "  [IO.File]::WriteAllText($ResultPath, $json, (New-Object System.Text.UTF8Encoding($false)))\n}\n";
    ps << "try {\n" << s.powershell << "Finish $true 'Done'\n} catch {\n";
    ps << "  $message = $_.Exception.Message\n";
    ps << "  if ($message -eq 'Cancelled') { Finish $false 'Cancelled' } else { Finish $false $message }\n  exit 1\n}\n";
    return ps.str();
}

void Library::add_disk(const std::string &path)
{
    const std::string l = lowered(path);
    if (std::none_of(disks.begin(), disks.end(), [&l](const std::string &d) { return lowered(d) == l; })) disks.push_back(path);
}

void Library::remove_disk(const std::string &path)
{
    const std::string l = lowered(path);
    disks.erase(std::remove_if(disks.begin(), disks.end(), [&l](const std::string &d) { return lowered(d) == l; }), disks.end());
}

void Library::record(HistoryEntry entry)
{
    history.insert(history.begin(), std::move(entry));
    if (history.size() > 200) history.resize(200);
}

std::string to_json(const Library &library)
{
    nlohmann::json j;
    j["version"] = 1;
    j["disks"]   = library.disks;
    j["folders"] = library.folders;
    j["history"] = nlohmann::json::array();
    for (const HistoryEntry &h : library.history)
        j["history"].push_back({{"time", h.time}, {"operation", h.operation}, {"disk", h.disk}, {"route", h.route}, {"result", h.result}, {"detail", h.detail}});
    return j.dump(1);
}

bool from_json(const std::string &json, Library &library)
{
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    Library out;
    for (const auto &d : j.value("disks", nlohmann::json::array()))
        if (d.is_string()) out.add_disk(d.get<std::string>());
    for (const auto &f : j.value("folders", nlohmann::json::array()))
        if (f.is_string()) out.folders.push_back(f.get<std::string>());
    for (const auto &h : j.value("history", nlohmann::json::array())) {
        if (!h.is_object()) continue;
        HistoryEntry e;
        e.time      = h.value("time", std::int64_t(0));
        e.operation = h.value("operation", std::string());
        e.disk      = h.value("disk", std::string());
        e.route     = h.value("route", std::string());
        e.result    = h.value("result", std::string());
        e.detail    = h.value("detail", std::string());
        if (!e.operation.empty()) out.history.push_back(e);
    }
    if (out.history.size() > 200) out.history.resize(200);
    library = std::move(out);
    return true;
}

}}} // namespace Slic3r::GUI::VirtualDisks
