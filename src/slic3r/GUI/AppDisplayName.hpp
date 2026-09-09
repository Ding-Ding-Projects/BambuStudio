#ifndef slic3r_GUI_AppDisplayName_hpp_
#define slic3r_GUI_AppDisplayName_hpp_

// User-renamable application display name.
//
// The display name is a LABEL ONLY. It feeds presentational surfaces (title bar
// wordmark, window title, About caption, dialog captions that introduce the app)
// and nothing else. Application identity -- the data directory, the config file
// name, log file names, the Squirrel/updater ids, HTTP user agents, crash and
// diagnostic report headers, file associations and registry entries -- keeps
// reading the compiled-in SLIC3R_APP_NAME / SLIC3R_APP_FULL_NAME constants.
// tests/app_display_name guards that split.
//
// This header is deliberately free of wxWidgets and libslic3r dependencies so
// the validation rules can be unit-tested without a GUI toolkit.

#include <string>

namespace Slic3r { namespace GUI { namespace AppDisplayName {

// AppConfig key. Empty value (or missing key) means "use the shipped name".
constexpr const char *CONFIG_KEY = "app_display_name";

// Validation bounds, counted in Unicode code points (UTF-8 input).
constexpr size_t MIN_LENGTH = 1;
constexpr size_t MAX_LENGTH = 40;

enum class Problem {
    None,
    Empty,             // nothing left after trimming whitespace
    TooLong,           // more than MAX_LENGTH code points
    ControlCharacters, // contains C0/C1 controls, DEL, or line breaks
};

struct Validation
{
    Problem problem = Problem::None;
    size_t  length  = 0; // code points of the candidate exactly as given
    bool ok() const { return problem == Problem::None; }
};

// Where the effective name came from.
enum class Provenance {
    Default, // no valid stored value: the shipped product name is shown
    Stored,  // a valid value from AppConfig is shown
};

// Count Unicode code points in a UTF-8 string (malformed bytes count as one each).
size_t utf8_length(const std::string &utf8);

// True for C0 controls (0x00-0x1F), DEL (0x7F) and C1 controls (U+0080-U+009F).
bool contains_control_characters(const std::string &utf8);

// Validate a candidate exactly as typed (no trimming, no truncation).
Validation validate(const std::string &candidate);

// Sanitize free text into the closest valid candidate: strip control characters,
// trim leading/trailing whitespace, collapse internal whitespace runs to one
// space, and truncate to MAX_LENGTH code points. The result may still be empty
// (validate() then reports Problem::Empty); it is never invalid for any other
// reason.
std::string sanitize(const std::string &text);

// The value that should be persisted for `candidate`: "" when the sanitized
// candidate equals the shipped name (so a user typing the shipped name back is
// indistinguishable from a reset), otherwise the sanitized candidate.
std::string to_stored_value(const std::string &candidate, const std::string &shipped_name);

// Resolve the effective display name: `stored` when it validates, else `shipped_name`.
std::string resolve(const std::string &stored, const std::string &shipped_name);

// Provenance of resolve(stored, shipped_name).
Provenance provenance(const std::string &stored, const std::string &shipped_name);

}}} // namespace Slic3r::GUI::AppDisplayName

#endif // slic3r_GUI_AppDisplayName_hpp_
