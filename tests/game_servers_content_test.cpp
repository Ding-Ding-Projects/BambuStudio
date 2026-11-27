#include "../src/slic3r/GUI/GameServers/ContentInstaller.hpp"
#include "../src/slic3r/GUI/GameServers/ContentCatalog.hpp"
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
