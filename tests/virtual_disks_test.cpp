// Standalone assertions for src/slic3r/GUI/VirtualDisks/DiskImage.cpp and
// DiskPlan.cpp: VHD (fixed and differencing), VHDX (current header chosen by
// sequence, metadata, parent locator) and ISO images built byte by byte and
// read back, the partition table identity inside them, chains and their
// tests, clashing disk IDs, the limits a new disk is checked against, every
// operation's PowerShell and DiskPart steps, and the library file. Compiled
// and run by tests/virtual_disks.test.mjs.
#include "src/slic3r/GUI/VirtualDisks/DiskImage.cpp"
#include "src/slic3r/GUI/VirtualDisks/DiskPlan.cpp"

#include <cstdio>
#include <cstring>

using namespace Slic3r::GUI::VirtualDisks;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

using Bytes = std::vector<std::uint8_t>;

static void put_le(Bytes &b, std::size_t at, std::uint64_t v, int n) { for (int i = 0; i < n; ++i) b[at + i] = std::uint8_t(v >> (8 * i)); }
static void put_be(Bytes &b, std::size_t at, std::uint64_t v, int n) { for (int i = 0; i < n; ++i) b[at + i] = std::uint8_t(v >> (8 * (n - 1 - i))); }
static void put_text(Bytes &b, std::size_t at, const char *t) { std::memcpy(b.data() + at, t, std::strlen(t)); }
static void put_guid(Bytes &b, std::size_t at, const char *text)
{
    unsigned a = 0, c = 0, d = 0, e[8] = {};
    std::sscanf(text, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &a, &c, &d, &e[0], &e[1], &e[2], &e[3], &e[4], &e[5], &e[6], &e[7]);
    put_le(b, at, a, 4), put_le(b, at + 4, c, 2), put_le(b, at + 6, d, 2);
    for (int i = 0; i < 8; ++i) b[at + 8 + i] = std::uint8_t(e[i]);
}
static void put_utf16(Bytes &b, std::size_t at, const std::string &s, bool big)
{
    for (std::size_t i = 0; i < s.size(); ++i) big ? put_be(b, at + 2 * i, std::uint8_t(s[i]), 2) : put_le(b, at + 2 * i, std::uint8_t(s[i]), 2);
}

// A sector 0 MBR with one NTFS partition and a signature.
static void put_mbr(Bytes &b, std::size_t at, std::uint32_t signature, std::uint8_t type = 0x07)
{
    put_le(b, at + 440, signature, 4);
    b[at + 446 + 4] = type;
    b[at + 510] = 0x55, b[at + 511] = 0xAA;
}

static void vhd_footer(Bytes &f, std::uint32_t type, std::uint64_t size, std::uint64_t data_offset, const char *unique)
{
    f.assign(512, 0);
    put_text(f, 0, "conectix");
    put_be(f, 8, 2, 4);
    put_be(f, 12, 0x00010000, 4);
    put_be(f, 16, data_offset, 8);
    put_text(f, 28, "test");
    put_text(f, 36, "Wi2k");
    put_be(f, 40, size, 8);
    put_be(f, 48, size, 8);
    put_be(f, 60, type, 4);
    put_guid(f, 68, unique);
    std::uint32_t sum = 0;
    for (std::uint8_t v : f) sum += v;
    put_be(f, 64, ~sum, 4);
}

static Bytes fixed_vhd(std::uint32_t signature, const char *unique)
{
    Bytes img(1024 * 1024, 0);
    put_mbr(img, 0, signature);
    Bytes f;
    vhd_footer(f, 2, img.size(), ~0ull, unique);
    img.insert(img.end(), f.begin(), f.end());
    return img;
}

