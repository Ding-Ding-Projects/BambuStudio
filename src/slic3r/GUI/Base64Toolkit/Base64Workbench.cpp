#include "Base64Workbench.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <system_error>

namespace Slic3r { namespace GUI { namespace Base64Toolkit {

namespace {

// Splits UTF-8 into units that start at every byte that is not a continuation
// byte, the same way code_points() counts.
std::vector<std::string_view> units(std::string_view utf8)
{
    std::vector<std::string_view> out;
    std::size_t                   start = 0;
    for (std::size_t i = 1; i <= utf8.size(); ++i) {
        if (i == utf8.size() || (static_cast<unsigned char>(utf8[i]) & 0xC0) != 0x80) {
            out.push_back(utf8.substr(start, i - start));
            start = i;
        }
    }
    return out;
}

std::uint32_t decode_unit(std::string_view unit)
{
    if (unit.empty()) return 0;
    const auto    b0 = static_cast<unsigned char>(unit[0]);
    std::uint32_t cp = 0;
    int           extra = 0;
    if (b0 < 0x80) return b0;
    if ((b0 & 0xE0) == 0xC0) { cp = b0 & 0x1F; extra = 1; }
    else if ((b0 & 0xF0) == 0xE0) { cp = b0 & 0x0F; extra = 2; }
    else if ((b0 & 0xF8) == 0xF0) { cp = b0 & 0x07; extra = 3; }
    else return 0xFFFD;
    if (int(unit.size()) != extra + 1) return 0xFFFD;
    for (int i = 1; i <= extra; ++i) cp = (cp << 6) | (static_cast<unsigned char>(unit[std::size_t(i)]) & 0x3F);
    return cp;
}

std::string visible(std::string_view unit)
{
    if (unit == "\n") return "\xE2\x86\xB5"; // ↵
    if (unit == "\r") return "\xE2\x90\x8D"; // ␍
    if (unit == "\t") return "\xE2\x87\xA5"; // ⇥
    return std::string(unit);
}

const char *alphabet_id(Alphabet a) { return a == Alphabet::UrlSafe ? "url-safe" : "standard"; }
const char *lines_id(LineMode m) { return m == LineMode::Mime ? "mime" : m == LineMode::Custom ? "custom" : "one-line"; }
const char *encoding_id(TextEncoding e)
{
    switch (e) {
    case TextEncoding::Utf16LE: return "utf-16le";
    case TextEncoding::Utf16BE: return "utf-16be";
    case TextEncoding::Latin1: return "latin-1";
    case TextEncoding::Utf8: break;
    }
    return "utf-8";
}

const char *problem_id(TextProblem p)
{
    switch (p) {
    case TextProblem::Empty: return "empty";
    case TextProblem::TooLarge: return "too-large";
    case TextProblem::InputEncoding: return "input-encoding";
    case TextProblem::Decode: return "decode";
    case TextProblem::OutputText: return "output-text";
    case TextProblem::None: break;
    }
    return "none";
}

std::string iso_time(std::int64_t seconds)
{
    const std::time_t t = std::time_t(seconds);
    std::tm           tm {};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buffer[80];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buffer;
}

nlohmann::json options_object(const TextOptions &o)
{
    return nlohmann::json{{"direction", o.encode ? "encode" : "decode"},
                          {"alphabet", alphabet_id(o.alphabet)},
                          {"lines", lines_id(o.lines)},
                          {"width", o.lines == LineMode::Mime ? 76 : o.lines == LineMode::Custom ? clamp_width(o.custom_width) : 0},
                          {"padding", o.padding},
                          {"strict", o.strict},
                          {"encoding", encoding_id(o.encoding)}};
}

bool read_options(const nlohmann::json &j, TextOptions &o)
{
    if (!j.is_object()) return false;
    TextOptions parsed;
    const auto  text = [&](const char *key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); };
    const std::string direction = text("direction"), alphabet = text("alphabet"), lines = text("lines"), encoding = text("encoding");
    if (direction != "encode" && direction != "decode") return false;
    parsed.encode = direction == "encode";
    if (alphabet == "url-safe") parsed.alphabet = Alphabet::UrlSafe;
    else if (alphabet != "standard") return false;
    if (lines == "mime") parsed.lines = LineMode::Mime;
    else if (lines == "custom") parsed.lines = LineMode::Custom;
    else if (lines != "one-line") return false;
    if (j.contains("width") && j["width"].is_number_integer() && parsed.lines == LineMode::Custom)
        parsed.custom_width = clamp_width(j["width"].get<int>());
    if (j.contains("padding") && j["padding"].is_boolean()) parsed.padding = j["padding"].get<bool>();
    if (j.contains("strict") && j["strict"].is_boolean()) parsed.strict = j["strict"].get<bool>();
    if (encoding == "utf-16le") parsed.encoding = TextEncoding::Utf16LE;
    else if (encoding == "utf-16be") parsed.encoding = TextEncoding::Utf16BE;
    else if (encoding == "latin-1") parsed.encoding = TextEncoding::Latin1;
    else if (encoding != "utf-8") return false;
    o = parsed;
    return true;
}

} // namespace

int clamp_width(int width) { return std::clamp(width, 4, 1000); }

EncodeOptions encode_options(const TextOptions &options)
{
    EncodeOptions e;
    e.alphabet    = options.alphabet;
    e.padding     = options.padding;
    e.line_length = options.lines == LineMode::Mime ? 76 : options.lines == LineMode::Custom ? clamp_width(options.custom_width) : 0;
    e.crlf        = options.lines == LineMode::Mime;
    return e;
}

DecodeOptions decode_options(const TextOptions &options)
{
    DecodeOptions d;
    d.alphabet        = options.alphabet;
    d.strict          = options.strict;
    d.require_padding = options.padding;
    return d;
}

std::size_t code_points(std::string_view utf8)
{
    std::size_t n = 0;
    for (const char c : utf8)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

TextOutcome run_text(std::string_view input, const TextOptions &options)
{
    TextOutcome out;
    out.input_characters = code_points(input);
    if (input.empty()) {
        out.problem = TextProblem::Empty;
        return out;
    }
    if (input.size() > kTextLimit) {
        out.problem = TextProblem::TooLarge;
        return out;
    }
    if (options.encode) {
        std::size_t bad = 0;
        if (!text_to_bytes(input, options.encoding, out.bytes, bad)) {
            out.problem = TextProblem::InputEncoding;
            out.offset  = bad;
            const auto parts = units(input);
            out.character = bad < parts.size() ? decode_unit(parts[bad]) : 0;
            out.bytes.clear();
            return out;
        }
        out.output            = encode(out.bytes, encode_options(options));
        out.output_characters = out.output.size();
        out.sha256            = sha256_hex(out.bytes.data(), out.bytes.size());
        DecodeOptions lenient;
        lenient.alphabet = options.alphabet;
        lenient.strict   = false;
        const DecodeResult back = decode(out.output, lenient);
        out.verified = out.bytes.empty() ? out.output.empty() : (back.ok() && back.bytes == out.bytes);
        return out;
    }

    const DecodeResult decoded = decode(input, decode_options(options));
    if (!decoded.ok()) {
        out.problem      = TextProblem::Decode;
        out.decode_error = decoded.error;
        out.offset       = decoded.offset;
        out.character    = decoded.character;
        return out;
    }
    out.bytes     = decoded.bytes;
    out.sha256    = sha256_hex(out.bytes.data(), out.bytes.size());
    out.canonical = encode(out.bytes, encode_options(options)) == input;
    out.verified  = true;
    std::size_t bad = 0;
    if (!bytes_to_text(out.bytes, options.encoding, out.output, bad)) {
        out.problem = TextProblem::OutputText;
        out.offset  = bad;
        out.output.clear();
        return out;
    }
    out.output_characters = code_points(out.output);
    return out;
}

std::string hex_preview(const Bytes &bytes, std::size_t max_bytes)
{
    static const char *digits = "0123456789abcdef";
    std::string        out;
    const std::size_t  n = std::min(bytes.size(), max_bytes);
    out.reserve(n * 3 + 4);
    for (std::size_t i = 0; i < n; ++i) {
        if (i) out += ' ';
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 0x0F];
    }
    if (bytes.size() > n) out += " \xE2\x80\xA6";
    return out;
}

std::string hex_dump(const Bytes &bytes, std::size_t max_bytes)
{
    static const char *digits = "0123456789abcdef";
    const std::size_t  n = std::min(bytes.size(), max_bytes);
    std::string        out;
    for (std::size_t line = 0; line < n; line += 16) {
        char offset[24];
        std::snprintf(offset, sizeof(offset), "%08llx  ", static_cast<unsigned long long>(line));
        out += offset;
        std::string ascii;
        for (std::size_t i = 0; i < 16; ++i) {
            if (line + i < n) {
                const std::uint8_t b = bytes[line + i];
                out += digits[b >> 4];
                out += digits[b & 0x0F];
                out += ' ';
                ascii += (b >= 0x20 && b < 0x7F) ? char(b) : '.';
            } else {
                out += "   ";
            }
            if (i == 7) out += ' ';
        }
        out += " |" + ascii + "|\n";
    }
    if (bytes.size() > n) out += "\xE2\x80\xA6\n";
    return out;
}

Excerpt excerpt_around(std::string_view utf8, std::size_t offset, std::size_t context)
{
    const auto  parts = units(utf8);
    Excerpt     e;
    const std::size_t at    = std::min(offset, parts.size());
    const std::size_t first = at > context ? at - context : 0;
    const std::size_t last  = std::min(parts.size(), at + 1 + context);
    e.clipped_before = first > 0;
    e.clipped_after  = last < parts.size();
    for (std::size_t i = first; i < at; ++i) e.before += visible(parts[i]);
    if (at < parts.size()) e.at = visible(parts[at]);
    for (std::size_t i = at + 1; i < last; ++i) e.after += visible(parts[i]);
    return e;
}

std::string remove_at(std::string_view utf8, std::size_t offset)
{
    const auto parts = units(utf8);
    if (offset >= parts.size()) return std::string(utf8);
    std::string out;
    out.reserve(utf8.size());
    for (std::size_t i = 0; i < parts.size(); ++i)
        if (i != offset) out += parts[i];
    return out;
}

std::string human_size(std::uint64_t bytes)
{
    char buffer[32];
    if (bytes < 1024) {
        std::snprintf(buffer, sizeof(buffer), bytes == 1 ? "%llu byte" : "%llu bytes", static_cast<unsigned long long>(bytes));
        return buffer;
    }
    const double kb = double(bytes) / 1024.0;
    if (kb < 1024.0) {
        if (kb < 10.0) {
            std::snprintf(buffer, sizeof(buffer), "%.1f", kb);
            std::string text = buffer;
            if (text.size() > 2 && text.compare(text.size() - 2, 2, ".0") == 0) text.resize(text.size() - 2);
            return text + " KB";
        }
        std::snprintf(buffer, sizeof(buffer), "%.0f KB", std::floor(kb));
        return buffer;
    }
    const char *unit  = "MB";
    double      value = kb / 1024.0;
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = "GB";
    }
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = "TB";
    }
    const char *format = value < 10.0 ? "%.2f %s" : value < 100.0 ? "%.1f %s" : "%.0f %s";
    std::snprintf(buffer, sizeof(buffer), format, value, unit);
    return buffer;
}

