#include "LocalJavaInstall.hpp"
#include "libslic3r/miniz_extension.hpp"
#include <algorithm>
#include <fstream>
namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
std::string read_meta(mz_zip_archive &zip, const char *name)
{
    int                      idx = mz_zip_reader_locate_file(&zip, name, nullptr, 0);
    mz_zip_archive_file_stat stat{};
    if (idx < 0 || !mz_zip_reader_file_stat(&zip, mz_uint(idx), &stat) || !stat.m_uncomp_size || stat.m_uncomp_size > 1024 * 1024) return {};
    std::string bytes(std::size_t(stat.m_uncomp_size), '\0');
    return mz_zip_reader_extract_to_mem(&zip, mz_uint(idx), bytes.data(), bytes.size(), 0) ? bytes : std::string();
}
std::string digest(const fs::path &path)
{
    Sha1          h;
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    char buf[65536];
    while (in) {
        in.read(buf, sizeof(buf));
        h.update(buf, std::size_t(in.gcount()));
    }
    return in.eof() ? h.hex() : std::string();
}
} // namespace
bool copy_runnable_server(const fs::path &source, const fs::path &destination, JarInfo &info, std::string &error)
{
    std::error_code ec;
    if (fs::file_size(source, ec) > 1024ULL * 1024 * 1024 || ec) {
        error = "The source server JAR is unavailable or too large.";
        return false;
    }
    const auto sum = digest(source);
    if (sum.empty() || !fs::copy_file(source, destination, fs::copy_options::none, ec) || digest(destination) != sum) {
        error = "The server JAR could not be copied and verified.";
        return false;
    }
    mz_zip_archive zip{};
    if (!open_zip_reader(&zip, destination.u8string())) {
        error = "The server JAR is unreadable.";
        return false;
    }
    const auto manifest = read_meta(zip, "META-INF/MANIFEST.MF");
    const auto version  = read_meta(zip, "version.json");
    close_zip_reader(&zip);
    if (manifest.find("Main-Class: ") == std::string::npos) {
        error = "Choose a runnable server JAR containing a Main-Class manifest entry.";
        return false;
    }
    std::string filename = source.filename().u8string();
    std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (filename.size()<5 || filename.substr(filename.size()-4)!=".jar") {error="Choose a server JAR.";return false;}
    if (filename.find("installer") != std::string::npos) {
        error = "This is an installer; choose its runnable server distribution instead.";
        return false;
    }
    info = identify_jar(source.filename().u8string(), version, manifest);
    return true;
}
bool publish_server_setup(const fs::path &stage, const fs::path &target, const std::string &jar, std::string &error)
{
    if (fs::u8path(jar).filename() != fs::u8path(jar) || jar.empty()) {
        error = "Invalid server filename.";
        return false;
    }
    std::error_code ec;
    const bool      created = fs::create_directory(target, ec);
    if (ec || !fs::is_directory(target, ec) || fs::is_symlink(target, ec) || !fs::is_empty(target, ec) || ec) {
        error = "Choose a new or empty server directory.";
        return false;
    }
    std::vector<fs::path> added;
    for (const auto &name : {std::string("eula.txt"), std::string("server.properties"), jar}) {
        auto src = stage / fs::u8path(name), dest = target / fs::u8path(name);
        fs::create_hard_link(src, dest, ec);
        if (ec) {
            error = "Could not publish the staged server files without replacing an existing file: " + ec.message();
            for (const auto &path : added)
                if (fs::equivalent(stage / path.filename(), path, ec)) fs::remove(path, ec);
            if (created) fs::remove(target, ec);
            return false;
        }
        added.push_back(dest);
    }
    return true;
}
}}} // namespace Slic3r::GUI::GameServers