// A differencing VHD whose block 0 is either absent or holds an MBR.
static Bytes child_vhd(const char *parent_unique, const std::string &relative, bool block0, std::uint32_t signature)
{
    Bytes img(8192, 0);
    Bytes f;
    vhd_footer(f, 4, 1024 * 1024, 512, "11111111-2222-3333-4444-555555555555");
    std::memcpy(img.data(), f.data(), 512);
    std::size_t h = 512;
    put_text(img, h, "cxsparse");
    put_be(img, h + 8, ~0ull, 8);
    put_be(img, h + 16, 1536, 8);       // BAT
    put_be(img, h + 24, 0x00010000, 4);
    put_be(img, h + 28, 1, 4);
    put_be(img, h + 32, 2 * 1024 * 1024, 4);
    put_guid(img, h + 40, parent_unique);
    put_utf16(img, h + 64, "base.vhd", true);
    put_be(img, h + 576, 0x57327275, 4); // W2ru
    put_be(img, h + 576 + 4, 512, 4);
    put_be(img, h + 576 + 8, std::uint32_t(relative.size() * 2), 4);
    put_be(img, h + 576 + 16, 2048, 8);
    put_utf16(img, 2048, relative, false);
    put_be(img, 1536, block0 ? 8 : 0xFFFFFFFFu, 4); // block 0 at sector 8
    if (block0) {
        img.resize(8 * 512 + 512 + 2 * 1024 * 1024, 0);
        put_mbr(img, 8 * 512 + 512, signature); // after the one-sector bitmap
    }
    img.insert(img.end(), f.begin(), f.end());
    return img;
}

static void seal(Bytes &b, std::size_t at, std::size_t size)
{
    put_le(b, at + 4, 0, 4);
    put_le(b, at + 4, crc32c(b.data() + at, size), 4);
}

// A VHDX: header 2 is newer than header 1 and names a different data write GUID.
static Bytes vhdx(bool child, const char *linkage, bool damaged = false)
{
    const std::size_t MB = 1024 * 1024;
    Bytes             img(4 * MB + 1024, 0);
    put_text(img, 0, "vhdxfile");
    for (int n = 0; n < 2; ++n) {
        const std::size_t at = (n == 0 ? 64 : 128) * 1024;
        put_text(img, at, "head");
        put_le(img, at + 8, std::uint64_t(n + 1), 8);
        put_guid(img, at + 32, n == 0 ? "AAAAAAAA-0000-0000-0000-000000000001" : "BBBBBBBB-0000-0000-0000-000000000002");
        put_le(img, at + 66, 1, 2);
        seal(img, at, 4096);
        if (damaged) img[at + 100] ^= 0xFF;
    }
    const std::size_t rt = 192 * 1024;
    put_text(img, rt, "regi");
    put_le(img, rt + 8, 2, 4);
    put_guid(img, rt + 16, "2DC27766-F623-4200-9D64-115E9BFD4A08");
    put_le(img, rt + 32, 3 * MB, 8);
    put_le(img, rt + 40, MB, 4);
    put_guid(img, rt + 48, "8B7CA206-4790-4B9A-B8FE-575F050F886E");
    put_le(img, rt + 64, 2 * MB, 8);
    put_le(img, rt + 72, MB, 4);
    seal(img, rt, 64 * 1024);

    const std::size_t md = 2 * MB;
    put_text(img, md, "metadata");
    struct Item { const char *guid; std::uint32_t offset, length; };
    std::vector<Item> items = {{"CAA16737-FA36-4D43-B3B6-33F0AA44E76B", 65536, 8},  {"2FA54224-CD1B-4876-B211-5DBED83BF4B8", 65544, 8},
                               {"BECA12AB-B2E6-4523-93EF-C309E000C746", 65552, 16}, {"8141BF1D-A96F-4709-BA47-F233A8FAAB5F", 65568, 4},
                               {"CDA348C7-445D-4471-9CC9-E9885251C556", 65572, 4}};
    if (child) items.push_back({"A8D35F2D-B30B-454D-ABF7-D3D84834AB0C", 65600, 400});
    put_le(img, md + 10, items.size(), 2);
    for (std::size_t i = 0; i < items.size(); ++i) {
        put_guid(img, md + 32 + i * 32, items[i].guid);
        put_le(img, md + 32 + i * 32 + 16, items[i].offset, 4);
        put_le(img, md + 32 + i * 32 + 20, items[i].length, 4);
    }
    put_le(img, md + 65536, 32 * MB, 4);
    put_le(img, md + 65540, child ? 2 : 0, 4);
    put_le(img, md + 65544, 4ull * 1024 * MB, 8);
    put_guid(img, md + 65552, "3F90AAAA-1111-2222-3333-0B1D00000000");
    put_le(img, md + 65568, 512, 4);
    put_le(img, md + 65572, 4096, 4);
    if (child) {
        const std::size_t pl = md + 65600;
        put_guid(img, pl, "B04AEFB7-D19E-4A81-B789-25B8E9445913");
        put_le(img, pl + 18, 2, 2);
        const std::string k1 = "parent_linkage", v1 = linkage, k2 = "relative_path", v2 = ".\\Base.vhdx";
        std::size_t       at = 44;
        auto              kv = [&](int n, const std::string &k, const std::string &v) {
            put_le(img, pl + 20 + n * 12, at, 4);
            put_utf16(img, pl + at, k, false);
            at += k.size() * 2;
            put_le(img, pl + 20 + n * 12 + 4, at, 4);
            put_utf16(img, pl + at, v, false);
            at += v.size() * 2;
            put_le(img, pl + 20 + n * 12 + 8, k.size() * 2, 2);
            put_le(img, pl + 20 + n * 12 + 10, v.size() * 2, 2);
        };
        kv(0, k1, v1);
        kv(1, k2, v2);
    }
    // Block 0 of a base disk holds a GPT; a child's is absent.
    put_le(img, 3 * MB, child ? 0 : ((4ull << 20) | 6), 8);
    if (!child) {
        img[4 * MB + 450] = 0xEE;
        img[4 * MB + 510] = 0x55, img[4 * MB + 511] = 0xAA;
        put_text(img, 4 * MB + 512, "EFI PART");
        put_guid(img, 4 * MB + 512 + 56, "7D1C0000-1111-2222-3333-44445555A42E");
    }
    return img;
}

