#ifndef slic3r_GUI_Export_ExportEverything_hpp_
#define slic3r_GUI_Export_ExportEverything_hpp_

// wx-free export engine: format capability matrix, serializers, loss report,
// ZIP writer (miniz) and the 7-Zip command-line bridge. The MD3 ExportDialog
// is the only wx consumer; everything here is exercised by
// tests/export_everything.

#include "ExportFormats.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r::GUI::Export {

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
ArchiveResult write_zip(const std::filesystem::path &archive_path, const std::vector<ArchiveEntry> &entries);

// Map the archive options to 7-Zip switches (without the "a", the archive
// name or the file list). `redact_password` replaces the -p value with "***".
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
                              const SevenZipLocation &seven_zip);

// --- One-shot export job ---------------------------------------------------

struct ExportJob
{
    Dataset               dataset;
    Format                format{Format::JSON};
    SerializeOptions      serialize_options;
    ArchiveOptions        archive;
    // Full output path. For ArchiveFormat::None this is the data file; for an
    // archive it is the .zip/.7z and the data file lives inside it.
    std::filesystem::path output_path;
    std::filesystem::path seven_zip_override;
};

struct ExportOutcome
{
    bool                     ok{false};
    std::string              error;
    std::filesystem::path    written_path;
    std::vector<std::string> members; // files inside the archive, or the single file name
    LossReport               loss;
    std::string              command_line;
};

ExportOutcome run_export(const ExportJob &job);

} // namespace Slic3r::GUI::Export

#endif // slic3r_GUI_Export_ExportEverything_hpp_
