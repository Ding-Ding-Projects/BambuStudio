#include "DiskImage.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>

namespace Slic3r { namespace GUI { namespace VirtualDisks {

namespace fs = std::filesystem;

namespace {

// Random access to an image, from a file or from memory.
struct Source
{
    std::uint64_t                                                   size = 0;
    std::function<bool(std::uint64_t, std::size_t, std::uint8_t *)> read;

    std::vector<std::uint8_t> bytes(std::uint64_t offset, std::size_t n) const
    {
        std::vector<std::uint8_t> b(n);
        if (n == 0 || offset > size || n > size - offset || !read(offset, n, b.data())) b.clear();
        return b;
    }
};

std::uint16_t le16(const std::uint8_t *p) { return std::uint16_t(p[0] | (p[1] << 8)); }
std::uint32_t le32(const std::uint8_t *p) { return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24); }
std::uint64_t le64(const std::uint8_t *p) { return std::uint64_t(le32(p)) | (std::uint64_t(le32(p + 4)) << 32); }
std::uint32_t be32(const std::uint8_t *p) { return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]); }
std::uint64_t be64(const std::uint8_t *p) { return (std::uint64_t(be32(p)) << 32) | std::uint64_t(be32(p + 4)); }

// "2DC27766-F623-4200-9D64-115E9BFD4A08" in the byte order Windows stores a GUID.
std::array<std::uint8_t, 16> guid_bytes(const char *text)
{
    std::array<std::uint8_t, 16> out{};
    unsigned a = 0, b = 0, c = 0, d[8] = {};
    std::sscanf(text, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &a, &b, &c, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]);
    for (int i = 0; i < 4; ++i) out[std::size_t(i)] = std::uint8_t(a >> (8 * i));
    out[4] = std::uint8_t(b), out[5] = std::uint8_t(b >> 8), out[6] = std::uint8_t(c), out[7] = std::uint8_t(c >> 8);
    for (int i = 0; i < 8; ++i) out[std::size_t(8 + i)] = std::uint8_t(d[i]);
    return out;
}

bool same_guid(const std::uint8_t *p, const std::array<std::uint8_t, 16> &g) { return std::memcmp(p, g.data(), 16) == 0; }

