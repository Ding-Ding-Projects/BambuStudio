#ifndef slic3r_GUI_Export_ExportEverything_hpp_
#define slic3r_GUI_Export_ExportEverything_hpp_

// wx-free export engine: format capability matrix, serializers, loss report,
// ZIP writer (miniz) and the 7-Zip command-line bridge. The MD3 ExportDialog
// is the only wx consumer; everything here is exercised by
// tests/export_everything.

#include "ExportFormats.hpp"

#include <filesystem>
#include <atomic>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r::GUI::Export {

// Only reviewed non-sensitive preference keys and bounded values are exportable.
bool preset_export_allowed(const std::string &key, const std::string &value);
bool preference_export_allowed(const std::string &section, const std::string &key, const std::string &value);

// --- Capability matrix -----------------------------------------------------

// What would be lost (or merely changed) when `dataset` is written as `format`.
LossReport compute_loss_report(const Dataset &dataset, Format format);

// --- Serializers -----------------------------------------------------------

// Serialize `dataset` as `format`. Never throws; every dataset can be written
// in every format, with the loss report saying what did not survive.
Serialized serialize(const Dataset &dataset, Format format, const SerializeOptions &options = {});

// Individual escapers, exposed for the tests and for surfaces that build
// their own snippets.
std::string json_escape(const std::string &text);
std::string csv_quote(const std::string &cell, char separator);
std::string tsv_escape(const std::string &cell);
std::string xml_escape(const std::string &text);
std::string toml_quote(const std::string &text);
std::string yaml_quote(const std::string &text);
std::string html_escape(const std::string &text);
std::string markdown_cell(const std::string &text);

// Apply the requested line ending to LF-normalized text.
std::string apply_line_ending(const std::string &lf_text, LineEnding le);

enum class ExportPhase { Preparing, Serializing, Writing, Compressing, Verifying, Publishing, Finished };
struct ExportControl {
    enum class State { Running, CancelRequested, Publishing, Finished };
    std::atomic<State> state{State::Running};
    std::atomic<ExportPhase> phase{ExportPhase::Preparing};
    bool request_cancel() {
        auto expected = State::Running;
        return state.compare_exchange_strong(expected, State::CancelRequested) || expected == State::CancelRequested;
    }
    bool cancelled() const { return state.load() == State::CancelRequested; }
    bool begin_publication() {
        auto expected = State::Running;
        return state.compare_exchange_strong(expected, State::Publishing);
    }
};

// --- Archives --------------------------------------------------------------

struct ArchiveEntry
{
    std::string relative_path; // forward slashes, no leading slash, no ".." segments
    std::string data;
};

// Normalize a candidate archive member path: backslashes become slashes,
// leading "./" and "/" are stripped. Returns nullopt for anything that could
// escape the extraction directory (absolute paths, drive letters, "..").
std::optional<std::string> sanitize_archive_path(const std::string &candidate);

struct ArchiveResult
{
    bool                  ok{false};
    std::string           error;
    std::filesystem::path archive_path;
    // Command line used for 7-Zip (password redacted), for the status line.
    std::string           command_line;
};

// Write `entries` into a ZIP at `archive_path` via miniz (Deflate). ZIP here is
// never encrypted; the dialog says so rather than offering a password.
ArchiveResult write_zip(const std::filesystem::path &archive_path, const std::vector<ArchiveEntry> &entries,
                        const std::shared_ptr<ExportControl> &control = {});

// Map the archive options to 7-Zip switches (without the "a", the archive
// name or the file list). Password values never enter switches. The legacy
// boolean selects display redaction; execution uses bare -p and private stdin.
std::vector<std::string> seven_zip_switches(const ArchiveOptions &options, bool redact_password = false);

// Human cost hints for the current options ("Ultra needs about 700 MiB RAM").
std::vector<std::string> seven_zip_cost_hints(const ArchiveOptions &options);

// True when the password is set but headers stay in the clear: the archive
// content is protected, the file names are not.
bool seven_zip_filenames_visible(const ArchiveOptions &options);

struct SevenZipLocation
{
    bool                  found{false};
    std::filesystem::path executable;
    std::string           searched; // where we looked, for the "not found" state
};

// Look for 7z.exe / 7za.exe on PATH, in Program Files and in the per-user
// install location. `override` (e.g. from a preference) is checked first.
SevenZipLocation find_seven_zip(const std::filesystem::path &override = {});

// Stage `entries` in a temporary directory and run 7-Zip on them with the
// given options. Paths inside the archive are relative to the staging root.
ArchiveResult write_seven_zip(const std::filesystem::path &archive_path,
                              const std::vector<ArchiveEntry> &entries,
                              const ArchiveOptions &options,
                              const SevenZipLocation &seven_zip,
                              const std::shared_ptr<ExportControl> &control = {});

// --- One-shot export job ---------------------------------------------------

struct ExportTargetSnapshot {
    bool exists = false;
    std::uintmax_t size = 0;
    std::filesystem::file_time_type time{};
    std::uint64_t identity = 0;
    unsigned long volume = 0;
    std::filesystem::path path;
};
std::optional<ExportTargetSnapshot> inspect_output_target(const std::filesystem::path &path);

struct ExportJob
{
    std::shared_ptr<ExportControl> control;
    Dataset               dataset;
    Format                format{Format::JSON};
    SerializeOptions      serialize_options;
    ArchiveOptions        archive;
    // Full output path. For ArchiveFormat::None this is the data file; for an
    // archive it is the .zip/.7z and the data file lives inside it.
    std::filesystem::path output_path;
    std::filesystem::path seven_zip_override;
    // Explicitly confirmed exact existing targets. Empty means never overwrite.
    std::vector<std::filesystem::path> overwrite_approved_paths;
    // Captured before user confirmation. A changed target invalidates approval.
    std::vector<ExportTargetSnapshot> destination_snapshots;
};

struct ExportOutcome
{
    bool                     ok{false};
    bool                     cancelled{false};
    std::string              error;
    std::filesystem::path    written_path;
    std::vector<std::string> members; // files inside the archive, or the single file name
    LossReport               loss;
    std::string              command_line;
};

std::vector<std::filesystem::path> planned_output_paths(const ExportJob &job);
ExportOutcome run_export(const ExportJob &job);

} // namespace Slic3r::GUI::Export

#endif // slic3r_GUI_Export_ExportEverything_hpp_