std::string short_hash(const std::string &hex)
{
    if (hex.size() < 12) return hex;
    return hex.substr(0, 4) + "\xE2\x80\xA6" + hex.substr(hex.size() - 4);
}

int percent(std::uint64_t done, std::uint64_t total)
{
    if (total == 0) return 0;
    if (done >= total) return 100;
    return int((done * 100) / total);
}

std::optional<int> seconds_left(std::uint64_t done, std::uint64_t total, double elapsed_seconds)
{
    if (done == 0 || total == 0 || elapsed_seconds < 1.0 || done >= total) return std::nullopt;
    const double rate = double(done) / elapsed_seconds;
    return int(std::ceil(double(total - done) / rate));
}

void UndoStack::commit(const TextState &state)
{
    if (!m_states.empty() && m_states[m_position] == state) return;
    if (!m_states.empty()) m_states.erase(m_states.begin() + std::ptrdiff_t(m_position) + 1, m_states.end());
    m_states.push_back(state);
    if (m_states.size() > kLimit) m_states.erase(m_states.begin(), m_states.begin() + std::ptrdiff_t(m_states.size() - kLimit));
    m_position = m_states.size() - 1;
}

std::optional<TextState> UndoStack::undo()
{
    if (!can_undo()) return std::nullopt;
    --m_position;
    return m_states[m_position];
}