static Bytes iso(const char *label)
{
    Bytes img(17 * 2048, 0);
    const std::size_t pvd = 16 * 2048;
    img[pvd] = 1;
    put_text(img, pvd + 1, "CD001");
    img[pvd + 6] = 1;
    std::memset(img.data() + pvd + 40, ' ', 32);
    put_text(img, pvd + 40, label);
    put_le(img, pvd + 80, 3000, 4);
    put_le(img, pvd + 128, 2048, 2);
    return img;
}

int main()
{
    // CRC-32C check value.
    CHECK(crc32c(reinterpret_cast<const std::uint8_t *>("123456789"), 9) == 0xE3069283u);

    // A fixed VHD: footer at the end, the MBR inside.
    const DiskInfo fixed = read_disk_bytes("D:\\Old\\Legacy.vhd", fixed_vhd(0x1A2B3C4D, "01020304-0506-0708-090A-0B0C0D0E0F10"));
    CHECK(fixed.readable && fixed.format == Format::Vhd && fixed.kind == Kind::Fixed);
    CHECK(fixed.virtual_size == 1024 * 1024 && fixed.partition_style == "MBR" && fixed.partition_disk_id == "1A2B3C4D");
    CHECK(fixed.virtual_disk_id == "{01020304-0506-0708-090A-0B0C0D0E0F10}");
    Bytes broken = fixed_vhd(1, "01020304-0506-0708-090A-0B0C0D0E0F10");
    broken[broken.size() - 512 + 50] ^= 1;
    CHECK(!read_disk_bytes("x.vhd", broken).readable && !read_disk_bytes("x.vhd", broken).problem.empty());

    // A differencing VHD names its parent by unique id, name and relative path.
    const DiskInfo vchild = read_disk_bytes("D:\\Old\\Child.vhd", child_vhd("01020304-0506-0708-090A-0B0C0D0E0F10", ".\\base.vhd", false, 0));
    CHECK(vchild.readable && vchild.kind == Kind::Differencing && vchild.block_size == 2 * 1024 * 1024);
    CHECK(vchild.parent_name == "base.vhd" && vchild.parent_relative == ".\\base.vhd" && vchild.partition_style.empty());
    CHECK(vchild.parent_linkage == "{01020304-0506-0708-090A-0B0C0D0E0F10}");

    // A VHDX: the newer header wins; metadata and the GPT inside are read.
    const DiskInfo base = read_disk_bytes("D:\\Disks\\Chain\\Base.vhdx", vhdx(false, ""));
    CHECK(base.readable && base.format == Format::Vhdx && base.kind == Kind::Dynamic);
    CHECK(base.block_size == 32u * 1024 * 1024 && base.virtual_size == 4ull * 1024 * 1024 * 1024);
    CHECK(base.logical_sector == 512 && base.physical_sector == 4096);
    CHECK(base.data_write_guid == "{BBBBBBBB-0000-0000-0000-000000000002}");
    CHECK(base.virtual_disk_id == "{3F90AAAA-1111-2222-3333-0B1D00000000}");
    CHECK(base.partition_style == "GPT" && base.partition_disk_id == "{7D1C0000-1111-2222-3333-44445555A42E}");
    const DiskInfo damaged = read_disk_bytes("D:\\Disks\\Bad.vhdx", vhdx(false, "", true));
    CHECK(!damaged.readable && damaged.format == Format::Vhdx && damaged.problem.find("checksum") != std::string::npos);

    // A VHDX child points at its parent by data write GUID and relative path.
    const DiskInfo scratch = read_disk_bytes("D:\\Disks\\Chain\\Scratch.avhdx", vhdx(true, "{BBBBBBBB-0000-0000-0000-000000000002}"));
    CHECK(scratch.readable && scratch.kind == Kind::Differencing && scratch.parent_relative == ".\\Base.vhdx");
    CHECK(scratch.partition_style.empty());

    // An ISO's label and size.
    const DiskInfo image = read_disk_bytes("D:\\Images\\Win.iso", iso("CCCOMA_X64FRE"));
    CHECK(image.readable && image.format == Format::Iso && image.kind == Kind::ReadOnly && image.label == "CCCOMA_X64FRE");
    CHECK(image.virtual_size == 3000ull * 2048);
    CHECK(!read_disk_bytes("D:\\x.txt", Bytes(4096, 0)).readable && format_from_extension("a.AVHDX") == Format::Vhdx);

    // Chains: found, intact, changed, missing.
    std::vector<DiskInfo> disks = {base, scratch, fixed, vchild, image};
    CHECK(find_parent(disks, 1) == 0 && find_parent(disks, 3) == -1); // Child.vhd wants D:\Old\base.vhd, not Legacy.vhd
    CHECK(test_chain(disks, 1).result == ChainResult::Passed && test_chain(disks, 0).result == ChainResult::Passed);
    CHECK(test_chain(disks, 3).result == ChainResult::ParentMissing && test_chain(disks, 4).result == ChainResult::NotAChain);
    CHECK(children_of(disks, 0) == std::vector<std::size_t>{1});
    disks[3].path = "D:\\Old\\base.vhd.child.vhd";
    DiskInfo renamed_parent = fixed;
    renamed_parent.path     = "D:\\Old\\base.vhd";
    disks.push_back(renamed_parent);
    CHECK(test_chain(disks, 3).result == ChainResult::Passed);
    std::vector<DiskInfo> stale = {base, read_disk_bytes("D:\\Disks\\Chain\\Scratch.avhdx", vhdx(true, "{AAAAAAAA-0000-0000-0000-000000000001}"))};
    CHECK(test_chain(stale, 1).result == ChainResult::ParentChanged);

    // Two disks with one partition table identity clash; a parent and its child do not.
    std::vector<DiskInfo> twins = {fixed, read_disk_bytes("D:\\Disks\\Copy.vhd", fixed_vhd(0x1A2B3C4D, "99999999-0506-0708-090A-0B0C0D0E0F10"))};
    CHECK(same_disk_ids(twins).size() == 1);
    DiskInfo own_parent = fixed;
    own_parent.path     = "D:\\Old\\base.vhd";
    std::vector<DiskInfo> family = {own_parent, read_disk_bytes("D:\\Old\\Kid.vhd", child_vhd("01020304-0506-0708-090A-0B0C0D0E0F10", ".\\base.vhd", true, 0x1A2B3C4D))};
    CHECK(family[1].partition_disk_id == "1A2B3C4D" && find_parent(family, 1) == 0);
    CHECK(same_disk_ids(family).empty());

    // The limits a new disk is checked against.
    CreateRequest r;
    r.path       = "D:\\Disks\\Prints.vhdx";
    r.size       = 256 * kGB;
    r.block_size = 32 * 1024 * 1024;
    CHECK(validate(r, 600 * kGB, false).ok());
    r.size = 65 * kTB;
    CHECK(!validate(r, 600 * kGB, false).ok());
    r.size = 64 * kTB;
    CHECK(validate(r, 600 * kGB, false).ok());
    r.block_size = 3 * 1024 * 1024;
    CHECK(!validate(r, 600 * kGB, false).ok());
    r.block_size = 0, r.size = 256 * kGB;
    CHECK(!validate(r, 600 * kGB, true).ok()); // never overwrites
    r.kind = Kind::Fixed;
    CHECK(!validate(r, 100 * kGB, false).ok() && validate(r, 300 * kGB, false).ok());
    r.kind = Kind::Dynamic, r.file_system = "FAT32";
    CHECK(!validate(r, 600 * kGB, false).ok());
    r.size = 16 * kGB, r.label = "TWELVE_CHARS";
    CHECK(!validate(r, 600 * kGB, false).ok());
    r.file_system = "NTFS", r.partition_style = "MBR", r.size = 3 * kTB;
    CHECK(!validate(r, 6 * kTB, false).ok());
    CreateRequest v;
    v.path = "D:\\Old\\Big.vhd", v.format = Format::Vhd, v.size = 2041 * kGB, v.physical_sector = 512;
    CHECK(!validate(v, 6 * kTB, false).ok());
    v.size = 2040 * kGB;
    CHECK(validate(v, 6 * kTB, false).ok());
    v.path = "D:\\Old\\Big.vhdx";
    CHECK(!validate(v, 6 * kTB, false).ok()); // a VHD ends in .vhd
    CreateRequest kid;
    kid.path = "D:\\Disks\\Chain\\New.avhdx", kid.kind = Kind::Differencing, kid.parent = "D:\\Disks\\Chain\\Base.vhdx";
    CHECK(validate(kid, kGB, false, &base).ok() && !validate(kid, kGB, false, &fixed).ok() && !validate(kid, kGB, false, nullptr).ok());

    // The steps, as PowerShell and as DiskPart.
    r = CreateRequest{};
    r.path = "D:\\Disks\\Prints.vhdx", r.size = 256 * kGB, r.block_size = 32 * 1024 * 1024, r.label = "Prints", r.drive_letter = 'P';
    const Script create = create_script(r);
    CHECK(create.powershell.find("[BambuVirtDisk]::Create('D:\\Disks\\Prints.vhdx', 3, 274877906944, 33554432, 512, 4096, '', $false)") != std::string::npos);
    CHECK(create.powershell.find("Initialize-Disk -Number $disk.Number -PartitionStyle GPT") != std::string::npos);
    CHECK(create.powershell.find("-DriveLetter P") != std::string::npos && create.powershell.find("-NewFileSystemLabel 'Prints'") != std::string::npos);
    CHECK(create.diskpart.find("create vdisk file=\"D:\\Disks\\Prints.vhdx\" maximum=262144 type=expandable") != std::string::npos);
    CHECK(create.diskpart.find("convert gpt") != std::string::npos && create.diskpart.find("assign letter=P") != std::string::npos);
    CHECK(ps_quote("D:\\Bob's\\x.vhdx") == "'D:\\Bob''s\\x.vhdx'");
    CHECK(attach_script("D:\\a.iso", true).powershell.find("Mount-DiskImage -ImagePath 'D:\\a.iso' -Access ReadOnly") != std::string::npos);
    CHECK(detach_script("D:\\a.vhdx").diskpart == "select vdisk file=\"D:\\a.vhdx\"\ndetach vdisk\n");
    const Script grow = grow_script("D:\\a.vhdx", 384 * kGB, true);
    CHECK(grow.powershell.find("[BambuVirtDisk]::Resize('D:\\a.vhdx', 412316860416)") != std::string::npos && grow.powershell.find("Resize-Partition") != std::string::npos);
    CHECK(grow_script("D:\\a.vhdx", 384 * kGB, false).powershell.find("Resize-Partition") == std::string::npos);
    CHECK(compact_script("D:\\a.vhdx", CompactMode::Full, true).powershell.find("-Access ReadOnly") != std::string::npos);
    CHECK(compact_script("D:\\a.vhdx", CompactMode::Quick, false).powershell.find("[BambuVirtDisk]::Compact('D:\\a.vhdx', $true)") != std::string::npos);
    const Script merge = merge_script("D:\\c.avhdx", "D:\\b.vhdx", "D:\\b (copy).vhdx");
    CHECK(merge.powershell.find("Get-FileHash") < merge.powershell.find("[BambuVirtDisk]::Merge('D:\\c.avhdx')"));
    CHECK(merge_script("D:\\c.avhdx", "D:\\b.vhdx", "").powershell.find("Copy-Item") == std::string::npos);
    CHECK(child_script("D:\\b.vhdx", "D:\\c.avhdx").powershell.find("[BambuVirtDisk]::Create('D:\\c.avhdx', 3, 0, 0, 0, 0, 'D:\\b.vhdx', $false)") != std::string::npos);
    CHECK(reconnect_parent_script("D:\\c.avhdx", "E:\\b.vhdx").diskpart.empty());
    CHECK(new_disk_id_script("D:\\l.vhd", "MBR").powershell.find("Set-Disk -Number $disk.Number -Signature") != std::string::npos);
    CHECK(new_disk_id_script("D:\\l.vhdx", "GPT").powershell.find("-Guid $guid") != std::string::npos);

    // Quick pack: sized to fit, never over a file that exists, cancel removes only the new disk.
    CHECK(pack_size(0, 0) == 64 * kMB && pack_size(7 * kGB, 1000) % kMB == 0 && pack_size(7 * kGB, 1000) > 7 * kGB);
    CHECK(unique_target("D:\\Archive", "Photos", {"D:\\Archive\\photos.vhdx"}) == "D:\\Archive\\Photos (1).vhdx");
    PackRequest pack;
    pack.source = "D:\\Archive\\Photos", pack.target = "D:\\Archive\\Photos (1).vhdx", pack.source_bytes = 17 * kGB, pack.source_files = 3310, pack.label = "Photos";
    const Script packed = pack_script(pack);
    CHECK(packed.powershell.find("/COPY:DAT") != std::string::npos && packed.powershell.find("SHA256") != std::string::npos);
    CHECK(packed.powershell.find("Remove-Item -LiteralPath $target") != std::string::npos && packed.powershell.find("Remove-Item -LiteralPath $source") == std::string::npos);
    CHECK(packed.powershell.find("Dismount-DiskImage -ImagePath $target | Out-Null") != std::string::npos);
    const std::string run = runner_script(packed, "C:\\t\\r.json", "C:\\t\\p.txt", "C:\\t\\c");
    CHECK(run.find("$ResultPath = 'C:\\t\\r.json'") != std::string::npos && run.find("Finish $true 'Done'") != std::string::npos);
    CHECK(run.find("Add-Type -TypeDefinition @'") != std::string::npos && run.find("\n'@\n") != std::string::npos);

    // The library file.
    Library lib;
    lib.add_disk("D:\\Disks\\Projects.vhdx");
    lib.add_disk("d:\\disks\\projects.VHDX");
    lib.folders.push_back("D:\\Disks");
    for (int i = 0; i < 205; ++i) lib.record({i, "Attach", "Projects.vhdx", "Storage module", "Done", ""});
    CHECK(lib.disks.size() == 1 && lib.history.size() == 200 && lib.history.front().time == 204);
    Library back;
    CHECK(from_json(to_json(lib), back) && back.disks == lib.disks && back.folders == lib.folders && back.history.size() == 200);
    CHECK(!from_json("[1,2]", back) && back.disks.size() == 1);

    CHECK(size_text(256 * kGB) == "256 GB" && size_text(std::uint64_t(41.2 * double(kGB))) == "41.2 GB" && size_text(512 * kMB) == "512 MB");
    CHECK(size_text(20 * kGB) == "20 GB" && size_text(5 * kGB + 800 * kMB) == "5.8 GB");

    if (failures) {
        std::printf("virtual disks: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("virtual disks: all checks passed\n");
    return 0;
}
