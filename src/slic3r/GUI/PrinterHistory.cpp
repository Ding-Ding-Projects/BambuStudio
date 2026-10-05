#include "PrinterHistory.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <limits>
#include <mutex>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r {
const std::string &data_dir();
namespace GUI {
namespace {
using json = nlohmann::json;
constexpr std::uintmax_t MAX_FILE_BYTES = 4 * 1024 * 1024;
constexpr std::size_t MAX_RECORDS = 10000;
constexpr std::size_t MAX_DESCRIPTION_BYTES = 4096;
std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
json encode(const PrinterHistoryEntry &e) {
    return {{"id", e.id}, {"device_id", e.device_id}, {"code", e.code}, {"severity", e.severity},
            {"description", e.description}, {"category", e.category}, {"first_seen_ms", e.first_seen_ms},
            {"last_seen_ms", e.last_seen_ms}, {"resolved_ms", e.resolved_ms}, {"state", e.state}};
}
}
class PrinterHistory::Impl {
public:
    Impl(std::filesystem::path p, std::size_t bound, bool snapshots)
        : file(std::move(p)), max_entries(std::clamp<std::size_t>(bound, 1, MAX_RECORDS)), record_snapshots(snapshots) {
        try {
            std::error_code ec;
            if (!std::filesystem::exists(file, ec)) {
                if (ec) throw std::runtime_error("Cannot inspect incident history");
                return;
            }
            if (std::filesystem::file_size(file) > MAX_FILE_BYTES) throw std::runtime_error("Incident history exceeds storage limit");
            std::ifstream input(file, std::ios::binary);
            if (!input) throw std::runtime_error("Cannot read incident history");
            json j; input >> j;
            if (!j.at("schema_version").is_number_integer() || j.at("schema_version") != 1 ||
                !j.at("entries").is_array() || j.at("entries").size() > MAX_RECORDS)
                throw std::runtime_error("Invalid incident history schema");
            std::vector<PrinterHistoryEntry> loaded;
            std::set<std::uint64_t> ids;
            for (const auto &v : j.at("entries")) {
                if (!v.is_object() || !v.at("id").is_number_unsigned() || !v.at("severity").is_number_integer() ||
                    !v.at("first_seen_ms").is_number_integer() || !v.at("last_seen_ms").is_number_integer() ||
                    !v.at("resolved_ms").is_number_integer())
                    throw std::runtime_error("Invalid incident history field type");
                PrinterHistoryEntry e;
                e.id = v.at("id").get<std::uint64_t>(); e.device_id = v.at("device_id").get<std::string>();
                e.code = v.at("code").get<std::string>(); e.severity = v.at("severity").get<int>();
                e.description = v.at("description").get<std::string>(); e.category = v.at("category").get<std::string>();
                e.first_seen_ms = v.at("first_seen_ms").get<std::int64_t>();
                e.last_seen_ms = v.at("last_seen_ms").get<std::int64_t>();
                e.resolved_ms = v.at("resolved_ms").get<std::int64_t>(); e.state = v.at("state").get<std::string>();
                if (!e.id || e.id == (std::numeric_limits<std::uint64_t>::max)() || !ids.insert(e.id).second ||
                    e.device_id.empty() || e.device_id.size() > 256 || e.code.empty() || e.code.size() > 128 ||
                    e.description.size() > MAX_DESCRIPTION_BYTES || (e.category != "hms" && e.category != "print_error") ||
                    (e.state != "active" && e.state != "unknown" && e.state != "resolved") ||
                    e.first_seen_ms <= 0 || e.last_seen_ms < e.first_seen_ms ||
                    e.resolved_ms < 0 || (e.state != "resolved" && e.resolved_ms != 0) ||
                    (e.state == "resolved" && e.resolved_ms < e.last_seen_ms))
                    throw std::runtime_error("Invalid incident history record");
                // Telemetry from a prior process is no longer confirmed current.
                if (e.state == "active") e.state = "unknown";
                loaded.push_back(std::move(e));
            }
            rows = std::move(loaded); trim();
            for (const auto &e : rows) next_id = (std::max)(next_id, e.id + 1);
        } catch (const std::exception &) {
            error = "Could not load local printer history; existing file preserved"; load_failed = true;
        }
    }
    ~Impl() {
        if (rev != 0 && !load_failed) { last_save = 0; save(false, now_ms()); }
        // Drain asynchronous snapshots before deleting their immutable inputs.
        history.reset();
        for (const auto &p : pending) { std::error_code ec; std::filesystem::remove(p.first, ec); }
    }
    void trim() {
        while (rows.size() > max_entries) {
            auto victim = std::find_if(rows.begin(), rows.end(), [](const auto &e) { return e.state == "resolved"; });
            rows.erase(victim == rows.end() ? rows.begin() : victim);
        }
    }
    void save(bool transition, std::int64_t timestamp) {
        ++rev;
        // Repeated polls update memory, with disk writes limited to 30 seconds.
        if (!transition && timestamp >= last_save && timestamp - last_save < 30000) return;
        if (load_failed) return; // Never replace unreadable history silently.
        try {
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);
            if (ec) throw std::runtime_error("Cannot create incident storage");
            json j = {{"schema_version", 1}, {"entries", json::array()}};
            for (const auto &e : rows) j["entries"].push_back(encode(e));
            std::string payload = j.dump(2);
            while (payload.size() > MAX_FILE_BYTES && !rows.empty()) {
                auto victim = std::find_if(rows.begin(), rows.end(), [](const auto &e) { return e.state == "resolved"; });
                rows.erase(victim == rows.end() ? rows.begin() : victim);
                j["entries"] = json::array();
                for (const auto &e : rows) j["entries"].push_back(encode(e));
                payload = j.dump(2);
            }
            auto temp = file; temp += ".tmp";
            { std::ofstream out(temp, std::ios::binary | std::ios::trunc); out.write(payload.data(), payload.size()); out.close();
              if (!out) throw std::runtime_error("Cannot write incident history"); }
#ifdef _WIN32
            if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Cannot replace incident history");
#else
            std::filesystem::rename(temp, file);
#endif
            last_save = timestamp; error.clear();
            for (auto it = pending.begin(); it != pending.end();) {
                if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const auto result = it->second.get();
                    if (!result.ok()) error = "Could not record incident history snapshot";
                    std::filesystem::remove(it->first, ec); it = pending.erase(it);
                } else ++it;
            }
            if (transition && record_snapshots) {
                if (pending.size() >= 32) { error = "Incident snapshot queue is full; current ledger saved"; return; }
                if (!history) history = std::make_unique<ProjectHistoryManager>(file.parent_path());
                auto staging = file.parent_path() / ("incident-snapshot-" + std::to_string(timestamp) + "-" + std::to_string(rev) + ".3mf");
                { std::ofstream out(staging, std::ios::binary | std::ios::trunc); out.write(payload.data(), payload.size()); out.close();
                  if (!out) throw std::runtime_error("Cannot stage incident snapshot"); }
                ProjectHistoryCommitOptions options; options.message = "Printer incident lifecycle change";
                pending.emplace_back(staging, history->commit_snapshot(file.parent_path() / "printer-incidents.history.3mf", staging, options));
            }
        } catch (const std::exception &) { error = "Could not persist local printer history"; }
    }
    mutable std::mutex mutex;
    std::filesystem::path file;
    std::size_t max_entries;
    bool record_snapshots, load_failed = false;
    std::vector<PrinterHistoryEntry> rows;
    std::uint64_t next_id = 1, rev = 0;
    std::int64_t last_save = 0;
    std::string error;
    std::unique_ptr<ProjectHistoryManager> history;
    std::vector<std::pair<std::filesystem::path, std::future<ProjectHistoryCommitResult>>> pending;
};
PrinterHistory::PrinterHistory(std::filesystem::path file, std::size_t bound, bool snapshots)
    : m_impl(std::make_unique<Impl>(std::move(file), bound, snapshots)) {}
