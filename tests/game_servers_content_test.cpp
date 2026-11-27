#include "../src/slic3r/GUI/GameServers/ContentInstaller.hpp"
#include "../src/slic3r/GUI/GameServers/ContentCatalog.hpp"
#include "../src/slic3r/GUI/GameServers/WorldJarData.hpp"
#include "../src/slic3r/GUI/GameServers/BedrockPack.hpp"
#include "../src/slic3r/GUI/GameServers/BedrockSettings.hpp"
#include "../src/slic3r/GUI/GameServers/BedrockDistributions.hpp"
#include "../src/slic3r/GUI/GameServers/BedrockHistory.hpp"
#include "../src/slic3r/GUI/GameServers/LocalJavaInstall.hpp"
#include "../src/slic3r/GUI/GameServers/ContentLifecycle.hpp"
#include "../src/slic3r/GUI/GameServers/LocalBedrockRecovery.hpp"
#include "../src/slic3r/GUI/GameServers/ServerSystem.hpp"
#include "libslic3r/miniz_extension.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace Slic3r::GUI::GameServers;
namespace fs = std::filesystem;

// Only the file-system world lock and GUI-side server period are substituted.
// The production installer, ZIP parser, filesystem operations and hashing run unchanged.
namespace Slic3r {
std::string MZ_Archive::get_errorstr(mz_zip_error err){return mz_zip_get_error_string(err);}
bool open_zip_reader(mz_zip_archive *zip, const std::string &path) { return mz_zip_reader_init_file(zip, path.c_str(), 0) != 0; }
bool open_zip_writer(mz_zip_archive *zip,const std::string &path){return mz_zip_writer_init_file(zip,path.c_str(),0)!=0;}
bool close_zip_writer(mz_zip_archive *zip){return mz_zip_writer_end(zip)!=0;}
bool close_zip_reader(mz_zip_archive *zip) { return mz_zip_reader_end(zip) != 0; }
namespace GUI { namespace GameServers {
bool read_file(const std::string &name,std::string &out,std::size_t limit){
    std::ifstream f(fs::u8path(name),std::ios::binary);if(!f || fs::file_size(fs::u8path(name))>limit)return false;
    out.assign(std::istreambuf_iterator<char>(f),{});return true;
}
bool write_file_atomic(const std::string &name,const std::string &data,std::string *){
    std::ofstream f(fs::u8path(name),std::ios::binary);f.write(data.data(),std::streamsize(data.size()));return bool(f);
}
ServerFolder inspect_folder(const std::string &dir)
{
    ServerFolder result;
    result.has_properties = fs::exists(fs::u8path(dir) / "server.properties");
    result.world_locked = fs::exists(fs::u8path(dir) / "locked");
    result.level_name = "world";
    return result;
}
}}}

static void check(bool value, const char *reason)
{
    if (!value) throw std::runtime_error(reason);
}

static void package(const fs::path &path, std::initializer_list<std::pair<std::string, std::string>> entries)
{
    mz_zip_archive zip{};
    check(mz_zip_writer_init_file(&zip, path.string().c_str(), 0) != 0, "open fixture archive");
    for (const auto &entry : entries)
        check(mz_zip_writer_add_mem(&zip, entry.first.c_str(), entry.second.data(), entry.second.size(), MZ_BEST_SPEED) != 0, "add archive member");
    check(mz_zip_writer_finalize_archive(&zip) && mz_zip_writer_end(&zip), "finish archive");
}

