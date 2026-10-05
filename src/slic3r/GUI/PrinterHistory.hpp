#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Slic3r {
class ProjectHistoryManager;
namespace GUI {
struct PrinterIncidentObservation {
    std::string code;
    int severity = 0;
    std::string description;
    std::string category = "hms";
};
struct PrinterHistoryEntry {
    std::uint64_t id = 0;
    std::string device_id, code;
    int severity = 0;
    std::string description;
    std::string category = "hms";
    std::int64_t first_seen_ms = 0, last_seen_ms = 0, resolved_ms = 0;
    std::string state = "active"; // active, unknown, resolved
};
// Local incident ledger. Reads return copies safe for the GUI thread.
// No network operations, printer commands or credential fields.
class PrinterHistory {
public:
    static constexpr std::size_t DEFAULT_MAX_ENTRIES = 1000;
    explicit PrinterHistory(std::filesystem::path storage_file, std::size_t max_entries = DEFAULT_MAX_ENTRIES,
                            bool record_snapshots = false);
    ~PrinterHistory();
    PrinterHistory(const PrinterHistory &) = delete;
    PrinterHistory &operator=(const PrinterHistory &) = delete;
    static PrinterHistory &instance();
    // HMS incidents resolve only on a fresh complete empty report. Print errors
    // resolve when absent from an explicit current value. Reconstructed
    // differential telemetry is not fresh.
    void observe(const std::string &device_id, const std::vector<PrinterIncidentObservation> &observations,
                 bool complete_report, std::int64_t timestamp_ms = 0, const std::string &category = "hms");
    void mark_unknown(const std::string &device_id, std::int64_t timestamp_ms = 0);
    std::vector<PrinterHistoryEntry> entries(const std::string &device_id = {}) const;
    std::uint64_t revision() const;
    std::string last_error() const;
    std::filesystem::path identity() const;
    ProjectHistoryManager *manager();
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
} }
