#ifndef slic3r_GUI_Bulk_BulkRenamePattern_hpp_
#define slic3r_GUI_Bulk_BulkRenamePattern_hpp_

#include <cstddef>
#include <codecvt>
#include <locale>
#include "../Widgets/BoundedRegex.hpp"
#include <set>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Bulk {

// Rename-by-pattern engine shared by every "Bulk rename..." surface.
//
// The spec is applied to every selected name in display order:
//
//   1. find / replace: when `find` is non-empty, every occurrence in the name
//      is replaced by `replace` (plain substring by default, ECMAScript regex
//      with $1..$9 back-references when `regex` is set);
//   2. pattern: when `pattern` is non-empty, the result of step 1 becomes
//      {name} and the pattern is expanded. Placeholders:
//        {name}   the (find/replace-processed) original name
//        {n}      1-based running index, starting at `start_index`, zero-padded
//                 to `pad` digits ({n} with pad 3 gives 001, 002, ...)
//        {i}      0-based running index (same padding)
//        {ext}    the file extension of the original name including the dot
//                 ("" when there is none)
//        {stem}   the original name without its extension
//        {{ / }}  literal braces
//
// The plan reports one row per name: before, after, and an outcome so the UI
// can state "N selected / M will change / K skipped (reason)" before anything
// is touched. Collisions are detected against the other planned names and
// against `reserved` (names that exist in the collection but are not being
// renamed); a collision or an invalid result skips that row and the caller
// must not apply it.
//
// Regex evaluation is bounded (pattern and subject size caps) and a malformed
// pattern is reported as `Invalid` for every row rather than thrown. No
// wxWidgets dependency, so this header is unit-testable in isolation.

inline constexpr std::size_t kRenameMaxPatternChars = 512;
inline constexpr std::size_t kRenameMaxNameChars    = 4096;
inline constexpr int         kRenameMaxPad          = 12;

struct RenameSpec
{
    std::string pattern;               // "" = keep the (find/replace) name as is
    std::string find;                  // "" = no find/replace step
    std::string replace;
    bool        regex          = false; // `find` is an ECMAScript regex
    bool        case_sensitive = true;  // find/replace case sensitivity
    int         start_index    = 1;     // value of {n} for the first row
    int         pad            = 0;     // zero-pad {n}/{i} to this many digits (0 = none)

    bool empty() const { return pattern.empty() && find.empty(); }
};

enum class RenameOutcome
{
    Unchanged,   // after == before: nothing to do, skipped
    Changed,     // will be renamed
    Collision,   // after equals another planned name or a reserved name
    Invalid,     // empty result, regex error, or a bounded-size violation
};

struct RenameRow
{
    std::string   before;
    std::string   after;
    RenameOutcome outcome = RenameOutcome::Unchanged;
    std::string   reason;  // plain-English skip reason, "" when Changed
};

struct RenamePlan
{
    std::vector<RenameRow> rows;
    std::string            error; // spec-level error (bad regex, bad pattern), "" when fine

    std::size_t selected() const { return rows.size(); }
    std::size_t will_change() const
    {
        std::size_t n = 0;
        for (const auto &r : rows)
            if (r.outcome == RenameOutcome::Changed)
                ++n;
        return n;
    }
    std::size_t skipped() const { return rows.size() - will_change(); }
    bool        has_collisions() const
    {
        for (const auto &r : rows)
            if (r.outcome == RenameOutcome::Collision)
                return true;
        return false;
    }
    bool applicable() const { return error.empty() && will_change() > 0; }
};

