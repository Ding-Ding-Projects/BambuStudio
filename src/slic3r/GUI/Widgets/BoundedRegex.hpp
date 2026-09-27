#ifndef slic3r_GUI_Widgets_BoundedRegex_hpp_
#define slic3r_GUI_Widgets_BoundedRegex_hpp_

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

namespace Slic3r::GUI::BoundedRegex {

// These limits apply in both the app and the worker. They are deliberately
// small enough for live filtering while still accommodating the builder's
// guided patterns and sample text.
inline constexpr std::size_t kMaxPatternCodeUnits = 512;
inline constexpr std::size_t kMaxSubjectCodeUnits = 8192;
inline constexpr std::size_t kMaxMatches          = 200;
inline constexpr std::size_t kMaxNestingDepth     = 32;
inline constexpr std::uint32_t kDefaultTimeoutMs  = 50;

enum class Status : std::uint32_t {
    Valid = 0,
    Match,
    NoMatch,
    InvalidPattern,
    PatternTooLong,
    SubjectTooLong,
    PatternTooComplex,
    TimedOut,
    WorkerUnavailable,
    ProtocolError,
};

// Stable counterpart of std::regex_constants::error_type. The C++ enum's
// numeric values are implementation-defined, so they never cross the worker
// protocol directly.
enum class ErrorDetail : std::uint32_t {
    None = 0,
    Collate,
    CharacterClass,
    Escape,
    BackReference,
    Bracket,
    Parenthesis,
    Brace,
    BadBrace,
    Range,
    Space,
    BadRepeat,
    Complexity,
    Stack,
    Unknown,
};

struct Capture {
    std::size_t begin   = 0;
    std::size_t length  = 0;
    bool        matched = false;
};

struct Match {
    // Group zero is the whole match; subsequent entries are capture groups.
    std::vector<Capture> groups;
};

struct Options {
    bool          case_sensitive = false;
    bool          multiline      = false;
    std::uint32_t timeout_ms     = kDefaultTimeoutMs;
};

struct Result {
    Status             status = Status::ProtocolError;
    ErrorDetail        detail = ErrorDetail::None;
    std::vector<Match> matches;
    bool               match_limit_reached = false;
    std::string        diagnostic;

    bool matched() const { return status == Status::Match; }
    bool definitive() const
    {
        return status == Status::Valid || status == Status::Match || status == Status::NoMatch;
    }
    // Search surfaces intentionally fail open: an invalid, timed-out, or
    // unavailable matcher must not make every row disappear.
    bool allows_candidate() const { return status != Status::NoMatch; }
};

// One SearchPass is created for one visible filtering pass (one fixed pattern
// and flag set, many candidate strings). It enforces a single aggregate wall-
// clock budget and opens a fail-safe circuit after the first timeout, worker
// fault, or other non-definitive result. This prevents N candidates from each
// consuming the full per-request deadline on the UI thread.
class SearchPass
{
public:
    explicit SearchPass(std::wstring pattern, const Options &options = {});

    Result evaluate(const std::wstring &subject);
    Result find_all(const std::wstring &subject, std::size_t max_matches = kMaxMatches);
    bool   allows_candidate(const std::wstring &subject) { return evaluate(subject).allows_candidate(); }
    bool   circuit_open() const { return m_circuit_open; }
    Status circuit_status() const { return m_circuit_result.status; }

private:
    void open_circuit(Result result);
    bool remaining_options(Options &options);

    std::wstring m_pattern;
    Options      m_options;
    std::chrono::steady_clock::time_point m_deadline;
    bool         m_circuit_open = false;
    Result       m_circuit_result;
};

Result validate(const std::wstring &pattern, const Options &options = {});
Result search(const std::wstring &pattern, const std::wstring &subject, const Options &options = {});
Result find_all(const std::wstring &pattern, const std::wstring &subject,
                std::size_t max_matches = kMaxMatches, const Options &options = {});

// Starts the persistent contained worker on an owned background task. This is
// safe to call during application initialization and is deliberately
// nonblocking; requests made while startup is in flight fail open immediately.
void prewarm();

// Literal search stays in-process because it has deterministic linear bounds;
// no regex parsing is involved. This preserves SearchField's historical plain
// text / whole-word semantics and makes the mode distinction testable without
// a GUI harness.
bool plain_search(const std::wstring &needle, const std::wstring &subject,
                  bool case_sensitive, bool whole_word = false);

const char *status_name(Status status);

namespace Testing {
// Tests point the client at the just-built helper instead of relying on the
// production same-directory lookup. Empty restores the production lookup.
void set_worker_path(const std::wstring &path);
void reset_worker();
// POSIX regression probe: starts a fresh helper and asks it whether a parent
// descriptor number is absent after exec. Returns false on Windows.
bool worker_fd_is_closed(int fd);
// POSIX regression probe: verifies that the executed worker installed finite
// address-space and stack limits before accepting user-authored regex.
bool worker_resource_limits_active();
} // namespace Testing

} // namespace Slic3r::GUI::BoundedRegex

#endif