std::optional<TextState> UndoStack::redo()
{
    if (!can_redo()) return std::nullopt;
    ++m_position;
    return m_states[m_position];
}

std::string options_json(const TextOptions &options) { return options_object(options).dump(); }

bool parse_options_json(std::string_view json, TextOptions &options)
{
    const nlohmann::json j = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
    return !j.is_discarded() && read_options(j, options);
}

std::string to_line(const HistoryEntry &e)
{
    nlohmann::json j{{"t", e.time}, {"a", e.action}, {"n", e.name}, {"in", e.bytes_in}, {"out", e.bytes_out}, {"h", e.sha256}};
    if (e.has_options) j["opt"] = options_object(e.options);
    if (e.has_payload) j["p"] = e.payload;
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool from_line(std::string_view line, HistoryEntry &entry)
{
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.remove_suffix(1);
    if (line.empty()) return false;
    const nlohmann::json j = nlohmann::json::parse(line.begin(), line.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    if (!j.contains("t") || !j["t"].is_number_integer() || !j.contains("a") || !j["a"].is_string()) return false;
    HistoryEntry e;
    e.time   = j["t"].get<std::int64_t>();
    e.action = j["a"].get<std::string>();
    static const char *known[] = {"encode-text", "decode-text", "encode-file", "decode-file", "build-url", "read-url"};
    if (std::find(std::begin(known), std::end(known), e.action) == std::end(known)) return false;
    if (j.contains("n") && j["n"].is_string()) e.name = j["n"].get<std::string>();
    if (j.contains("in") && j["in"].is_number_unsigned()) e.bytes_in = j["in"].get<std::uint64_t>();
    if (j.contains("out") && j["out"].is_number_unsigned()) e.bytes_out = j["out"].get<std::uint64_t>();
    if (j.contains("h") && j["h"].is_string()) e.sha256 = j["h"].get<std::string>();
    if (j.contains("opt")) e.has_options = read_options(j["opt"], e.options);
    if (j.contains("p") && j["p"].is_string()) {
        e.has_payload = true;
        e.payload     = j["p"].get<std::string>();
    }
    entry = std::move(e);
    return true;
}

void History::load()
{
    m_entries.clear();
    if (m_file.empty()) return;
    std::ifstream in(m_file, std::ios::binary);
    std::string   line;
    while (std::getline(in, line)) {
        HistoryEntry e;
        if (from_line(line, e)) m_entries.push_back(std::move(e));
    }
    if (m_entries.size() > kLimit) m_entries.erase(m_entries.begin(), m_entries.begin() + std::ptrdiff_t(m_entries.size() - kLimit));
}

void History::append(HistoryEntry entry, bool keep_payload)
{
#ifndef SLIC3R_BASE64_WORKBENCH_NEGATIVE_FIXTURE
    if (!keep_payload) {
        entry.has_payload = false;
        entry.payload.clear();
    }
#else
    (void) keep_payload;
#endif
    m_entries.push_back(std::move(entry));
    if (m_entries.size() > kLimit) m_entries.erase(m_entries.begin(), m_entries.begin() + std::ptrdiff_t(m_entries.size() - kLimit));
    save();
}

void History::clear()
{
    m_entries.clear();
    if (m_file.empty()) return;
    std::error_code ec;
    std::filesystem::remove(m_file, ec);
}

void History::forget_payloads()
{
    bool changed = false;
    for (HistoryEntry &e : m_entries) {
        changed = changed || e.has_payload;
        e.has_payload = false;
        e.payload.clear();
        e.payload.shrink_to_fit();
    }
    if (changed) save();
}

void History::save() const
{
    if (m_file.empty()) return;
    std::error_code ec;
    if (m_file.has_parent_path()) std::filesystem::create_directories(m_file.parent_path(), ec);
    const std::filesystem::path partial = m_file.parent_path() / std::filesystem::u8path(m_file.filename().u8string() + ".partial");
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out) return;
        for (const HistoryEntry &e : m_entries) out << to_line(e) << '\n';
        if (!out) {
            out.close();
            std::filesystem::remove(partial, ec);
            return;
        }
    }
    std::filesystem::rename(partial, m_file, ec);
    if (ec) std::filesystem::remove(partial, ec);
}

