#ifndef slic3r_GUI_Widgets_BoundedRegexProtocol_hpp_
#define slic3r_GUI_Widgets_BoundedRegexProtocol_hpp_

#include "BoundedRegex.hpp"

#include <boost/regex.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace Slic3r::GUI::BoundedRegex::Protocol {

inline constexpr std::uint32_t kVersion       = 1;
inline constexpr std::uint32_t kRequestMagic  = 0x31525842; // BXR1
inline constexpr std::uint32_t kResponseMagic = 0x31535842; // BXS1
inline constexpr std::size_t   kPrefixBytes   = 12;
inline constexpr std::size_t   kMaxRequestBytes  = 128 * 1024;
inline constexpr std::size_t   kMaxResponseBytes = 4 * 1024 * 1024;

enum class Mode : std::uint32_t { Ping = 0, Validate = 1, Search = 2, FindAll = 3 };

struct Request {
    Mode         mode = Mode::Search;
    bool         case_sensitive = false;
    bool         multiline      = false;
    std::size_t  max_matches    = 1;
    std::wstring pattern;
    std::wstring subject;
};

inline void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 24));
}

inline bool read_u32(const std::vector<std::uint8_t> &in, std::size_t &offset, std::uint32_t &value)
{
    if (offset > in.size() || in.size() - offset < 4)
        return false;
    value = static_cast<std::uint32_t>(in[offset])
          | (static_cast<std::uint32_t>(in[offset + 1]) << 8)
          | (static_cast<std::uint32_t>(in[offset + 2]) << 16)
          | (static_cast<std::uint32_t>(in[offset + 3]) << 24);
    offset += 4;
    return true;
}

inline void append_bytes(std::vector<std::uint8_t> &out, const void *data, std::size_t size)
{
    const auto *first = static_cast<const std::uint8_t *>(data);
    out.insert(out.end(), first, first + size);
}