namespace detail {

inline std::string pad_number(long long value, int pad)
{
    std::string digits = std::to_string(value < 0 ? -value : value);
    if (pad > kRenameMaxPad)
        pad = kRenameMaxPad;
    while (static_cast<int>(digits.size()) < pad)
        digits.insert(digits.begin(), '0');
    return value < 0 ? "-" + digits : digits;
}

inline std::string lower_ascii(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// Plain substring replace, all occurrences, optionally case-insensitive (ASCII).
inline std::string replace_plain(const std::string &subject, const std::string &find, const std::string &replace,
                                 bool case_sensitive)
{
    if (find.empty())
        return subject;
    std::string       out;
    const std::string hay    = case_sensitive ? subject : lower_ascii(subject);
    const std::string needle = case_sensitive ? find : lower_ascii(find);
    std::size_t       pos    = 0;
    while (true) {
        const std::size_t hit = hay.find(needle, pos);
        if (hit == std::string::npos) {
            out.append(subject, pos, std::string::npos);
            break;
        }
        out.append(subject, pos, hit - pos);
        out += replace;
        pos = hit + needle.size();
    }
    return out;
}

// Expand {name}/{n}/{i}/{ext}/{stem}/{{/}} in `pattern`. Returns false on an
// unbalanced brace or an unknown placeholder and fills `error`.
inline bool expand_pattern(const std::string &pattern, const std::string &name, long long n, long long i, int pad,
                           std::string &out, std::string &error)
{
    out.clear();
    const std::size_t dot  = name.find_last_of('.');
    const std::string ext  = (dot == std::string::npos || dot == 0) ? std::string() : name.substr(dot);
    const std::string stem = ext.empty() ? name : name.substr(0, dot);
    for (std::size_t p = 0; p < pattern.size();) {
        const char c = pattern[p];
        if (c == '{') {
            if (p + 1 < pattern.size() && pattern[p + 1] == '{') {
                out += '{';
                p += 2;
                continue;
            }
            const std::size_t close = pattern.find('}', p);
            if (close == std::string::npos) {
                error = "Unbalanced '{' in the pattern";
                return false;
            }
            const std::string key = pattern.substr(p + 1, close - p - 1);
            if (key == "name")
                out += name;
            else if (key == "n")
                out += pad_number(n, pad);
            else if (key == "i")
                out += pad_number(i, pad);
            else if (key == "ext")
                out += ext;
            else if (key == "stem")
                out += stem;
            else {
                error = "Unknown placeholder {" + key + "}; use {name}, {n}, {i}, {stem} or {ext}";
                return false;
            }
            p = close + 1;
        } else if (c == '}') {
            if (p + 1 < pattern.size() && pattern[p + 1] == '}') {
                out += '}';
                p += 2;
                continue;
            }
            error = "Unbalanced '}' in the pattern";
            return false;
        } else {
            out += c;
            ++p;
        }
    }
    return true;
}

} // namespace detail

// Validate the spec without any names. Returns "" when fine.
inline std::string validate_rename_spec(const RenameSpec &spec)
{
    if (spec.pattern.size() > kRenameMaxPatternChars || spec.find.size() > kRenameMaxPatternChars ||
        spec.replace.size() > kRenameMaxPatternChars)
        return "Pattern is longer than " + std::to_string(kRenameMaxPatternChars) + " characters";
    if (spec.regex && !spec.find.empty()) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conversion;
        try {
            BoundedRegex::Options options;
            options.case_sensitive = spec.case_sensitive;
            const auto result = BoundedRegex::validate(conversion.from_bytes(spec.find), options);
            if (!result.definitive()) return std::string("Regular expression unavailable: ") + BoundedRegex::status_name(result.status);
        } catch (const std::range_error &) { return "Invalid UTF-8 regular expression"; }

    }
    if (!spec.pattern.empty()) {
        std::string out, error;
        if (!detail::expand_pattern(spec.pattern, "probe", 1, 0, 0, out, error))
            return error;
    }
    return std::string();
}

