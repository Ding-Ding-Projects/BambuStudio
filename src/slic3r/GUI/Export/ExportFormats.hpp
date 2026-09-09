#ifndef slic3r_GUI_Export_ExportFormats_hpp_
#define slic3r_GUI_Export_ExportFormats_hpp_

// Shared "export everything" vocabulary. This header is deliberately free of
// wxWidgets and of every other GUI dependency so the serializers, the loss
// report and the archive writer can be unit-tested from a plain console
// target (tests/export_everything) and reused by any surface in the app.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r::GUI::Export {

// ---------------------------------------------------------------------------
// Value tree
// ---------------------------------------------------------------------------

// A small JSON-shaped value. Objects keep insertion order so every serializer
// emits keys in the order the surface declared them.
struct Value
{
    enum class Type { Null, Bool, Integer, Number, String, Array, Object };

    Type                                       type{Type::Null};
    bool                                       boolean{false};
    long long                                  integer{0};
    double                                     number{0.0};
    std::string                                string;
    std::vector<Value>                         array;
    std::vector<std::pair<std::string, Value>> object;

    Value() = default;
    static Value null() { return Value(); }
    static Value from_bool(bool v) { Value r; r.type = Type::Bool; r.boolean = v; return r; }
    static Value from_int(long long v) { Value r; r.type = Type::Integer; r.integer = v; return r; }
    static Value from_number(double v) { Value r; r.type = Type::Number; r.number = v; return r; }
    static Value from_string(std::string v) { Value r; r.type = Type::String; r.string = std::move(v); return r; }
    static Value make_array() { Value r; r.type = Type::Array; return r; }
    static Value make_object() { Value r; r.type = Type::Object; return r; }

    bool is_null() const { return type == Type::Null; }
    bool is_scalar() const { return type != Type::Array && type != Type::Object; }
    bool is_container() const { return !is_scalar(); }

