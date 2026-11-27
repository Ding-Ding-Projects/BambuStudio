#include "BedrockLifecycle.hpp"
#include "BedrockPack.hpp"
#include "ServerSystem.hpp"
#include <algorithm>
#include <random>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
fs::path slot(const fs::path &root, const InstalledJavaPackage &item, const std::string &state)
{
    auto base = state == "enabled" ? root : root / (state == "disabled" ? ".bambustudio-disabled" : ".bambustudio-removed");
    return base / fs::u8path(item.subdir) / fs::u8path(item.name);
}
bool inside(const fs::path &root, const fs::path &dest)
{
    std::error_code ec;
    auto            p = fs::weakly_canonical(dest, ec);
    if (ec) return false;
    auto rel = p.lexically_relative(root);
    return !rel.empty() && !rel.is_absolute() && *rel.begin() != ".." && rel != ".";
}
BedrockPack descriptor(const fs::path &where)
{
    std::error_code ec;
    if (fs::is_symlink(where, ec) || fs::is_symlink(where / "manifest.json", ec)) return {};
    std::string raw;
    return read_file((where / "manifest.json").u8string(), raw, 1024 * 1024) ? bedrock_pack(raw) : BedrockPack{};
}
struct Edit
{
    fs::path    path;
    std::string before, after;
    bool        existed = false;
};
bool matches(const Edit &e)
{
    std::error_code ec;
    auto            exists = fs::exists(e.path, ec);
    std::string     raw;
    return !ec && exists == e.existed && (!exists || (read_file(e.path.u8string(), raw, 1024 * 1024) && raw == e.before));
}
bool apply(std::vector<Edit> &edits, std::string &error)
{
    std::size_t done = 0;
    for (; done < edits.size(); ++done) {
        const auto &e = edits[done];
        if (!matches(e)) {
            error = "A world pack registration changed during this operation.";
            break;
        }
        if (!write_file_atomic(e.path.u8string(), e.after, &error)) break;
    }
    if (done == edits.size()) return true;
    for (std::size_t i = done; i > 0; --i) {
        const auto &e = edits[i - 1];
        std::string actual;
        if (!read_file(e.path.u8string(), actual, 1024 * 1024) || actual != e.after) continue;
        if (e.existed)
            write_file_atomic(e.path.u8string(), e.before);
        else {
            std::error_code ignore;
            fs::remove(e.path, ignore);
        }
    }
    return false;
}
} // namespace
std::vector<InstalledJavaPackage> list_local_bedrock_content(const fs::path &dir)
{
    std::error_code                   ec;
    auto                              root = fs::canonical(dir, ec);
    std::vector<InstalledJavaPackage> out;
    if (ec) return out;
    for (const std::string group : {"behavior_packs", "resource_packs"})
        for (const std::string state : {"enabled", "disabled", "removed"}) {
            auto parent = slot(root, {group, "_", state, {}}, state).parent_path();
            if (!inside(root, parent) || !fs::is_directory(parent, ec)) continue;
            for (fs::directory_iterator it(parent, ec), end; !ec && it != end; it.increment(ec)) {
                if (out.size() >= 1000) return out;
                const auto name = it->path().filename().u8string();
                const auto p    = descriptor(it->path());
                if (p.id.empty() || p.id != name || p.group + "_packs" != group) continue;
                const auto fp = fs::last_write_time(it->path() / "manifest.json", ec).time_since_epoch().count();
                if (ec) break;
                out.push_back({group, name, state, std::to_string(fp) + "/" + p.version.dump()});
            }
            ec.clear();
        }
    return out;
}
std::string change_local_bedrock_content(const fs::path &dir, const InstalledJavaPackage &item, const std::string &action, const fs::path &replacement)
{
#ifndef _WIN32
    return "Local Bedrock pack maintenance requires Windows database-lock support.";
#else
    std::error_code ec;
    auto            root = fs::canonical(dir, ec);
    if (ec || !fs::is_regular_file(root / "bedrock_server.exe", ec)) return "The Bedrock installation is unavailable.";
    auto inventory = list_local_bedrock_content(root);
    if (std::none_of(inventory.begin(), inventory.end(),
                     [&](const auto &e) { return e.name == item.name && e.subdir == item.subdir && e.state == item.state && e.digest == item.digest; }))
        return "The selected pack changed; refresh the inventory.";
    const auto src   = slot(root, item, item.state);
    const auto old   = descriptor(src);
    const auto shelf = root / ".bambustudio-pack-history" / old.id;
    if (!inside(root, src) || !inside(root, shelf)) return "Invalid pack location.";
    fs::create_directories(shelf, ec);
    if (ec) return ec.message();
    auto lock = shelf / "operation.lock";
    if (!fs::create_directory(lock, ec)) return "Another operation owns this pack.";
    struct Unlock
    {
        fs::path dir;
        ~Unlock()
        {
            std::error_code e;
            fs::remove_all(dir, e);
        }
    } unlock{lock};
    auto        active_file = shelf / "active-worlds.json";
    auto        previous    = shelf / "previous";
    std::string prior_active;
    auto        activate = nlohmann::json::array();
    if (read_file(active_file.u8string(), prior_active, 1024 * 1024)) {
        activate = nlohmann::json::parse(prior_active, nullptr, false);
        if (!activate.is_array()) return "The saved activation inventory is unreadable.";
    }
    const bool  version_change = action == "update" || action == "rollback";
    std::string next           = item.state;
    if (action == "enable")
        next = "enabled";
    else if (action == "disable")
        next = "disabled";
    else if (action == "remove")
        next = "removed";
    else if (!version_change)
        return "Unsupported pack action.";
    if (!version_change && next == item.state) return "The package already has that state.";
    if (version_change && item.state == "removed") return "Restore the removed pack before replacing it.";
    auto        to = slot(root, item, next);
    fs::path    incoming;
    BedrockPack desired = old;
    if (version_change) {
        if (action == "update") {
            incoming = lock / "new";
            auto zip = extract_zip(replacement.u8string(), incoming.u8string(), nullptr, 1024ULL * 1024 * 1024);
            if (!zip.ok) return zip.error;
        } else {
            std::string name;
            if (!read_file(previous.u8string(), name, 80) || name.rfind("revision-", 0) != 0 ||
                name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
                return "There is no verified previous pack version.";
            incoming = shelf / name / "package";
        }
        desired = descriptor(incoming);
        if (desired.id != old.id || desired.group != old.group) return "The replacement pack UUID or module group does not match.";
    } else {
        if (!inside(root, to) || fs::exists(to, ec)) return "A destination pack is already present.";
        fs::create_directories(to.parent_path(), ec);
        if (ec) return ec.message();
    }
    struct Locks
    {
        std::vector<HANDLE> files;
        ~Locks()
        {
            for (auto f : files) CloseHandle(f);
        }
    } held;
    std::vector<Edit> edits;
    nlohmann::json    was_active = nlohmann::json::array();
    auto              worlds     = root / "worlds";
    if (fs::is_directory(worlds, ec)) {
        for (fs::directory_iterator it(worlds, ec), end; !ec && it != end; it.increment(ec)) {
            const auto world = it->path();
            if (it->is_symlink(ec)) return "Linked worlds require separate review.";
            if (!fs::is_regular_file(world / "level.dat", ec)) {
                ec.clear();
                continue;
            }
            auto db = world / "db" / "LOCK";
            if (fs::exists(db, ec)) {
                auto f = CreateFileW(db.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (f == INVALID_HANDLE_VALUE) return "A Bedrock world is open. Stop it before changing packs.";
                held.files.push_back(f);
            }
            const auto world_name = world.filename().u8string();
            auto       reg        = world / ("world_" + old.group + "_packs.json");
            if (!inside(root, reg) || fs::is_symlink(reg, ec)) return "Invalid world pack-list path.";
            Edit edit{reg, {}, {}, fs::exists(reg, ec)};
            if (edit.existed && !read_file(reg.u8string(), edit.before, 1024 * 1024)) return "Could not read a world pack list.";
            auto entries = edit.existed ? nlohmann::json::parse(edit.before, nullptr, false) : nlohmann::json::array();
            if (!entries.is_array()) return "An existing world pack list is malformed.";
            bool existed = false;
            auto result  = nlohmann::json::array();
            for (auto e : entries) {
                if (!e.is_object() || !e.contains("pack_id") || !e["pack_id"].is_string()) return "An existing pack registration is malformed.";
                auto id = e["pack_id"].get<std::string>();
                std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                if (id != old.id) {
                    result.push_back(std::move(e));
                    continue;
                }
                existed = true;
                if (next == "enabled") {
                    e["version"] = desired.version;
                    result.push_back(std::move(e));
                }
            }
            if (existed) was_active.push_back(world_name);
            if (!existed && action == "enable" && std::find(activate.begin(), activate.end(), world_name) != activate.end())
                result.push_back({{"pack_id", old.id}, {"version", desired.version}});
            if (result != entries) {
                edit.after = result.dump(2) + "\n";
                edits.push_back(std::move(edit));
            }
        }
        if (ec) return "Could not enumerate every affected world.";
    }
    std::string error;
    if (!version_change) {
        if (item.state == "enabled" && !write_file_atomic(active_file.u8string(), was_active.dump(), &error)) return error;
        fs::rename(src, to, ec);
        if (ec) return ec.message();
        if (!apply(edits, error)) {
            fs::rename(to, src, ec);
            return error;
        }
    } else {
        auto revision = shelf / ("revision-" + std::to_string(std::random_device{}()));
        if (!fs::create_directory(revision, ec)) return "Could not reserve a pack recovery directory.";
        auto before = revision / "package";
        fs::rename(src, before, ec);
        if (ec) return ec.message();
        fs::rename(incoming, src, ec);
        if (ec) {
            fs::rename(before, src, ec);
            return "Could not publish the replacement; previous pack was retained.";
        }
        std::string earlier;
        bool        had = read_file(previous.u8string(), earlier, 80);
        if (!write_file_atomic(previous.u8string(), revision.filename().u8string(), &error) || !apply(edits, error)) {
            fs::rename(src, incoming, ec);
            fs::rename(before, src, ec);
            if (had)
                write_file_atomic(previous.u8string(), earlier);
            else
                fs::remove(previous, ec);
            return error;
        }
    }
    return {};
#endif
}
}}} // namespace Slic3r::GUI::GameServers