int main(int argc, char **argv) try
{
    if (argc != 2) return 2;
    const fs::path dir = fs::u8path(argv[1]), root = dir / "server";
    fs::create_directory(root);
    std::ofstream(root / "server.properties") << "level-name=world\n";
    const auto fixture = dir / "version.jar";
    package(fixture, {
        {"version.json",R"({"id":"1.21.4","pack_version":{"data":61}})"},
        {"data/minecraft/worldgen/biome/plains.json","{}"},
        {"data/minecraft/worldgen/structure_set/villages.json","{}"},
        {"data/minecraft/worldgen/world_preset/normal.json",R"({"dimensions":{"minecraft:overworld":{"type":"minecraft:overworld","generator":{"type":"minecraft:noise","settings":"minecraft:overworld"}},"minecraft:the_nether":{"keep":true}}})"},
        {"data/minecraft/worldgen/noise_settings/overworld.json",R"({"sea_level":63,"noise":{"min_y":-64,"height":384},"noise_router":{"keep":true},"aquifers_enabled":true,"ore_veins_enabled":true})"},
        {"data/minecraft/dimension_type/overworld.json",R"({"min_y":-64,"height":384,"logical_height":384})"}});
    const auto jar=read_world_jar(fixture.u8string());
    check(jar.error.empty() && jar.pack_format==61 && jar.biomes.size()==1 && jar.structure_sets.size()==1, "versioned metadata extraction");
    VerticalWorld options;
    options.sea=12;options.min_y=-96;options.height=320;options.ore_veins=false;
    check(write_vertical_pack(jar,options,dir/"custom-world").empty(),"write generated data pack");
    const auto output=dir/"custom-world"/"datapacks"/"bambustudio-generated";
    const auto noise=nlohmann::json::parse(std::ifstream(output/"data"/"bambustudio"/"worldgen"/"noise_settings"/"configured.json"));
    check(noise["noise"]["min_y"]==-96 && noise["sea_level"]==12 && !noise["ore_veins_enabled"].get<bool>() && noise["noise_router"]["keep"]==true,"noise copy and overrides");
    check(!write_vertical_pack(jar,options,dir/"custom-world").empty(),"overwrite of existing data pack");
    options.min_y=-95;check(!write_vertical_pack(jar,options,dir/"invalid-world").empty(),"unaligned dimension");
    if(const char *official=std::getenv("BAMBU_TEST_OFFICIAL_SERVER_JAR")){
        auto actual=read_world_jar(official);
        check(actual.version=="1.21.4" && actual.pack_format==61 && actual.biomes.size()==65 && actual.structure_sets.size()==20,"official 1.21.4 metadata");
        options.min_y=-96;check(write_vertical_pack(actual,options,dir/"official-world").empty(),"official template pack output");
        std::cout<<"Official 1.21.4 nested JAR: 65 biomes, 20 structure sets, pack format 61 and custom overworld pack verified.\n";
    }
    std::string bds="server-port=19132\r\n# retained\r\nscript-debugger-passcode=fixture\r\nfuture-key=private\r\ndifficulty=easy\r\ndifficulty=normal\r\n";
    const auto settings=read_bedrock_settings(bds);
    check(settings.size()==4 && settings[1].value.empty() && settings[2].value.empty(),"protected or unknown BDS value exposed");
    check(!change_bedrock_setting(bds,"difficulty","hard") && !change_bedrock_setting(bds,"server-port","99999"),"unsafe Bedrock setting accepted");
    check(change_bedrock_setting(bds,"server-port","19140") && bds.find("19140\r\n# retained")!=std::string::npos,"Bedrock line-ending preservation");
    const auto links=bedrock_distributions(R"({"result":{"links":[{"downloadType":"serverBedrockWindows","downloadUrl":"https://www.minecraft.net/bedrockdedicatedserver/bin-win/bedrock-server-1.26.66.1.zip"},{"downloadType":"serverBedrockLinux","downloadUrl":"https://example.org/tool.zip"}]}})");
    check(links.size()==1 && links[0].windows && links[0].version=="1.26.66.1","Bedrock download origin and version selection");
    auto observed=merge_bedrock_history({},links,100);
    auto other=links.front();other.version="1.26.68.1";other.file="bedrock-server-1.26.68.1.zip";other.url="https://www.minecraft.net/bedrockdedicatedserver/bin-win/"+other.file;
    observed=merge_bedrock_history(observed,{other},200);
    auto historical=read_bedrock_history(write_bedrock_history(observed));
    check(historical.size()==2 && historical[0].current && !historical[1].current && historical[1].last==100,"observed Bedrock history lost previous source");
    const auto flavor=dir/"paper-1.21.4-99.jar", staged=dir/"setup-stage", fresh=dir/"new-server";
    package(flavor,{{"META-INF/MANIFEST.MF","Manifest-Version: 1.0\nMain-Class: io.papermc.paperclip.Paperclip\n"},{"version.json",R"({"id":"1.21.4","java_version":21})"}});
    fs::create_directory(staged);JarInfo inspected;std::string why;
    check(copy_runnable_server(flavor,staged/flavor.filename(),inspected,why) && inspected.flavour==Flavour::Paper,"runnable Paper import");
    std::ofstream(staged/"server.properties")<<"server-port=25565\n";std::ofstream(staged/"eula.txt")<<"eula=false\n";
    check(publish_server_setup(staged,fresh,flavor.filename().string(),why) && fs::exists(fresh/flavor.filename()),"new local server staging");
    check(!publish_server_setup(staged,fresh,flavor.filename().string(),why) && fs::exists(fresh/"eula.txt"),"existing installation overwritten");
#ifdef _WIN32
    const auto bedrock_dir=dir/"bedrock-local",archive=dir/"bedrock-backup.zip",restore=dir/"bedrock-restore";
    fs::create_directories(bedrock_dir/"worlds"/"Example"/"db");std::ofstream(bedrock_dir/"bedrock_server.exe")<<"inert fixture";
    std::ofstream(bedrock_dir/"worlds"/"Example"/"level.dat")<<"test metadata";std::ofstream(bedrock_dir/"worlds"/"Example"/"db"/"LOCK");
    std::ofstream(bedrock_dir/"server.properties",std::ios::binary)<<"server-name=example\nscript-debugger-passcode=disposable-fixture\n";
    {const auto snapshot=backup_local_bedrock(bedrock_dir.u8string(),archive.u8string());if(!snapshot.ok)throw std::runtime_error("native Bedrock snapshot: "+snapshot.error);}
    check(local_bedrock_archive(archive.u8string(),bedrock_dir.u8string()) && !local_bedrock_archive(archive.u8string(),root.u8string()),"native Bedrock owner marker");
    check(extract_zip(archive.u8string(),restore.u8string()).ok && !extract_zip(archive.u8string(),restore.u8string()).ok,"native verified separate restore");
    std::string restored;check(read_file((restore/"server.properties").u8string(),restored,1024)&&restored=="server-name=example\n","restore protected properties exclusion");
    check(fs::exists(restore/"worlds"/"Example"/"level.dat") && !fs::exists(restore/"worlds"/"Example"/"db"/"LOCK"),"restored world files and excluded live lock");
#endif
    const auto pack_info = R"({"header":{"uuid":"ec9410a2-de16-4f98-8eec-9efa1319db18","version":[1,0,4]},"modules":[{"type":"data"},{"type":"script"}]})";
    check(bedrock_pack(pack_info).group == "behavior", "valid Bedrock behavior pack was rejected");
    check(bedrock_pack("{}").id.empty(), "missing Bedrock manifest accepted");
    const auto projects = catalog_projects(R"({"hits":[{"project_id":"abc123","title":"Fixture"},{"project_id":"../bad","title":"bad"}]})");
    check(projects.size() == 1, "invalid project id passed");
    auto release = nlohmann::json::parse(R"([{"loaders":["paper"],"game_versions":["1.21.4"],"files":[{"filename":"plugin.jar","size":22,"url":"https://cdn.modrinth.com/data/p/v/plugin.jar","hashes":{"sha1":"0123456789012345678901234567890123456789"}}]}])");
    check(catalog_files(release.dump(), "1.21.4", catalog_loaders("Paper")).size() == 1, "valid catalog package missing");
    check(catalog_files(release.dump(), "1.21.4", catalog_loaders("Fabric")).empty(), "unmatched loader passed");
    check(catalog_files(release.dump(), "1.21.5", catalog_loaders("Paper")).empty(), "unmatched version passed");
    release[0]["files"][0]["url"]="https://example.org/example.jar";
    check(catalog_files(release.dump(), "1.21.4", catalog_loaders("Paper")).empty(), "untrusted download origin passed");
    const auto plugin = dir / "test-plugin.jar", pack = dir / "pack.zip", mod = dir / "mod.jar";
    package(plugin, {{"plugin.yml", "name: fixture\n"}});
    package(mod, {{"fabric.mod.json", "{}"}});
    package(pack, {{"pack.mcmeta", "{\"pack\":{\"pack_format\":48,\"description\":\"fixture\"}}"}, {"data/demo/tags/a.json", "{}"}});
    check(!install_java_content(root.u8string(), "Vanilla", plugin.u8string()).ok, "wrong loader accepted");
    check(install_java_content(root.u8string(), "Paper", plugin.u8string()).ok, "plugin install");
    check(!install_java_content(root.u8string(), "Paper", plugin.u8string()).ok, "duplicate accepted");
    check(install_java_content(root.u8string(), "Fabric", mod.u8string()).ok, "mod install");
    check(install_java_content(root.u8string(), "Vanilla", pack.u8string()).ok, "pack install");
    check(fs::exists(root / "plugins" / plugin.filename()), "plugin destination");
    check(fs::exists(root / "mods" / mod.filename()), "mod destination");
    check(fs::exists(root / "world" / "datapacks" / pack.filename()), "pack destination");
    auto selected_package=[&]{
        for(auto e:list_installed_java_content(root))if(e.name==plugin.filename().string())return e;
        throw std::runtime_error("package missing from active or recovery inventory");
    };
    const auto next=dir/"updated.jar";
    package(next,{{"plugin.yml","name: fixture\nversion: 2\n"}});
    check(change_installed_java_content(root,"Paper",selected_package(),"disable").empty(),"disable package");
    check(selected_package().state=="disabled" && !fs::exists(root/"plugins"/plugin.filename()),"disabled package still active");
    check(change_installed_java_content(root,"Paper",selected_package(),"update",next).empty(),"update disabled package");
    check(change_installed_java_content(root,"Paper",selected_package(),"rollback").empty(),"restore prior version");
    check(change_installed_java_content(root,"Paper",selected_package(),"remove").empty() && selected_package().state=="removed","remove package with recovery");
    check(change_installed_java_content(root,"Paper",selected_package(),"enable").empty() && selected_package().state=="enabled","restore removed package");
    std::ofstream(root / "locked") << "yes";
    check(!install_java_content(root.u8string(), "Fabric", mod.u8string()).ok, "running world accepted");
    std::cout << "Content installation: three real ZIP/JAR installs and six ownership, loader and destination checks passed.\n";
}

catch(const std::exception &e){std::cerr<<"Content fixture: "<<e.what()<<"\n";return 1;}