inline std::vector<std::uint8_t> frame(std::uint32_t magic, const std::vector<std::uint8_t> &payload)
{
    std::vector<std::uint8_t> out;
    out.reserve(kPrefixBytes + payload.size());
    append_u32(out, magic);
    append_u32(out, kVersion);
    append_u32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

inline bool decode_prefix(const std::vector<std::uint8_t> &prefix, std::uint32_t expected_magic,
                          std::size_t max_payload, std::size_t &payload_size)
{
    if (prefix.size() != kPrefixBytes)
        return false;
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, size = 0;
    if (!read_u32(prefix, offset, magic) || !read_u32(prefix, offset, version) ||
        !read_u32(prefix, offset, size))
        return false;
    if (magic != expected_magic || version != kVersion || size > max_payload)
        return false;
    payload_size = size;
    return true;
}

inline std::vector<std::uint8_t> encode_request(const Request &request)
{
    std::vector<std::uint8_t> payload;
    append_u32(payload, static_cast<std::uint32_t>(request.mode));
    append_u32(payload, (request.case_sensitive ? 1u : 0u) | (request.multiline ? 2u : 0u));
    append_u32(payload, static_cast<std::uint32_t>(request.max_matches));
    append_u32(payload, static_cast<std::uint32_t>(request.pattern.size()));
    append_u32(payload, static_cast<std::uint32_t>(request.subject.size()));
    append_bytes(payload, request.pattern.data(), request.pattern.size() * sizeof(wchar_t));
    append_bytes(payload, request.subject.data(), request.subject.size() * sizeof(wchar_t));
    return frame(kRequestMagic, payload);
}

inline bool decode_request(const std::vector<std::uint8_t> &payload, Request &request)
{
    std::size_t offset = 0;
    std::uint32_t mode = 0, flags = 0, max_matches = 0, pattern_units = 0, subject_units = 0;
    if (!read_u32(payload, offset, mode) || !read_u32(payload, offset, flags) ||
        !read_u32(payload, offset, max_matches) || !read_u32(payload, offset, pattern_units) ||
        !read_u32(payload, offset, subject_units))
        return false;
    if (mode > static_cast<std::uint32_t>(Mode::FindAll) || max_matches > kMaxMatches ||
        pattern_units > kMaxPatternCodeUnits || subject_units > kMaxSubjectCodeUnits)
        return false;
    const std::size_t pattern_bytes = static_cast<std::size_t>(pattern_units) * sizeof(wchar_t);
    const std::size_t subject_bytes = static_cast<std::size_t>(subject_units) * sizeof(wchar_t);
    if (offset > payload.size() || pattern_bytes > payload.size() - offset)
        return false;
    request.pattern.resize(pattern_units);
    if (pattern_bytes != 0)
        std::memcpy(request.pattern.data(), payload.data() + offset, pattern_bytes);
    offset += pattern_bytes;
    if (offset > payload.size() || subject_bytes != payload.size() - offset)
        return false;
    request.subject.resize(subject_units);
    if (subject_bytes != 0)
        std::memcpy(request.subject.data(), payload.data() + offset, subject_bytes);
    request.mode           = static_cast<Mode>(mode);
    request.case_sensitive = (flags & 1u) != 0;
    request.multiline      = (flags & 2u) != 0;
    request.max_matches    = max_matches;
    return true;
}

inline std::vector<std::uint8_t> encode_result(const Result &result)
{
    std::vector<std::uint8_t> payload;
    append_u32(payload, static_cast<std::uint32_t>(result.status));
    append_u32(payload, static_cast<std::uint32_t>(result.detail));
    append_u32(payload, result.match_limit_reached ? 1u : 0u);
    append_u32(payload, static_cast<std::uint32_t>(result.matches.size()));
    append_u32(payload, static_cast<std::uint32_t>(result.diagnostic.size()));
    append_bytes(payload, result.diagnostic.data(), result.diagnostic.size());
    for (const Match &match : result.matches) {
        append_u32(payload, static_cast<std::uint32_t>(match.groups.size()));
        for (const Capture &group : match.groups) {
            append_u32(payload, group.matched ? 1u : 0u);
            append_u32(payload, static_cast<std::uint32_t>(group.begin));
            append_u32(payload, static_cast<std::uint32_t>(group.length));
        }
    }
    return frame(kResponseMagic, payload);
}

inline bool decode_result(const std::vector<std::uint8_t> &payload, Result &result)
{
    std::size_t offset = 0;
    std::uint32_t status = 0, detail = 0, flags = 0, match_count = 0, diagnostic_bytes = 0;
    if (!read_u32(payload, offset, status) || !read_u32(payload, offset, detail) ||
        !read_u32(payload, offset, flags) || !read_u32(payload, offset, match_count) ||
        !read_u32(payload, offset, diagnostic_bytes))
        return false;
    if (status > static_cast<std::uint32_t>(Status::ProtocolError) ||
        detail > static_cast<std::uint32_t>(ErrorDetail::Unknown) ||
        match_count > kMaxMatches || offset > payload.size() ||
        diagnostic_bytes > payload.size() - offset)
        return false;
    result.status = static_cast<Status>(status);
    result.detail = static_cast<ErrorDetail>(detail);
    result.match_limit_reached = (flags & 1u) != 0;
    result.diagnostic.assign(reinterpret_cast<const char *>(payload.data() + offset), diagnostic_bytes);
    offset += diagnostic_bytes;
    result.matches.clear();
    result.matches.reserve(match_count);
    for (std::uint32_t match_index = 0; match_index < match_count; ++match_index) {
        std::uint32_t group_count = 0;
        if (!read_u32(payload, offset, group_count) || group_count > kMaxPatternCodeUnits + 1)
            return false;
        Match match;
        match.groups.reserve(group_count);
        for (std::uint32_t group_index = 0; group_index < group_count; ++group_index) {
            std::uint32_t matched = 0, begin = 0, length = 0;
            if (!read_u32(payload, offset, matched) || !read_u32(payload, offset, begin) ||
                !read_u32(payload, offset, length) || begin > kMaxSubjectCodeUnits ||
                length > kMaxSubjectCodeUnits || begin + length > kMaxSubjectCodeUnits)
                return false;
            match.groups.push_back(Capture{begin, length, matched != 0});
        }
        result.matches.push_back(std::move(match));
    }
    return offset == payload.size();
}

inline ErrorDetail map_regex_error(boost::regex_constants::error_type type)
{
    using namespace boost::regex_constants;
    switch (type) {
    case error_collate: return ErrorDetail::Collate;
    case error_ctype: return ErrorDetail::CharacterClass;
    case error_escape: return ErrorDetail::Escape;
    case error_backref: return ErrorDetail::BackReference;
    case error_brack: return ErrorDetail::Bracket;
    case error_paren: return ErrorDetail::Parenthesis;
    case error_brace: return ErrorDetail::Brace;
    case error_badbrace: return ErrorDetail::BadBrace;
    case error_range: return ErrorDetail::Range;
    case error_space: return ErrorDetail::Space;
    case error_badrepeat: return ErrorDetail::BadRepeat;
    case error_complexity: return ErrorDetail::Complexity;
    case error_stack: return ErrorDetail::Stack;
    default: return ErrorDetail::Unknown;
    }
}

inline bool is_complexity_error(ErrorDetail detail)
{
    return detail == ErrorDetail::Complexity || detail == ErrorDetail::Space ||
           detail == ErrorDetail::Stack;
}

inline bool structurally_bounded(const std::wstring &pattern)
{
    std::size_t depth = 0;
    std::size_t quantifiers = 0;
    std::size_t alternations = 0;
    bool escaped = false;
    bool in_class = false;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const wchar_t ch = pattern[i];
        if (ch == L'\0')
            return false;
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == L'\\') {
            escaped = true;
            continue;
        }
        if (in_class) {
            if (ch == L']')
                in_class = false;
            continue;
        }
        if (ch == L'[') {
            in_class = true;
        } else if (ch == L'(') {
            if (++depth > kMaxNestingDepth)
                return false;
        } else if (ch == L')') {
            if (depth != 0)
                --depth;
        } else if (ch == L'|') {
            if (++alternations > 128)
                return false;
        } else if (ch == L'*' || ch == L'+' || ch == L'?' || ch == L'{') {
            if (++quantifiers > 128)
                return false;
            if (ch == L'{') {
                std::uint64_t value = 0;
                bool saw_digit = false;
                for (std::size_t j = i + 1; j < pattern.size() && pattern[j] != L'}'; ++j) {
                    if (pattern[j] >= L'0' && pattern[j] <= L'9') {
                        saw_digit = true;
                        value = std::min<std::uint64_t>(10001, value * 10 + (pattern[j] - L'0'));
                        if (value > 10000)
                            return false;
                    } else if (pattern[j] == L',') {
                        // {n,m} has two independent bounds. Carrying the lower
                        // digits into the upper bound turned valid {100,100}
                        // into the fictitious value 100100.
                        value = 0;
                        saw_digit = false;
                    } else if (pattern[j] != L' ') {
                        break;
                    }
                }
                (void) saw_digit;
            }
        }
    }
    return true;
}

