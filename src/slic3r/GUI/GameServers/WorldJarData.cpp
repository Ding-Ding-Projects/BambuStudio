#include "WorldJarData.hpp"
#include "libslic3r/miniz_extension.hpp"
#include <algorithm>
#include <fstream>
#include <set>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs=std::filesystem;
namespace {
bool member(mz_zip_archive &zip,const std::string &name,std::string &out,std::size_t max)
{
    const int idx=mz_zip_reader_locate_file(&zip,name.c_str(),nullptr,MZ_ZIP_FLAG_CASE_SENSITIVE);
    mz_zip_archive_file_stat st{};
    if(idx<0 || !mz_zip_reader_file_stat(&zip,mz_uint(idx),&st) || !st.m_uncomp_size || st.m_uncomp_size>max) return false;
    out.resize(std::size_t(st.m_uncomp_size));
    if(mz_zip_reader_extract_to_mem(&zip,mz_uint(idx),out.data(),out.size(),0)) return true;
    out.clear();return false;
}

nlohmann::json json_member(mz_zip_archive &zip,const char *name)
{
    std::string raw;
    if(!member(zip,name,raw,2*1024*1024))return {};
    return nlohmann::json::parse(raw,nullptr,false);
}
void read_inner(mz_zip_archive &zip,WorldJarData &r)
{
    auto version=json_member(zip,"version.json");
    if(!version.is_object() || !version.contains("id") || !version["id"].is_string() || !version.contains("pack_version") ||
       !version["pack_version"].is_object() || !version["pack_version"].contains("data")) { r.error="This JAR has no supported data-pack version record.";return; }
    r.version=version["id"].get<std::string>();
    const auto &format=version["pack_version"]["data"];
    if(format.is_number_integer()) r.pack_format=format.get<int>();
    else if(format.is_object() && format.contains("major") && format["major"].is_number_integer()) {
        r.pack_format=format["major"].get<int>();r.minor_format=format.value("minor",0);
    }
    if(r.pack_format<=0 || r.minor_format<0){r.error="The JAR's data-pack version uses an unsupported format.";return;}
    const auto count=mz_zip_reader_get_num_files(&zip);
    if(count>250000){r.error="The JAR exceeds the registry inventory limit.";return;}
    std::set<std::string>biomes,sets;
    for(mz_uint i=0;i<count;++i) {
        mz_zip_archive_file_stat st{};
        if(!mz_zip_reader_file_stat(&zip,i,&st)){r.error="Could not read the JAR directory.";return;}
        const std::string path=st.m_filename;
        if(path.size()<6 || path.substr(path.size()-5)!=".json")continue;
        for(const auto &pair:{std::make_pair(std::string("data/minecraft/worldgen/biome/"),&biomes),
                             std::make_pair(std::string("data/minecraft/worldgen/structure_set/"),&sets)}) {
            if(path.rfind(pair.first,0)==0 && path.size()>pair.first.size()+5)
                pair.second->insert("minecraft:"+path.substr(pair.first.size(),path.size()-pair.first.size()-5));
        }
    }
    r.biomes.assign(biomes.begin(),biomes.end());r.structure_sets.assign(sets.begin(),sets.end());
    r.preset=json_member(zip,"data/minecraft/worldgen/world_preset/normal.json");
    r.dimension=json_member(zip,"data/minecraft/dimension_type/overworld.json");
    r.noise=json_member(zip,"data/minecraft/worldgen/noise_settings/overworld.json");
    if(!r.preset.is_object() || !r.dimension.is_object() || !r.noise.is_object() || r.biomes.empty())
        r.error="This JAR does not contain the expected vanilla overworld registries.";
}
bool json_file(const fs::path &p,const nlohmann::json &data)
{
    std::error_code ec;fs::create_directories(p.parent_path(),ec);
    if(ec)return false;
    std::ofstream f(p,std::ios::binary);f<<data.dump(2)<<"\n";f.close();return bool(f);
}
}

