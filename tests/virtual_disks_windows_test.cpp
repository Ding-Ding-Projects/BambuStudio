// Real virtual disks on a hosted Windows machine, through the same code the
// app runs: create a VHDX with chosen block and sector sizes, attach it and
// find its mount point without administrator permission, read it back, make
// a child, write through the child and merge it into the parent (after a
// verified copy), grow it detached and attached, compact it, give a copy a new
// disk ID so the two no longer clash, reconnect a moved parent, and Quick pack
// a folder with every file checked. Windows only; run by
// tests/virtual_disks_windows.test.mjs.
#include "src/slic3r/GUI/VirtualDisks/DiskImage.cpp"
#include "src/slic3r/GUI/VirtualDisks/DiskPlan.cpp"
#include "src/slic3r/GUI/VirtualDisks/DiskSystem.cpp"

#ifdef _MSC_VER
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#include <cstdio>
#include <cstdlib>

using namespace Slic3r::GUI::VirtualDisks;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static std::string g_work;

static bool run(const Script &s, std::string *extra = nullptr)
{
    std::printf("== %s\n", s.summary.c_str());
    const RunOutcome o = run_elevated(s, g_work, [](const std::string &line) { std::printf("   %s\n", line.c_str()); }, nullptr, nullptr);
    std::printf("   -> %s %s\n", o.status == RunOutcome::Status::Done ? "Done" : "NOT DONE", o.message.c_str());
    if (extra) *extra = o.extra_json;
    return o.status == RunOutcome::Status::Done;
}

