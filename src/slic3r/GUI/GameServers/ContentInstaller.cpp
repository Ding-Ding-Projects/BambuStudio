#include "ContentInstaller.hpp"
#include "ServerSystem.hpp"
#include "libslic3r/miniz_extension.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {

struct ContentManifest {
    std::set<std::string> names;
    std::string pack_meta;
    bool data = false;
};

bool inspect(const fs::path &file, ContentManifest &out)
{
    mz_zip_archive zip{};
    if (!open_zip_reader(&zip, file.u8string())) return false;
    const auto count = mz_zip_reader_get_num_files(&zip);
    bool valid = count > 0 && count <= 200000;
    for (mz_uint i = 0; valid && i < count; ++i) {
        mz_zip_archive_file_stat st{};
        valid = mz_zip_reader_file_stat(&zip, i, &st) != 0;
        if (!valid) break;
        const std::string name = st.m_filename;
        if (name.rfind("data/", 0) == 0 && !mz_zip_reader_is_file_a_directory(&zip, i)) out.data = true;
        if (name == "plugin.yml" || name == "paper-plugin.yml" || name == "fabric.mod.json" ||
            name == "META-INF/mods.toml" || name == "META-INF/neoforge.mods.toml" || name == "quilt.mod.json") out.names.insert(name);
        if (name == "pack.mcmeta") {
            if (!out.pack_meta.empty() || st.m_uncomp_size == 0 || st.m_uncomp_size > 1024 * 1024) { valid = false; break; }
            out.pack_meta.resize(std::size_t(st.m_uncomp_size));
            valid = mz_zip_reader_extract_to_mem(&zip, i, out.pack_meta.data(), out.pack_meta.size(), 0) != 0;
        }
    }
    close_zip_reader(&zip);
    return valid;
}

std::string hash_file(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    Sha1 hash;
    char buf[65536];
    while (in) { in.read(buf, sizeof(buf)); hash.update(buf, std::size_t(in.gcount())); }
    return in.eof() ? hash.hex() : std::string();
}

bool inside(const fs::path &root, const fs::path &path)
{
    std::error_code ec;
    const auto resolved = fs::weakly_canonical(path, ec);
    if (ec) return false;
    const auto rel = resolved.lexically_relative(root);
    return !rel.empty() && !rel.is_absolute() && *rel.begin() != ".." && rel != ".";
}
}

std::string validate_java_replacement(const fs::path &src,const std::string &flavour,const std::string &group)
{
    std::error_code ec;
    if(fs::file_size(src,ec)>1024ULL*1024*1024 || ec)return "The source is unavailable or oversized.";
    ContentManifest manifest;if(!inspect(src,manifest))return "The package metadata could not be read.";
    auto has=[&](const char *key){return manifest.names.count(key)!=0;};
    const bool paper=flavour=="Paper"||flavour=="Purpur";
    if(group=="plugins" && src.extension()==".jar" && ((paper&&has("paper-plugin.yml"))||((paper||flavour=="Spigot")&&has("plugin.yml"))))return {};
    if(group=="mods" && src.extension()==".jar" && ((flavour=="Fabric"&&has("fabric.mod.json"))||(flavour=="Forge"&&has("META-INF/mods.toml"))||
       (flavour=="NeoForge"&&has("META-INF/neoforge.mods.toml"))||(flavour=="Quilt"&&has("quilt.mod.json"))))return {};
    if(group=="datapacks" && src.extension()==".zip" && manifest.data){
        auto meta=nlohmann::json::parse(manifest.pack_meta,nullptr,false);if(meta.is_object()&&meta.contains("pack")&&meta["pack"].is_object())return {};
    }
    return "Package descriptor does not match the selected loader and content type.";
}
ContentInstallResult install_java_content(const std::string &server_folder, const std::string &flavour, const std::string &archive)
{
    std::error_code ec;
    const auto root = fs::canonical(fs::u8path(server_folder), ec);
    if (ec || !fs::is_directory(root, ec)) return {false, "The server folder is unavailable."};
    const auto src = fs::u8path(archive);
    const auto bytes = fs::file_size(src, ec);
    if (ec || bytes > std::uint64_t(1024) * 1024 * 1024) return {false, "Select a readable package of at most 1 GiB."};
    const auto props = inspect_folder(root.u8string());
    if (!props.has_properties || props.world_locked) return {false, "Stop the server and select its configured directory."};
    const auto original_hash = hash_file(src);
    if (original_hash.empty()) return {false, "Could not read the selected file."};
    ContentManifest manifest;
    if (!inspect(src, manifest)) return {false, "The package is not a readable ZIP or JAR with bounded metadata."};
    fs::path dest;
    if (src.extension() == ".jar") {
        const auto has = [&](const char *name) { return manifest.names.count(name) != 0; };
        const bool paper = flavour == "Paper" || flavour == "Purpur";
        if ((paper || flavour == "Spigot") && (has("plugin.yml") || (paper && has("paper-plugin.yml")))) dest = root / "plugins";
        else if ((flavour == "Fabric" && has("fabric.mod.json")) || (flavour == "Forge" && has("META-INF/mods.toml")) ||
                 (flavour == "NeoForge" && has("META-INF/neoforge.mods.toml")) || (flavour == "Quilt" && has("quilt.mod.json"))) dest = root / "mods";
        else return {false, "The package has no recognized plugin or mod descriptor for this server distribution."};
    } else if (src.extension() == ".zip" && manifest.data) {
        const auto meta = nlohmann::json::parse(manifest.pack_meta, nullptr, false);
        if (!meta.is_object() || !meta.contains("pack") || !meta["pack"].is_object()) return {false, "The data pack is missing valid pack.mcmeta metadata."};
        const auto level = fs::u8path(props.level_name);
        if (level.empty() || level.has_parent_path() || level == "." || level == ".." || level.has_root_name()) return {false, "The configured world is outside the server folder."};
        dest = root / level / "datapacks";
    } else return {false, "Select a plugin or mod JAR, or a ZIP with a data/ tree and pack.mcmeta."};
    if (!inside(root, dest)) return {false, "The content directory resolves outside the server folder."};
    fs::create_directories(dest, ec);
    if (ec) return {false, ec.message()};
    const auto final = dest / src.filename();
    if (fs::exists(final, ec)) return {false, "A package already uses this filename. No files were replaced."};
    const auto stage = dest / (".package-" + std::to_string(std::random_device{}()));
    if (!fs::create_directory(stage, ec)) return {false, "Could not create a private staging directory."};
    const auto copy = stage / "content";
    std::string error;
    if (!fs::copy_file(src, copy, fs::copy_options::none, ec)) error = ec.message();
    else {
        if (hash_file(copy) != original_hash) error = "The copied package failed readback.";
        else {
            // Link publication is all-or-nothing even when another installer uses the same name.
            fs::create_hard_link(copy, final, ec);
            if (ec) error = ec.message();
        }
    }
    fs::remove(copy, ec); fs::remove(stage, ec);
    return error.empty() ? ContentInstallResult{true, final.u8string()} : ContentInstallResult{false, error};
}
}}}
