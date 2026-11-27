#include "ContentLifecycle.hpp"
#include "ContentInstaller.hpp"
#include "ServerSystem.hpp"
#include <algorithm>
#include <fstream>
#include <random>
namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
std::string hash(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) return {};
    Sha1 h;
    char bytes[65536];
    while (in) {
        in.read(bytes, sizeof(bytes));
        h.update(bytes, std::size_t(in.gcount()));
    }
    return in.eof() ? h.hex() : std::string();
}
bool inside(const fs::path &base, const fs::path &p)
{
    std::error_code ec;
    auto            r = fs::weakly_canonical(p, ec);
    if (ec) return false;
    auto rel = r.lexically_relative(base);
    return !rel.empty() && !rel.is_absolute() && *rel.begin() != ".." && rel != ".";
}
std::vector<fs::path> groups(const fs::path &root)
{
    std::vector<fs::path> result = {"plugins", "mods"};
    std::string           text;
    if (read_file((root / "server.properties").u8string(), text, 1024 * 1024)) {
        auto w = fs::u8path(parse_properties(text).get("level-name").value_or("world"));
        if (!w.empty() && !w.has_root_name() && !w.has_parent_path() && w != "." && w != "..") result.push_back(w / "datapacks");
    }
    return result;
}
fs::path live(const fs::path &root, const InstalledJavaPackage &p, const std::string &state)
{
    if (state == "removed") return root / ".bambustudio-removed" / fs::u8path(p.subdir) / fs::u8path(p.name);
    return root / fs::u8path(p.subdir) / fs::u8path(p.name + (state == "disabled" ? ".disabled" : ""));
}
bool place_without_replacing(const fs::path &from, const fs::path &to, std::error_code &ec)
{
    fs::create_hard_link(from, to, ec);
    if (ec) return false;
    if (fs::remove(from, ec)) return true;
    std::error_code ignored;
    if (fs::equivalent(from, to, ignored)) fs::remove(to, ignored);
    return false;
}
} // namespace
std::vector<InstalledJavaPackage> list_installed_java_content(const fs::path &folder)
{
    std::error_code                   ec;
    const auto                        root = fs::canonical(folder, ec);
    std::vector<InstalledJavaPackage> result;
    if (ec) return result;
    for (const auto &g : groups(root))
        for (bool removed : {false, true}) {
            const auto dir = (removed ? root / ".bambustudio-removed" : root) / g;
            if (!inside(root, dir)) continue;
            for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
                if (result.size() >= 1000) return result;
                std::error_code e;
                if (!it->is_regular_file(e) || it->is_symlink(e) || it->file_size(e) > 1024ULL * 1024 * 1024) continue;
                auto name     = it->path().filename().u8string();
                bool disabled = !removed && name.size() > 9 && name.substr(name.size() - 9) == ".disabled";
                if (disabled) name.resize(name.size() - 9);
                auto ext = fs::u8path(name).extension().u8string();
                if (ext != (g.filename() == "datapacks" ? ".zip" : ".jar")) continue;
                auto modified = it->last_write_time(e);
                if (e) continue;
                const auto length = it->file_size(e);
                if (e) continue;
                result.push_back({g.generic_u8string(), name,
                                  removed  ? "removed" :
                                  disabled ? "disabled" :
                                             "enabled",
                                  std::to_string(modified.time_since_epoch().count()) + "/" + std::to_string(length)});
            }
            ec.clear();
        }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return std::tie(a.subdir, a.name, a.state) < std::tie(b.subdir, b.name, b.state); });
    return result;
}
std::string change_installed_java_content(const fs::path &folder, const std::string &flavour, const InstalledJavaPackage &p, const std::string &action, const fs::path &replacement)
{
    std::error_code ec;
    auto            root = fs::canonical(folder, ec);
    if (ec) return "Server directory is unavailable.";
    const auto valid = list_installed_java_content(root);
    if (std::none_of(valid.begin(), valid.end(), [&](const auto &v) { return v.name == p.name && v.subdir == p.subdir && v.state == p.state && v.digest == p.digest; }))
        return "The selected installed file changed. Refresh the list.";
    const auto state = inspect_folder(root.u8string());
    if (state.world_locked) return "Stop the server before changing content.";
    auto from  = live(root, p, p.state);
    auto store = root / ".bambustudio-content-history" / sha1_hex(p.subdir + "/" + p.name);
    if (!inside(root, from) || !inside(root, store)) return "The selected path resolves outside the server.";
    fs::create_directories(store, ec);
    if (ec) return ec.message();
    auto lock = store / "operation.lock";
    if (!fs::create_directory(lock, ec)) return "Another content operation is pending for this package.";
    struct Unlock
    {
        fs::path p;
        ~Unlock()
        {
            std::error_code ignore;
            fs::remove(p, ignore);
        }
    } unlock{lock};
    auto before = hash(from);
    if (before.empty()) return "Could not read the installed package.";
    const auto snapshot = store / (before + ".archive");
    if (!fs::exists(snapshot, ec) && !fs::copy_file(from, snapshot, fs::copy_options::none, ec)) return "Could not preserve the current package.";
    if (hash(snapshot) != before) return "The recovery copy did not verify.";
    if (action == "disable" || action == "enable" || action == "remove") {
        auto next = action == "disable" ? "disabled" : action == "enable" ? "enabled" : "removed";
        if (next == p.state) return "The package already has that state.";
        auto to = live(root, p, next);
        if (!inside(root, to)) return "The destination is outside the server.";
        fs::create_directories(to.parent_path(), ec);
        if (ec || !place_without_replacing(from, to, ec)) return "An existing destination or open file prevented the change.";
        return {};
    }
    if (action != "update" && action != "rollback") return "Unsupported content action.";
    if (p.state == "removed") return "Restore the removed package before changing its version.";
    fs::path candidate;
    auto     previous = store / "previous.sha1";
    if (action == "rollback") {
        std::string id;
        if (!read_file(previous.u8string(), id, 64) || id.size() != 40 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
            return "There is no verified previous version.";
        candidate = store / (id + ".archive");
        if (!inside(root, candidate) || hash(candidate) != id) return "The previous version failed verification.";
    } else {
        const auto expected = fs::u8path(p.subdir).filename().u8string();
        auto       failure  = validate_java_replacement(replacement, flavour, expected);
        if (!failure.empty()) return failure;
        candidate = replacement;
    }
    const auto temp = lock / "replacement";
    if (!fs::copy_file(candidate, temp, fs::copy_options::none, ec)) return "Could not stage the replacement.";
    struct Drop
    {
        fs::path p;
        ~Drop()
        {
            std::error_code ignored;
            fs::remove(p, ignored);
        }
    } drop{temp};
    const auto wanted = hash(candidate);
    if (wanted.empty() || hash(temp) != wanted || hash(from) != before) return "The installed or replacement file changed during staging.";
    if (wanted == before) return "The replacement is identical to the installed package.";
    // Save the last known-good identity before atomic publication, including when replacing a disabled file.
    std::string earlier;
    const bool  had_earlier = read_file(previous.u8string(), earlier, 64);
    std::string error;
    if (!write_file_atomic(previous.u8string(), before, &error)) return error;
    fs::rename(temp, from, ec);
    if (ec) {
        if (had_earlier)
            write_file_atomic(previous.u8string(), earlier);
        else
            fs::remove(previous, ec);
        return "The replacement could not be published. The previous file is preserved.";
    }
    if (hash(from) != wanted) return "Readback failed; the previous version remains in the recovery directory.";
    return {};
}
}}} // namespace Slic3r::GUI::GameServers
