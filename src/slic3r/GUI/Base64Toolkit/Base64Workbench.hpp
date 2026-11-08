#ifndef slic3r_GUI_Base64Toolkit_Base64Workbench_hpp_
#define slic3r_GUI_Base64Toolkit_Base64Workbench_hpp_

// Everything the Base64 toolkit page decides without wxWidgets: the text
// conversion with its verification, the error excerpt, previews, sizes, the
// undo stack, the history record and the exported record. Tested on its own by
// tests/base64_workbench_test.cpp.

#include "Base64Codec.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace Base64Toolkit {

enum class LineMode { OneLine, Mime, Custom };

struct TextOptions
{
    bool         encode       = true;
    Alphabet     alphabet     = Alphabet::Standard;
    LineMode     lines        = LineMode::OneLine;
    int          custom_width = 64; // characters per line for LineMode::Custom, 4 to 1000
    bool         padding      = true;
    bool         strict       = true;
    TextEncoding encoding     = TextEncoding::Utf8;

    bool operator==(const TextOptions &o) const
    {
        return encode == o.encode && alphabet == o.alphabet && lines == o.lines && custom_width == o.custom_width &&
               padding == o.padding && strict == o.strict && encoding == o.encoding;
    }
    bool operator!=(const TextOptions &o) const { return !(*this == o); }
};

EncodeOptions encode_options(const TextOptions &options);
DecodeOptions decode_options(const TextOptions &options);
int           clamp_width(int width);

// Text larger than this belongs on the Files tab.
constexpr std::size_t kTextLimit = std::size_t(16) << 20;

enum class TextProblem {
    None,
    Empty,         // nothing typed yet
    TooLarge,      // over kTextLimit
    InputEncoding, // encoding: a character the chosen text encoding cannot hold
    Decode,        // decoding: the Base64 is not valid under the chosen rules
    OutputText,    // decoding: the bytes are not text in the chosen encoding (bytes stay available)
};

struct TextOutcome
{
    TextProblem   problem = TextProblem::None;
    std::string   output;                // UTF-8: Base64 when encoding, the text when decoding
    Bytes         bytes;                 // the plain side
    std::size_t   input_characters  = 0; // code points
    std::size_t   output_characters = 0; // code points
    std::string   sha256;                // of bytes
    bool          verified  = false;     // encoding: the output decodes back to the same bytes
    bool          canonical = false;     // decoding: the input is exactly the canonical form for these options
    DecodeError   decode_error = DecodeError::None;
    std::size_t   offset    = 0; // code points into the input (Decode, InputEncoding) or bytes into bytes (OutputText)
    std::uint32_t character = 0; // the character found at offset (Decode, InputEncoding)
};

TextOutcome run_text(std::string_view input_utf8, const TextOptions &options);

std::size_t code_points(std::string_view utf8);

// "62 72 61 63" for the first max_bytes bytes; "…" is appended when cut short.
std::string hex_preview(const Bytes &bytes, std::size_t max_bytes);
// Classic 16-bytes-per-line dump with offsets and an ASCII column.
std::string hex_dump(const Bytes &bytes, std::size_t max_bytes);

struct Excerpt
{
    std::string before, at, after; // UTF-8; `at` is the one character at the offset ("" at the end)
    bool        clipped_before = false, clipped_after = false;
};
// Up to `context` code points either side of a code-point offset. Line breaks
// and tabs are shown as visible symbols so the excerpt stays on one line.
Excerpt     excerpt_around(std::string_view utf8, std::size_t offset, std::size_t context);
// The input without the code point at offset (unchanged when out of range).
std::string remove_at(std::string_view utf8, std::size_t offset);

// "31 bytes", "86 KB", "1.38 MB", "2.41 GB": 1024-based, whole KB from 10 KB up.
std::string human_size(std::uint64_t bytes);
// "3f1c…a20e" for a 64-character digest; shorter input is returned as is.
std::string short_hash(const std::string &hex);
// Whole percent, 0 to 100.
int percent(std::uint64_t done, std::uint64_t total);
// Seconds left at the average rate so far, or nothing while it cannot be estimated.
std::optional<int> seconds_left(std::uint64_t done, std::uint64_t total, double elapsed_seconds);

// Undo and redo for the Text tab: the typed text and every option, in memory
// only, 50 states at most.
struct TextState
{
    std::string input;
    TextOptions options;
    bool operator==(const TextState &o) const { return input == o.input && options == o.options; }
};

class UndoStack
{
public:
    static constexpr std::size_t kLimit = 50;
    // Records a new current state; a state equal to the current one is ignored.
    void                     commit(const TextState &state);
    bool                     can_undo() const { return m_position > 0; }
    bool                     can_redo() const { return m_position + 1 < m_states.size(); }
    std::optional<TextState> undo();
    std::optional<TextState> redo();
    std::size_t              size() const { return m_states.size(); }

private:
    std::vector<TextState> m_states;
    std::size_t            m_position = 0;
};

// One history row. Names, sizes and hashes only, unless the person turned on
// Keep payloads: then a text operation also keeps its input.
struct HistoryEntry
{
    std::int64_t  time = 0; // seconds since 1970-01-01 UTC
    std::string   action;   // encode-text, decode-text, encode-file, decode-file, build-url, read-url
    std::string   name;     // a file name, a MIME type or a short label; never the payload
    std::uint64_t bytes_in  = 0;
    std::uint64_t bytes_out = 0;
    std::string   sha256;
    bool          has_options = false;
    TextOptions   options;
    bool          has_payload = false;
    std::string   payload;
};

std::string options_json(const TextOptions &options);
bool        parse_options_json(std::string_view json, TextOptions &options);
std::string to_line(const HistoryEntry &entry);
bool        from_line(std::string_view line, HistoryEntry &entry);

class History
{
public:
    static constexpr std::size_t kLimit = 500;
    explicit History(std::filesystem::path file = {}) : m_file(std::move(file)) {}
    void load();
    // Appends and saves; keep_payload false drops the payload before anything is kept.
    void append(HistoryEntry entry, bool keep_payload);
    void clear();
    // Drops every kept payload, in memory and on disk.
    void forget_payloads();
    const std::vector<HistoryEntry> &entries() const { return m_entries; } // oldest first
    std::string                      export_json() const;
    const std::filesystem::path     &file() const { return m_file; }

private:
    void                      save() const;
    std::filesystem::path     m_file;
    std::vector<HistoryEntry> m_entries;
};

// The record Export record (JSON) writes: options, sizes and hashes, plus the
// input and output only when include_payload is true.
std::string record_json(const TextOptions &options, const TextOutcome &outcome, std::int64_t time,
                        const std::string *input_when_included);

}}} // namespace Slic3r::GUI::Base64Toolkit

#endif // slic3r_GUI_Base64Toolkit_Base64Workbench_hpp_
