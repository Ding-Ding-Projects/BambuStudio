#include "Base64Codec.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace Slic3r { namespace GUI { namespace Base64Toolkit {

namespace {

const char *alphabet_chars(Alphabet alphabet)
{
    return alphabet == Alphabet::UrlSafe ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
                                         : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

int value_of(std::uint32_t c, Alphabet alphabet)
{
    if (c >= 'A' && c <= 'Z') return int(c - 'A');
    if (c >= 'a' && c <= 'z') return int(c - 'a') + 26;
    if (c >= '0' && c <= '9') return int(c - '0') + 52;
    if (alphabet == Alphabet::Standard) {
        if (c == '+') return 62;
        if (c == '/') return 63;
    } else {
        if (c == '-') return 62;
        if (c == '_') return 63;
    }
    return -1;
}

bool is_space(std::uint32_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

// Decodes one UTF-8 code point at text[i] and returns its length. Returns 0 when
// a sequence that is valid so far is cut off by the end of text (the caller
// waits for more input), and sets cp to kInvalid with length 1 for an invalid
// byte, an overlong form, a surrogate or a value above U+10FFFF.
constexpr std::uint32_t kInvalid = 0xFFFFFFFFu;
std::size_t next_code_point(std::string_view text, std::size_t i, std::uint32_t &cp)
{
    const auto b0 = static_cast<unsigned char>(text[i]);
    if (b0 < 0x80) { cp = b0; return 1; }
    std::size_t   need = 0;
    std::uint32_t min  = 0;
    if ((b0 & 0xE0) == 0xC0) { need = 1; cp = b0 & 0x1F; min = 0x80; }
    else if ((b0 & 0xF0) == 0xE0) { need = 2; cp = b0 & 0x0F; min = 0x800; }
    else if ((b0 & 0xF8) == 0xF0) { need = 3; cp = b0 & 0x07; min = 0x10000; }
    else { cp = kInvalid; return 1; }
    const std::size_t available = text.size() - i - 1;
    const std::size_t check     = std::min(need, available);
    for (std::size_t k = 1; k <= check; ++k)
        if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) { cp = kInvalid; return 1; }
    if (available < need) return 0;
    for (std::size_t k = 1; k <= need; ++k) cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { cp = kInvalid; return 1; }
    return need + 1;
}

void append_utf8(std::string &out, std::uint32_t cp)
{
    if (cp < 0x80) out.push_back(char(cp));
    else if (cp < 0x800) { out.push_back(char(0xC0 | (cp >> 6))); out.push_back(char(0x80 | (cp & 0x3F))); }
    else if (cp < 0x10000) {
        out.push_back(char(0xE0 | (cp >> 12)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(char(0xF0 | (cp >> 18)));
        out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    }
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}

bool ends_with_nocase(const std::string &value, const std::string &suffix)
{
    return value.size() >= suffix.size() && lower(value.substr(value.size() - suffix.size())) == suffix;
}

bool starts_with(const std::uint8_t *data, std::size_t size, const char *magic, std::size_t magic_size, std::size_t at = 0)
{
    return size >= at + magic_size && std::memcmp(data + at, magic, magic_size) == 0;
}

// "report.pdf" with n=1 becomes "report (1).pdf"; "notes" becomes "notes (1)".
std::string numbered(const std::string &name, int n)
{
    if (n <= 0) return name;
    const std::string tag = " (" + std::to_string(n) + ")";
    const auto        dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return name + tag;
    return name.substr(0, dot) + tag + name.substr(dot);
}

bool path_exists(const std::filesystem::path &path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) || ec;
}

} // namespace

const char *error_name(DecodeError error)
{
    switch (error) {
    case DecodeError::None: return "none";
    case DecodeError::Empty: return "empty";
    case DecodeError::InvalidCharacter: return "invalid-character";
    case DecodeError::WrongPadding: return "wrong-padding";
    case DecodeError::TrailingData: return "trailing-data";
    case DecodeError::TruncatedInput: return "truncated-input";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Encoding

void StreamEncoder::put(char c, std::string &out)
{
    if (m_options.line_length > 0 && m_column == m_options.line_length) {
        out += m_options.crlf ? "\r\n" : "\n";
        m_column = 0;
    }
    out.push_back(c);
    ++m_column;
}

std::string StreamEncoder::feed(const std::uint8_t *data, std::size_t size)
{
    std::string out;
    out.reserve((size / 3 + 2) * 4 + (m_options.line_length > 0 ? size / std::size_t(m_options.line_length) * 2 + 2 : 0));
    const char *chars = alphabet_chars(m_options.alphabet);
    m_bytes_in += size;
    std::size_t i = 0;
    while (i < size) {
        m_pending[m_pending_size++] = data[i++];
        if (m_pending_size == 3) {
            const std::uint32_t v = (std::uint32_t(m_pending[0]) << 16) | (std::uint32_t(m_pending[1]) << 8) | m_pending[2];
            put(chars[(v >> 18) & 63], out);
            put(chars[(v >> 12) & 63], out);
            put(chars[(v >> 6) & 63], out);
            put(chars[v & 63], out);
            m_pending_size = 0;
        }
    }
    return out;
}

std::string StreamEncoder::finish()
{
    std::string out;
    const char *chars = alphabet_chars(m_options.alphabet);
    // The negative regression in tests/base64_codec.test.mjs compiles this file
    // once with SLIC3R_BASE64_NEGATIVE_FIXTURE_DROP_PADDING and expects the codec
    // tests to fail; production builds never define it.
#ifdef SLIC3R_BASE64_NEGATIVE_FIXTURE_DROP_PADDING
    const bool padding = false;
#else
    const bool padding = m_options.padding;
#endif
    if (m_pending_size == 1) {
        const std::uint32_t v = std::uint32_t(m_pending[0]) << 16;
        put(chars[(v >> 18) & 63], out);
        put(chars[(v >> 12) & 63], out);
        if (padding) { put('=', out); put('=', out); }
    } else if (m_pending_size == 2) {
        const std::uint32_t v = (std::uint32_t(m_pending[0]) << 16) | (std::uint32_t(m_pending[1]) << 8);
        put(chars[(v >> 18) & 63], out);
        put(chars[(v >> 12) & 63], out);
        put(chars[(v >> 6) & 63], out);
        if (padding) put('=', out);
    }
    m_pending_size = 0;
    return out;
}

std::string encode(const std::uint8_t *data, std::size_t size, const EncodeOptions &options)
{
    StreamEncoder encoder(options);
    std::string   out = encoder.feed(data, size);
    out += encoder.finish();
    return out;
}

// ---------------------------------------------------------------------------
// Decoding

bool StreamDecoder::fail(DecodeError error, std::uint32_t character)
{
    if (m_error == DecodeError::None) {
        m_error           = error;
        m_error_offset    = m_offset;
        m_error_character = character;
    }
    return false;
}

void StreamDecoder::flush_quad(Bytes &out)
{
    const int data_chars = m_quad_size;
    std::uint32_t v = 0;
    for (int k = 0; k < 4; ++k) v = (v << 6) | (k < data_chars ? m_quad[k] : 0);
    if (data_chars >= 2) out.push_back(std::uint8_t((v >> 16) & 0xFF));
    if (data_chars >= 3) out.push_back(std::uint8_t((v >> 8) & 0xFF));
    if (data_chars >= 4) out.push_back(std::uint8_t(v & 0xFF));
    m_quad_size = 0;
}

bool StreamDecoder::feed(std::string_view chunk, Bytes &out)
{
    if (m_error != DecodeError::None) return false;
    std::string joined;
    std::string_view text = chunk;
    if (!m_utf8_tail.empty()) {
        joined = m_utf8_tail;
        joined.append(chunk.data(), chunk.size());
        m_utf8_tail.clear();
        text = joined;
    }
    std::size_t i = 0;
    while (i < text.size()) {
        std::uint32_t cp  = 0;
        const auto    len = next_code_point(text, i, cp);
        if (len == 0) { m_utf8_tail.assign(text.data() + i, text.size() - i); break; }
        i += len;
        if (is_space(cp)) {
            if (m_options.strict) return fail(DecodeError::InvalidCharacter, cp);
            ++m_offset;
            continue;
        }
        if (cp == '=') {
            if (m_options.strict && !m_options.require_padding) return fail(DecodeError::WrongPadding, cp);
            if (!m_closed) {
                if (m_quad_size < 2) return fail(DecodeError::WrongPadding, cp);
                m_closed        = true;
                m_padding_start = m_offset;
            }
            ++m_padding;
            if (m_quad_size + m_padding > 4) return fail(DecodeError::WrongPadding, cp);
            ++m_offset;
            continue;
        }
        const int value = cp == kInvalid ? -1 : value_of(cp, m_options.alphabet);
        if (value < 0) return fail(DecodeError::InvalidCharacter, cp == kInvalid ? 0xFFFD : cp);
        if (m_closed) return fail(DecodeError::TrailingData, cp);
        m_any = true;
        m_quad[m_quad_size++] = std::uint8_t(value);
        if (m_quad_size == 4) flush_quad(out);
        ++m_offset;
    }
    return true;
}

bool StreamDecoder::finish(Bytes &out)
{
    if (m_error != DecodeError::None) return false;
    if (!m_utf8_tail.empty()) return fail(DecodeError::InvalidCharacter, 0xFFFD);
    if (!m_any && m_padding == 0) return fail(DecodeError::Empty, 0);
    if (m_quad_size == 0 && m_padding == 0) return true;
    if (m_quad_size == 1) return fail(DecodeError::TruncatedInput, 0);
    if (m_quad_size == 0 && m_padding > 0) return fail(DecodeError::WrongPadding, '=');
    const int expected_padding = 4 - m_quad_size;
    if (m_options.strict) {
        if (m_options.require_padding && m_padding != expected_padding) return fail(DecodeError::WrongPadding, m_padding ? '=' : 0);
        // Unused low bits of the last character must be zero in canonical form.
        const std::uint8_t last = m_quad[m_quad_size - 1];
        const std::uint8_t mask = m_quad_size == 2 ? 0x0F : 0x03;
        if (last & mask) {
            m_offset = m_padding ? m_padding_start - 1 : m_offset - 1;
            return fail(DecodeError::WrongPadding, std::uint32_t(alphabet_chars(m_options.alphabet)[last]));
        }
    } else if (m_padding > expected_padding) {
        return fail(DecodeError::WrongPadding, '=');
    }
    flush_quad(out);
    m_padding = 0;
    return true;
}

DecodeResult decode(std::string_view text, const DecodeOptions &options)
{
    DecodeResult  result;
    StreamDecoder decoder(options);
    if (!decoder.feed(text, result.bytes) || !decoder.finish(result.bytes)) {
        result.bytes.clear();
        result.error     = decoder.error();
        result.offset    = decoder.error_offset();
        result.character = decoder.error_character();
    }
    return result;
}

// ---------------------------------------------------------------------------
// Text encodings

const char *encoding_label(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Utf8: return "UTF-8";
    case TextEncoding::Utf16LE: return "UTF-16 LE";
    case TextEncoding::Utf16BE: return "UTF-16 BE";
    case TextEncoding::Latin1: return "Latin-1";
    }
    return "UTF-8";
}

bool text_to_bytes(std::string_view utf8, TextEncoding encoding, Bytes &out, std::size_t &bad_offset)
{
    out.clear();
    std::size_t index = 0;
    for (std::size_t i = 0; i < utf8.size(); ++index) {
        std::uint32_t cp  = 0;
        const auto    len = next_code_point(utf8, i, cp);
        if (len == 0 || cp == kInvalid) {
            bad_offset = index;
            return false;
        }
        switch (encoding) {
        case TextEncoding::Utf8: out.insert(out.end(), utf8.begin() + i, utf8.begin() + i + len); break;
        case TextEncoding::Latin1:
            if (cp > 0xFF) { bad_offset = index; return false; }
            out.push_back(std::uint8_t(cp));
            break;
        case TextEncoding::Utf16LE:
        case TextEncoding::Utf16BE: {
            auto unit = [&](std::uint16_t u) {
                if (encoding == TextEncoding::Utf16LE) { out.push_back(std::uint8_t(u & 0xFF)); out.push_back(std::uint8_t(u >> 8)); }
                else { out.push_back(std::uint8_t(u >> 8)); out.push_back(std::uint8_t(u & 0xFF)); }
            };
            if (cp >= 0x10000) {
                const std::uint32_t v = cp - 0x10000;
                unit(std::uint16_t(0xD800 | (v >> 10)));
                unit(std::uint16_t(0xDC00 | (v & 0x3FF)));
            } else unit(std::uint16_t(cp));
            break;
        }
        }
        i += len;
    }
    return true;
}

bool bytes_to_text(const Bytes &bytes, TextEncoding encoding, std::string &utf8, std::size_t &bad_offset)
{
    utf8.clear();
    switch (encoding) {
    case TextEncoding::Latin1:
        for (const auto b : bytes) append_utf8(utf8, b);
        return true;
    case TextEncoding::Utf8: {
        const std::string_view view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        for (std::size_t i = 0; i < view.size();) {
            std::uint32_t cp  = 0;
            const auto    len = next_code_point(view, i, cp);
            if (len == 0 || cp == kInvalid) { bad_offset = i; return false; }
            utf8.append(view.data() + i, len);
            i += len;
        }
        return true;
    }
    case TextEncoding::Utf16LE:
    case TextEncoding::Utf16BE: {
        if (bytes.size() % 2) { bad_offset = bytes.size() - 1; return false; }
        auto unit_at = [&](std::size_t k) -> std::uint16_t {
            return encoding == TextEncoding::Utf16LE ? std::uint16_t(bytes[k] | (bytes[k + 1] << 8))
                                                     : std::uint16_t((bytes[k] << 8) | bytes[k + 1]);
        };
        for (std::size_t k = 0; k < bytes.size(); k += 2) {
            const std::uint16_t u = unit_at(k);
            if (u >= 0xD800 && u <= 0xDBFF) {
                if (k + 3 >= bytes.size()) { bad_offset = k; return false; }
                const std::uint16_t low = unit_at(k + 2);
                if (low < 0xDC00 || low > 0xDFFF) { bad_offset = k; return false; }
                append_utf8(utf8, 0x10000 + ((std::uint32_t(u - 0xD800) << 10) | std::uint32_t(low - 0xDC00)));
                k += 2;
            } else if (u >= 0xDC00 && u <= 0xDFFF) {
                bad_offset = k;
                return false;
            } else append_utf8(utf8, u);
        }
        return true;
    }
    }
    return false;
}

// ---------------------------------------------------------------------------
// data: URLs

std::string build_data_url(const std::string &mime, const Bytes &bytes, const std::string &charset)
{
    std::string url = "data:" + (mime.empty() ? std::string("application/octet-stream") : mime);
    if (!charset.empty()) url += ";charset=" + charset;
    url += ";base64,";
    url += encode(bytes, EncodeOptions{});
    return url;
}

bool parse_data_url(std::string_view text, DataUrl &out, std::string &error)
{
    out = DataUrl{};
    while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    if (text.size() < 5 || lower(std::string(text.substr(0, 5))) != "data:") { error = "not-a-data-url"; return false; }
    const auto comma = text.find(',');
    if (comma == std::string_view::npos) { error = "missing-comma"; return false; }
    const std::string header(text.substr(5, comma - 5));
    std::string_view  payload = text.substr(comma + 1);
    std::size_t       start   = 0;
    bool              first   = true;
    while (start <= header.size()) {
        const auto  semi  = header.find(';', start);
        std::string token = header.substr(start, semi == std::string::npos ? std::string::npos : semi - start);
        if (first) {
            if (!token.empty()) {
                if (token.find('/') == std::string::npos) { error = "invalid-media-type"; return false; }
                out.mime = lower(token);
            }
            first = false;
        } else if (lower(token) == "base64") {
            out.base64 = true;
        } else if (!token.empty()) {
            const auto eq = token.find('=');
            if (eq == std::string::npos) { error = "invalid-parameter"; return false; }
            out.parameters.emplace_back(lower(token.substr(0, eq)), token.substr(eq + 1));
        }
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    if (header.empty() || header.front() == ';')
        if (out.parameters.empty()) out.parameters.emplace_back("charset", "US-ASCII");
    // Percent-decoding applies to both forms (RFC 2397 uses URL syntax).
    std::string raw;
    raw.reserve(payload.size());
    for (std::size_t i = 0; i < payload.size(); ++i) {
        if (payload[i] == '%') {
            if (i + 2 >= payload.size() || !std::isxdigit(static_cast<unsigned char>(payload[i + 1])) ||
                !std::isxdigit(static_cast<unsigned char>(payload[i + 2]))) { error = "invalid-percent-escape"; return false; }
            raw.push_back(char(std::stoi(std::string(payload.substr(i + 1, 2)), nullptr, 16)));
            i += 2;
        } else raw.push_back(payload[i]);
    }
    if (!out.base64) {
        out.bytes.assign(raw.begin(), raw.end());
        return true;
    }
    DecodeOptions options;
    options.strict = false;
    auto decoded   = decode(raw, options);
    if (!decoded.ok()) {
        options.alphabet = Alphabet::UrlSafe;
        decoded          = decode(raw, options);
    }
    if (!decoded.ok() && decoded.error != DecodeError::Empty) { error = std::string("base64-") + error_name(decoded.error); return false; }
    out.bytes = std::move(decoded.bytes);
    return true;
}

// ---------------------------------------------------------------------------
// Type detection

DetectedType detect_type(const std::uint8_t *data, std::size_t size_in)
{
    const std::size_t size = std::min<std::size_t>(size_in, 4096);
    DetectedType      t;
    auto set = [&](const char *mime, const char *ext, const char *label, DetectedType::Preview preview) {
        t.mime = mime; t.extension = ext; t.label = label; t.preview = preview;
        return t;
    };
    using P = DetectedType::Preview;
    if (starts_with(data, size, "\x89PNG\r\n\x1a\n", 8)) return set("image/png", ".png", "PNG image", P::Image);
    if (starts_with(data, size, "\xFF\xD8\xFF", 3)) return set("image/jpeg", ".jpg", "JPEG image", P::Image);
    if (starts_with(data, size, "GIF87a", 6) || starts_with(data, size, "GIF89a", 6)) return set("image/gif", ".gif", "GIF image", P::Image);
    if (starts_with(data, size, "RIFF", 4) && starts_with(data, size, "WEBP", 4, 8)) return set("image/webp", ".webp", "WebP image", P::Image);
    if (starts_with(data, size, "RIFF", 4) && starts_with(data, size, "WAVE", 4, 8)) return set("audio/wav", ".wav", "WAV audio", P::Audio);
    if (starts_with(data, size, "RIFF", 4) && starts_with(data, size, "AVI ", 4, 8)) return set("video/x-msvideo", ".avi", "AVI video", P::Video);
    if (starts_with(data, size, "BM", 2) && size > 14) return set("image/bmp", ".bmp", "BMP image", P::Image);
    if (starts_with(data, size, "\x00\x00\x01\x00", 4)) return set("image/x-icon", ".ico", "Icon", P::Image);
    if (starts_with(data, size, "II*\x00", 4) || starts_with(data, size, "MM\x00*", 4)) return set("image/tiff", ".tif", "TIFF image", P::Image);
    if (starts_with(data, size, "%PDF-", 5)) return set("application/pdf", ".pdf", "PDF document", P::None);
    if (starts_with(data, size, "PK\x03\x04", 4)) {
        const std::string head(reinterpret_cast<const char *>(data), size);
        if (head.find("3D/3dmodel.model") != std::string::npos ||
            (head.find("[Content_Types].xml") != std::string::npos && head.find("3D/") != std::string::npos))
            return set("model/3mf", ".3mf", "3MF project", P::None);
        return set("application/zip", ".zip", "ZIP archive", P::None);
    }
    if (starts_with(data, size, "\x1F\x8B", 2)) return set("application/gzip", ".gz", "Gzip archive", P::None);
    if (starts_with(data, size, "7z\xBC\xAF\x27\x1C", 6)) return set("application/x-7z-compressed", ".7z", "7-Zip archive", P::None);
    if (starts_with(data, size, "Rar!\x1A\x07", 6)) return set("application/vnd.rar", ".rar", "RAR archive", P::None);
    if (starts_with(data, size, "OggS", 4)) return set("audio/ogg", ".ogg", "Ogg audio", P::Audio);
    if (starts_with(data, size, "fLaC", 4)) return set("audio/flac", ".flac", "FLAC audio", P::Audio);
    if (starts_with(data, size, "ID3", 3) || (size >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0 && (data[1] & 0x06) != 0))
        return set("audio/mpeg", ".mp3", "MP3 audio", P::Audio);
    if (starts_with(data, size, "ftyp", 4, 4)) {
        if (starts_with(data, size, "qt  ", 4, 8)) return set("video/quicktime", ".mov", "QuickTime video", P::Video);
        if (starts_with(data, size, "M4A ", 4, 8)) return set("audio/mp4", ".m4a", "M4A audio", P::Audio);
        return set("video/mp4", ".mp4", "MP4 video", P::Video);
    }
    if (starts_with(data, size, "\x1A\x45\xDF\xA3", 4)) {
        const std::string head(reinterpret_cast<const char *>(data), std::min<std::size_t>(size, 64));
        if (head.find("webm") != std::string::npos) return set("video/webm", ".webm", "WebM video", P::Video);
        return set("video/x-matroska", ".mkv", "Matroska video", P::Video);
    }
    // Text family: valid UTF-8 (a cut final sequence allowed) without NUL bytes.
    const std::string_view view(reinterpret_cast<const char *>(data), size);
    bool text = size > 0;
    for (std::size_t i = 0; text && i < view.size();) {
        if (view[i] == '\0') { text = false; break; }
        std::uint32_t cp  = 0;
        const auto    len = next_code_point(view, i, cp);
        if (len == 0) break;
        if (cp == kInvalid) { text = false; break; }
        i += len;
    }
    if (!text) return t;
    std::size_t s = 0;
    if (starts_with(data, size, "\xEF\xBB\xBF", 3)) s = 3;
    while (s < view.size() && is_space(static_cast<unsigned char>(view[s]))) ++s;
    const std::string body  = lower(std::string(view.substr(s, std::min<std::size_t>(1024, view.size() - s))));
    if (body.rfind("<svg", 0) == 0 || (body.rfind("<?xml", 0) == 0 && body.find("<svg") != std::string::npos))
        return set("image/svg+xml", ".svg", "SVG image", P::Text);
    if (body.rfind("<?xml", 0) == 0) return set("application/xml", ".xml", "XML document", P::Text);
    if (body.rfind("<!doctype html", 0) == 0 || body.rfind("<html", 0) == 0) return set("text/html", ".html", "HTML document", P::Text);
    if (!body.empty() && (body[0] == '{' || body[0] == '[')) return set("application/json", ".json", "JSON document", P::Text);
    if (body.rfind("solid", 0) == 0 && body.find("facet") != std::string::npos) return set("model/stl", ".stl", "ASCII STL model", P::Text);
    if (body.find("; generated by") != std::string::npos || body.find("\ng1 ") != std::string::npos || body.rfind("g1 ", 0) == 0 ||
        body.rfind("; header_block_start", 0) == 0)
        return set("text/x-gcode", ".gcode", "G-code", P::Text);
    return set("text/plain", ".txt", "Plain text", P::Text);
}

std::string extension_for_mime(const std::string &mime_in)
{
    const std::string mime = lower(mime_in.substr(0, mime_in.find(';')));
    static const std::pair<const char *, const char *> table[] = {
        {"image/png", ".png"}, {"image/jpeg", ".jpg"}, {"image/gif", ".gif"}, {"image/webp", ".webp"},
        {"image/bmp", ".bmp"}, {"image/svg+xml", ".svg"}, {"image/x-icon", ".ico"}, {"image/tiff", ".tif"},
        {"application/pdf", ".pdf"}, {"application/zip", ".zip"}, {"application/json", ".json"},
        {"application/xml", ".xml"}, {"text/xml", ".xml"}, {"text/html", ".html"}, {"text/plain", ".txt"},
        {"text/csv", ".csv"}, {"text/x-gcode", ".gcode"}, {"model/stl", ".stl"}, {"model/3mf", ".3mf"},
        {"audio/mpeg", ".mp3"}, {"audio/wav", ".wav"}, {"audio/ogg", ".ogg"}, {"audio/flac", ".flac"},
        {"audio/mp4", ".m4a"}, {"video/mp4", ".mp4"}, {"video/webm", ".webm"}, {"video/quicktime", ".mov"},
        {"application/gzip", ".gz"}, {"application/x-7z-compressed", ".7z"}, {"application/vnd.rar", ".rar"},
        {"application/octet-stream", ".bin"},
    };
    for (const auto &[m, e] : table)
        if (mime == m) return e;
    return {};
}

// ---------------------------------------------------------------------------
// Naming

bool looks_like_base64_file(const std::filesystem::path &path)
{
    const std::string name = path.filename().u8string();
    return ends_with_nocase(name, ".base64.txt") || ends_with_nocase(name, ".b64") || ends_with_nocase(name, ".txt");
}

std::filesystem::path non_clashing(const std::filesystem::path &wanted)
{
    if (!path_exists(wanted)) return wanted;
    const std::string name = wanted.filename().u8string();
    for (int n = 1; n < 100000; ++n) {
        const auto candidate = wanted.parent_path() / std::filesystem::u8path(numbered(name, n));
        if (!path_exists(candidate)) return candidate;
    }
    return {};
}

std::filesystem::path encoded_path_for(const std::filesystem::path &source)
{
    const std::string name = source.filename().u8string();
    for (int n = 0; n < 100000; ++n) {
        const auto candidate = source.parent_path() / std::filesystem::u8path(numbered(name, n) + ".base64.txt");
        if (!path_exists(candidate)) return candidate;
    }
    return {};
}

std::filesystem::path restored_path_for(const std::filesystem::path &encoded, const std::string &data_url_mime,
                                        const DetectedType &detected)
{
    return non_clashing(restored_name_for(encoded, data_url_mime, detected));
}

std::filesystem::path restored_name_for(const std::filesystem::path &encoded, const std::string &data_url_mime,
                                        const DetectedType &detected)
{
    std::string name = encoded.filename().u8string();
    std::string original;
    if (ends_with_nocase(name, ".base64.txt")) original = name.substr(0, name.size() - 11);
    else if (ends_with_nocase(name, ".b64")) original = name.substr(0, name.size() - 4);
    else if (ends_with_nocase(name, ".txt")) original = name.substr(0, name.size() - 4);
    else original = name;
    if (original.empty()) original = "decoded";
    const auto dot = original.find_last_of('.');
    const bool has_extension = dot != std::string::npos && dot > 0 && dot + 1 < original.size() &&
                               original.size() - dot <= 10;
    if (!has_extension) {
        std::string ext = data_url_mime.empty() ? std::string() : extension_for_mime(data_url_mime);
        if (ext.empty()) ext = detected.extension.empty() ? std::string(".bin") : detected.extension;
        original += ext;
    }
    return encoded.parent_path() / std::filesystem::u8path(original);
}

// ---------------------------------------------------------------------------
// SHA-256

namespace {
constexpr std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
} // namespace

Sha256::Sha256()
    : m_state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}
{}

void Sha256::block(const std::uint8_t *p)
{
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = (std::uint32_t(p[i * 4]) << 24) | (std::uint32_t(p[i * 4 + 1]) << 16) | (std::uint32_t(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3], e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
    m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
}

void Sha256::update(const std::uint8_t *data, std::size_t size)
{
    m_length += size;
    while (size > 0) {
        const std::size_t take = std::min<std::size_t>(size, 64 - m_buffer_size);
        std::memcpy(m_buffer + m_buffer_size, data, take);
        m_buffer_size += take;
        data += take;
        size -= take;
        if (m_buffer_size == 64) { block(m_buffer); m_buffer_size = 0; }
    }
}

std::array<std::uint8_t, 32> Sha256::finish()
{
    const std::uint64_t bits = m_length * 8;
    const std::uint8_t  one  = 0x80, zero = 0;
    update(&one, 1);
    while (m_buffer_size != 56) update(&zero, 1);
    std::uint8_t len[8];
    for (int i = 0; i < 8; ++i) len[i] = std::uint8_t(bits >> (56 - 8 * i));
    update(len, 8);
    std::array<std::uint8_t, 32> digest {};
    for (int i = 0; i < 8; ++i)
        for (int k = 0; k < 4; ++k) digest[i * 4 + k] = std::uint8_t(m_state[i] >> (24 - 8 * k));
    return digest;
}

std::string hex(const std::array<std::uint8_t, 32> &digest)
{
    static const char *digits = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (const auto b : digest) { out.push_back(digits[b >> 4]); out.push_back(digits[b & 15]); }
    return out;
}

std::string sha256_hex(const std::uint8_t *data, std::size_t size)
{
    Sha256 sha;
    sha.update(data, size);
    return hex(sha.finish());
}

}}} // namespace Slic3r::GUI::Base64Toolkit