    Value &push(Value v) { array.push_back(std::move(v)); return array.back(); }
    Value &set(std::string key, Value v)
    {
        for (auto &kv : object)
            if (kv.first == key) { kv.second = std::move(v); return kv.second; }
        object.emplace_back(std::move(key), std::move(v));
        return object.back().second;
    }
    const Value *find(const std::string &key) const
    {
        for (const auto &kv : object)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Dataset
// ---------------------------------------------------------------------------

enum class DatasetKind {
    Tabular,    // columns + rows of scalar cells
    Structured, // arbitrary value tree rooted at `root`
    Prose       // a titled block of text
};

struct Column
{
    std::string name;
    // Declared cell type; recorded in the schema header so a reader can
    // restore numbers and booleans from a text-only format such as CSV.
    Value::Type type{Value::Type::String};
};

struct Dataset
{
    std::string name;                   // human-readable, e.g. "Project version history"
    std::string schema_id;              // stable identifier, e.g. "bambustudio.project-history"
    int         schema_version{1};
    DatasetKind kind{DatasetKind::Tabular};

    // Tabular
    std::vector<Column>             columns;
    std::vector<std::vector<Value>> rows;

    // Structured
    Value root;

    // Prose
    std::string prose_title;
    std::string prose;

    // Suggested output file stem ("project-history"); the dialog appends the
    // format extension.
    std::string file_stem;

    std::size_t record_count() const
    {
        switch (kind) {
        case DatasetKind::Tabular: return rows.size();
        case DatasetKind::Structured: return root.type == Value::Type::Array ? root.array.size() : root.object.size();
        case DatasetKind::Prose: return prose.empty() ? 0 : 1;
        }
        return 0;
    }
};

// ---------------------------------------------------------------------------
// Formats
// ---------------------------------------------------------------------------

enum class Format { JSON, JSONL, YAML, TOML, XML, CSV, TSV, Markdown, HTML };

inline const std::vector<Format> &all_formats()
{
    static const std::vector<Format> formats{Format::JSON, Format::JSONL, Format::YAML, Format::TOML, Format::XML,
                                             Format::CSV,  Format::TSV,   Format::Markdown, Format::HTML};
    return formats;
}

inline const char *format_name(Format f)
{
    switch (f) {
    case Format::JSON: return "JSON";
    case Format::JSONL: return "JSON Lines";
    case Format::YAML: return "YAML";
    case Format::TOML: return "TOML";
    case Format::XML: return "XML";
    case Format::CSV: return "CSV";
    case Format::TSV: return "TSV";
    case Format::Markdown: return "Markdown";
    case Format::HTML: return "HTML";
    }
    return "?";
}

inline const char *format_extension(Format f)
{
    switch (f) {
    case Format::JSON: return "json";
    case Format::JSONL: return "jsonl";
    case Format::YAML: return "yaml";
    case Format::TOML: return "toml";
    case Format::XML: return "xml";
    case Format::CSV: return "csv";
    case Format::TSV: return "tsv";
    case Format::Markdown: return "md";
    case Format::HTML: return "html";
    }
    return "txt";
}

inline std::optional<Format> format_from_extension(const std::string &ext)
{
    for (Format f : all_formats())
        if (ext == format_extension(f)) return f;
    if (ext == "yml") return Format::YAML;
    if (ext == "htm") return Format::HTML;
    if (ext == "markdown") return Format::Markdown;
    if (ext == "ndjson") return Format::JSONL;
    return std::nullopt;
}

// Which datum family a format is the natural home for. The dialog sorts the
// natural formats first; every format stays offered, with its loss report.
inline bool format_is_natural_for(Format f, DatasetKind kind)
{
    switch (kind) {
    case DatasetKind::Tabular: return f == Format::CSV || f == Format::TSV;
    case DatasetKind::Structured: return f == Format::JSON || f == Format::YAML || f == Format::TOML;
    case DatasetKind::Prose: return f == Format::Markdown || f == Format::HTML;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Serializer options and loss report
// ---------------------------------------------------------------------------

enum class LineEnding { LF, CRLF };

inline const char *line_ending_name(LineEnding le) { return le == LineEnding::LF ? "LF" : "CRLF"; }
inline const char *line_ending_sequence(LineEnding le) { return le == LineEnding::LF ? "\n" : "\r\n"; }

struct SerializeOptions
{
    LineEnding line_ending{LineEnding::LF};
    // Every export is UTF-8. A byte-order mark is only useful for a spreadsheet
    // opening CSV/TSV by double-click; it is never written by default.
    bool write_bom{false};
    // Emit the schema/encoding header (comment, envelope or sidecar).
    bool include_schema_header{true};
    // Free-form provenance placed in the header, e.g. the app version.
    std::string generator{"BambuStudio"};
};

struct LossReport
{
    bool                     lossless{true};
    // Fields or properties that the format cannot carry. Non-empty means the
    // export is lossy and the user is told exactly what goes missing.
    std::vector<std::string> losses;
    // Informational caveats that do not lose data (e.g. "numbers become text;
    // the schema header records column types").
    std::vector<std::string> notes;
};

// One serialized export: the main file body plus, for formats that have no
// comment syntax (CSV/TSV), a JSON sidecar carrying the schema header.
struct Serialized
{
    std::string                body;
    std::optional<std::string> sidecar_name; // relative file name, e.g. "history.csv.meta.json"
    std::string                sidecar_body;
};

// ---------------------------------------------------------------------------
// Archives
// ---------------------------------------------------------------------------

enum class ArchiveFormat { None, Zip, SevenZip };

enum class SevenZipMethod { LZMA2, LZMA, PPMd, BZip2, Deflate };

// 0 = store, 1 = fastest, 3 = fast, 5 = normal, 7 = maximum, 9 = ultra.
enum class SevenZipLevel : int { Store = 0, Fastest = 1, Fast = 3, Normal = 5, Maximum = 7, Ultra = 9 };

struct ArchiveOptions
{
    ArchiveFormat  format{ArchiveFormat::None};

    // --- 7-Zip only -------------------------------------------------------
    SevenZipMethod method{SevenZipMethod::LZMA2};
    SevenZipLevel  level{SevenZipLevel::Normal};
    // Dictionary size in MiB; 0 lets 7-Zip pick the level default.
    unsigned       dictionary_mib{0};
    // Word (fast bytes) size; 0 = level default. LZMA/LZMA2: 5..273, PPMd order 2..32.
    unsigned       word_size{0};
    // Solid archive: files share one compression stream (smaller, but any
    // single file needs the whole block decompressed).
    bool           solid{true};
    // Solid block size in MiB; 0 = level default.
    unsigned       solid_block_mib{0};
    // Worker threads; 0 = 7-Zip default (all cores).
    unsigned       threads{0};
    // Split volume size such as "100m" or "4g"; empty = single file.
    std::string    split_volume;
    // AES-256 content encryption when non-empty. Never persisted.
    std::string    password;
    // Also encrypt the archive headers so file names are hidden (-mhe=on).
    bool           encrypt_headers{true};
};

inline const char *seven_zip_method_switch(SevenZipMethod m)
{
    switch (m) {
    case SevenZipMethod::LZMA2: return "LZMA2";
    case SevenZipMethod::LZMA: return "LZMA";
    case SevenZipMethod::PPMd: return "PPMd";
    case SevenZipMethod::BZip2: return "BZip2";
    case SevenZipMethod::Deflate: return "Deflate";
    }
    return "LZMA2";
}

} // namespace Slic3r::GUI::Export

#endif // slic3r_GUI_Export_ExportFormats_hpp_