std::string History::export_json() const
{
    nlohmann::json list = nlohmann::json::array();
    for (const HistoryEntry &e : m_entries) {
        nlohmann::json j{{"time", iso_time(e.time)}, {"action", e.action}, {"name", e.name},
                         {"bytes_in", e.bytes_in}, {"bytes_out", e.bytes_out}, {"sha256", e.sha256}};
        if (e.has_options) j["options"] = options_object(e.options);
        if (e.has_payload) j["payload"] = e.payload;
        list.push_back(std::move(j));
    }
    return nlohmann::json{{"tool", "Base64 toolkit history"}, {"entries", std::move(list)}}.dump(2, ' ', false,
                                                                                               nlohmann::json::error_handler_t::replace);
}

std::string record_json(const TextOptions &options, const TextOutcome &outcome, std::int64_t time, const std::string *input)
{
    nlohmann::json j{{"tool", "Base64 toolkit"},
                     {"time", iso_time(time)},
                     {"options", options_object(options)},
                     {"problem", problem_id(outcome.problem)},
                     {"input_characters", outcome.input_characters},
                     {"bytes", outcome.bytes.size()},
                     {"sha256", outcome.sha256},
                     {"output_characters", outcome.output_characters},
                     {"verified", outcome.verified},
                     {"payload_included", input != nullptr}};
    if (!options.encode) j["canonical"] = outcome.canonical;
    if (outcome.problem == TextProblem::Decode) {
        j["error"]  = error_name(outcome.decode_error);
        j["offset"] = outcome.offset;
    }
    if (input != nullptr) {
        j["input"]  = *input;
        j["output"] = outcome.output;
    }
    return j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
}

}}} // namespace Slic3r::GUI::Base64Toolkit