class Engine {
public:
    Result evaluate(const Request &request)
    {
        if (request.mode == Mode::Ping) {
            Result ready;
            ready.status = Status::Valid;
            return ready;
        }
        if (request.pattern.size() > kMaxPatternCodeUnits)
            return failure(Status::PatternTooLong, "pattern exceeds worker limit");
        if (request.subject.size() > kMaxSubjectCodeUnits)
            return failure(Status::SubjectTooLong, "subject exceeds worker limit");
        if (!structurally_bounded(request.pattern))
            return failure(Status::PatternTooComplex, "pattern exceeds structural limit");
        if (request.max_matches > kMaxMatches ||
            (request.mode != Mode::Validate && request.max_matches == 0))
            return failure(Status::ProtocolError, "invalid match limit");

        Result compiled = ensure_compiled(request);
        if (compiled.status != Status::Valid)
            return compiled;
        if (request.mode == Mode::Validate)
            return compiled;

        Result result;
        try {
            boost::wsregex_iterator it(request.subject.begin(), request.subject.end(), *m_regex), end;
            for (; it != end; ++it) {
                // FindAll consumes one look-ahead result solely to distinguish
                // "exactly at the cap" from genuine truncation. Search asks for
                // one result by design and must never report that as a capped
                // result set.
                if (request.mode == Mode::FindAll &&
                    result.matches.size() >= request.max_matches) {
                    result.match_limit_reached = true;
                    break;
                }
                const boost::wsmatch &native_match = *it;
                Match match;
                match.groups.reserve(native_match.size());
                for (std::size_t group_index = 0; group_index < native_match.size(); ++group_index) {
                    const bool matched = native_match[group_index].matched;
                    const auto position = matched ? native_match.position(group_index) : 0;
                    const auto length   = matched ? native_match.length(group_index) : 0;
                    match.groups.push_back(Capture{
                        static_cast<std::size_t>(position), static_cast<std::size_t>(length), matched});
                }
                result.matches.push_back(std::move(match));
                if (request.mode == Mode::Search)
                    break;
            }
            result.status = result.matches.empty() ? Status::NoMatch : Status::Match;
            return result;
        } catch (const boost::regex_error &error) {
            const ErrorDetail detail = map_regex_error(error.code());
            Result failed = failure(is_complexity_error(detail) ? Status::PatternTooComplex
                                                                 : Status::InvalidPattern,
                                    "regex evaluation rejected by engine");
            failed.detail = detail;
            return failed;
        } catch (const std::bad_alloc &) {
            return failure(Status::PatternTooComplex, "regex allocation limit reached");
        } catch (const std::exception &) {
            return failure(Status::ProtocolError, "regex evaluation failed");
        }
    }

private:
    Result ensure_compiled(const Request &request)
    {
        if (m_has_key && request.pattern == m_pattern && request.case_sensitive == m_case_sensitive &&
            request.multiline == m_multiline)
            return m_compile_result;

        m_has_key        = true;
        m_pattern        = request.pattern;
        m_case_sensitive = request.case_sensitive;
        m_multiline      = request.multiline;
        m_regex.reset();
        m_compile_result = Result{};
        try {
            // Boost.Regex exposes portable multiline anchors on every
            // supported compiler, including MSVC where std::regex still omits
            // std::regex_constants::multiline. Keep ECMAScript's dot behavior
            // explicit and make multiline anchors opt-in.
            boost::wregex::flag_type flags = boost::regex_constants::ECMAScript |
                                             boost::regex_constants::no_mod_s;
            if (!request.case_sensitive)
                flags |= boost::regex_constants::icase;
            if (!request.multiline)
                flags |= boost::regex_constants::no_mod_m;
            m_regex.emplace(request.pattern, flags);
            m_compile_result.status = Status::Valid;
        } catch (const boost::regex_error &error) {
            const ErrorDetail detail = map_regex_error(error.code());
            m_compile_result = failure(is_complexity_error(detail) ? Status::PatternTooComplex
                                                                    : Status::InvalidPattern,
                                       "regex compilation rejected by engine");
            m_compile_result.detail = detail;
        } catch (const std::bad_alloc &) {
            m_compile_result = failure(Status::PatternTooComplex, "regex allocation limit reached");
        } catch (const std::exception &) {
            m_compile_result = failure(Status::ProtocolError, "regex compilation failed");
        }
        return m_compile_result;
    }

    static Result failure(Status status, std::string diagnostic)
    {
        Result result;
        result.status = status;
        result.diagnostic = std::move(diagnostic);
        return result;
    }

    bool                       m_has_key = false;
    bool                       m_case_sensitive = false;
    bool                       m_multiline = false;
    std::wstring               m_pattern;
    std::optional<boost::wregex> m_regex;
    Result                     m_compile_result;
};

} // namespace Slic3r::GUI::BoundedRegex::Protocol

#endif
