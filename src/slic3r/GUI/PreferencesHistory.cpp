#include "PreferencesHistory.hpp"
#include "GUI_App.hpp"
#include "Appearance/ElementStyle.hpp"
#include "Schedule/ScheduledSettings.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"
#include "libslic3r/Utils.hpp"

#include <chrono>
#include <memory>
#include <fstream>
#include <set>
#include <atomic>
#include <cmath>

#include <wx/timer.h>

namespace Slic3r { namespace GUI { namespace PreferencesHistory {

namespace {

constexpr int kDebounceMs = 2000; // burst of saves -> one snapshot
// Room for the scheduled-settings rule document (at most
// Schedule::kMaxDocumentBytes, which grows when escaped inside the snapshot's
// JSON string) beside the other bounded values.
constexpr std::uintmax_t kMaxSnapshotBytes = 4 * Schedule::kMaxDocumentBytes;
constexpr std::size_t kMaxValueBytes = 4096;
constexpr std::uintmax_t kMaxAppearanceBytes = 2 * 1024 * 1024;
constexpr const char *kAppearanceFormat = "bambu-appearance";
constexpr const char *kAppearanceChange = "Appearance change";
std::string pending_reason = "Preferences change";
std::string pending_appearance_label; // a named action waiting for its save

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
        "hide_new_filament_prompt", "show_support_recommend_dialog", "gamma_correct_in_import_obj",
        // Preferences > Schedules rules (Schedule/ScheduledSettings): a new rule,
        // one started from a preset included, is an ordinary recorded change.
        Schedule::kDocumentConfigKey
    };
    return keys;
}

std::size_t value_limit(const std::string &key)
{
    return key == Schedule::kDocumentConfigKey ? Schedule::kMaxDocumentBytes : kMaxValueBytes;
}

bool valid_value(const std::string &key, const std::string &value)
{
    if (key == "ui_density") return value == "compact" || value == "comfortable";
    if (key == "ui_font_scale") return value == "0.9" || value == "1.0" || value == "1.15";
    if (key == "prepare_sidebar_dock") return value == "left" || value == "right" || value == "top" || value == "bottom";
    if (key == "enable_assemble_view_preview") return value == "Auto" || value == "Open" || value == "Close";
    if (key == "dark_color_mode") return value == "0" || value == "1";
    if (key == Schedule::kDocumentConfigKey) return value.empty() || Schedule::parse_document(value).ok;
    if (key == "funny_level_en" || key == "funny_level_yue")
        return value.size() == 1 && value[0] >= '1' && value[0] <= '5';
    if (key == "max_recent_count" || key == "backup_interval" || key == "grabber_size_factor") {
        try {
            size_t used = 0; const double number = std::stod(value, &used);
            const double minimum = key == "grabber_size_factor" ? 1.0 : 0.0;
            const double maximum = key == "backup_interval" ? 86400 : key == "max_recent_count" ? 10000 : 2.5;
            return used == value.size() && std::isfinite(number) && number >= minimum && number <= maximum &&
                (key == "grabber_size_factor" || std::floor(number) == number);
        } catch (...) { return false; }
    }
    static const std::set<std::string> boolean_keys = {
        "use_inches", "use_12h_time_format", "reverse_mouse_wheel_zoom", "zoom_to_mouse", "canvas_drag_to_move",
        "show_shells_in_preview", "show_assembly_bvh_bounds", "show_bed_heat_soak_area", "enable_lod", "enable_bvh",
        "backup_switch", "single_instance", "auto_calculate_flush",
        "hide_new_filament_prompt", "show_support_recommend_dialog", "gamma_correct_in_import_obj"
    };
    if (boolean_keys.count(key)) return value == "true" || value == "false" || value == "1" || value == "0";
    // Other allowlisted textual presentation values are bounded by read_snapshot.
    return true;
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

// Write `payload` to a uniquely named staging file for the history engine and
// prune this prefix's stale staging copies. The engine only accepts .3mf
// snapshot files; its worker reads the copy asynchronously, so copies are
// pruned on later writes once they are safely committed. Empty on failure.
std::filesystem::path stage(const std::string &prefix, const std::string &payload)
{
    std::error_code ec;
    const std::filesystem::path staging_dir = profiles_root() / ".staging";
    std::filesystem::create_directories(staging_dir, ec);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count();
    static std::atomic<unsigned long long> sequence{0};
    const std::filesystem::path staging =
        staging_dir / (prefix + std::to_string(now) + "-" + std::to_string(sequence.fetch_add(1)) + ".3mf");
    try {
        std::ofstream output(staging, std::ios::binary | std::ios::trunc);
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        output.close();
        if (!output) return {};
    } catch (const std::exception &) { return {}; }
    for (auto it = std::filesystem::directory_iterator(staging_dir, ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.rfind(prefix, 0) != 0 || it->path() == staging)
            continue;
        const auto written = std::filesystem::last_write_time(it->path(), ec);
        if (!ec && decltype(written)::clock::now() - written > std::chrono::minutes(5)) {
            std::error_code remove_ec;
            std::filesystem::remove(it->path(), remove_ec);
        }
    }
    return staging;
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
        if (wxGetApp().app_config == nullptr)
            return;
        const std::filesystem::path staging = stage("preferences-", safe_snapshot(*wxGetApp().app_config).dump(2));
        if (staging.empty())
            return;
        ProjectHistoryCommitOptions options;
        options.message = pending_reason;
        pending_reason = "Preferences change";
        // Fire-and-forget: the engine serializes and dedupes on its worker;
        // the returned future is deliberately dropped (shutdown drains it).
        history->commit_snapshot(identity(), staging, options);
    }
};

// Record the live appearance document. Identical documents dedupe inside the
// engine, so recording an unchanged state costs nothing.
void commit_appearance(const std::string &message)
{
    ProjectHistoryManager *history = shared_manager();
    if (history == nullptr || ElementStyle::storage_dir().empty())
        return;
    const nlohmann::json payload = {{"format", kAppearanceFormat}, {"version", 1},
                                    {"document", ElementStyle::registry().to_json()}};
    const std::filesystem::path staging = stage("appearance-", payload.dump(2));
    if (staging.empty())
        return;
    ProjectHistoryCommitOptions options;
    options.message = message;
    history->commit_snapshot(appearance_identity(), staging, options);
}

class AppearanceTimer : public wxTimer
{
public:
    void Notify() override { commit_appearance(kAppearanceChange); }
};

AppearanceTimer *appearance_timer()
{
    static AppearanceTimer *s_timer = new AppearanceTimer(); // app-lifetime
    return s_timer;
}

void on_appearance_saved()
{
    if (!pending_appearance_label.empty()) {
        // A named action gets its own revision right away.
        appearance_timer()->Stop();
        const std::string label = pending_appearance_label;
        pending_appearance_label.clear();
        commit_appearance(label);
        return;
    }
    appearance_timer()->StartOnce(kDebounceMs);
}

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
        if (config.get("app", key, value) && value.size() <= value_limit(key))
            settings[key] = value;
    }
    // Version 2 snapshots always speak for the schedule rules: an absent rule
    // document means there were none. Version 1 predates recording them.
    return {{"format", "bambu-safe-preferences"}, {"version", 2}, {"settings", settings}};
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
            !parsed.at("version").is_number_integer() || (parsed.at("version") != 1 && parsed.at("version") != 2) ||
            !parsed.at("settings").is_object()) {
            error = "Unsupported preferences snapshot. Legacy raw configuration is preserved but cannot be restored.";
            return false;
        }
        for (auto it = parsed["settings"].begin(); it != parsed["settings"].end(); ++it) {
            if (safe_keys().count(it.key()) == 0 || !it.value().is_string() ||
                it.value().get_ref<const std::string &>().size() > value_limit(it.key()) ||
                !valid_value(it.key(), it.value().get_ref<const std::string &>())) {
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
    // A version 1 snapshot predates recording the schedule rules and says
    // nothing about them, so their rules are kept rather than erased.
    const bool records_schedules = snapshot["version"] != 1;
    try {
        for (const auto &key : safe_keys()) {
            const auto it = snapshot["settings"].find(key);
            if (it == snapshot["settings"].end()) {
                if (key != Schedule::kDocumentConfigKey || records_schedules) config->erase("app", key);
            } else
                config->set(key, it->get<std::string>());
        }
        config->set_dirty();
        config->save();
        pending_reason = "Restore preferences snapshot";
        snapshot_timer()->Stop();
        snapshot_timer()->Notify();
        // The schedule rules may have changed with the snapshot.
        Schedule::Scheduler::instance().reload();
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

void label_next_snapshot(const std::string &label)
{
    pending_reason = label.empty() ? std::string("Preferences change") : label;
}

std::filesystem::path appearance_identity()
{
    // Same .3mf suffix rule as identity().
    return profiles_root() / "appearance.history.3mf";
}

void begin_appearance_action(const std::string &label)
{
    if (appearance_timer()->IsRunning()) {
        // Keep the unnamed changes before this action in a revision of their own.
        appearance_timer()->Stop();
        commit_appearance(kAppearanceChange);
    }
    pending_appearance_label = label.empty() ? std::string(kAppearanceChange) : label;
}

void cancel_appearance_action() { pending_appearance_label.clear(); }

bool read_appearance_snapshot(const std::filesystem::path &path, nlohmann::json &document, std::string &error)
{
    error.clear();
    document = nlohmann::json();
    try {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > kMaxAppearanceBytes) {
            error = "Appearance snapshot is unreadable or too large.";
            return false;
        }
        std::ifstream input(path, std::ios::binary);
        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (!input.read(bytes.data(), static_cast<std::streamsize>(size))) {
            error = "Unable to read appearance snapshot.";
            return false;
        }
        const auto parsed = nlohmann::json::parse(bytes);
        if (!parsed.is_object() || parsed.size() != 3 || parsed.value("format", std::string()) != kAppearanceFormat ||
            !parsed.at("version").is_number_integer() || parsed.at("version") != 1 || !parsed.at("document").is_object()) {
            error = "Unsupported appearance snapshot.";
            return false;
        }
        const nlohmann::json &stored = parsed.at("document");
        const auto schema = stored.find("schema");
        if (schema == stored.end() || !schema->is_number_integer() || schema->get<int>() > StyleRegistry::kSchema) {
            error = "The appearance snapshot was written by a newer version.";
            return false;
        }
        document = stored;
        return true;
    } catch (const std::exception &) {
        error = "Invalid appearance snapshot.";
        return false;
    }
}

bool apply_appearance_snapshot(const std::filesystem::path &path, std::string &error)
{
    nlohmann::json document;
    if (!read_appearance_snapshot(path, document, error))
        return false;
    StyleRegistry &registry = ElementStyle::registry();
    const nlohmann::json previous = registry.to_json();
    begin_appearance_action("Restore appearance snapshot");
    const StyleLoadReport report = registry.from_json(document);
    if (!report.ok) {
        cancel_appearance_action();
        registry.from_json(previous);
        error = "The appearance snapshot could not be applied: " + report.error;
        return false;
    }
    if (!ElementStyle::save()) {
        cancel_appearance_action();
        registry.from_json(previous);
        error = "Unable to save the restored appearance. The previous appearance was kept.";
        return false;
    }
    return true;
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
    // Appearance saves come from the editor and the command palette, both on
    // the main thread. The startup state is recorded first so the session's
    // first change, a preset application included, can be undone.
    ElementStyle::set_save_observer([]() { on_appearance_saved(); });
    commit_appearance("Appearance at startup");
    // The preferences at startup too, so the first change of a session (a
    // schedule rule started from a preset included) has a version to undo to.
    // Identical snapshots dedupe in the engine.
    pending_reason = "Preferences at startup";
    snapshot_timer()->Notify();
}

} } } // namespace Slic3r::GUI::PreferencesHistory