static std::string strip(std::string g)
{
    std::string out;
    for (char c : g)
        if (c != '{' && c != '}') out += char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

int main()
{
    const Routes routes = find_routes();
    std::printf("%s; VirtDisk %d, Storage %d, DiskPart %d, Robocopy %d, Hyper-V %d\n", routes.windows_name.c_str(), routes.virtdisk, routes.storage,
                routes.diskpart, routes.robocopy, routes.hyperv);
    CHECK(routes.windows && routes.virtdisk && routes.storage && routes.diskpart && routes.robocopy);

    const char *temp = std::getenv("RUNNER_TEMP");
    const fs::path dir = fs::path(temp ? temp : ".") / "bambu-vd";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "moved", ec);
    g_work = (dir / "work").u8string();
    const std::string base = (dir / "Base.vhdx").u8string();

    // Create, attach and format, with chosen block and sector sizes.
    CreateRequest r;
    r.path = base, r.size = 256 * kMB, r.block_size = 8 * 1024 * 1024, r.logical_sector = 512, r.physical_sector = 4096, r.label = "BambuTest";
    CHECK(validate(r, free_space(base), false).ok());
    CHECK(run(create_script(r)));

    bool available = false;
    auto attached  = attached_disks(available);
    CHECK(available && attached[path_key(base)].attached && !attached[path_key(base)].mount_points.empty());
    std::string mount = attached[path_key(base)].mount_points.empty() ? std::string() : attached[path_key(base)].mount_points.front();
    std::printf("   mounted at %s\n", mount.c_str());
    CHECK(run(detach_script(base)));
    attached = attached_disks(available);
    CHECK(!attached[path_key(base)].attached);

    DiskInfo info = read_disk(base);
    CHECK(info.readable && info.format == Format::Vhdx && info.kind == Kind::Dynamic);
    CHECK(info.block_size == 8u * 1024 * 1024 && info.virtual_size == 256 * kMB && info.logical_sector == 512 && info.physical_sector == 4096);
    CHECK(info.partition_style == "GPT" && !info.partition_disk_id.empty());
    std::printf("   GPT disk %s, data write %s\n", info.partition_disk_id.c_str(), info.data_write_guid.c_str());

    // A child, written through, then merged into the parent after a verified copy.
    const std::string child = (dir / "Scratch.avhdx").u8string();
    CHECK(run(child_script(base, child)));
    DiskInfo kid = read_disk(child);
    CHECK(kid.readable && kid.kind == Kind::Differencing && strip(kid.parent_linkage) == strip(read_disk(base).data_write_guid));
    CHECK(test_chain({read_disk(base), kid}, 1).result == ChainResult::Passed);
    CHECK(run(attach_script(child, false)));
    attached = attached_disks(available);
    mount    = attached[path_key(child)].mount_points.empty() ? std::string() : attached[path_key(child)].mount_points.front();
    CHECK(!mount.empty());
    if (!mount.empty()) std::ofstream(fs::u8path(mount) / "from-the-child.txt") << "written through the child\n";
    CHECK(run(detach_script(child)));
    CHECK(run(merge_script(child, base, (dir / "Base (copy).vhdx").u8string())));
    CHECK(fs::exists(dir / "Base (copy).vhdx"));
    CHECK(run(attach_script(base, true)));
    attached = attached_disks(available);
    mount    = attached[path_key(base)].mount_points.empty() ? std::string() : attached[path_key(base)].mount_points.front();
    CHECK(!mount.empty() && fs::exists(fs::u8path(mount) / "from-the-child.txt"));
    CHECK(run(detach_script(base)));

    // Grow detached, then attached with the volume extended.
    CHECK(run(grow_script(base, 384 * kMB, false)));
    CHECK(read_disk(base).virtual_size == 384 * kMB);
    CHECK(run(attach_script(base, false)));
    CHECK(run(grow_script(base, 512 * kMB, true)));
    attached = attached_disks(available);
    mount    = attached[path_key(base)].mount_points.empty() ? std::string() : attached[path_key(base)].mount_points.front();
    if (!mount.empty()) {
        const fs::space_info s = fs::space(fs::u8path(mount), ec);
        std::printf("   volume now %llu MB\n", static_cast<unsigned long long>(s.capacity / kMB));
        CHECK(s.capacity > 400 * kMB);
    } else {
        CHECK(!"the grown disk has a mount point");
    }
    CHECK(run(detach_script(base)));
    CHECK(read_disk(base).virtual_size == 512 * kMB);

    // Compact, detached.
    std::string extra;
    CHECK(run(compact_script(base, CompactMode::Full, false), &extra));
    std::printf("   compact %s\n", extra.c_str());

    // A byte-for-byte copy clashes; a new disk ID ends the clash.
    const std::string twin = (dir / "Twin.vhdx").u8string();
    fs::copy_file(fs::u8path(base), fs::u8path(twin), ec);
    CHECK(same_disk_ids({read_disk(base), read_disk(twin)}).size() == 1);
    CHECK(run(new_disk_id_script(twin, "GPT")));
    CHECK(same_disk_ids({read_disk(base), read_disk(twin)}).empty());

    // A moved parent, reconnected.
    const std::string child2 = (dir / "Second.avhdx").u8string();
    CHECK(run(child_script(base, child2)));
    const std::string moved = (dir / "moved" / "Base.vhdx").u8string();
    fs::rename(fs::u8path(base), fs::u8path(moved), ec);
    CHECK(!ec && test_chain({read_disk(moved), read_disk(child2)}, 1).result == ChainResult::ParentMissing);
    CHECK(run(reconnect_parent_script(child2, moved)));
    CHECK(test_chain({read_disk(moved), read_disk(child2)}, 1).result == ChainResult::Passed);

    // Quick pack: a folder with a nested tree and a web mark, every file checked.
    const fs::path folder = dir / "Pack";
    fs::create_directories(folder / "nested" / "deeper", ec);
    for (int i = 0; i < 30; ++i) {
        const fs::path f = folder / (i % 3 == 0 ? fs::path("nested") / ("part-" + std::to_string(i) + ".3mf") : fs::path("file-" + std::to_string(i) + ".gcode"));
        std::ofstream(f, std::ios::binary) << std::string(std::size_t(1000 + i * 977), char('a' + i % 26));
    }
    std::ofstream(fs::path(folder / "nested" / "deeper" / "plate.png").concat(":Zone.Identifier")) << "[ZoneTransfer]\nZoneId=3\n";
    std::ofstream(folder / "nested" / "deeper" / "plate.png", std::ios::app) << "png";
    const FolderStats stats = folder_stats(folder.u8string());
    CHECK(stats.complete && stats.files == 31);
    PackRequest pack;
    pack.source = folder.u8string(), pack.target = unique_target(dir.u8string(), "Pack", {}), pack.source_bytes = stats.bytes, pack.source_files = stats.files;
    pack.label = "Pack";
    CHECK(run(pack_script(pack), &extra));
    std::printf("   pack %s\n", extra.c_str());
    CHECK(extra.find("\"files\":31") != std::string::npos);
    CHECK(fs::exists(fs::u8path(pack.target)) && read_disk(pack.target).readable);
    // The copy keeps the web mark (an alternate data stream).
    CHECK(run(attach_script(pack.target, true)));
    attached = attached_disks(available);
    mount    = attached[path_key(pack.target)].mount_points.empty() ? std::string() : attached[path_key(pack.target)].mount_points.front();
    CHECK(!mount.empty() && fs::exists(fs::path(fs::u8path(mount) / "Pack" / "nested" / "deeper" / "plate.png").concat(":Zone.Identifier"), ec));
    CHECK(run(detach_script(pack.target)));

    if (failures) {
        std::printf("virtual disks on Windows: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("virtual disks on Windows: all checks passed\n");
    return 0;
}
