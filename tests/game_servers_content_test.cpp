#include "../src/slic3r/GUI/GameServers/ContentInstaller.hpp"
#include "../src/slic3r/GUI/GameServers/ContentCatalog.hpp"
#include "../src/slic3r/GUI/GameServers/WorldJarData.hpp"
#include "../src/slic3r/GUI/GameServers/BedrockPack.hpp"
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
bool open_zip_reader(mz_zip_archive *zip, const std::string &path) { return mz_zip_reader_init_file(zip, path.c_str(), 0) != 0; }
bool close_zip_reader(mz_zip_archive *zip) { return mz_zip_reader_end(zip) != 0; }
namespace GUI { namespace GameServers {
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

int main(int argc, char **argv)
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
    std::ofstream(root / "locked") << "yes";
    check(!install_java_content(root.u8string(), "Fabric", mod.u8string()).ok, "running world accepted");
    std::cout << "Content installation: three real ZIP/JAR installs and six ownership, loader and destination checks passed.\n";
}