// UTF-16 (little or big endian) to UTF-8, stopping at a NUL.
std::string utf16_to_utf8(const std::uint8_t *p, std::size_t bytes, bool big_endian)
{
    std::string out;
    for (std::size_t i = 0; i + 1 < bytes; i += 2) {
        std::uint32_t c = big_endian ? std::uint32_t((p[i] << 8) | p[i + 1]) : std::uint32_t(p[i] | (p[i + 1] << 8));
        if (c == 0) break;
        if (c >= 0xD800 && c <= 0xDBFF && i + 3 < bytes) {
            const std::uint32_t lo = big_endian ? std::uint32_t((p[i + 2] << 8) | p[i + 3]) : std::uint32_t(p[i + 2] | (p[i + 3] << 8));
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        if (c < 0x80) out += char(c);
        else if (c < 0x800) out += char(0xC0 | (c >> 6)), out += char(0x80 | (c & 0x3F));
        else if (c < 0x10000) out += char(0xE0 | (c >> 12)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
        else out += char(0xF0 | (c >> 18)), out += char(0x80 | ((c >> 12) & 0x3F)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
    }
    return out;
}

const auto kBatRegion        = guid_bytes("2DC27766-F623-4200-9D64-115E9BFD4A08");
const auto kMetadataRegion   = guid_bytes("8B7CA206-4790-4B9A-B8FE-575F050F886E");
const auto kFileParameters   = guid_bytes("CAA16737-FA36-4D43-B3B6-33F0AA44E76B");
const auto kVirtualDiskSize  = guid_bytes("2FA54224-CD1B-4876-B211-5DBED83BF4B8");
const auto kPage83Data       = guid_bytes("BECA12AB-B2E6-4523-93EF-C309E000C746");
const auto kLogicalSector    = guid_bytes("8141BF1D-A96F-4709-BA47-F233A8FAAB5F");
const auto kPhysicalSector   = guid_bytes("CDA348C7-445D-4471-9CC9-E9885251C556");
const auto kParentLocator    = guid_bytes("A8D35F2D-B30B-454D-ABF7-D3D84834AB0C");

// The partition table at the start of a disk's content: sector 0 and 1.
void read_partition_identity(const std::vector<std::uint8_t> &first, std::uint32_t sector, DiskInfo &d)
{
    if (first.size() < std::size_t(sector) * 2) return;
    if (std::all_of(first.begin(), first.begin() + sector, [](std::uint8_t v) { return v == 0; })) {
        d.partition_style = "Raw";
        return;
    }
    if (first[510] != 0x55 || first[511] != 0xAA) {
        d.partition_style = "Raw";
        return;
    }
    if (first[450] == 0xEE && std::memcmp(first.data() + sector, "EFI PART", 8) == 0) {
        d.partition_style   = "GPT";
        d.partition_disk_id = guid_text(first.data() + sector + 56);
        return;
    }
    char sig[16];
    std::snprintf(sig, sizeof(sig), "%08X", le32(first.data() + 440));
    d.partition_style   = "MBR";
    d.partition_disk_id = sig;
}

bool parse_vhd(const Source &src, DiskInfo &d)
{
    if (src.size < 512) return false;
    std::vector<std::uint8_t> footer = src.bytes(src.size - 512, 512);
    if (footer.empty() || std::memcmp(footer.data(), "conectix", 8) != 0) {
        // Some tools write a 511-byte footer; dynamic disks also keep a copy at the start.
        std::vector<std::uint8_t> alt = src.bytes(src.size - 511, 511);
        if (!alt.empty() && std::memcmp(alt.data(), "conectix", 8) == 0) {
            alt.push_back(0);
            footer = alt;
        } else {
            alt = src.bytes(0, 512);
            if (alt.empty() || std::memcmp(alt.data(), "conectix", 8) != 0) return false;
            footer = alt;
        }
    }
    d.format = Format::Vhd;
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < 512; ++i)
        if (i < 64 || i > 67) sum += footer[i];
    if (~sum != be32(footer.data() + 64)) {
        d.problem = "The VHD footer's checksum does not match: the file is damaged or was cut short.";
        return true;
    }
    d.virtual_size    = be64(footer.data() + 48);
    d.virtual_disk_id = guid_text(footer.data() + 68);
    switch (be32(footer.data() + 60)) {
    case 2: d.kind = Kind::Fixed; break;
    case 3: d.kind = Kind::Dynamic; break;
    case 4: d.kind = Kind::Differencing; break;
    default: d.problem = "The VHD footer names a disk type this reader does not know."; return true;
    }
    if (d.kind == Kind::Fixed) {
        d.readable = true;
        read_partition_identity(src.bytes(0, 1024), 512, d);
        return true;
    }
    const std::uint64_t        offset = be64(footer.data() + 16);
    std::vector<std::uint8_t> header = src.bytes(offset, 1024);
    if (header.empty() || std::memcmp(header.data(), "cxsparse", 8) != 0) {
        d.problem = "The VHD's dynamic header is missing.";
        return true;
    }
    const std::uint64_t table = be64(header.data() + 16);
    d.block_size              = be32(header.data() + 32);
    if (d.kind == Kind::Differencing) {
        d.parent_linkage = guid_text(header.data() + 40);
        d.parent_name    = utf16_to_utf8(header.data() + 64, 512, true);
        for (int i = 0; i < 8; ++i) {
            const std::uint8_t *e    = header.data() + 576 + i * 24;
            const std::uint32_t code = be32(e);
            const std::uint32_t len  = be32(e + 8);
            const std::uint64_t at   = be64(e + 16);
            if (len == 0 || len > 4096) continue;
            const std::vector<std::uint8_t> data = src.bytes(at, len);
            if (data.empty()) continue;
            if (code == 0x57327275) d.parent_relative = utf16_to_utf8(data.data(), data.size(), false); // "W2ru"
            if (code == 0x57326B75) d.parent_absolute = utf16_to_utf8(data.data(), data.size(), false); // "W2ku"
        }
    }
    d.readable = true;
    // Where block 0 lives, if it lives in this file.
    const std::vector<std::uint8_t> bat = src.bytes(table, 4);
    if (!bat.empty() && d.block_size >= 512) {
        const std::uint32_t sector = be32(bat.data());
        if (sector == 0xFFFFFFFFu) {
            if (d.kind == Kind::Dynamic) d.partition_style = "Raw";
        } else {
            const std::uint64_t bitmap = ((std::uint64_t(d.block_size) / 512 / 8 + 511) / 512) * 512;
            read_partition_identity(src.bytes(std::uint64_t(sector) * 512 + bitmap, 1024), 512, d);
        }
    }
    return true;
}

bool header_valid(const std::vector<std::uint8_t> &h, std::uint32_t signature, std::size_t size)
{
    if (h.size() < size || le32(h.data()) != signature) return false;
    std::vector<std::uint8_t> copy(h.begin(), h.begin() + std::ptrdiff_t(size));
    const std::uint32_t        stored = le32(copy.data() + 4);
    std::memset(copy.data() + 4, 0, 4);
    return crc32c(copy.data(), copy.size()) == stored;
}

bool parse_vhdx(const Source &src, DiskInfo &d)
{
    const std::vector<std::uint8_t> id = src.bytes(0, 8);
    if (id.empty() || std::memcmp(id.data(), "vhdxfile", 8) != 0) return false;
    d.format = Format::Vhdx;
    // Two headers; the valid one with the higher sequence number is current.
    const std::vector<std::uint8_t> h1 = src.bytes(64 * 1024, 4096), h2 = src.bytes(128 * 1024, 4096);
    const bool ok1 = header_valid(h1, 0x64616568, 4096), ok2 = header_valid(h2, 0x64616568, 4096);
    if (!ok1 && !ok2) {
        d.problem = "Neither VHDX header passes its checksum: the file is damaged.";
        return true;
    }
    const std::vector<std::uint8_t> &h = (ok1 && (!ok2 || le64(h1.data() + 8) >= le64(h2.data() + 8))) ? h1 : h2;
    d.data_write_guid = guid_text(h.data() + 32);
    // Two region tables, the same rule.
    std::vector<std::uint8_t> regions = src.bytes(192 * 1024, 64 * 1024);
    if (!header_valid(regions, 0x69676572, 64 * 1024)) regions = src.bytes(256 * 1024, 64 * 1024);
    if (!header_valid(regions, 0x69676572, 64 * 1024)) {
        d.problem = "Neither VHDX region table passes its checksum: the file is damaged.";
        return true;
    }
    std::uint64_t bat_offset = 0, meta_offset = 0;
    const std::uint32_t entries = std::min<std::uint32_t>(le32(regions.data() + 8), 2047);
    for (std::uint32_t i = 0; i < entries; ++i) {
        const std::uint8_t *e = regions.data() + 16 + i * 32;
        if (same_guid(e, kBatRegion)) bat_offset = le64(e + 16);
        if (same_guid(e, kMetadataRegion)) meta_offset = le64(e + 16);
    }
    const std::vector<std::uint8_t> table = src.bytes(meta_offset, 64 * 1024);
    if (meta_offset == 0 || table.empty() || std::memcmp(table.data(), "metadata", 8) != 0) {
        d.problem = "The VHDX metadata region is missing.";
        return true;
    }
    bool has_parent = false, leave_allocated = false;
    const std::uint16_t count = std::min<std::uint16_t>(le16(table.data() + 10), 2047);
    for (std::uint16_t i = 0; i < count; ++i) {
        const std::uint8_t *e      = table.data() + 32 + i * 32;
        const std::uint32_t offset = le32(e + 16), length = le32(e + 20);
        if (length == 0 || length > 64 * 1024 * 16) continue;
        const std::vector<std::uint8_t> item = src.bytes(meta_offset + offset, length);
        if (item.empty()) continue;
        if (same_guid(e, kFileParameters) && length >= 8) {
            d.block_size    = le32(item.data());
            leave_allocated = (le32(item.data() + 4) & 1) != 0;
            has_parent      = (le32(item.data() + 4) & 2) != 0;
        } else if (same_guid(e, kVirtualDiskSize) && length >= 8) {
            d.virtual_size = le64(item.data());
        } else if (same_guid(e, kPage83Data) && length >= 16) {
            d.virtual_disk_id = guid_text(item.data());
        } else if (same_guid(e, kLogicalSector) && length >= 4) {
            d.logical_sector = le32(item.data());
        } else if (same_guid(e, kPhysicalSector) && length >= 4) {
            d.physical_sector = le32(item.data());
        } else if (same_guid(e, kParentLocator) && length >= 20) {
            const std::uint16_t pairs = le16(item.data() + 18);
            for (std::uint16_t k = 0; k < pairs && 20 + std::size_t(k + 1) * 12 <= item.size(); ++k) {
                const std::uint8_t *kv  = item.data() + 20 + k * 12;
                const std::uint32_t ko = le32(kv), vo = le32(kv + 4);
                const std::uint16_t kl = le16(kv + 8), vl = le16(kv + 10);
                if (std::size_t(ko) + kl > item.size() || std::size_t(vo) + vl > item.size()) continue;
                const std::string key   = utf16_to_utf8(item.data() + ko, kl, false);
                const std::string value = utf16_to_utf8(item.data() + vo, vl, false);
                if (key == "parent_linkage") d.parent_linkage = value;
                else if (key == "relative_path") d.parent_relative = value;
                else if (key == "absolute_win32_path") d.parent_absolute = value;
            }
        }
    }
    d.kind     = has_parent ? Kind::Differencing : leave_allocated ? Kind::Fixed : Kind::Dynamic;
    d.readable = d.virtual_size > 0 && d.block_size > 0;
    if (!d.readable) {
        d.problem = "The VHDX metadata lacks its size or block size.";
        return true;
    }
    // Block 0 of the content, if this file holds it.
    const std::vector<std::uint8_t> bat = src.bytes(bat_offset, 8);
    if (!bat.empty()) {
        const std::uint64_t entry = le64(bat.data());
        const unsigned      state = unsigned(entry & 7);
        if (state == 6 || (state == 7 && d.kind != Kind::Differencing)) {
            const std::uint64_t at = (entry >> 20) * 1024 * 1024;
            read_partition_identity(src.bytes(at, std::size_t(d.logical_sector) * 2), d.logical_sector, d);
        } else if (d.kind != Kind::Differencing && (state == 0 || state == 2 || state == 3)) {
            d.partition_style = "Raw";
        }
    }
    return true;
}

bool parse_iso(const Source &src, DiskInfo &d)
{
    const std::vector<std::uint8_t> pvd = src.bytes(16 * 2048, 2048);
    if (pvd.empty() || pvd[0] != 1 || std::memcmp(pvd.data() + 1, "CD001", 5) != 0) return false;
    d.format   = Format::Iso;
    d.kind     = Kind::ReadOnly;
    std::string label(reinterpret_cast<const char *>(pvd.data() + 40), 32);
    while (!label.empty() && (label.back() == ' ' || label.back() == '\0')) label.pop_back();
    d.label        = label;
    d.virtual_size = std::uint64_t(le32(pvd.data() + 80)) * le16(pvd.data() + 128);
    d.readable     = true;
    d.partition_style = "ISO 9660";
    return true;
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

// Paths compared the way Windows does: case-insensitive, either slash.
std::string normal_path(std::string p)
{
    std::replace(p.begin(), p.end(), '/', '\\');
    while (p.size() > 1 && p.back() == '\\') p.pop_back();
    return lower(p);
}

std::string folder_of(const std::string &path)
{
    const std::string n = normal_path(path);
    const auto        at = n.find_last_of('\\');
    return at == std::string::npos ? std::string() : n.substr(0, at);
}

std::string file_of(const std::string &path)
{
    const std::string n = normal_path(path);
    const auto        at = n.find_last_of('\\');
    return at == std::string::npos ? n : n.substr(at + 1);
}

std::string strip_guid(std::string g)
{
    g.erase(std::remove_if(g.begin(), g.end(), [](char c) { return c == '{' || c == '}'; }), g.end());
    return lower(g);
}

} // namespace

std::uint32_t crc32c(const std::uint8_t *data, std::size_t size)
{
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0x82F63B78u : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::string guid_text(const std::uint8_t b[16], bool big_endian)
{
    char out[40];
    if (big_endian)
        std::snprintf(out, sizeof(out), "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}", b[0], b[1], b[2], b[3], b[4], b[5], b[6],
                      b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    else
        std::snprintf(out, sizeof(out), "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", le32(b), le16(b + 4), le16(b + 6), b[8], b[9], b[10], b[11],
                      b[12], b[13], b[14], b[15]);
    return out;
}

Format format_from_extension(std::string_view path)
{
    std::string p = lower(std::string(path));
    const auto  dot = p.find_last_of('.');
    if (dot == std::string::npos) return Format::Unknown;
    const std::string ext = p.substr(dot + 1);
    if (ext == "vhd") return Format::Vhd;
    if (ext == "vhdx" || ext == "avhdx") return Format::Vhdx;
    if (ext == "avhd") return Format::Vhd;
    if (ext == "vhds") return Format::VhdSet;
    if (ext == "iso") return Format::Iso;
    return Format::Unknown;
}

const char *format_name(Format format)
{
    switch (format) {
    case Format::Vhd: return "VHD";
    case Format::Vhdx: return "VHDX";
    case Format::VhdSet: return "VHD Set";
    case Format::Iso: return "ISO";
    default: return "Unknown";
    }
}

const char *kind_name(Kind kind)
{
    switch (kind) {
    case Kind::Fixed: return "fixed";
    case Kind::Dynamic: return "dynamic";
    case Kind::Differencing: return "differencing";
    case Kind::ReadOnly: return "read-only";
    default: return "unknown";
    }
}

static DiskInfo read_source(const std::string &path, const Source &src)
{
    DiskInfo d;
    d.path      = path;
    d.file_size = src.size;
    const Format by_name = format_from_extension(path);
    if (parse_vhdx(src, d) || parse_vhd(src, d) || parse_iso(src, d)) return d;
    d.format = by_name;
    if (by_name == Format::VhdSet) {
        // A VHD Set's own file is small; its checkpoints are read by Windows.
        d.readable = src.size > 0;
        if (!d.readable) d.problem = "The VHD Set file is empty.";
        return d;
    }
    d.problem = by_name == Format::Unknown ? "Not a VHD, VHDX, VHD Set or ISO file." : "The file does not start or end like a " + std::string(format_name(by_name)) + " file.";
    return d;
}

DiskInfo read_disk_bytes(const std::string &path, const std::vector<std::uint8_t> &bytes)
{
    Source src;
    src.size = bytes.size();
    src.read = [&bytes](std::uint64_t offset, std::size_t n, std::uint8_t *out) {
        std::memcpy(out, bytes.data() + offset, n);
        return true;
    };
    return read_source(path, src);
}

DiskInfo read_disk(const std::string &path)
{
    std::error_code ec;
    const fs::path  p = fs::u8path(path);
    const auto      size = fs::file_size(p, ec);
    if (ec) {
        DiskInfo d;
        d.path    = path;
        d.format  = format_from_extension(path);
        d.problem = "The file cannot be opened: " + ec.message();
        return d;
    }
    std::ifstream in(p, std::ios::binary);
    Source        src;
    src.size = size;
    src.read = [&in](std::uint64_t offset, std::size_t n, std::uint8_t *out) {
        in.clear();
        in.seekg(std::streamoff(offset));
        in.read(reinterpret_cast<char *>(out), std::streamsize(n));
        return std::size_t(in.gcount()) == n;
    };
    if (!in) {
        DiskInfo d;
        d.path    = path;
        d.format  = format_from_extension(path);
        d.problem = "The file cannot be opened for reading (it may be attached read-write).";
        return d;
    }
    return read_source(path, src);
}

int find_parent(const std::vector<DiskInfo> &disks, std::size_t child)
{
    if (child >= disks.size()) return -1;
    const DiskInfo &c = disks[child];
    if (c.kind != Kind::Differencing) return -1;
    std::vector<std::string> wanted;
    if (!c.parent_absolute.empty()) wanted.push_back(normal_path(c.parent_absolute));
    const std::string folder = folder_of(c.path);
    if (!c.parent_relative.empty()) {
        std::string rel = normal_path(c.parent_relative);
        while (rel.rfind(".\\", 0) == 0) rel = rel.substr(2);
        std::string base = folder;
        while (rel.rfind("..\\", 0) == 0) {
            rel  = rel.substr(3);
            base = folder_of(base);
        }
        wanted.push_back(base.empty() ? rel : base + "\\" + rel);
    }
    for (const std::string &name : {c.parent_absolute, c.parent_relative, c.parent_name})
        if (!name.empty()) wanted.push_back(folder.empty() ? file_of(name) : folder + "\\" + file_of(name));
    for (const std::string &w : wanted)
        for (std::size_t i = 0; i < disks.size(); ++i)
            if (i != child && normal_path(disks[i].path) == w) return int(i);
    return -1;
}

ChainTest test_chain(const std::vector<DiskInfo> &disks, std::size_t disk)
{
    ChainTest t;
    if (disk >= disks.size()) return t;
    if (disks[disk].format == Format::Iso) return t;
    std::set<std::size_t> seen;
    std::size_t           at = disk;
    for (int depth = 0; depth < 64; ++depth) {
        const DiskInfo &d = disks[at];
        if (!d.readable) {
            t.result = ChainResult::Unreadable;
            t.detail = file_of(d.path) + ": " + d.problem;
            return t;
        }
        if (!seen.insert(at).second) {
            t.result = ChainResult::Loop;
            t.detail = file_of(d.path) + " leads back to itself.";
            return t;
        }
        if (d.kind != Kind::Differencing) {
            t.result = ChainResult::Passed;
            t.detail = depth == 0 ? "No parent: the disk stands on its own." : "Every parent was found and matches.";
            return t;
        }
        const int p = find_parent(disks, at);
        if (p < 0) {
            t.result = ChainResult::ParentMissing;
            const std::string stored = !d.parent_absolute.empty() ? d.parent_absolute : !d.parent_relative.empty() ? d.parent_relative : d.parent_name;
            t.detail = file_of(d.path) + " needs " + (stored.empty() ? std::string("a parent it does not name") : stored) + ", which is not in the list.";
            return t;
        }
        const DiskInfo &parent = disks[std::size_t(p)];
        // A VHDX child records its parent's data write GUID; a VHD child its parent's unique id.
        const std::string expected = d.format == Format::Vhdx ? parent.data_write_guid : parent.virtual_disk_id;
        if (parent.readable && !d.parent_linkage.empty() && !expected.empty() && strip_guid(d.parent_linkage) != strip_guid(expected)) {
            t.result = ChainResult::ParentChanged;
            t.detail = file_of(parent.path) + " was written to after " + file_of(d.path) + " was made from it, so the child no longer matches.";
            return t;
        }
        at = std::size_t(p);
    }
    t.result = ChainResult::Loop;
    t.detail = "The chain is deeper than 64 disks.";
    return t;
}

std::vector<std::size_t> children_of(const std::vector<DiskInfo> &disks, std::size_t parent)
{
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < disks.size(); ++i)
        if (find_parent(disks, i) == int(parent)) out.push_back(i);
    return out;
}

std::vector<std::pair<std::size_t, std::size_t>> same_disk_ids(const std::vector<DiskInfo> &disks)
{
    std::vector<std::pair<std::size_t, std::size_t>> out;
    for (std::size_t i = 0; i < disks.size(); ++i)
        for (std::size_t j = i + 1; j < disks.size(); ++j) {
            const DiskInfo &a = disks[i], &b = disks[j];
            if (a.partition_disk_id.empty() || a.partition_style != b.partition_style) continue;
#ifndef SLIC3R_VIRTUAL_DISKS_NEGATIVE_FIXTURE
            // A child shares its parent's partition table by design: that is not a clash.
            if (find_parent(disks, j) == int(i) || find_parent(disks, i) == int(j)) continue;
#endif
            if (lower(a.partition_disk_id) == lower(b.partition_disk_id)) out.emplace_back(i, j);
        }
    return out;
}

std::string size_text(std::uint64_t bytes)
{
    const char *units[] = {"bytes", "KB", "MB", "GB", "TB", "PB"};
    double      value   = double(bytes);
    int         unit    = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    char out[32];
    if (unit == 0) std::snprintf(out, sizeof(out), "%llu bytes", static_cast<unsigned long long>(bytes));
    else if (value >= 100.0 || (value >= 10.0 && value == double(static_cast<long long>(value)))) std::snprintf(out, sizeof(out), "%.0f %s", value, units[unit]);
    else std::snprintf(out, sizeof(out), "%.1f %s", value, units[unit]);
    return out;
}

}}} // namespace Slic3r::GUI::VirtualDisks
