#ifndef slic3r_GUI_Base64Toolkit_Base64Codec_hpp_
#define slic3r_GUI_Base64Toolkit_Base64Codec_hpp_

// The local Base64 toolkit's codec. It has no wxWidgets dependency so it can be
// tested on its own (tests/base64_codec_test.cpp): RFC 4648 standard and URL-safe
// alphabets, optional padding, MIME or custom line wrapping, a strict decoder that
// names the exact offset of the first bad character and a lenient one that ignores
// whitespace and missing padding, chunked encoders and decoders for files larger
// than memory, text encodings, data: URLs, bounded byte-signature detection, the
// same-folder file naming rules and SHA-256 for round-trip verification.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI { namespace Base64Toolkit {

using Bytes = std::vector<std::uint8_t>;

enum class Alphabet { Standard, UrlSafe };
enum class TextEncoding { Utf8, Utf16LE, Utf16BE, Latin1 };

struct EncodeOptions
{
    Alphabet alphabet    = Alphabet::Standard;
    bool     padding     = true;
    // 0 writes one line. 76 with crlf is MIME (RFC 2045); any other positive
    // value wraps at that many characters.
    int      line_length = 0;
    bool     crlf        = true;
};

struct DecodeOptions
{
    Alphabet alphabet        = Alphabet::Standard;
    // Strict: every character must belong to the alphabet (whitespace included),
    // padding must be exactly right and unused trailing bits must be zero.
    // Lenient: whitespace and line breaks are skipped and padding may be missing.
    bool     strict          = true;
    // Strict mode only: whether the canonical form carries '=' padding.
    bool     require_padding = true;
};

enum class DecodeError {
    None,
    Empty,            // nothing to decode
    InvalidCharacter, // a character outside the alphabet
    WrongPadding,     // '=' in the wrong place, missing in strict mode, or non-zero unused bits
    TrailingData,     // data after the closing padding
    TruncatedInput,   // one lone character in the last group can never decode
};

struct DecodeResult
{
    Bytes         bytes;
    DecodeError   error = DecodeError::None;
    // Offset of the problem in Unicode code points from the start of the input,
    // and the character found there (0 at the end of the input).
    std::size_t   offset    = 0;
    std::uint32_t character = 0;

    bool ok() const { return error == DecodeError::None; }
};

std::string  encode(const std::uint8_t *data, std::size_t size, const EncodeOptions &options);
inline std::string encode(const Bytes &data, const EncodeOptions &options) { return encode(data.data(), data.size(), options); }
DecodeResult decode(std::string_view text, const DecodeOptions &options);
// Stable ASCII name of an error, for tests and logs. Contains no input data.
const char  *error_name(DecodeError error);

// Encodes a stream in chunks of any size; the concatenated output equals
// encode() over the whole input, line wrapping included.
class StreamEncoder
{
public:
    explicit StreamEncoder(EncodeOptions options) : m_options(options) {}
    std::string feed(const std::uint8_t *data, std::size_t size);
    std::string finish();
    std::uint64_t bytes_in() const { return m_bytes_in; }

private:
    void put(char c, std::string &out);
    EncodeOptions m_options;
    std::uint8_t  m_pending[3] {};
    int           m_pending_size = 0;
    int           m_column       = 0;
    std::uint64_t m_bytes_in     = 0;
};

// Decodes a stream of text in chunks of any size; the result equals decode()
// over the whole text. Offsets are counted across chunks. feed() returns false
// after the first error, which stays available from result().
class StreamDecoder
{
public:
    explicit StreamDecoder(DecodeOptions options) : m_options(options) {}
    bool feed(std::string_view text, Bytes &out);
    bool finish(Bytes &out);
    DecodeError   error() const { return m_error; }
    std::size_t   error_offset() const { return m_error_offset; }
    std::uint32_t error_character() const { return m_error_character; }
    std::uint64_t characters_in() const { return m_offset; }

private:
    bool fail(DecodeError error, std::uint32_t character);
    void flush_quad(Bytes &out);
    DecodeOptions m_options;
    std::uint8_t  m_quad[4] {};
    int           m_quad_size     = 0;
    int           m_padding       = 0;
    bool          m_closed        = false; // padding seen: only padding (or whitespace when lenient) may follow
    bool          m_any           = false;
    std::size_t   m_offset        = 0;     // code points consumed so far
    std::size_t   m_padding_start = 0;
    std::string   m_utf8_tail;             // an incomplete UTF-8 sequence split across chunks
    DecodeError   m_error         = DecodeError::None;
    std::size_t   m_error_offset  = 0;
    std::uint32_t m_error_character = 0;
};

// Text <-> bytes. Input and output text are UTF-8. On failure bad_offset is the
// code point (text_to_bytes) or byte (bytes_to_text) offset of the problem.
bool text_to_bytes(std::string_view utf8, TextEncoding encoding, Bytes &out, std::size_t &bad_offset);
bool bytes_to_text(const Bytes &bytes, TextEncoding encoding, std::string &utf8, std::size_t &bad_offset);
const char *encoding_label(TextEncoding encoding);

// data: URLs (RFC 2397).
struct DataUrl
{
    std::string                                      mime = "text/plain";
    std::vector<std::pair<std::string, std::string>> parameters;
    bool                                             base64 = false;
    Bytes                                            bytes;
};
std::string build_data_url(const std::string &mime, const Bytes &bytes, const std::string &charset = {});
// error names the problem in plain ASCII when parsing fails.
bool        parse_data_url(std::string_view text, DataUrl &out, std::string &error);

// Bounded byte-signature inspection (the first 4 KiB at most).
struct DetectedType
{
    std::string mime      = "application/octet-stream";
    std::string extension = ".bin";
    std::string label     = "Binary data";
    enum class Preview { None, Text, Image, Audio, Video } preview = Preview::None;
};
DetectedType detect_type(const std::uint8_t *data, std::size_t size);
inline DetectedType detect_type(const Bytes &data) { return detect_type(data.data(), data.size()); }
std::string  extension_for_mime(const std::string &mime);

// Same-folder naming. encoded_path_for("D:/a/report.pdf") is
// "D:/a/report.pdf.base64.txt"; a clash numbers the original stem
// ("report (1).pdf.base64.txt") so the pattern still restores. Neither function
// ever returns an existing path.
std::filesystem::path encoded_path_for(const std::filesystem::path &source);
// restored_path_for picks the name from the ".base64.txt"/".b64" pattern first,
// then the data: MIME type (when known), then the detected type, then ".bin".
std::filesystem::path restored_path_for(const std::filesystem::path &encoded, const std::string &data_url_mime,
                                        const DetectedType &detected);
// The name restored_path_for starts from, before any numbering. It may exist.
std::filesystem::path restored_name_for(const std::filesystem::path &encoded, const std::string &data_url_mime,
                                        const DetectedType &detected);
std::filesystem::path non_clashing(const std::filesystem::path &wanted);
bool                  looks_like_base64_file(const std::filesystem::path &path);

// SHA-256 (FIPS 180-4).
class Sha256
{
public:
    Sha256();
    void                         update(const std::uint8_t *data, std::size_t size);
    std::array<std::uint8_t, 32> finish();

private:
    void                         block(const std::uint8_t *chunk);
    std::array<std::uint32_t, 8> m_state;
    std::uint8_t                 m_buffer[64] {};
    std::size_t                  m_buffer_size = 0;
    std::uint64_t                m_length      = 0;
};
std::string hex(const std::array<std::uint8_t, 32> &digest);
std::string sha256_hex(const std::uint8_t *data, std::size_t size);

}}} // namespace Slic3r::GUI::Base64Toolkit

#endif
