#include "BedrockPack.hpp"
#include "ContentInstaller.hpp"
#include "ServerSystem.hpp"
#include "libslic3r/miniz_extension.hpp"
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
bool pack_inside(const fs::path &root, const fs::path &p)
{
    std::error_code ec;
    const auto dest=fs::weakly_canonical(p,ec);
    if (ec) return false;
    const auto rel=dest.lexically_relative(root);
    return !rel.empty() && !rel.is_absolute() && *rel.begin()!=".." && rel!=".";
}

std::string pack_manifest(const fs::path &archive)
{
    mz_zip_archive zip{};
    if (!open_zip_reader(&zip,archive.u8string())) return {};
    mz_zip_archive_file_stat stat{};
    const int idx=mz_zip_reader_locate_file(&zip,"manifest.json",nullptr,MZ_ZIP_FLAG_CASE_SENSITIVE);
    std::string text;
    if (idx>=0 && mz_zip_reader_file_stat(&zip,mz_uint(idx),&stat) && stat.m_uncomp_size>0 && stat.m_uncomp_size<=1024*1024) {
        text.resize(std::size_t(stat.m_uncomp_size));
        if (!mz_zip_reader_extract_to_mem(&zip,mz_uint(idx),text.data(),text.size(),0)) text.clear();
    }
    close_zip_reader(&zip);
    return text;
}
}

ContentInstallResult install_bedrock_content(const std::string &folder,const std::string &archive)
{
    std::error_code ec;
    const auto root=fs::canonical(fs::u8path(folder),ec);
    if (ec || !fs::is_regular_file(root/"bedrock_server.exe",ec) || !fs::is_regular_file(root/"server.properties",ec))
        return {false,"Select a local Bedrock server directory."};
    const auto file=fs::u8path(archive);
    const auto meta=bedrock_pack(pack_manifest(file));
    if (meta.id.empty()) return {false,"Select a .mcpack containing a single resource or behavior pack with a valid manifest."};
    const auto dest=root/(meta.group+"_packs")/meta.id;
    if (!pack_inside(root,dest)) return {false,"The pack destination resolves outside the server directory."};
    const auto extracted=extract_zip(archive,dest.u8string(),nullptr,std::uint64_t(1024)*1024*1024);
    if (!extracted.ok) return {false,extracted.error};
    return {true,dest.u8string()};
}

ContentInstallResult activate_bedrock_content(const std::string &folder,const std::string &manifest)
{
    std::error_code ec;
    const auto root=fs::canonical(fs::u8path(folder),ec);
    if (ec || !fs::is_regular_file(root/"bedrock_server.exe",ec)) return {false,"Select the Bedrock server directory first."};
    const auto path=fs::canonical(fs::u8path(manifest),ec);
    std::string raw;
    if (ec || !pack_inside(root,path) || path.filename()!="manifest.json" || !read_file(path.u8string(),raw,1024*1024))
        return {false,"Select manifest.json from a pack installed in this server."};
    const auto pack=bedrock_pack(raw);
    if (pack.id.empty() || path.parent_path().parent_path()!=root/(pack.group+"_packs")) return {false,"The manifest is outside its declared pack group."};
    if (!read_file((root/"server.properties").u8string(),raw,1024*1024)) return {false,"Could not read server.properties."};
    const auto level=parse_properties(raw).get("level-name");
    if (!level || level->empty()) return {false,"The server does not have a configured world."};
    const auto name=fs::u8path(*level);
    if (name.has_parent_path() || name.has_root_name() || name=="." || name=="..") return {false,"The configured world name is not a directory name."};
    const auto world=root/"worlds"/name;
    if (!pack_inside(root,world) || !fs::is_regular_file(world/"level.dat",ec)) return {false,"Start the world once, then stop it before enabling a pack."};
    const auto list=world/("world_"+pack.group+"_packs.json");
    if (!pack_inside(root,list)) return {false,"The pack list is outside the server."};
#ifdef _WIN32
    const auto level_lock=world/"db"/"LOCK";
    HANDLE db=CreateFileW(level_lock.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (db==INVALID_HANDLE_VALUE) return {false,"The world database is open or its lock cannot be verified. Stop the server first."};
    struct CloseLock { HANDLE h; ~CloseLock(){ CloseHandle(h); } } hold{db};
#else
    return {false,"Local Bedrock database ownership cannot be verified on this host."};
#endif
    nlohmann::json entries=nlohmann::json::array();
    if (fs::exists(list,ec)) {
        if (!read_file(list.u8string(),raw,1024*1024)) return {false,"Could not read the existing world pack list."};
        entries=nlohmann::json::parse(raw,nullptr,false);
        if (!entries.is_array()) return {false,"The existing pack list is malformed; it was preserved."};
    }
    for (const auto &e : entries) {
        if (!e.is_object() || !e.contains("pack_id") || !e["pack_id"].is_string()) return {false,"The existing pack list contains an invalid entry."};
        std::string id=e["pack_id"].get<std::string>();
        std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c){return char(std::tolower(c));});
        if (id==pack.id) return {false,"This pack ID is already listed. Its installed version was preserved."};
    }
    entries.push_back({{"pack_id",pack.id},{"version",pack.version}});
    std::string error;
    if (!write_file_atomic(list.u8string(),entries.dump(2)+"\n",&error)) return {false,error};
    return {true,list.u8string()};
}
}}}
