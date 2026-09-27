#include "PreferencesHistory.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"
#include "libslic3r/Utils.hpp"

#include <chrono>
#include <memory>

#include <wx/timer.h>

namespace Slic3r { namespace GUI { namespace PreferencesHistory {

namespace {

constexpr int kDebounceMs = 2000; // burst of saves -> one snapshot

std::filesystem::path profiles_root()
{
    return std::filesystem::path(data_dir()).parent_path() / "BambuStudio-profiles";
}

std::filesystem::path conf_path()
{
    // The canonical location AppConfig itself writes to.
    return std::filesystem::path(AppConfig::config_path(AppConfig::EAppMode::Editor));
}

ProjectHistoryManager *shared_manager()
{
    static std::unique_ptr<ProjectHistoryManager> s_manager = []() -> std::unique_ptr<ProjectHistoryManager> {
        try {
            std::error_code ec;
            std::filesystem::create_directories(profiles_root(), ec);
            return std::make_unique<ProjectHistoryManager>(profiles_root());
        } catch (const std::exception &) {
            return nullptr;
        }
    }();
    return s_manager.get();
}

// Debounce timer: each save restarts it; on expiry one snapshot is queued.
class SnapshotTimer : public wxTimer
{
public:
    void Notify() override
    {
        ProjectHistoryManager *history = shared_manager();
        if (history == nullptr)
            return;
        std::error_code ec;
        if (!std::filesystem::exists(conf_path(), ec) || ec)
            return;
        // The engine only accepts .3mf snapshot files, so stage a copy under
        // a unique name (the worker reads it asynchronously; stale staging
        // copies are pruned on later ticks once they are safely committed).
        const std::filesystem::path staging_dir = profiles_root() / ".staging";
        std::filesystem::create_directories(staging_dir, ec);
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
        const std::filesystem::path staging =
            staging_dir / ("preferences-" + std::to_string(now) + ".3mf");
        std::filesystem::copy_file(conf_path(), staging,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
            return;
        for (auto it = std::filesystem::directory_iterator(staging_dir, ec);
             !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            const std::string name = it->path().filename().string();
            if (name.rfind("preferences-", 0) != 0 || it->path() == staging)
                continue;
            const auto written = std::filesystem::last_write_time(it->path(), ec);
            if (!ec && decltype(written)::clock::now() - written > std::chrono::minutes(5)) {
                std::error_code remove_ec;
                std::filesystem::remove(it->path(), remove_ec);
            }
        }
        ProjectHistoryCommitOptions options;
        options.message = "Preferences change";
        // Fire-and-forget: the engine serializes and dedupes on its worker;
        // the returned future is deliberately dropped (shutdown drains it).
        history->commit_snapshot(identity(), staging, options);
    }
};

SnapshotTimer *snapshot_timer()
{
    static SnapshotTimer *s_timer = new SnapshotTimer(); // app-lifetime
    return s_timer;
}

} // namespace

std::filesystem::path identity()
{
    // The engine only accepts .3mf-suffixed identity paths (it validates the
    // extension even though the identity is just a hashing key), hence the
    // odd-looking suffix.
    return profiles_root() / "preferences.history.3mf";
}

ProjectHistoryManager *manager()
{
    return shared_manager();
}

void install()
{
    AppConfig::set_save_observer([]() {
        // AppConfig::save() guarantees the main thread, so restarting the
        // debounce timer here is safe.
        snapshot_timer()->StartOnce(kDebounceMs);
    });
}

} } } // namespace Slic3r::GUI::PreferencesHistory