bool WorldJarData::matches(const std::string &path) const
{
    std::error_code ec;
    auto p=fs::u8path(path);
    if(!error.empty() || source!=path || fs::file_size(p,ec)!=size || ec)return false;
    return fs::last_write_time(p,ec)==modified && !ec;
}
WorldJarData read_world_jar(const std::string &jar)
{
    WorldJarData out;out.source=jar;
    std::error_code ec;const auto p=fs::u8path(jar);out.size=fs::file_size(p,ec);
    if(ec || out.size>1024ULL*1024*1024){out.error="The selected JAR is unavailable or too large.";return out;}
    out.modified=fs::last_write_time(p,ec);
    mz_zip_archive zip{};
    if(ec || !open_zip_reader(&zip,jar)){out.error="Could not open the selected server JAR.";return out;}
    try {
        if(mz_zip_reader_locate_file(&zip,"data/minecraft/worldgen/world_preset/normal.json",nullptr,0)>=0)read_inner(zip,out);
        else {
            std::string versions,nested;
            if(!member(zip,"META-INF/versions.list",versions,65536))out.error="This distribution does not expose a vanilla version bundle.";
            else {
                const auto first=versions.find('\t'),second=first==std::string::npos?first:versions.find('\t',first+1);
                if(second==std::string::npos)out.error="Malformed bundled version index.";
                else {
                    auto n=versions.substr(second+1);auto e=n.find_first_of("\r\n");if(e!=std::string::npos)n.resize(e);
                    if(n.empty() || n.find("..")!=std::string::npos || !member(zip,"META-INF/versions/"+n,nested,256*1024*1024))out.error="The versioned server archive is missing or oversized.";
                    else {
                        mz_zip_archive inner{};
                        if(mz_zip_reader_init_mem(&inner,nested.data(),nested.size(),0)){
                            try {read_inner(inner,out);} catch (...) {mz_zip_reader_end(&inner);throw;}
                            mz_zip_reader_end(&inner);
                        }else out.error="The inner server archive is unreadable.";
                    }
                }
            }
        }
    } catch(const std::exception &) {out.error="Invalid server metadata in the selected JAR.";}
    close_zip_reader(&zip);
    if(out.error.empty() && !out.matches(jar))out.error="The server JAR changed during extraction.";
    return out;
}

std::string write_vertical_pack(const WorldJarData &jar,const VerticalWorld &o,const fs::path &world)
{
    if(!jar.error.empty() || jar.pack_format<=0)return "Read the selected JAR's versioned metadata first.";
    if(o.min_y%16 || o.height%16 || o.min_y< -2032 || o.min_y>2016 || o.height<16 || o.height>4064 ||
       o.min_y+o.height>2032 || o.sea<o.min_y || o.sea>=o.min_y+o.height)return "Lowest Y and height must be multiples of 16 in range; sea level must be within the dimension.";
    auto preset=jar.preset,dim=jar.dimension,noise=jar.noise;
    if(!preset.contains("dimensions") || !preset["dimensions"].contains("minecraft:overworld") ||
       !preset["dimensions"]["minecraft:overworld"].contains("generator") || !noise.contains("noise") || !noise["noise"].is_object() ||
       !dim.contains("min_y") || !dim.contains("height") || !dim.contains("logical_height") ||
       !noise.contains("sea_level") || !noise.contains("aquifers_enabled") || !noise.contains("ore_veins_enabled"))
        return "The selected version's overworld format is unsupported.";
    auto &over=preset["dimensions"]["minecraft:overworld"];
    if(over.value("type",std::string())!="minecraft:overworld" || over["generator"].value("settings",std::string())!="minecraft:overworld")
        return "The selected version's normal preset uses an unsupported generator.";
    over["type"]="bambustudio:configured";over["generator"]["settings"]="bambustudio:configured";
    dim["min_y"]=o.min_y;dim["height"]=o.height;dim["logical_height"]=o.height;
    noise["noise"]["min_y"]=o.min_y;noise["noise"]["height"]=o.height;
    noise["sea_level"]=o.sea;noise["aquifers_enabled"]=o.aquifers;noise["ore_veins_enabled"]=o.ore_veins;
    nlohmann::json info={{"description","Configured overworld"}};
    if(jar.pack_format>=82){info["min_format"]={jar.pack_format,jar.minor_format};info["max_format"]={jar.pack_format,jar.minor_format};}
    else info["pack_format"]=jar.pack_format;
    const auto dir=world/"datapacks"/"bambustudio-generated";
    std::error_code ec;fs::create_directories(dir.parent_path(),ec);
    if(ec || !fs::create_directory(dir,ec))return "The generated pack directory already exists or cannot be created.";
    const auto data=dir/"data"/"bambustudio";
    if(!json_file(dir/"pack.mcmeta",{{"pack",info}}) || !json_file(data/"worldgen"/"world_preset"/"configured.json",preset) ||
       !json_file(data/"worldgen"/"noise_settings"/"configured.json",noise) || !json_file(data/"dimension_type"/"configured.json",dim)) {
        fs::remove_all(dir,ec);return "Could not write the generated pack.";
    }
    return {};
}
}}}