PrinterHistory::~PrinterHistory() = default;
PrinterHistory &PrinterHistory::instance() {
    static PrinterHistory h(std::filesystem::path(data_dir()) / "printer_history" / "incidents.json", DEFAULT_MAX_ENTRIES, true);
    return h;
}
void PrinterHistory::observe(const std::string &device, const std::vector<PrinterIncidentObservation> &observations,
                             bool complete, std::int64_t timestamp, const std::string &category) {
    if (device.empty() || device.size() > 256 || (category != "hms" && category != "print_error")) return;
    if (!timestamp) timestamp = now_ms();
    if (timestamp <= 0) return;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    bool changed = false, transition = false;
    std::set<std::string> seen;
    for (const auto &o : observations) {
        if (o.category != category || o.code.empty() || o.code.size() > 128 || o.description.size() > MAX_DESCRIPTION_BYTES) {
            complete = false; continue;
        }
        if (!seen.insert(o.code).second) continue;
        auto it = std::find_if(m_impl->rows.rbegin(), m_impl->rows.rend(), [&](const auto &e) {
            return e.device_id == device && e.category == category && e.code == o.code && e.state != "resolved";
        });
        if (it == m_impl->rows.rend()) {
            PrinterHistoryEntry e; e.id = m_impl->next_id++; e.device_id = device; e.code = o.code;
            e.category = category; e.severity = o.severity; e.description = o.description;
            e.first_seen_ms = e.last_seen_ms = timestamp; m_impl->rows.push_back(std::move(e));
            changed = transition = true;
        } else {
            if (timestamp < it->last_seen_ms) { complete = false; continue; }
            const bool row_transition = it->state != "active" || it->severity != o.severity ||
                                        (!o.description.empty() && it->description != o.description);
            transition |= row_transition; changed |= row_transition || it->last_seen_ms != timestamp;
            it->state = "active"; it->severity = o.severity; it->last_seen_ms = timestamp;
            if (!o.description.empty()) it->description = o.description;
        }
    }
    // HMS recovery is conservative: only a fresh complete empty report clears
    // incidents. An unrelated remaining HMS item does not prove recovery.
    if (complete && (category != "hms" || observations.empty())) for (auto &e : m_impl->rows) {
        if (e.device_id == device && e.category == category && e.state != "resolved" && !seen.count(e.code) && timestamp >= e.last_seen_ms) {
            e.state = "resolved"; e.resolved_ms = timestamp; changed = transition = true;
        }
    }
    m_impl->trim();
    if (changed) m_impl->save(transition, timestamp);
}
void PrinterHistory::mark_unknown(const std::string &device, std::int64_t timestamp) {
    if (!timestamp) timestamp = now_ms();
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    bool changed = false;
    for (auto &e : m_impl->rows) if (e.device_id == device && e.state == "active") { e.state = "unknown"; changed = true; }
    if (changed) m_impl->save(true, timestamp);
}
std::vector<PrinterHistoryEntry> PrinterHistory::entries(const std::string &device) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    std::vector<PrinterHistoryEntry> result;
    for (auto it = m_impl->rows.rbegin(); it != m_impl->rows.rend(); ++it)
        if (device.empty() || it->device_id == device) result.push_back(*it);
    return result;
}
std::uint64_t PrinterHistory::revision() const { std::lock_guard<std::mutex> lock(m_impl->mutex); return m_impl->rev; }
std::string PrinterHistory::last_error() const { std::lock_guard<std::mutex> lock(m_impl->mutex); return m_impl->error; }
std::filesystem::path PrinterHistory::identity() const { return m_impl->file.parent_path() / "printer-incidents.history.3mf"; }
ProjectHistoryManager *PrinterHistory::manager() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->record_snapshots) return nullptr;
    try {
        if (!m_impl->history) m_impl->history = std::make_unique<ProjectHistoryManager>(m_impl->file.parent_path());
        return m_impl->history.get();
    } catch (const std::exception &) { m_impl->error = "Could not initialize incident snapshot history"; return nullptr; }
}
} }
