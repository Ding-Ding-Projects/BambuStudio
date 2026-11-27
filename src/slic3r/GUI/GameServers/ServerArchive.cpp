#include "ServerSystem.hpp"
#include "libslic3r/miniz_extension.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
FILE *archive_file(const fs::path &path, bool write)
{
#ifdef _WIN32
    return _wfopen(path.c_str(), write ? L"wb" : L"rb");
#else
    return std::fopen(path.c_str(), write ? "wb" : "rb");
#endif
}
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
std::time_t archive_time(fs::file_time_type t)
{
    return std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(t - fs::file_time_type::clock::now() + std::chrono::system_clock::now()));
}
} // namespace
namespace {
bool safe_entry(const std::string &name);
}

ZipOutcome write_zip(const std::string                                       &root_u8,
                     const std::vector<std::string>                          &entries,
                     const std::string                                       &zip_path,
                     const std::function<void(std::uint64_t, std::uint64_t)> &progress,
                     const std::atomic<bool>                                 *cancel,
                     const std::map<std::string, std::string>                &memory_entries)
{
    ZipOutcome      r;
    const fs::path  root = fs::u8path(root_u8), target = fs::u8path(zip_path);
    std::error_code ec;
    if (fs::exists(target, ec)) {
        r.error = "A file with that name is already there.";
        return r;
    }
    struct Item
    {
        fs::path      path;
        std::string   name;
        std::uint64_t size;
    };
    std::vector<Item> items;
    std::uint64_t     total = 0;
    for (const std::string &entry : entries) {
        const fs::path p = root / fs::u8path(entry);
        if (!safe_entry(entry) || fs::is_symlink(p, ec)) {
            r.error = "Unsafe or linked backup entry.";
            return r;
        }
        if (memory_entries.count(entry)) continue;
        if (fs::is_directory(p, ec)) {
            for (fs::recursive_directory_iterator it(p, fs::directory_options::none, ec), end; !ec && it != end; it.increment(ec)) {
                std::error_code e2;
                if (it->is_symlink(e2) || e2) {
                    r.error = "Symbolic links require a separate backup decision.";
                    return r;
                }
                if (!it->is_regular_file(e2) || it->path().filename() == "session.lock" || it->path().filename() == "LOCK") continue;
                const std::uint64_t size = it->file_size(e2);
                items.push_back({it->path(), fs::relative(it->path(), root, e2).generic_u8string(), size});
                total += size;
            }
            if (ec) {
                r.error = "Could not read " + entry + ".";
                return r;
            }
        } else if (fs::is_regular_file(p, ec)) {
            const std::uint64_t size = fs::file_size(p, ec);
            items.push_back({p, fs::path(fs::u8path(entry)).generic_u8string(), size});
            total += size;
        }
    }
    fs::create_directories(target.parent_path(), ec);
    for (const auto &item : memory_entries) {
        if (!safe_entry(item.first) || item.second.size() > 4 * 1024 * 1024) {
            r.error = "Invalid generated backup entry.";
            return r;
        }
        items.push_back({{}, item.first, item.second.size()});
        total += item.second.size();
    }
    const auto staging = target.parent_path() / (".backup-" + std::to_string(std::random_device{}()));
    if (!fs::create_directory(staging, ec)) {
        r.error = "Could not reserve a new backup staging directory.";
        return r;
    }
    const fs::path partial = staging / "world.zip";
    struct Teardown
    {
        fs::path dir, file;
        ~Teardown()
        {
            std::error_code e;
            fs::remove(file, e);
            fs::remove(dir, e);
        }
    } hold{staging, partial};
    mz_zip_archive zip{};
    if (!open_zip_writer(&zip, partial.u8string())) {
        r.error = "Could not create " + partial.u8string() + ".";
        return r;
    }
    std::uint64_t done   = 0;
    bool          failed = false;
    for (const Item &item : items) {
        if (cancel && cancel->load()) {
            r.cancelled = true;
            failed      = true;
            break;
        }
        const auto generated = memory_entries.find(item.name);
        if (generated != memory_entries.end()) {
            if (!mz_zip_writer_add_mem(&zip, item.name.c_str(), generated->second.data(), generated->second.size(), MZ_BEST_SPEED)) {
                r.error = "Could not write a generated archive entry.";
                failed  = true;
                break;
            }
            done += item.size;
            ++r.files;
            if (progress) progress(done, total);
            continue;
        }
        FILE *f = archive_file(item.path, false);
        if (!f) {
            r.error = "Could not open " + item.name + ".";
            failed  = true;
            break;
        }
        const MZ_TIME_T when = archive_time(fs::last_write_time(item.path, ec));
        const bool      ok   = mz_zip_writer_add_cfile(&zip, item.name.c_str(), f, item.size, &when, nullptr, 0, MZ_BEST_SPEED, nullptr, 0, nullptr, 0);
        fclose(f);
        if (!ok) {
            r.error = "Could not add " + item.name + " (" + MZ_Archive::get_errorstr(zip.m_last_error) + ").";
            failed  = true;
            break;
        }
        done += item.size;
        ++r.files;
        if (progress) progress(done, total);
    }
    if (!failed && !mz_zip_writer_finalize_archive(&zip)) {
        r.error = "Could not finish the zip file.";
        failed  = true;
    }
    close_zip_writer(&zip);
    if (failed) {
        fs::remove(partial, ec);
        return r;
    }
    // Every file is read back and checked against its CRC before the backup counts.
    mz_zip_archive check{};
    bool           valid = open_zip_reader(&check, partial.u8string());
    if (valid) {
        valid = mz_zip_reader_get_num_files(&check) == mz_uint(items.size()) && mz_zip_validate_archive(&check, 0);
        close_zip_reader(&check);
    }
    if (!valid) {
        fs::remove(partial, ec);
        r.error = "The new zip file did not read back correctly.";
        return r;
    }
#ifdef _WIN32
    if (!MoveFileExW(partial.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
        ec = std::error_code(int(GetLastError()), std::system_category());
    else
        ec.clear();
#else
    fs::create_hard_link(partial, target, ec);
#endif
    if (ec) {
        r.error = "Could not publish the finished zip without replacing an existing file.";
        return r;
    }
    r.bytes = fs::file_size(target, ec);
    r.ok    = true;
    return r;
}

namespace {

// A zip entry name that stays inside the folder it is extracted into.
bool safe_entry(const std::string &name)
{
    if (name.empty() || name[0] == '/' || name[0] == '\\' || name.find(':') != std::string::npos) return false;
    std::string normal = name;
    std::replace(normal.begin(), normal.end(), '\\', '/');
    std::istringstream in(normal);
    std::string        part;
    while (std::getline(in, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part.back() == '.' || part.back() == ' ' || part.find_first_of("<>\"|?*") != std::string::npos ||
            std::any_of(part.begin(), part.end(), [](unsigned char c) { return c < 32; }))
            return false;
        const auto stem = lower(part.substr(0, part.find('.')));
        if (stem == "con" || stem == "prn" || stem == "nul" || stem == "aux" ||
            (stem.size() == 4 && (stem.substr(0, 3) == "com" || stem.substr(0, 3) == "lpt") && stem[3] >= '0' && stem[3] <= '9'))
            return false;
    }
    return true;
}

} // namespace

ZipOutcome extract_zip(const std::string &zip_path, const std::string &target_u8, const std::atomic<bool> *cancel, std::uint64_t byte_limit)
{
    ZipOutcome      r;
    const fs::path  target = fs::u8path(target_u8);
    std::error_code ec;
    if (fs::exists(target, ec)) {
        r.error = "The folder to restore into is already there.";
        return r;
    }
    mz_zip_archive zip{};
    if (!open_zip_reader(&zip, zip_path)) {
        r.error = "Could not open the backup.";
        return r;
    }
    if (!target.parent_path().empty()) fs::create_directories(target.parent_path(), ec);
    if (ec || !fs::create_directory(target, ec)) {
        close_zip_reader(&zip);
        r.error = "The destination folder could not be created exclusively.";
        return r;
    }
    const mz_uint count  = mz_zip_reader_get_num_files(&zip);
    bool          failed = false;
    for (mz_uint i = 0; i < count && !failed; ++i) {
        if (cancel && cancel->load()) {
            r.cancelled = true;
            failed      = true;
            break;
        }
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st)) {
            r.error = "The backup is damaged.";
            failed  = true;
            break;
        }
        if (st.m_uncomp_size > byte_limit - r.bytes) {
            r.error = "The archive exceeds the extraction limit.";
            failed  = true;
            break;
        }
        const std::string name = st.m_filename;
        if (!safe_entry(name)) {
            r.error = "The backup names a file outside its folder: " + name;
            failed  = true;
            break;
        }
        const fs::path out = target / fs::u8path(name);
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            fs::create_directories(out, ec);
            continue;
        }
        fs::create_directories(out.parent_path(), ec);
        if (ec || fs::exists(out, ec)) {
            r.error = "Duplicate or unavailable archive destination.";
            failed  = true;
            break;
        }
        FILE *f = archive_file(out, true);
        if (!f) {
            r.error = "Could not write " + name + ".";
            failed  = true;
            break;
        }
        const bool    extracted = mz_zip_reader_extract_to_cfile(&zip, i, f, 0) != 0;
        const bool    closed    = fclose(f) == 0;
        mz_ulong      crc       = mz_crc32(MZ_CRC32_INIT, nullptr, 0);
        std::uint64_t bytes     = 0;
        std::ifstream verified(out, std::ios::binary);
        char          chunk[65536];
        while (extracted && closed && verified) {
            verified.read(chunk, sizeof(chunk));
            const auto got = verified.gcount();
            crc            = mz_crc32(crc, reinterpret_cast<const mz_uint8 *>(chunk), std::size_t(got));
            bytes += std::uint64_t(got);
        }
        const bool ok = extracted && closed && verified.eof() && bytes == st.m_uncomp_size && crc == st.m_crc32;
        if (!ok) {
            r.error = "Could not extract " + name + ".";
            failed  = true;
            break;
        }
        r.bytes += st.m_uncomp_size;
        ++r.files;
    }
    close_zip_reader(&zip);
    if (failed) {
        fs::remove_all(target, ec);
        return r;
    }
    r.ok = true;
    return r;
}

std::vector<std::string> zip_top_entries(const std::string &zip_path)
{
    std::set<std::string> top;
    mz_zip_archive        zip{};
    if (!open_zip_reader(&zip, zip_path)) return {};
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        std::string name = st.m_filename;
        std::replace(name.begin(), name.end(), '\\', '/');
        top.insert(name.substr(0, name.find('/')));
    }
    close_zip_reader(&zip);
    return {top.begin(), top.end()};
}

}}} // namespace Slic3r::GUI::GameServers
