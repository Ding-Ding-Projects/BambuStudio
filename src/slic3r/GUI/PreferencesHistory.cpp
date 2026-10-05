#include "PreferencesHistory.hpp"
#include "GUI_App.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"
#include "libslic3r/Utils.hpp"

#include <chrono>
#include <memory>
#include <fstream>
#include <set>
#include <atomic>

#include <wx/timer.h>

namespace Slic3r { namespace GUI { namespace PreferencesHistory {

namespace {

constexpr int kDebounceMs = 2000; // burst of saves -> one snapshot
constexpr std::uintmax_t kMaxSnapshotBytes = 256 * 1024;
std::string pending_reason = "Preferences change";

const std::set<std::string> &safe_keys()
{
    static const std::set<std::string> keys = {
        "language", "language_mode", "funny_level_en", "funny_level_yue",
        "dark_color_mode", "ui_density", "ui_accent_seed", "ui_font_family", "ui_font_scale",
        "use_inches", "use_12h_time_format", "toolbar_style", "prepare_sidebar_dock",
        "reverse_mouse_wheel_zoom", "zoom_to_mouse", "canvas_drag_to_move", "grabber_size_factor",
        "show_shells_in_preview", "show_assembly_bvh_bounds", "show_bed_heat_soak_area",
        "enable_lod", "enable_bvh", "enable_assemble_view_preview", "max_recent_count",
        "backup_switch", "backup_interval", "single_instance", "auto_calculate_flush",
        "hide_new_filament_prompt", "show_support_recommend_dialog", "gamma_correct_in_import_obj"
    };
    return keys;
}

std::filesystem::path profiles_root()
{
    return std::filesystem::path(data_dir()).parent_path() / "BambuStudio-profiles";
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
        if (wxGetApp().app_config == nullptr)
            return;
        // The engine only accepts .3mf snapshot files, so stage a copy under
        // a unique name (the worker reads it asynchronously; stale staging
        // copies are pruned on later ticks once they are safely committed).
        const std::filesystem::path staging_dir = profiles_root() / ".staging";
        std::filesystem::create_directories(staging_dir, ec);
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
        static std::atomic<unsigned long long> sequence{0};
        const std::filesystem::path staging =
            staging_dir / ("preferences-" + std::to_string(now) + "-" +
                           std::to_string(sequence.fetch_add(1)) + ".3mf");
        try {
            std::ofstream output(staging, std::ios::binary | std::ios::trunc);
            const std::string payload = safe_snapshot(*wxGetApp().app_config).dump(2);
            output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            output.close();
            if (!output) return;
        } catch (const std::exception &) { return; }
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
        options.message = pending_reason;
        pending_reason = "Preferences change";
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

nlohmann::json safe_snapshot(const AppConfig &config)
{
    nlohmann::json settings = nlohmann::json::object();
    for (const auto &key : safe_keys()) {
        std::string value;
        if (config.get("app", key, value) && value.size() <= 4096)
            settings[key] = value;
    }
    return {{"format", "bambu-safe-preferences"}, {"version", 1}, {"settings", settings}};
}

bool read_snapshot(const std::filesystem::path &path, nlohmann::json &snapshot, std::string &error)
{
    error.clear();
    snapshot = nlohmann::json();
    try {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > kMaxSnapshotBytes) {
            error = "Preferences snapshot is unreadable or too large.";
            return false;
        }
        std::ifstream input(path, std::ios::binary);
        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (!input.read(bytes.data(), static_cast<std::streamsize>(size))) {
            error = "Unable to read preferences snapshot.";
            return false;
        }
        auto parsed = nlohmann::json::parse(bytes);
        if (!parsed.is_object() || parsed.size() != 3 ||
            parsed.value("format", std::string()) != "bambu-safe-preferences" ||
            !parsed.at("version").is_number_integer() || parsed.at("version") != 1 ||
            !parsed.at("settings").is_object()) {
            error = "Unsupported preferences snapshot. Legacy raw configuration is preserved but cannot be restored.";
            return false;
        }
        for (auto it = parsed["settings"].begin(); it != parsed["settings"].end(); ++it) {
            if (safe_keys().count(it.key()) == 0 || !it.value().is_string() ||
                it.value().get_ref<const std::string &>().size() > 4096) {
                error = "Preferences snapshot contains unsupported settings.";
                return false;
            }
        }
        snapshot = std::move(parsed);
        return true;
    } catch (const std::exception &) {
        error = "Invalid preferences snapshot. Legacy raw configuration is preserved but cannot be restored.";
        return false;
    }
}

bool apply_snapshot(const std::filesystem::path &path, std::string &error)
{
    nlohmann::json snapshot;
    if (!read_snapshot(path, snapshot, error)) return false;
    AppConfig *config = wxGetApp().app_config;
    if (config == nullptr) {
        error = "Preferences are not available.";
        return false;
    }
    const auto previous = safe_snapshot(*config)["settings"];
    try {
        for (const auto &key : safe_keys()) {
            const auto it = snapshot["settings"].find(key);
            if (it == snapshot["settings"].end()) config->erase("app", key);
            else config->set(key, it->get<std::string>());
        }
        config->set_dirty();
        config->save();
        pending_reason = "Restore preferences snapshot";
        snapshot_timer()->Stop();
        snapshot_timer()->Notify();
        return true;
    } catch (const std::exception &) {
        for (const auto &key : safe_keys()) {
            const auto it = previous.find(key);
            if (it == previous.end()) config->erase("app", key);
            else config->set(key, it->get<std::string>());
        }
        config->set_dirty();
        error = "Unable to save restored preferences.";
        return false;
    }
}

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