// Build the plan. `reserved` holds names in the collection that are NOT being
// renamed (they still count for collisions). Rows keep the order of `names`.
inline RenamePlan plan_rename(const std::vector<std::string> &names, const RenameSpec &spec,
                              const std::set<std::string> &reserved = {})
{
    RenamePlan plan;
    plan.rows.reserve(names.size());
    plan.error = validate_rename_spec(spec);

    const bool use_regex = spec.regex && !spec.find.empty() && plan.error.empty();
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conversion;
    BoundedRegex::Options options;
    options.case_sensitive = spec.case_sensitive;
    BoundedRegex::SearchPass regex_pass(use_regex ? conversion.from_bytes(spec.find) : std::wstring(), options);

    long long i = 0;
    for (const std::string &before : names) {
        RenameRow row;
        row.before = before;
        row.after  = before;
        if (!plan.error.empty()) {
            row.outcome = RenameOutcome::Invalid;
            row.reason  = plan.error;
            plan.rows.push_back(row);
            ++i;
            continue;
        }
        if (before.size() > kRenameMaxNameChars) {
            row.outcome = RenameOutcome::Invalid;
            row.reason  = "Name is longer than " + std::to_string(kRenameMaxNameChars) + " characters";
            plan.rows.push_back(row);
            ++i;
            continue;
        }
        std::string working = before;
        if (!spec.find.empty()) {
            if (use_regex) {
                try {
                    const auto subject = conversion.from_bytes(working);
                    const auto replacement = conversion.from_bytes(spec.replace);
                    const auto result = regex_pass.find_all(subject);
                    if (!result.definitive() || result.match_limit_reached) {
                        row.outcome = RenameOutcome::Invalid;
                        row.reason = result.match_limit_reached ? "Regular expression match limit exceeded" :
                            std::string("Regular expression unavailable: ") + BoundedRegex::status_name(result.status);
                        plan.rows.push_back(row); ++i; continue;
                    }
                    std::wstring output;
                    std::size_t cursor = 0;
                    for (const auto &match : result.matches) {
                        if (match.groups.empty()) continue;
                        const auto &whole = match.groups.front();
                        output.append(subject, cursor, whole.begin - cursor);
                        for (std::size_t offset = 0; offset < replacement.size(); ++offset) {
                            if (replacement[offset] != L'$' || offset + 1 == replacement.size()) { output += replacement[offset]; continue; }
                            const wchar_t next = replacement[offset + 1];
                            if (next == L'$') { output += L'$'; ++offset; }
                            else if (next == L'&') { output.append(subject, whole.begin, whole.length); ++offset; }
                            else if (next >= L'1' && next <= L'9') {
                                std::size_t group = next - L'0'; ++offset;
                                if (offset + 1 < replacement.size() && replacement[offset + 1] >= L'0' && replacement[offset + 1] <= L'9')
                                    group = group * 10 + replacement[++offset] - L'0';
                                if (group < match.groups.size() && match.groups[group].matched)
                                    output.append(subject, match.groups[group].begin, match.groups[group].length);
                            } else if (next == L'`') { output.append(subject, 0, whole.begin); ++offset; }
                            else if (next == L'\'') { output.append(subject, whole.begin + whole.length, std::wstring::npos); ++offset; }
                            else output += replacement[offset];
                        }
                        cursor = whole.begin + whole.length;
                        if (output.size() > kRenameMaxNameChars) break;
                    }
                    output.append(subject, cursor, std::wstring::npos);
                    working = conversion.to_bytes(output);
                } catch (const std::range_error &) {
                    row.outcome = RenameOutcome::Invalid;
                    row.reason = "Invalid UTF-8 rename input";
                    plan.rows.push_back(row); ++i; continue;
                }
            } else {
                working = detail::replace_plain(working, spec.find, spec.replace, spec.case_sensitive);
            }
        }
        if (!spec.pattern.empty()) {
            std::string expanded, error;
            if (!detail::expand_pattern(spec.pattern, working, spec.start_index + i, i, spec.pad, expanded, error)) {
                row.outcome = RenameOutcome::Invalid;
                row.reason  = error;
                plan.rows.push_back(row);
                ++i;
                continue;
            }
            working = expanded;
        }
        row.after = working;
        if (working.empty()) {
            row.outcome = RenameOutcome::Invalid;
            row.reason  = "Result would be an empty name";
        } else if (working.size() > kRenameMaxNameChars) {
            row.outcome = RenameOutcome::Invalid;
            row.reason  = "Result is longer than " + std::to_string(kRenameMaxNameChars) + " characters";
        } else if (working == before) {
            row.outcome = RenameOutcome::Unchanged;
            row.reason  = "Name would not change";
        } else {
            row.outcome = RenameOutcome::Changed;
        }
        plan.rows.push_back(row);
        ++i;
    }

    // Collision pass: a planned "after" that equals a reserved name, or another
    // row's "after", or another row's untouched "before", is refused.
    std::set<std::string> taken(reserved);
    for (const RenameRow &row : plan.rows)
        if (row.outcome != RenameOutcome::Changed)
            taken.insert(row.before);
    std::set<std::string> planned;
    for (RenameRow &row : plan.rows) {
        if (row.outcome != RenameOutcome::Changed)
            continue;
        if (taken.count(row.after) != 0) {
            row.outcome = RenameOutcome::Collision;
            row.reason  = "'" + row.after + "' already exists";
        } else if (!planned.insert(row.after).second) {
            row.outcome = RenameOutcome::Collision;
            row.reason  = "'" + row.after + "' is produced by more than one item";
        }
    }
    // The first of two rows producing the same name was inserted into
    // `planned` and kept; mark it too so neither half of a duplicate lands.
    std::set<std::string> dupes;
    for (const RenameRow &row : plan.rows)
        if (row.outcome == RenameOutcome::Collision && row.reason.find("more than one") != std::string::npos)
            dupes.insert(row.after);
    for (RenameRow &row : plan.rows)
        if (row.outcome == RenameOutcome::Changed && dupes.count(row.after) != 0) {
            row.outcome = RenameOutcome::Collision;
            row.reason  = "'" + row.after + "' is produced by more than one item";
        }
    return plan;
}

inline const char *rename_outcome_name(RenameOutcome outcome)
{
    switch (outcome) {
    case RenameOutcome::Changed: return "Will rename";
    case RenameOutcome::Unchanged: return "Unchanged";
    case RenameOutcome::Collision: return "Collision";
    case RenameOutcome::Invalid: return "Invalid";
    }
    return "";
}

} } } // namespace Slic3r::GUI::Bulk

#endif // slic3r_GUI_Bulk_BulkRenamePattern_hpp_
