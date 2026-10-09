#ifndef slic3r_GUI_Widgets_RegexAnalysis_hpp_
#define slic3r_GUI_Widgets_RegexAnalysis_hpp_

// Structure, capability and backtracking-risk analysis for the regex builder.
//
// The model is pure C++17 (no wxWidgets, no Boost) so it can be tested on its
// own. It describes the dialect that bambu-regex-worker really compiles:
// Boost.Regex's Perl syntax, selected by boost::regex_constants::ECMAScript,
// with no_mod_s always set and no_mod_m set unless multiline anchors are on.
// The worker remains the only authority on whether a pattern is valid; this
// model explains a pattern token by token, builds its structure tree, lists
// the engine's capability matrix, and warns about portability traps and
// backtracking risk. Every capability row carries a probe that the tests (and
// the builder's "Check against the engine" action) run through the real
// worker, so the matrix cannot silently drift from the engine.
//
// User-visible text is English source text marked with L() for extraction and
// translated by the GUI at display time; arguments are substituted for each
// %s in order (see substitute()).

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#pragma push_macro("L")
#undef L
#define L(s) s

namespace Slic3r::GUI::RegexAnalysis {

inline constexpr std::size_t npos = static_cast<std::size_t>(-1);
// Group nesting the analysis descends into; twice the worker's structural
// limit (BoundedRegex::kMaxNestingDepth), so every pattern the engine can
// accept is analysed in full.
inline constexpr int kMaxAnalysisDepth = 64;

// ---------------------------------------------------------------- text ----

struct Text
{
    const char               *msgid = "";
    std::vector<std::wstring> args;
};

inline Text text(const char *msgid, std::vector<std::wstring> args = {})
{
    Text out;
    out.msgid = msgid;
    out.args  = std::move(args);
    return out;
}

// Minimal UTF-8 decoder for the English source text (a few messages reuse
// existing catalogue entries that contain an em dash).
inline std::wstring from_utf8(const char *s)
{
    std::wstring out;
    if (!s)
        return out;
    const auto *u = reinterpret_cast<const unsigned char *>(s);
    while (*u) {
        std::uint32_t cp = *u++;
        int extra = 0;
        if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
        else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
        else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
        for (int i = 0; i < extra && (*u & 0xC0) == 0x80; ++i)
            cp = (cp << 6) | (*u++ & 0x3F);
        if (cp >= 0x10000 && sizeof(wchar_t) == 2) {
            cp -= 0x10000;
            out += static_cast<wchar_t>(0xD800 + (cp >> 10));
            out += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
        } else {
            out += static_cast<wchar_t>(cp);
        }
    }
    return out;
}

// Replaces each %s in order with the next argument; %% is a literal percent.
inline std::wstring substitute(const std::wstring &tmpl, const std::vector<std::wstring> &args)
{
    std::wstring out;
    std::size_t next = 0;
    for (std::size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == L'%' && i + 1 < tmpl.size()) {
            if (tmpl[i + 1] == L's') {
                if (next < args.size())
                    out += args[next];
                ++next;
                ++i;
                continue;
            }
            if (tmpl[i + 1] == L'%') {
                out += L'%';
                ++i;
                continue;
            }
        }
        out += tmpl[i];
    }
    return out;
}

inline std::wstring english(const Text &t) { return substitute(from_utf8(t.msgid), t.args); }

inline std::wstring number(std::size_t value) { return std::to_wstring(value); }

inline std::wstring hex_code(std::uint32_t value)
{
    static const wchar_t digits[] = L"0123456789ABCDEF";
    std::wstring out;
    do {
        out.insert(out.begin(), digits[value & 0xF]);
        value >>= 4;
    } while (value != 0);
    while (out.size() < 4)
        out.insert(out.begin(), L'0');
    return out;
}

// Pattern excerpt for display: control characters are shown escaped and long
// spans are clipped with "...".
inline std::wstring fragment(const std::wstring &pattern, std::size_t begin, std::size_t end,
                             std::size_t max_len = 40)
{
    std::wstring out;
    end = std::min(end, pattern.size());
    for (std::size_t i = begin; i < end; ++i) {
        const wchar_t c = pattern[i];
        if (c == L'\n') out += L"\\n";
        else if (c == L'\r') out += L"\\r";
        else if (c == L'\t') out += L"\\t";
        else if (c < 0x20) out += L"\\x" + hex_code(static_cast<std::uint32_t>(c)).substr(2);
        else out += c;
        if (out.size() >= max_len && i + 1 < end) {
            out += L"...";
            break;
        }
    }
    return out;
}

// --------------------------------------------------------------- engine ----

// Parsed form of the descriptor bambu-regex-worker returns for a Describe
// request (Protocol::engine_descriptor()).
struct EngineInfo
{
    bool               reported       = false;
    unsigned long      version        = 0; // BOOST_VERSION encoding
    unsigned           code_unit_bits = static_cast<unsigned>(sizeof(wchar_t) * 8);
    std::string        traits;             // win32, cpp or c
    std::string        syntax;             // perl
    unsigned long long max_states = 0;
};

inline std::wstring version_text(unsigned long version)
{
    if (version == 0)
        return L"?";
    return std::to_wstring(version / 100000) + L"." + std::to_wstring(version / 100 % 1000) + L"." +
           std::to_wstring(version % 100);
}

inline EngineInfo parse_engine_descriptor(const std::string &descriptor)
{
    EngineInfo info;
    bool saw_engine = false;
    std::size_t start = 0;
    while (start <= descriptor.size()) {
        std::size_t stop = descriptor.find(';', start);
        if (stop == std::string::npos)
            stop = descriptor.size();
        const std::string item = descriptor.substr(start, stop - start);
        const std::size_t eq = item.find('=');
        if (eq != std::string::npos) {
            const std::string key = item.substr(0, eq);
            const std::string value = item.substr(eq + 1);
            auto as_number = [&value]() {
                unsigned long long n = 0;
                for (char c : value) {
                    if (c < '0' || c > '9')
                        return 0ull;
                    n = n * 10 + static_cast<unsigned long long>(c - '0');
                    if (n > 1000000000000ull)
                        return 0ull;
                }
                return n;
            };
            if (key == "engine") saw_engine = value == "boost.regex";
            else if (key == "version") info.version = static_cast<unsigned long>(as_number());
            else if (key == "code_unit_bits") info.code_unit_bits = static_cast<unsigned>(as_number());
            else if (key == "traits") info.traits = value;
            else if (key == "syntax") info.syntax = value;
            else if (key == "max_states") info.max_states = as_number();
        }
        start = stop + 1;
    }
    info.reported = saw_engine && info.version != 0 &&
                    (info.code_unit_bits == 16 || info.code_unit_bits == 32);
    if (info.code_unit_bits != 16 && info.code_unit_bits != 32)
        info.code_unit_bits = static_cast<unsigned>(sizeof(wchar_t) * 8);
    return info;
}

// Human description of the locale traits name reported by the worker.
inline const char *traits_label(const std::string &traits)
{
    if (traits == "win32") return L("Windows locale (w32_regex_traits)");
    if (traits == "cpp") return L("C++ locale (cpp_regex_traits)");
    if (traits == "c") return L("C locale (c_regex_traits)");
    return L("unknown locale traits");
}

// -------------------------------------------------------- static tables ----

struct FlagRow
{
    const char    *name;        // builder flag or engine setting
    const wchar_t *engine;      // the exact engine flag it maps to
    const wchar_t *inline_form; // the in-pattern equivalent
    const char    *effect;
};

inline const std::vector<FlagRow> &flag_rows()
{
    static const std::vector<FlagRow> rows{
        {L("Regex mode"), L"ECMAScript (Perl syntax)", L"-",
         L("Off by default, so the query is plain text. When on, the worker compiles the query as a pattern.")},
        {L("Case sensitive"), L"icase (added when off)", L"(?i) (?-i)",
         L("Off by default, so letters match in either case. Folding follows the engine's locale traits.")},
        {L("Multiline anchors"), L"no_mod_m (cleared when on)", L"(?m) (?-m)",
         L("Off by default. When on, ^ and $ also match at line breaks.")},
        {L("Dot matches line breaks"), L"no_mod_s (always set)", L"(?s) (?-s)",
         L("Never set by a search field, so . stops at line breaks. Use (?s) inside the pattern to change it.")},
        {L("Free spacing"), L"mod_x (never set)", L"(?x) (?-x)",
         L("Spaces and # comments are ignored only after (?x) inside the pattern.")},
        {L("Whole word"), L"-", L"\\b",
         L("Applies to plain-text search only. In regex mode, write \\b around the word.")},
    };
    return rows;
}

struct EscapeRule
{
    const wchar_t *syntax;
    const char    *meaning;
};

inline const std::vector<EscapeRule> &escape_rules()
{
    static const std::vector<EscapeRule> rows{
        {L"\\ ^ $ . | ? * + ( ) [ ] { }",
         L("Outside brackets these characters are syntax. Put a backslash before one to match it literally.")},
        {L"[ \\] \\\\ \\[ \\^ \\- ]",
         L("Inside brackets, escape ], \\, [, a leading ^ and a - that is not first or last.")},
        {L"\\Q...\\E",
         L("Everything between \\Q and \\E is literal. Without \\E the quoted text runs to the end.")},
        {L"\\xHH \\x{HHHH}",
         L("A code unit by hexadecimal value. With 16-bit code units, values above FFFF are rejected.")},
        {L"\\0 \\0oo",
         L("NUL or an octal value. A backslash and a digit from 1 to 9 is a backreference, never octal.")},
        {L"\\t \\n \\r \\f \\a \\e \\cX",
         L("Tab, newline, carriage return, form feed, bell, escape and Ctrl+X control characters.")},
        {L"\\y \\j \\o",
         L("A backslash before a letter with no meaning here matches that letter. Other engines may reject it.")},
        {L"\\# \\ (space)",
         L("Under (?x), escape # and spaces that should match literally.")},
    };
    return rows;
}

enum class Support : std::uint8_t { Supported, Partial, Unsupported };
enum class ProbeExpect : std::uint8_t { Match, NoMatch, Rejected };

struct Capability
{
    const char    *id;
    const char    *feature;
    const wchar_t *syntax;
    Support        support;
    const char    *explanation;
    // Probe run through the real worker with default flags (case
    // insensitive, single-line anchors). It demonstrates the documented
    // behaviour, so a mismatch means this row no longer describes the engine.
    const wchar_t *probe;
    const wchar_t *subject;
    ProbeExpect    expect;
    const wchar_t *expected_match; // whole match text when expect == Match
};

inline const char *support_label(Support support)
{
    switch (support) {
    case Support::Supported: return L("Supported");
    case Support::Partial: return L("Partly supported");
    case Support::Unsupported: return L("Not supported");
    }
    return L("Not supported");
}

inline const std::vector<Capability> &capabilities()
{
    using S = Support;
    using P = ProbeExpect;
    static const std::vector<Capability> rows{
        {"literals", L("Literal characters"), L"abc \\.", S::Supported,
         L("Characters match themselves. Escape a metacharacter with a backslash."),
         L"a\\.b", L"xa.b", P::Match, L"a.b"},
        {"quoted-literals", L("Quoted literal text"), L"\\Q...\\E", S::Supported,
         L("Everything between \\Q and \\E matches literally."),
         L"\\Qa.b\\E", L"axb a.b", P::Match, L"a.b"},
        {"hex-escapes", L("Hexadecimal code units"), L"\\xHH \\x{HHHH}", S::Supported,
         L("One code unit by value. With 16-bit code units (Windows), values above FFFF are rejected, so type such characters directly."),
         L"\\x{41}\\x42", L"AB", P::Match, L"AB"},
        {"unicode-escape", L("\\uHHHH code point escape"), L"\\u0041", S::Unsupported,
         L("\\u is the uppercase-letter class here, so \\u0041 means an uppercase letter followed by 0041. Use \\x{0041}."),
         L"\\u0041", L"A", P::NoMatch, L""},
        {"control-escapes", L("Control escapes"), L"\\t \\n \\r \\f \\a \\e \\cX \\0", S::Supported,
         L("Tab, newline, carriage return, form feed, bell, escape, Ctrl+X and NUL."),
         L"\\t\\cA", L"\t\x01", P::Match, L"\t\x01"},
        {"octal-brace", L("\\o{...} octal escape"), L"\\o{12}", S::Unsupported,
         L("Not recognised: \\o is the letter o, so \\o{12} means twelve o characters. Use \\012 or \\x{A}."),
         L"\\o{12}", L"\n", P::NoMatch, L""},
        {"named-characters", L("\\N{name} named characters"), L"\\N{SPACE}", S::Unsupported,
         L("Rejected: this engine has no \\N escape."),
         L"\\N{SPACE}", L" ", P::Rejected, L""},
        {"character-sets", L("Character sets and ranges"), L"[abc] [^abc] [a-z]", S::Supported,
         L("Match one character from the list, or one not in it after ^."),
         L"[^a-c][x-z]", L"dy", P::Match, L"dy"},
        {"shorthand-classes", L("Class shorthands"), L"\\d \\w \\s \\h \\v \\l \\u", S::Supported,
         L("\\h is horizontal and \\v vertical whitespace; \\l and \\u are lowercase and uppercase letters. Capitals negate."),
         L"\\d\\s\\w", L"1 a", P::Match, L"1 a"},
        {"posix-classes", L("POSIX classes"), L"[[:alpha:]] [[:^digit:]]", S::Supported,
         L("Named classes inside brackets, including negated [:^name:] forms."),
         L"[[:alpha:]][[:^alpha:]]", L"a1", P::Match, L"a1"},
        {"class-names", L("Named classes with \\p"), L"\\p{alpha} \\P{digit}", S::Partial,
         L("Only the engine's class names work (alnum, alpha, blank, cntrl, digit, graph, lower, print, punct, space, upper, word, xdigit and the letters d, h, l, s, u, v, w). \\p{L} means lowercase here, not Letter. Inside brackets \\p is just the letter p."),
         L"\\p{alpha}\\P{digit}", L"ab", P::Match, L"ab"},
        {"unicode-categories", L("Unicode categories and scripts"), L"\\p{Lu} \\p{Nd} \\p{Han}", S::Unsupported,
         L("Rejected: the engine has no Unicode property tables."),
         L"\\p{Lu}", L"A", P::Rejected, L""},
        {"set-operations", L("Set intersection and subtraction"), L"[a-z&&[^aeiou]] [a-z--[aeiou]]", S::Unsupported,
         L("Not available: && is two literal ampersands and -- after a range is an error."),
         L"[a-z--[aeiou]]", L"b", P::Rejected, L""},
        {"anchors", L("Anchors"), L"^ $ \\A \\z \\Z \\G", S::Supported,
         L("Without multiline anchors, $ matches only at the very end; \\Z also allows one final line break."),
         L"\\Aab\\z", L"ab", P::Match, L"ab"},
        {"word-boundaries", L("Word boundaries"), L"\\b \\B \\< \\>", S::Supported,
         L("\\< and \\> match the start and end of a word."),
         L"\\<ab\\>", L"x ab y", P::Match, L"ab"},
        {"buffer-anchors", L("Buffer anchors"), L"\\` \\'", S::Supported,
         L("\\` and \\' are start-of-text and end-of-text anchors, not quote characters."),
         L"\\`ab\\'", L"ab", P::Match, L"ab"},
        {"numbered-groups", L("Numbered capture groups"), L"( )", S::Supported,
         L("Groups are numbered by their opening parenthesis."),
         L"(a)(b)", L"ab", P::Match, L"ab"},
        {"named-groups", L("Named capture groups"), L"(?<name>...) (?'name'...)", S::Supported,
         L("Named groups are also numbered; refer to them with \\k<name> or \\g{name}."),
         L"(?<n>a)\\k<n>", L"aa", P::Match, L"aa"},
        {"python-named-groups", L("Python-style named groups"), L"(?P<name>...) (?P=name)", S::Unsupported,
         L("Rejected: use (?<name>...), \\k<name> and (?&name) instead."),
         L"(?P<n>a)", L"a", P::Rejected, L""},
        {"non-capturing-groups", L("Non-capturing groups"), L"(?:...)", S::Supported,
         L("Group without creating a capture."),
         L"(?:ab)+", L"abab", P::Match, L"abab"},
        {"atomic-groups", L("Atomic groups"), L"(?>...)", S::Supported,
         L("Once the group matches, the engine never backtracks into it."),
         L"(?>a+)b", L"aab", P::Match, L"aab"},
        {"branch-reset", L("Branch reset groups"), L"(?|...)", S::Supported,
         L("Each alternative reuses the same group numbers."),
         L"(?|(a)|(b))\\1", L"bb", P::Match, L"bb"},
        {"alternation", L("Alternation"), L"a|b", S::Supported,
         L("Alternatives are tried from left to right."),
         L"red|blue", L"navy blue", P::Match, L"blue"},
        {"greedy-quantifiers", L("Greedy quantifiers"), L"* + ? {n} {n,} {n,m}", S::Supported,
         L("A { that does not form a count, such as {,3}, is a literal brace."),
         L"a{2,3}", L"aaaa", P::Match, L"aaa"},
        {"lazy-quantifiers", L("Lazy quantifiers"), L"*? +? ?? {n,m}?", S::Supported,
         L("Match as few repetitions as possible."),
         L"a{2,3}?", L"aaa", P::Match, L"aa"},
        {"possessive-quantifiers", L("Possessive quantifiers"), L"*+ ++ ?+ {n,m}+", S::Supported,
         L("Never give characters back, so a*+a cannot match."),
         L"a*+a", L"aaa", P::NoMatch, L""},
        {"lookahead", L("Lookahead"), L"(?=...) (?!...)", S::Supported,
         L("Checks what follows without consuming it."),
         L"a(?=b)", L"ab", P::Match, L"a"},
        {"lookbehind", L("Lookbehind"), L"(?<=...) (?<!...)", S::Partial,
         L("Fixed width only: every alternative must have the same constant length, and repeated groups, \\R, \\X and backreferences are rejected inside."),
         L"(?<=a{2})b", L"aab", P::Match, L"b"},
        {"variable-lookbehind", L("Variable-width lookbehind"), L"(?<=a+) (?<=a|bc)", S::Unsupported,
         L("Rejected by the engine. JavaScript and .NET accept it; rewrite with a fixed width or use \\K."),
         L"(?<=a+)b", L"aab", P::Rejected, L""},
        {"backreferences", L("Backreferences"), L"\\1 \\g{n} \\g{-n} \\k<name>", S::Supported,
         L("A backslash takes only one digit: \\10 is group 1 then 0, so write \\g{10}. \\g<1> is a backreference here, not a subroutine call."),
         L"(a)\\g{-1}", L"aa", P::Match, L"aa"},
        {"conditionals", L("Conditionals"), L"(?(1)yes|no) (?(<name>)...) (?(?=...)...) (?(DEFINE)...)", S::Supported,
         L("Choose an alternative by a group, a lookaround, recursion or a DEFINE block."),
         L"(a)?(?(1)b|c)", L"c", P::Match, L"c"},
        {"subroutines", L("Recursion and subroutine calls"), L"(?R) (?1) (?-1) (?+1) (?&name)", S::Supported,
         L("Re-run the whole pattern or one group at this point."),
         L"a(?R)?b", L"aabb", P::Match, L"aabb"},
        {"inline-modifiers", L("Inline modifiers"), L"(?imsx-imsx) (?imsx-imsx:...)", S::Partial,
         L("i, m, s and x work, for the rest of the group or inside (?flags:...)."),
         L"(?i)ab(?-i:c)", L"ABc", P::Match, L"ABc"},
        {"other-modifiers", L("Other inline modifiers"), L"(?n) (?U) (?J) (?^)", S::Unsupported,
         L("Rejected: only i, m, s and x are modifiers in this engine."),
         L"(?U)a+", L"aa", P::Rejected, L""},
        {"comments", L("Comments"), L"(?#...) # under (?x)", S::Supported,
         L("Ignored by the engine. An unclosed (?# runs to the end of the pattern."),
         L"a(?#note)b", L"ab", P::Match, L"ab"},
        {"keep-out", L("Match start reset"), L"\\K", S::Supported,
         L("Text before \\K must match but is left out of the reported match."),
         L"a\\Kb", L"ab", P::Match, L"b"},
        {"verbs", L("Backtracking control verbs"), L"(*ACCEPT) (*COMMIT) (*FAIL) (*PRUNE) (*SKIP) (*THEN)", S::Partial,
         L("These six verbs work. Verbs with an argument, such as (*MARK:name), are rejected."),
         L"a(*FAIL)|b", L"ab", P::Match, L"b"},
        {"marked-verbs", L("Verbs with names"), L"(*MARK:name) (*SKIP:name)", S::Unsupported,
         L("Rejected: verb arguments are not supported."),
         L"(*MARK:x)a", L"a", P::Rejected, L""},
        {"special-sequences", L("Line breaks, graphemes, code units"), L"\\R \\X \\C", S::Supported,
         L("\\R matches any line break (CRLF as one), \\X one grapheme cluster and \\C one code unit."),
         L"a\\Rb", L"a\r\nb", P::Match, L"a\r\nb"},
        {"case-folding", L("Case-insensitive matching"), L"icase (?i)", S::Supported,
         L("Case folding follows the engine's locale traits, so letters outside ASCII fold as that locale says."),
         L"(?i)ab", L"AB", P::Match, L"AB"},
        {"supplementary-characters", L("Characters above U+FFFF"), L"\U0001F600", S::Partial,
         L("With 16-bit code units (Windows) such a character is two code units: . and [ ] see each half, and a quantifier repeats only the second half. Type it directly outside brackets."),
         L"x\U0001F600", L"x\U0001F600", P::Match, L"x\U0001F600"},
    };
    return rows;
}

// Observation of one capability probe made by the caller through the worker.
struct ProbeObservation
{
    bool         answered = false; // the worker gave a definitive answer
    bool         rejected = false; // the pattern did not compile
    bool         matched  = false;
    std::wstring whole;            // whole match text when matched
};

inline bool confirms(const Capability &row, const ProbeObservation &seen)
{
    if (!seen.answered)
        return false;
    switch (row.expect) {
    case ProbeExpect::Rejected: return seen.rejected;
    case ProbeExpect::NoMatch: return !seen.rejected && !seen.matched;
    case ProbeExpect::Match:
        return !seen.rejected && seen.matched && seen.whole == std::wstring(row.expected_match);
    }
    return false;
}

// ----------------------------------------------------------- char sets ----

// Conservative character-set approximation used by the risk analysis. ASCII
// is exact; everything above U+007F is tracked as three buckets (letters and
// digits, whitespace, everything else), each either partly or fully covered.
struct CharSet
{
    static constexpr std::uint8_t kWord = 1, kSpace = 2, kSymbol = 4, kAll = 7;

    std::bitset<128> ascii;
    std::uint8_t     some = 0; // buckets that may contain members
    std::uint8_t     full = 0; // buckets entirely contained

    static std::uint8_t bucket_of(std::uint32_t c)
    {
        if (c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
            c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000)
            return kSpace;
        return kWord | kSymbol; // no Unicode tables here: letter or symbol
    }
    void add(std::uint32_t c)
    {
        if (c < 128) ascii.set(static_cast<std::size_t>(c));
        else some |= bucket_of(c);
    }
    void add_range(std::uint32_t first, std::uint32_t last)
    {
        for (std::uint32_t c = first; c <= last && c < 128; ++c)
            ascii.set(c);
        if (last >= 128)
            some = kAll;
    }
    void merge(const CharSet &o)
    {
        ascii |= o.ascii;
        some |= o.some;
        full |= o.full;
    }
    bool intersects(const CharSet &o) const { return (ascii & o.ascii).any() || (some & o.some) != 0; }
    bool empty() const { return ascii.none() && some == 0; }
    CharSet intersection(const CharSet &o) const
    {
        CharSet s;
        s.ascii = ascii & o.ascii;
        s.some = some & o.some;
        s.full = full & o.full;
        return s;
    }
    CharSet complement() const
    {
        CharSet s;
        s.ascii = ~ascii;
        s.some = static_cast<std::uint8_t>(kAll & ~full);
        s.full = static_cast<std::uint8_t>(kAll & ~some);
        return s;
    }
    static CharSet all()
    {
        CharSet s;
        s.ascii.set();
        s.some = s.full = kAll;
        return s;
    }
};

// ------------------------------------------------------------- analysis ----

enum class TokenKind : std::uint8_t {
    Literal, Escaped, Quoted, CodeUnit, Control, AnyChar, Shorthand, Property,
    SetOpen, SetMember, SetRange, PosixClass, SetClose,
    Anchor, Boundary, Keep, GroupOpen, GroupClose, Alternation, Quantifier,
    Backreference, Subroutine, Condition, Modifier, Comment, Verb, Special, Ignored, Error
};

enum class NodeKind : std::uint8_t { Alternation, Sequence, Empty, Group, Quantified, Set, Atom };

enum class GroupKind : std::uint8_t {
    None, Capture, NamedCapture, NonCapture, Atomic, BranchReset,
    LookAhead, NegativeLookAhead, LookBehind, NegativeLookBehind,
    Conditional, Modifiers, Unsupported
};

enum class QuantMode : std::uint8_t { Greedy, Lazy, Possessive };
enum class Severity : std::uint8_t { Info, Warning, Danger };
enum class RiskLevel : std::uint8_t { Low, Moderate, High };

inline const char *risk_label(RiskLevel level)
{
    switch (level) {
    case RiskLevel::Low: return L("Low");
    case RiskLevel::Moderate: return L("Moderate");
    case RiskLevel::High: return L("High");
    }
    return L("Low");
}

struct Token
{
    TokenKind   kind  = TokenKind::Literal;
    std::size_t begin = 0;
    std::size_t end   = 0;
    int         depth = 0; // group nesting depth
    Text        explanation;
};

struct Node
{
    NodeKind    kind  = NodeKind::Atom;
    GroupKind   group = GroupKind::None;
    std::size_t begin = 0;
    std::size_t end   = 0;
    Text        label;
    std::size_t parent = npos;
    std::vector<std::size_t> children;
    // Quantified nodes.
    std::size_t min = 1;
    std::size_t max = 1;
    bool        unbounded = false;
    QuantMode   mode = QuantMode::Greedy;
    // Match length in code units.
    std::size_t min_len = 0;
    std::size_t max_len = 0;
    bool        max_infinite = false;
    bool        zero_width   = false;
    bool        quantifiable = true;
    bool        single       = false; // matches exactly one character from `chars`
    bool        variable_atom = false; // \R, \X, backreference or call
    bool        any_char = false;      // the . atom
    bool        backref   = false;     // subtree contains a backreference
    bool        recursion = false;     // subtree contains recursion or a call
    CharSet     chars; // characters the subtree can consume
    CharSet     first; // characters a match can start with
    CharSet     last;  // characters a match can end with
    // Literal atoms only: the matched text and whether it may merge with
    // neighbouring literals into one "Literal text" node.
    std::wstring literal;
    bool        mergeable = false;
    std::size_t token = npos;
    std::size_t capture = 0;
};

struct Finding
{
    const char  *id = "";
    Severity     severity = Severity::Info;
    std::size_t  begin = 0;
    std::size_t  end   = 0;
    Text         message;
    std::wstring example; // adversarial input for risk findings, may be empty
};

struct Options
{
    bool     regex_mode     = true;
    bool     case_sensitive = false;
    bool     multiline      = false;
    bool     whole_word     = false;
    unsigned code_unit_bits = static_cast<unsigned>(sizeof(wchar_t) * 8);
};

struct Analysis
{
    std::wstring              pattern;
    std::vector<Token>        tokens;
    std::vector<Node>         nodes;
    std::size_t               root = npos;
    std::vector<Finding>      compatibility;
    std::vector<Finding>      risks;
    RiskLevel                 risk = RiskLevel::Low;
    std::size_t               capture_groups = 0;
    std::size_t               named_groups   = 0;
    bool                      can_match_empty = false;
    bool                      complete = true; // false when unbalanced or unterminated syntax was found

    bool has_compatibility(const char *id) const
    {
        for (const Finding &f : compatibility)
            if (std::string(f.id) == id)
                return true;
        return false;
    }
    bool has_risk(const char *id) const
    {
        for (const Finding &f : risks)
            if (std::string(f.id) == id)
                return true;
        return false;
    }
};

namespace detail {

inline bool is_digit(wchar_t c) { return c >= L'0' && c <= L'9'; }
inline bool is_hex(wchar_t c)
{
    return is_digit(c) || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
}
inline unsigned hex_value(wchar_t c)
{
    if (is_digit(c)) return static_cast<unsigned>(c - L'0');
    if (c >= L'a' && c <= L'f') return static_cast<unsigned>(c - L'a' + 10);
    return static_cast<unsigned>(c - L'A' + 10);
}
inline bool is_ascii_letter(wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'); }
inline bool is_space(wchar_t c)
{
    return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r' || c == L'\f' || c == L'\v';
}
inline bool is_high_surrogate(wchar_t c) { return c >= 0xD800 && c <= 0xDBFF; }
inline bool is_low_surrogate(wchar_t c) { return c >= 0xDC00 && c <= 0xDFFF; }
inline wchar_t to_lower(wchar_t c) { return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c + 32) : c; }

inline std::wstring lower(std::wstring s)
{
    for (wchar_t &c : s)
        c = to_lower(c);
    return s;
}

// Class names accepted by Boost.Regex's traits (case-insensitive), used by
// \p{name} and [:name:].
inline bool known_class_name(const std::wstring &name)
{
    static const wchar_t *names[] = {L"alnum", L"alpha", L"blank", L"cntrl", L"d", L"digit", L"graph",
                                     L"h", L"l", L"lower", L"print", L"punct", L"s", L"space", L"u",
                                     L"unicode", L"upper", L"v", L"w", L"word", L"xdigit"};
    const std::wstring key = lower(name);
    for (const wchar_t *n : names)
        if (key == n)
            return true;
    return false;
}

inline CharSet class_chars(const std::wstring &raw_name, bool icase)
{
    const std::wstring name = lower(raw_name);
    CharSet s;
    auto letters_lower = [&s]() { s.add_range('a', 'z'); s.some |= CharSet::kWord; };
    auto letters_upper = [&s]() { s.add_range('A', 'Z'); s.some |= CharSet::kWord; };
    if (name == L"d" || name == L"digit") { s.add_range('0', '9'); s.some = CharSet::kWord; }
    else if (name == L"w" || name == L"word") {
        s.add_range('0', '9'); s.add_range('a', 'z'); s.add_range('A', 'Z'); s.add('_');
        s.some = s.full = CharSet::kWord;
    }
    else if (name == L"alnum") { s.add_range('0', '9'); s.add_range('a', 'z'); s.add_range('A', 'Z'); s.some = CharSet::kWord; }
    else if (name == L"alpha") { s.add_range('a', 'z'); s.add_range('A', 'Z'); s.some = CharSet::kWord; }
    else if (name == L"s" || name == L"space") {
        for (wchar_t c : {L' ', L'\t', L'\n', L'\r', L'\f', L'\v'}) s.add(static_cast<std::uint32_t>(c));
        s.some = s.full = CharSet::kSpace;
    }
    else if (name == L"h" || name == L"blank") { s.add(' '); s.add('\t'); s.some = CharSet::kSpace; }
    else if (name == L"v") { for (wchar_t c : {L'\n', L'\r', L'\f', L'\v'}) s.add(static_cast<std::uint32_t>(c)); s.some = CharSet::kSpace; }
    else if (name == L"l" || name == L"lower") { letters_lower(); if (icase) letters_upper(); }
    else if (name == L"u" || name == L"upper") { letters_upper(); if (icase) letters_lower(); }
    else if (name == L"xdigit") { s.add_range('0', '9'); s.add_range('a', 'f'); s.add_range('A', 'F'); }
    else if (name == L"punct") { s.add_range('!', '/'); s.add_range(':', '@'); s.add_range('[', '`'); s.add_range('{', '~'); s.some = CharSet::kSymbol; }
    else if (name == L"cntrl") { s.add_range(0, 31); s.add(0x7F); s.some = CharSet::kSymbol; }
    else if (name == L"unicode") { s.some = CharSet::kAll; }
    else s = CharSet::all();
    return s;
}

inline CharSet shorthand_chars(wchar_t letter, bool icase)
{
    const wchar_t base = to_lower(letter);
    std::wstring name(1, base);
    CharSet s = class_chars(name, icase);
    if (letter >= L'A' && letter <= L'Z')
        s = s.complement();
    return s;
}

} // namespace detail

class Parser
{
public:
    Parser(const std::wstring &pattern, const Options &options, Analysis &out)
        : p(pattern), n(pattern.size()), o(options), a(out)
    {}

    void run()
    {
        a.pattern = p;
        Flags flags{!o.case_sensitive, o.multiline, false, false};
        a.root = parse_alternation(flags, 0, false);
        while (pos < n) {
            // Only an unbalanced ')' stops a top-level alternation.
            add_token(TokenKind::Error, pos, pos + 1, 0, text(L("Unbalanced closing parenthesis")));
            a.complete = false;
            ++pos;
            const std::size_t rest = parse_alternation(flags, 0, false);
            const std::size_t seq = new_node(NodeKind::Sequence, a.nodes[a.root].begin, pos);
            adopt(seq, a.root);
            adopt(seq, rest);
            a.root = seq;
        }
        finish();
    }

private:
    struct Flags
    {
        bool icase, multiline, dotall, extended;
    };

    struct Ref
    {
        std::size_t  node;
        std::size_t  begin, end;
        std::size_t  number; // 0 when by name
        std::wstring name;
        bool         call;  // subroutine call rather than backreference
        bool         early; // the group had not closed when the reference was read
    };

    struct Quant
    {
        std::size_t length = 0;
        std::size_t min = 0, max = 0;
        bool        unbounded = false;
        QuantMode   mode = QuantMode::Greedy;
    };

    const std::wstring &p;
    const std::size_t   n;
    const Options      &o;
    Analysis           &a;
    std::size_t         pos = 0;
    std::size_t         captures = 0;
    std::vector<bool>   closed{false}; // closed[k]: group k has closed by now
    std::vector<std::pair<std::wstring, std::size_t>> names;
    std::vector<Ref>    refs;
    std::vector<std::string> noted; // compatibility ids already reported once

    // --- building blocks ----------------------------------------------------

    std::size_t add_token(TokenKind kind, std::size_t b, std::size_t e, int depth, Text t)
    {
        Token tok;
        tok.kind = kind;
        tok.begin = b;
        tok.end = e;
        tok.depth = depth;
        tok.explanation = std::move(t);
        a.tokens.push_back(std::move(tok));
        return a.tokens.size() - 1;
    }

    std::size_t new_node(NodeKind kind, std::size_t b, std::size_t e)
    {
        Node node;
        node.kind = kind;
        node.begin = b;
        node.end = e;
        a.nodes.push_back(std::move(node));
        return a.nodes.size() - 1;
    }

    void adopt(std::size_t parent, std::size_t child)
    {
        if (child == npos)
            return;
        a.nodes[parent].children.push_back(child);
        a.nodes[child].parent = parent;
    }

    void note(const char *id, Severity severity, std::size_t b, std::size_t e, Text message, bool once = true)
    {
        if (once) {
            for (const std::string &seen : noted)
                if (seen == id)
                    return;
            noted.emplace_back(id);
        }
        Finding f;
        f.id = id;
        f.severity = severity;
        f.begin = b;
        f.end = e;
        f.message = std::move(message);
        a.compatibility.push_back(std::move(f));
    }

    std::wstring slice(std::size_t b, std::size_t e) const { return fragment(p, b, e); }

    std::size_t atom(std::size_t b, std::size_t e, TokenKind kind, int depth, Text label, const CharSet &chars,
                     bool single = true)
    {
        const std::size_t tok = add_token(kind, b, e, depth, label);
        const std::size_t id = new_node(NodeKind::Atom, b, e);
        Node &node = a.nodes[id];
        node.label = std::move(label);
        node.chars = chars;
        node.first = chars;
        node.last = chars;
        node.single = single;
        node.min_len = node.max_len = single ? 1 : 0;
        node.token = tok;
        return id;
    }

    std::size_t zero_width(std::size_t b, std::size_t e, TokenKind kind, int depth, Text label,
                           bool quantifiable = false)
    {
        const std::size_t tok = add_token(kind, b, e, depth, label);
        const std::size_t id = new_node(NodeKind::Atom, b, e);
        Node &node = a.nodes[id];
        node.label = std::move(label);
        node.zero_width = true;
        node.quantifiable = quantifiable;
        node.token = tok;
        return id;
    }

    CharSet char_of(wchar_t c, bool icase) const
    {
        CharSet s;
        s.add(c);
        if (icase && detail::is_ascii_letter(c))
            s.add(static_cast<wchar_t>(c ^ 0x20));
        return s;
    }

    std::size_t literal_atom(std::size_t b, std::size_t e, const std::wstring &value, TokenKind kind, int depth,
                             const Flags &flags, Text label)
    {
        CharSet chars;
        for (wchar_t c : value)
            chars.merge(char_of(c, flags.icase));
        const std::size_t id = atom(b, e, kind, depth, std::move(label), chars);
        Node &node = a.nodes[id];
        node.literal = value;
        node.mergeable = true;
        node.min_len = node.max_len = value.size();
        node.single = value.size() == 1 ||
                      (value.size() == 2 && detail::is_high_surrogate(value[0]) && detail::is_low_surrogate(value[1]));
        if (!value.empty()) {
            node.first = char_of(value[0], flags.icase);
            node.last = char_of(value.back(), flags.icase);
        }
        return id;
    }

    // --- grammar --------------------------------------------------------------

    void skip_extended(const Flags &flags, int depth)
    {
        if (!flags.extended)
            return;
        while (pos < n) {
            if (detail::is_space(p[pos])) {
                const std::size_t b = pos;
                while (pos < n && detail::is_space(p[pos]))
                    ++pos;
                add_token(TokenKind::Ignored, b, pos, depth, text(L("Ignored whitespace (free-spacing mode)")));
            } else if (p[pos] == L'#') {
                const std::size_t b = pos;
                while (pos < n && p[pos] != L'\n')
                    ++pos;
                add_token(TokenKind::Comment, b, pos, depth, text(L("Comment, ignored by the engine")));
            } else {
                break;
            }
        }
    }

    std::size_t parse_alternation(Flags &flags, int depth, bool branch_reset)
    {
        const std::size_t b = pos;
        std::vector<std::size_t> branches;
        const std::size_t base = captures;
        std::size_t most = captures;
        branches.push_back(parse_sequence(flags, depth));
        while (pos < n && p[pos] == L'|') {
            add_token(TokenKind::Alternation, pos, pos + 1, depth, text(L("Alternation — matches either side")));
            ++pos;
            if (branch_reset) {
                most = std::max(most, captures);
                captures = base;
            }
            branches.push_back(parse_sequence(flags, depth));
        }
        if (branch_reset)
            captures = std::max(most, captures);
        if (branches.size() == 1)
            return branches.front();
        const std::size_t alt = new_node(NodeKind::Alternation, b, pos);
        a.nodes[alt].label = text(L("One of %s alternatives"), {number(branches.size())});
        for (std::size_t branch : branches)
            adopt(alt, branch);
        return alt;
    }

    std::size_t parse_sequence(Flags &flags, int depth)
    {
        const std::size_t b = pos;
        std::vector<std::size_t> items;
        for (;;) {
            skip_extended(flags, depth);
            if (pos >= n || p[pos] == L'|' || p[pos] == L')')
                break;
            std::size_t item = parse_atom(flags, depth);
            if (item == npos)
                continue;
            for (;;) {
                skip_extended(flags, depth);
                const Quant q = quantifier_at(pos);
                if (q.length == 0)
                    break;
                if (!a.nodes[item].quantifiable) {
                    add_token(TokenKind::Error, pos, pos + q.length, depth,
                              text(L("Quantifier with nothing to repeat")));
                    pos += q.length;
                    continue;
                }
                // A quantifier repeats only the last character of quoted text.
                Node &target = a.nodes[item];
                if (target.mergeable && target.literal.size() > 1 && !target.single) {
                    const bool pair = target.literal.size() >= 2 &&
                                      detail::is_low_surrogate(target.literal.back()) &&
                                      detail::is_high_surrogate(target.literal[target.literal.size() - 2]);
                    const std::size_t keep = target.literal.size() - (pair ? 2 : 1);
                    const std::wstring tail = target.literal.substr(keep);
                    target.literal.resize(keep);
                    target.min_len = target.max_len = keep;
                    target.label = text(L("Quoted literal text: %s"), {target.literal});
                    CharSet chars;
                    for (wchar_t c : target.literal)
                        chars.merge(char_of(c, flags.icase));
                    target.chars = chars;
                    target.last = char_of(target.literal.back(), flags.icase);
                    const bool quote_closed = target.end >= 2 && p.compare(target.end - 2, 2, L"\\E") == 0;
                    const std::size_t content_end = quote_closed ? target.end - 2 : target.end;
                    const std::size_t tail_begin = content_end - tail.size();
                    target.end = tail_begin;
                    items.push_back(item);
                    const std::size_t last = new_node(NodeKind::Atom, tail_begin, content_end);
                    Node &tail_node = a.nodes[last];
                    tail_node.label = text(L("Literal text: %s"), {tail});
                    tail_node.literal = tail;
                    tail_node.single = true;
                    tail_node.min_len = tail_node.max_len = tail.size();
                    tail_node.chars = char_of(tail[0], flags.icase);
                    tail_node.first = tail_node.chars;
                    tail_node.last = tail_node.chars;
                    item = last;
                }
                const std::size_t qb = pos;
                pos += q.length;
                add_token(TokenKind::Quantifier, qb, pos, depth, quantifier_text(q));
                const std::size_t wrap = new_node(NodeKind::Quantified, a.nodes[item].begin, pos);
                Node &w = a.nodes[wrap];
                w.min = q.min;
                w.max = q.max;
                w.unbounded = q.unbounded;
                w.mode = q.mode;
                w.label = quantifier_text(q);
                adopt(wrap, item);
                item = wrap;
                if (q.mode == QuantMode::Possessive)
                    portability(L"*+ ++ ?+", qb, pos);
            }
            items.push_back(item);
        }
        merge_literals(items);
        if (items.empty()) {
            const std::size_t empty = new_node(NodeKind::Empty, b, b);
            a.nodes[empty].label = text(L("Empty: matches the empty string"));
            return empty;
        }
        if (items.size() == 1)
            return items.front();
        const std::size_t seq = new_node(NodeKind::Sequence, b, pos);
        a.nodes[seq].label = text(L("Sequence of %s items"), {number(items.size())});
        for (std::size_t item : items)
            adopt(seq, item);
        return seq;
    }

    // Folds runs of adjacent unquantified literal atoms into one node and one
    // token, so "PLA" reads as one literal rather than three.
    void merge_literals(std::vector<std::size_t> &items)
    {
        std::vector<std::size_t> out;
        for (std::size_t item : items) {
            Node &node = a.nodes[item];
            if (!out.empty()) {
                Node &prev = a.nodes[out.back()];
                const bool adjacent_tokens = prev.token != npos && node.token != npos &&
                                             node.token == prev.token + 1 &&
                                             a.tokens[prev.token].end == a.tokens[node.token].begin;
                if (prev.mergeable && node.mergeable && adjacent_tokens &&
                    a.tokens[prev.token].kind != TokenKind::Quoted && a.tokens[node.token].kind != TokenKind::Quoted) {
                    prev.literal += node.literal;
                    prev.end = node.end;
                    prev.min_len = prev.max_len = prev.literal.size();
                    prev.single = false;
                    prev.chars.merge(node.chars);
                    prev.last = node.last;
                    prev.label = text(L("Literal text: %s"), {prev.literal});
                    Token &tok = a.tokens[prev.token];
                    tok.end = a.tokens[node.token].end;
                    tok.kind = TokenKind::Literal;
                    tok.explanation = prev.label;
                    a.tokens.erase(a.tokens.begin() + static_cast<std::ptrdiff_t>(node.token));
                    for (Node &other : a.nodes)
                        if (other.token != npos && other.token > prev.token && &other != &node)
                            --other.token;
                    node.token = npos;
                    node.kind = NodeKind::Empty; // orphaned; never adopted
                    node.mergeable = false;
                    continue;
                }
            }
            out.push_back(item);
        }
        items.swap(out);
    }

    Quant quantifier_at(std::size_t at) const
    {
        Quant q;
        if (at >= n)
            return q;
        std::size_t i = at;
        const wchar_t c = p[i];
        if (c == L'*') { q.min = 0; q.unbounded = true; ++i; }
        else if (c == L'+') { q.min = 1; q.unbounded = true; ++i; }
        else if (c == L'?') { q.min = 0; q.max = 1; ++i; }
        else if (c == L'{') {
            std::size_t j = i + 1;
            auto spaces = [&]() { while (j < n && p[j] == L' ') ++j; };
            auto digits = [&](std::size_t &value) {
                const std::size_t start = j;
                value = 0;
                while (j < n && detail::is_digit(p[j])) {
                    value = std::min<std::size_t>(value * 10 + static_cast<std::size_t>(p[j] - L'0'), 1000000000);
                    ++j;
                }
                return j > start;
            };
            spaces();
            std::size_t low = 0, high = 0;
            if (!digits(low))
                return Quant{};
            spaces();
            if (j < n && p[j] == L'}') {
                q.min = q.max = low;
                i = j + 1;
            } else if (j < n && p[j] == L',') {
                ++j;
                spaces();
                const bool has_high = digits(high);
                spaces();
                if (j >= n || p[j] != L'}')
                    return Quant{};
                q.min = low;
                if (has_high) q.max = high;
                else q.unbounded = true;
                i = j + 1;
            } else {
                return Quant{};
            }
        } else {
            return q;
        }
        if (i < n && p[i] == L'?') { q.mode = QuantMode::Lazy; ++i; }
        else if (i < n && p[i] == L'+') { q.mode = QuantMode::Possessive; ++i; }
        q.length = i - at;
        if (!q.unbounded && q.max < q.min)
            q.max = q.min; // the engine rejects {3,1}; keep the model consistent
        return q;
    }

    static Text quantifier_text(const Quant &q)
    {
        if (!q.unbounded && q.min == q.max)
            return text(L("Repeat exactly %s times"), {number(q.min)});
        if (q.unbounded) {
            switch (q.mode) {
            case QuantMode::Greedy: return text(L("Repeat %s or more times (greedy: as many as possible)"), {number(q.min)});
            case QuantMode::Lazy: return text(L("Repeat %s or more times (lazy: as few as possible)"), {number(q.min)});
            case QuantMode::Possessive: return text(L("Repeat %s or more times (possessive: never gives back)"), {number(q.min)});
            }
        }
        switch (q.mode) {
        case QuantMode::Greedy: return text(L("Repeat %s to %s times (greedy: as many as possible)"), {number(q.min), number(q.max)});
        case QuantMode::Lazy: return text(L("Repeat %s to %s times (lazy: as few as possible)"), {number(q.min), number(q.max)});
        case QuantMode::Possessive: return text(L("Repeat %s to %s times (possessive: never gives back)"), {number(q.min), number(q.max)});
        }
        return text(L("Repeat exactly %s times"), {number(q.min)});
    }

    void portability(const wchar_t *construct, std::size_t b, std::size_t e)
    {
        // One portability note per construct family keeps the list readable.
        for (const Finding &f : a.compatibility)
            if (std::string(f.id) == "javascript-portability" && !f.message.args.empty() &&
                f.message.args.front() == construct)
                return;
        Finding f;
        f.id = "javascript-portability";
        f.severity = Severity::Info;
        f.begin = b;
        f.end = e;
        f.message = text(L("JavaScript RegExp has no %s, so this pattern will not work there unchanged."), {construct});
        a.compatibility.push_back(std::move(f));
    }

    std::size_t parse_atom(Flags &flags, int depth)
    {
        const std::size_t b = pos;
        const wchar_t c = p[pos];
        switch (c) {
        case L'(':
            return parse_group(flags, depth);
        case L'[':
            return parse_set(flags, depth);
        case L'.': {
            ++pos;
            CharSet chars = CharSet::all();
            std::size_t id;
            if (!flags.dotall) {
                chars.ascii.reset(static_cast<std::size_t>(L'\n'));
                id = atom(b, pos, TokenKind::AnyChar, depth, text(L("Any single character except newline")), chars);
            } else {
                id = atom(b, pos, TokenKind::AnyChar, depth, text(L("Any character, including line breaks")), chars);
            }
            a.nodes[id].any_char = true;
            return id;
        }
        case L'^':
            ++pos;
            return zero_width(b, pos, TokenKind::Anchor, depth,
                              flags.multiline ? text(L("Start of a line")) : text(L("Start of the text")));
        case L'$':
            ++pos;
            if (!flags.multiline) {
                note("dollar-end", Severity::Info, b, pos,
                     text(L("Without multiline anchors $ matches only at the very end, not before a final line break as in Perl and PCRE. Use \\Z to allow one.")));
                return zero_width(b, pos, TokenKind::Anchor, depth, text(L("End of the text")));
            }
            return zero_width(b, pos, TokenKind::Anchor, depth, text(L("End of a line")));
        case L'\\':
            return parse_escape(flags, depth);
        case L'*': case L'+': case L'?': {
            const Quant q = quantifier_at(pos);
            pos += std::max<std::size_t>(1, q.length);
            add_token(TokenKind::Error, b, pos, depth, text(L("Quantifier with nothing to repeat")));
            return npos;
        }
        case L'{': {
            const Quant q = quantifier_at(pos);
            if (q.length != 0) {
                pos += q.length;
                add_token(TokenKind::Error, b, pos, depth, text(L("Quantifier with nothing to repeat")));
                return npos;
            }
            ++pos;
            if (pos < n && (p[pos] == L',' || detail::is_digit(p[pos])))
                note("literal-brace", Severity::Info, b, pos,
                     text(L("This { does not form a valid count, so it is a literal brace. Some engines read {,n} as a count.")));
            return literal_atom(b, pos, L"{", TokenKind::Literal, depth, flags, text(L("Literal text: %s"), {L"{"}));
        }
        default:
            break;
        }
        // Plain literal (a surrogate pair is one character of two code units).
        std::size_t e = pos + 1;
        if (detail::is_high_surrogate(c) && e < n && detail::is_low_surrogate(p[e]))
            ++e;
        const std::wstring value = p.substr(pos, e - pos);
        pos = e;
        if (e - b == 2 || static_cast<std::uint32_t>(c) > 0xFFFF)
            supplementary(b, e);
        return literal_atom(b, e, value, TokenKind::Literal, depth, flags, text(L("Literal text: %s"), {value}));
    }

    void supplementary(std::size_t b, std::size_t e)
    {
        if (o.code_unit_bits == 16)
            note("supplementary-character", Severity::Warning, b, e,
                 text(L("A character above U+FFFF is two UTF-16 code units for this engine: inside [ ] each half is separate and a quantifier repeats only the second half.")));
        else
            note("supplementary-character", Severity::Info, b, e,
                 text(L("A character above U+FFFF is one code unit here but two in UTF-16 engines, including the Windows build.")));
    }

    std::size_t control_atom(std::size_t b, std::size_t e, std::uint32_t value, int depth, const Flags &flags,
                             const char *msgid)
    {
        const std::wstring v(1, static_cast<wchar_t>(value));
        const std::size_t id = literal_atom(b, e, v, TokenKind::Control, depth, flags,
                                            text(msgid, {hex_code(value)}));
        a.nodes[id].mergeable = false;
        return id;
    }

    // Parses \x.. at pos (pointing at the backslash). Returns false on error.
    bool hex_escape(std::size_t &value)
    {
        std::size_t i = pos + 2;
        value = 0;
        if (i < n && p[i] == L'{') {
            std::size_t j = i + 1;
            while (j < n && detail::is_hex(p[j]) && j - i <= 8) {
                value = value * 16 + detail::hex_value(p[j]);
                ++j;
            }
            if (j == i + 1 || j >= n || p[j] != L'}') {
                pos = std::min(n, j + (j < n && p[j] == L'}' ? 1 : 0));
                return false;
            }
            pos = j + 1;
            return true;
        }
        std::size_t j = i;
        while (j < n && j < i + 2 && detail::is_hex(p[j])) {
            value = value * 16 + detail::hex_value(p[j]);
            ++j;
        }
        if (j == i) {
            pos = std::min(n, i);
            return false;
        }
        pos = j;
        return true;
    }

    std::size_t parse_escape(Flags &flags, int depth)
    {
        const std::size_t b = pos;
        if (pos + 1 >= n) {
            pos = n;
            add_token(TokenKind::Error, b, n, depth, text(L("Trailing backslash with nothing to escape")));
            a.complete = false;
            return npos;
        }
        const wchar_t e = p[pos + 1];
        if (e >= L'1' && e <= L'9') {
            pos += 2;
            const std::size_t number_ref = static_cast<std::size_t>(e - L'0');
            if (pos < n && detail::is_digit(p[pos]))
                note("single-digit-backreference", Severity::Warning, b, pos + 1,
                     text(L("A backslash takes only one digit here: %s is group %s followed by the digit %s. Write \\g{%s} for a higher group."),
                          {slice(b, pos + 1), number(number_ref), std::wstring(1, p[pos]), slice(b + 1, pos + 1)}));
            return backreference(b, pos, number_ref, L"", depth, false);
        }
        if (e == L'0') {
            std::size_t j = pos + 2;
            std::uint32_t value = 0;
            while (j < n && j < pos + 4 && p[j] >= L'0' && p[j] <= L'7') {
                value = value * 8 + static_cast<std::uint32_t>(p[j] - L'0');
                ++j;
            }
            pos = j;
            return control_atom(b, pos, value, depth, flags, L("Octal escape for code unit U+%s"));
        }
        switch (e) {
        case L'x': {
            std::size_t value = 0;
            if (!hex_escape(value)) {
                add_token(TokenKind::Error, b, std::max(pos, b + 2), depth,
                          text(L("Incomplete hexadecimal escape")));
                pos = std::max(pos, b + 2);
                return npos;
            }
            if (value > 0xFFFF) {
                if (o.code_unit_bits == 16)
                    note("hex-above-ffff", Severity::Warning, b, pos,
                         text(L("%s is above FFFF, which 16-bit code units cannot hold, so the Windows engine rejects it. Type the character itself."), {slice(b, pos)}));
                else
                    supplementary(b, pos);
            }
            return control_atom(b, pos, static_cast<std::uint32_t>(std::min<std::size_t>(value, 0x10FFFF)), depth, flags,
                                L("Code unit U+%s by hexadecimal value"));
        }
        case L'c': {
            if (pos + 2 >= n) {
                add_token(TokenKind::Error, b, n, depth, text(L("Incomplete control escape")));
                pos = n;
                return npos;
            }
            const wchar_t letter = p[pos + 2];
            pos += 3;
            return control_atom(b, pos, static_cast<std::uint32_t>(letter) % 32, depth, flags,
                                L("Control character U+%s"));
        }
        case L'a': pos += 2; return control_atom(b, pos, 0x07, depth, flags, L("Bell character U+%s"));
        case L'e': pos += 2; return control_atom(b, pos, 0x1B, depth, flags, L("Escape character U+%s"));
        case L'f': pos += 2; return control_atom(b, pos, 0x0C, depth, flags, L("Form feed U+%s"));
        case L'n': pos += 2; return control_atom(b, pos, 0x0A, depth, flags, L("Newline (line feed) U+%s"));
        case L'r': pos += 2; return control_atom(b, pos, 0x0D, depth, flags, L("Carriage return U+%s"));
        case L't': pos += 2; return control_atom(b, pos, 0x09, depth, flags, L("Tab character U+%s"));
        case L'd': case L'D': case L'w': case L'W': case L's': case L'S':
        case L'h': case L'H': case L'v': case L'V': case L'l': case L'L': case L'u': case L'U': {
            pos += 2;
            shorthand_notes(e, b, flags);
            return atom(b, pos, TokenKind::Shorthand, depth, shorthand_text(e),
                        detail::shorthand_chars(e, flags.icase));
        }
        case L'p': case L'P': {
            std::size_t j = pos + 2;
            std::wstring name;
            if (j < n && p[j] == L'{') {
                const std::size_t close = p.find(L'}', j);
                if (close == std::wstring::npos) {
                    pos = n;
                    add_token(TokenKind::Error, b, n, depth, text(L("Unterminated \\p{...} class name")));
                    return npos;
                }
                name = p.substr(j + 1, close - j - 1);
                j = close + 1;
            } else if (j < n) {
                name = p.substr(j, 1);
                ++j;
            } else {
                pos = n;
                add_token(TokenKind::Error, b, n, depth, text(L("Unterminated \\p{...} class name")));
                return npos;
            }
            pos = j;
            property_notes(name, b, pos);
            const bool negated = e == L'P';
            CharSet chars = detail::known_class_name(name) ? detail::class_chars(name, flags.icase) : CharSet::all();
            if (negated && detail::known_class_name(name))
                chars = chars.complement();
            return atom(b, pos, TokenKind::Property, depth,
                        negated ? text(L("Characters outside class %s"), {name})
                                : text(L("Characters in class %s"), {name}),
                        chars);
        }
        case L'b':
            pos += 2;
            return zero_width(b, pos, TokenKind::Boundary, depth, text(L("Word boundary")));
        case L'B':
            pos += 2;
            return zero_width(b, pos, TokenKind::Boundary, depth, text(L("Not a word boundary")));
        case L'<':
            pos += 2;
            note("word-anchors", Severity::Info, b, pos,
                 text(L("\\< and \\> are word start and end anchors here, not literal angle brackets. Escape-free < and > match themselves.")));
            return zero_width(b, pos, TokenKind::Boundary, depth, text(L("Start of a word")));
        case L'>':
            pos += 2;
            note("word-anchors", Severity::Info, b, pos,
                 text(L("\\< and \\> are word start and end anchors here, not literal angle brackets. Escape-free < and > match themselves.")));
            return zero_width(b, pos, TokenKind::Boundary, depth, text(L("End of a word")));
        case L'A':
            pos += 2;
            portability(L"\\A \\z \\Z", b, pos);
            return zero_width(b, pos, TokenKind::Anchor, depth, text(L("Start of the text, even with multiline anchors")));
        case L'z':
            pos += 2;
            portability(L"\\A \\z \\Z", b, pos);
            return zero_width(b, pos, TokenKind::Anchor, depth, text(L("End of the text, even with multiline anchors")));
        case L'Z':
            pos += 2;
            portability(L"\\A \\z \\Z", b, pos);
            return zero_width(b, pos, TokenKind::Anchor, depth, text(L("End of the text or before a final line break")));
        case L'G':
            pos += 2;
            return zero_width(b, pos, TokenKind::Anchor, depth, text(L("Where the previous match ended")));
        case L'`':
        case L'\'':
            pos += 2;
            note("buffer-anchors", Severity::Warning, b, pos,
                 text(L("%s is a start-of-text or end-of-text anchor here, not a quote character. Write the quote without a backslash."), {slice(b, pos)}));
            return zero_width(b, pos, TokenKind::Anchor, depth,
                              e == L'`' ? text(L("Start of the text (buffer anchor)"))
                                        : text(L("End of the text (buffer anchor)")));
        case L'K':
            pos += 2;
            portability(L"\\K", b, pos);
            return zero_width(b, pos, TokenKind::Keep, depth, text(L("Reported match starts here; earlier text must still match")));
        case L'R': {
            pos += 2;
            CharSet chars;
            for (wchar_t c : {L'\n', L'\r', L'\f', L'\v'})
                chars.add(static_cast<std::uint32_t>(c));
            chars.some = CharSet::kSpace;
            const std::size_t id = atom(b, pos, TokenKind::Special, depth,
                                        text(L("Any line break sequence, CRLF counted as one")), chars, false);
            a.nodes[id].min_len = 1;
            a.nodes[id].max_len = 2;
            a.nodes[id].variable_atom = true;
            return id;
        }
        case L'X': {
            pos += 2;
            const std::size_t id = atom(b, pos, TokenKind::Special, depth,
                                        text(L("One grapheme cluster: a character with its combining marks")),
                                        CharSet::all(), false);
            a.nodes[id].min_len = 1;
            a.nodes[id].max_infinite = true;
            a.nodes[id].variable_atom = true;
            return id;
        }
        case L'C':
            pos += 2;
            return atom(b, pos, TokenKind::Special, depth, text(L("Exactly one code unit")), CharSet::all());
        case L'Q': {
            const std::size_t start = pos + 2;
            std::size_t close = p.find(L"\\E", start);
            const std::size_t content_end = close == std::wstring::npos ? n : close;
            pos = close == std::wstring::npos ? n : close + 2;
            portability(L"\\Q...\\E", b, pos);
            const std::wstring value = p.substr(start, content_end - start);
            if (value.empty()) {
                add_token(TokenKind::Quoted, b, pos, depth, text(L("Empty quoted text")));
                return npos;
            }
            const std::size_t id = literal_atom(b, pos, value, TokenKind::Quoted, depth, flags,
                                                text(L("Quoted literal text: %s"), {value}));
            a.nodes[id].single = value.size() == 1;
            return id;
        }
        case L'E':
            pos += 2;
            note("stray-quote-end", Severity::Info, b, pos,
                 text(L("\\E without an earlier \\Q is the literal letter E here.")));
            return literal_atom(b, pos, L"E", TokenKind::Escaped, depth, flags, text(L("Literal text: %s"), {L"E"}));
        case L'g':
            return parse_g_reference(b, flags, depth);
        case L'k':
            return parse_k_reference(b, depth);
        case L'N':
            pos += 2;
            if (pos < n && p[pos] == L'{') {
                const std::size_t close = p.find(L'}', pos);
                pos = close == std::wstring::npos ? n : close + 1;
            }
            add_token(TokenKind::Error, b, pos, depth, text(L("Not recognised by this engine: %s"), {slice(b, pos)}));
            return npos;
        default:
            break;
        }
        // Escaped literal.
        std::size_t end = pos + 2;
        if (detail::is_high_surrogate(e) && end < n && detail::is_low_surrogate(p[end]))
            ++end;
        const std::wstring value = p.substr(pos + 1, end - pos - 1);
        pos = end;
        if (detail::is_ascii_letter(e)) {
            if (e == L'o' && pos < n && p[pos] == L'{')
                note("octal-brace", Severity::Warning, b, pos + 1,
                     text(L("\\o{...} is not an octal escape here: \\o is the letter o and the braces repeat it. Use \\0oo or \\x{...}.")));
            else
                note("unknown-escape", Severity::Warning, b, pos,
                     text(L("%s has no special meaning here, so it matches the letter %s. JavaScript with the u flag, Python and .NET reject it."),
                          {slice(b, pos), value}));
        }
        return literal_atom(b, pos, value, TokenKind::Escaped, depth, flags,
                            text(L("Escaped %s: matches the character itself"), {value}));
    }

    void shorthand_notes(wchar_t e, std::size_t b, const Flags &flags)
    {
        (void) flags;
        if (e == L'u' || e == L'U') {
            bool four_hex = pos + 4 <= n;
            for (std::size_t i = pos; four_hex && i < pos + 4; ++i)
                four_hex = detail::is_hex(p[i]);
            if (four_hex)
                note("u-escape", Severity::Warning, b, pos + 4,
                     text(L("%s is not a code point here: \\u is the uppercase-letter class, followed by the literal text %s. Write \\x{%s}."),
                          {slice(b, pos + 4), slice(pos, pos + 4), slice(pos, pos + 4)}),
                     false);
            else
                note("u-class", Severity::Warning, b, pos,
                     text(L("\\u and \\U are uppercase-letter classes in this engine. In JavaScript, Java, .NET and Python \\u starts a code point escape.")));
        } else if (e == L'l' || e == L'L') {
            note("l-class", Severity::Info, b, pos,
                 text(L("\\l and \\L are lowercase-letter classes that only Boost.Regex understands.")));
        } else if (e == L'h' || e == L'H' || e == L'v' || e == L'V') {
            note("hv-class", Severity::Info, b, pos,
                 text(L("\\h and \\v match horizontal and vertical whitespace here. In JavaScript \\v is only the vertical tab and \\h is not an escape.")));
        }
    }

    static Text shorthand_text(wchar_t e)
    {
        switch (e) {
        case L'd': return text(L("Digit 0-9"));
        case L'D': return text(L("Any character that is not a digit"));
        case L'w': return text(L("Word character: letter, digit, or underscore"));
        case L'W': return text(L("Any character that is not a word character"));
        case L's': return text(L("Whitespace character"));
        case L'S': return text(L("Any character that is not whitespace"));
        case L'h': return text(L("Horizontal whitespace: space or tab"));
        case L'H': return text(L("Not horizontal whitespace"));
        case L'v': return text(L("Vertical whitespace: a line break character"));
        case L'V': return text(L("Not vertical whitespace"));
        case L'l': return text(L("Lowercase letter"));
        case L'L': return text(L("Not a lowercase letter"));
        case L'u': return text(L("Uppercase letter"));
        case L'U': return text(L("Not an uppercase letter"));
        default: break;
        }
        return text(L("Digit 0-9"));
    }

    void property_notes(const std::wstring &name, std::size_t b, std::size_t e)
    {
        const std::wstring key = detail::lower(name);
        if (key.size() == 1 && detail::known_class_name(key)) {
            note("p-letter-class", Severity::Warning, b, e,
                 text(L("%s uses the engine class %s, not the Unicode general category %s. Write the full class name, such as \\p{alpha}."),
                      {slice(b, e), key, name}),
                 false);
        } else if (!detail::known_class_name(name)) {
            note("p-unknown-class", Severity::Warning, b, e,
                 text(L("%s is rejected: this engine has no Unicode categories or scripts, only its class names such as alpha, digit and upper."),
                      {slice(b, e)}),
                 false);
        }
    }

    std::size_t backreference(std::size_t b, std::size_t e, std::size_t number_ref, const std::wstring &name,
                              int depth, bool call)
    {
        std::size_t id;
        if (call) {
            Text label = name.empty()
                             ? (number_ref == 0 ? text(L("Recurse into the whole pattern"))
                                                : text(L("Call group %s as a subroutine"), {number(number_ref)}))
                             : text(L("Call group \"%s\" as a subroutine"), {name});
            id = atom(b, e, TokenKind::Subroutine, depth, label, CharSet::all(), false);
            a.nodes[id].recursion = true;
            portability(L"(?R) (?1) (?&name)", b, e);
        } else {
            Text label = name.empty() ? text(L("Backreference to group %s"), {number(number_ref)})
                                      : text(L("Backreference to group \"%s\""), {name});
            id = atom(b, e, TokenKind::Backreference, depth, label, CharSet::all(), false);
            a.nodes[id].backref = true;
        }
        Node &node = a.nodes[id];
        node.min_len = 0;
        node.max_infinite = true;
        node.variable_atom = true;
        Ref ref;
        ref.node = id;
        ref.begin = b;
        ref.end = e;
        ref.number = number_ref;
        ref.name = name;
        ref.call = call;
        ref.early = false;
        // A backreference to a group that has not closed yet can never match
        // at this point; record that state now, while parsing in order.
        if (!call && name.empty() && number_ref != 0 && (number_ref >= closed.size() || !closed[number_ref]))
            ref.early = true;
        if (!call && !name.empty()) {
            bool known_closed = false;
            for (const auto &entry : names)
                if (entry.first == name && entry.second < closed.size() && closed[entry.second])
                    known_closed = true;
            ref.early = !known_closed;
        }
        refs.push_back(ref);
        return id;
    }

    // \g{n} \g{-n} \g{name} \g<n> \g'n' \gn \g-n
    std::size_t parse_g_reference(std::size_t b, const Flags &flags, int depth)
    {
        (void) flags;
        std::size_t j = pos + 2;
        std::wstring body;
        wchar_t close = 0;
        if (j < n && (p[j] == L'{' || p[j] == L'<' || p[j] == L'\'')) {
            close = p[j] == L'{' ? L'}' : (p[j] == L'<' ? L'>' : L'\'');
            const std::size_t end = p.find(close, j + 1);
            if (end == std::wstring::npos) {
                pos = n;
                add_token(TokenKind::Error, b, n, depth, text(L("Incomplete \\g or \\k reference")));
                return npos;
            }
            body = p.substr(j + 1, end - j - 1);
            j = end + 1;
        } else {
            const std::size_t start = j;
            if (j < n && (p[j] == L'-' || p[j] == L'+'))
                ++j;
            while (j < n && detail::is_digit(p[j]))
                ++j;
            body = p.substr(start, j - start);
        }
        pos = j;
        if (body.empty()) {
            add_token(TokenKind::Error, b, pos, depth, text(L("Incomplete \\g or \\k reference")));
            return npos;
        }
        if (close == L'>' || close == L'\'')
            note("g-angle-reference", Severity::Info, b, pos,
                 text(L("%s is a backreference here. In PCRE and Oniguruma the same syntax calls the group as a subroutine; use (?1) or (?&name) for that."),
                      {slice(b, pos)}));
        return reference_from_text(b, pos, body, depth, false);
    }

    std::size_t parse_k_reference(std::size_t b, int depth)
    {
        std::size_t j = pos + 2;
        if (j < n && (p[j] == L'<' || p[j] == L'\'' || p[j] == L'{')) {
            const wchar_t close = p[j] == L'<' ? L'>' : (p[j] == L'{' ? L'}' : L'\'');
            const std::size_t end = p.find(close, j + 1);
            if (end != std::wstring::npos && end > j + 1) {
                const std::wstring body = p.substr(j + 1, end - j - 1);
                pos = end + 1;
                return reference_from_text(b, pos, body, depth, false);
            }
        }
        pos = std::min(n, j + 1);
        add_token(TokenKind::Error, b, pos, depth, text(L("Incomplete \\g or \\k reference")));
        return npos;
    }

    std::size_t reference_from_text(std::size_t b, std::size_t e, const std::wstring &body, int depth, bool call)
    {
        bool sign = false, negative = false;
        std::size_t i = 0;
        if (!body.empty() && (body[0] == L'-' || body[0] == L'+')) {
            sign = true;
            negative = body[0] == L'-';
            i = 1;
        }
        bool numeric = i < body.size();
        std::size_t value = 0;
        for (std::size_t k = i; k < body.size(); ++k) {
            if (!detail::is_digit(body[k])) {
                numeric = false;
                break;
            }
            value = std::min<std::size_t>(value * 10 + static_cast<std::size_t>(body[k] - L'0'), 100000);
        }
        if (!numeric)
            return backreference(b, e, 0, body, depth, call);
        std::size_t resolved = value;
        if (sign && negative)
            resolved = value <= captures ? captures - value + 1 : 0;
        else if (sign)
            resolved = captures + value;
        return backreference(b, e, resolved, L"", depth, call);
    }

    std::size_t parse_group(Flags &flags, int depth)
    {
        const std::size_t b = pos;
        // The engine already rejects more than 32 levels as too complex. The
        // cap keeps the recursive descent's stack small on the UI thread for
        // a hostile 512-parenthesis pattern.
        if (depth >= kMaxAnalysisDepth) {
            pos = n;
            add_token(TokenKind::Error, b, n, depth,
                      text(L("Groups nested too deeply to analyse further; the engine rejects this nesting as too complex")));
            a.complete = false;
            return npos;
        }
        if (pos + 1 < n && p[pos + 1] == L'*')
            return parse_verb(depth);
        if (pos + 1 >= n || p[pos + 1] != L'?') {
            pos += 1;
            const std::size_t number_group = ++captures;
            if (closed.size() <= number_group)
                closed.resize(number_group + 1, false);
            closed[number_group] = false;
            return group_body(b, GroupKind::Capture, text(L("Capture group %s"), {number(number_group)}), flags, depth,
                              number_group);
        }
        if (pos + 2 >= n) {
            pos = n;
            add_token(TokenKind::Error, b, n, depth, text(L("Unclosed group")));
            a.complete = false;
            return npos;
        }
        const wchar_t k = p[pos + 2];
        switch (k) {
        case L'#': {
            const std::size_t close = p.find(L')', pos + 3);
            if (close == std::wstring::npos) {
                pos = n;
                note("open-comment", Severity::Warning, b, n,
                     text(L("This (?# comment has no closing parenthesis, so the rest of the pattern is ignored.")));
            } else {
                pos = close + 1;
            }
            add_token(TokenKind::Comment, b, pos, depth, text(L("Comment, ignored by the engine")));
            return npos;
        }
        case L':':
            pos += 3;
            return group_body(b, GroupKind::NonCapture, text(L("Non-capturing group — groups without remembering")),
                              flags, depth, 0);
        case L'=':
            pos += 3;
            return group_body(b, GroupKind::LookAhead, text(L("Lookahead: must be followed by this")), flags, depth, 0);
        case L'!':
            pos += 3;
            return group_body(b, GroupKind::NegativeLookAhead, text(L("Negative lookahead: must not be followed by this")),
                              flags, depth, 0);
        case L'>':
            pos += 3;
            portability(L"(?>...)", b, pos);
            return group_body(b, GroupKind::Atomic, text(L("Atomic group: never backtracks into its content")), flags,
                              depth, 0);
        case L'|':
            pos += 3;
            portability(L"(?|...)", b, pos);
            return group_body(b, GroupKind::BranchReset,
                              text(L("Branch reset group: alternatives share group numbers")), flags, depth, 0);
        case L'<':
            if (pos + 3 < n && (p[pos + 3] == L'=' || p[pos + 3] == L'!')) {
                const bool negative = p[pos + 3] == L'!';
                pos += 4;
                return group_body(b, negative ? GroupKind::NegativeLookBehind : GroupKind::LookBehind,
                                  negative ? text(L("Negative lookbehind: must not be preceded by this"))
                                           : text(L("Lookbehind: must be preceded by this")),
                                  flags, depth, 0);
            }
            return named_group(b, L'>', 3, flags, depth);
        case L'\'':
            return named_group(b, L'\'', 3, flags, depth);
        case L'P': {
            std::size_t end = pos + 3;
            if (end < n && p[end] == L'<') {
                const std::size_t close = p.find(L'>', end);
                end = close == std::wstring::npos ? n : close + 1;
                note("python-named-group", Severity::Warning, b, end,
                     text(L("Python-style %s is rejected here. Use (?<name>...), \\k<name> and (?&name)."), {slice(b, end)}));
                add_token(TokenKind::Error, b, end, depth, text(L("Not recognised by this engine: %s"), {slice(b, end)}));
                pos = end;
                const std::size_t body = group_body(b, GroupKind::Unsupported, text(L("Not recognised by this engine: %s"), {slice(b, end)}),
                                                    flags, depth, 0, false);
                return body;
            }
            const std::size_t close = p.find(L')', end);
            pos = close == std::wstring::npos ? n : close + 1;
            note("python-named-group", Severity::Warning, b, pos,
                 text(L("Python-style %s is rejected here. Use (?<name>...), \\k<name> and (?&name)."), {slice(b, pos)}));
            add_token(TokenKind::Error, b, pos, depth, text(L("Not recognised by this engine: %s"), {slice(b, pos)}));
            return npos;
        }
        case L'(':
            return parse_conditional(flags, depth);
        case L'&': {
            const std::size_t close = p.find(L')', pos + 3);
            if (close == std::wstring::npos) {
                pos = n;
                add_token(TokenKind::Error, b, n, depth, text(L("Unclosed group")));
                a.complete = false;
                return npos;
            }
            const std::wstring name = p.substr(pos + 3, close - pos - 3);
            pos = close + 1;
            return backreference(b, pos, 0, name, depth, true);
        }
        default:
            break;
        }
        if (k == L'R' || detail::is_digit(k) || ((k == L'+' || k == L'-') && pos + 3 < n && detail::is_digit(p[pos + 3]))) {
            const std::size_t close = p.find(L')', pos + 2);
            if (close == std::wstring::npos) {
                pos = n;
                add_token(TokenKind::Error, b, n, depth, text(L("Unclosed group")));
                a.complete = false;
                return npos;
            }
            std::wstring body = p.substr(pos + 2, close - pos - 2);
            pos = close + 1;
            if (body == L"R" || body == L"0")
                return backreference(b, pos, 0, L"", depth, true);
            const std::size_t id = reference_from_text(b, pos, body, depth, true);
            return id;
        }
        return parse_modifiers(flags, depth);
    }

    std::size_t named_group(std::size_t b, wchar_t close_char, std::size_t name_offset, Flags &flags, int depth)
    {
        const std::size_t start = pos + name_offset;
        const std::size_t close = p.find(close_char, start);
        if (close == std::wstring::npos) {
            pos = n;
            add_token(TokenKind::Error, b, n, depth, text(L("Unclosed group")));
            a.complete = false;
            return npos;
        }
        const std::wstring name = p.substr(start, close - start);
        pos = close + 1;
        const std::size_t number_group = ++captures;
        if (closed.size() <= number_group)
            closed.resize(number_group + 1, false);
        closed[number_group] = false;
        names.emplace_back(name, number_group);
        ++a.named_groups;
        return group_body(b, GroupKind::NamedCapture,
                          text(L("Capture group %s named \"%s\""), {number(number_group), name}), flags, depth,
                          number_group);
    }

    std::size_t group_body(std::size_t b, GroupKind kind, Text label, Flags &flags, int depth, std::size_t capture,
                           bool open_token = true)
    {
        if (open_token)
            add_token(TokenKind::GroupOpen, b, pos, depth, label);
        Flags inner = flags;
        const std::size_t content = parse_alternation(inner, depth + 1, kind == GroupKind::BranchReset);
        if (pos < n && p[pos] == L')') {
            add_token(TokenKind::GroupClose, pos, pos + 1, depth, text(L("End of group")));
            ++pos;
        } else {
            add_token(TokenKind::Error, n, n, depth, text(L("Unclosed group opened at offset %s"), {number(b)}));
            a.complete = false;
        }
        if (capture != 0 && capture < closed.size())
            closed[capture] = true;
        const std::size_t id = new_node(NodeKind::Group, b, pos);
        Node &node = a.nodes[id];
        node.group = kind;
        node.label = std::move(label);
        node.capture = capture;
        adopt(id, content);
        return id;
    }

    std::size_t parse_verb(int depth)
    {
        const std::size_t b = pos;
        const std::size_t close = p.find(L')', pos + 2);
        pos = close == std::wstring::npos ? n : close + 1;
        const std::wstring name = p.substr(b + 2, (close == std::wstring::npos ? n : close) - b - 2);
        static const wchar_t *known[] = {L"ACCEPT", L"COMMIT", L"F", L"FAIL", L"PRUNE", L"SKIP", L"THEN"};
        for (const wchar_t *verb : known)
            if (name == verb) {
                portability(L"(*VERB)", b, pos);
                return zero_width(b, pos, TokenKind::Verb, depth, text(L("Backtracking control verb %s"), {name}));
            }
        note("unsupported-verb", Severity::Warning, b, pos,
             text(L("%s is rejected: only (*ACCEPT), (*COMMIT), (*FAIL), (*PRUNE), (*SKIP) and (*THEN) work, without arguments."),
                  {slice(b, pos)}),
             false);
        add_token(TokenKind::Error, b, pos, depth, text(L("Not recognised by this engine: %s"), {slice(b, pos)}));
        if (close == std::wstring::npos)
            a.complete = false;
        return npos;
    }

    std::size_t parse_conditional(Flags &flags, int depth)
    {
        const std::size_t b = pos;
        pos += 2; // at the condition's '('
        Text label;
        portability(L"(?(...)...)", b, pos);
        if (pos + 1 < n && p[pos + 1] == L'?' && pos + 2 < n &&
            (p[pos + 2] == L'=' || p[pos + 2] == L'!' || p[pos + 2] == L'<')) {
            add_token(TokenKind::Condition, b, pos, depth, text(L("Conditional on a lookaround assertion")));
            Flags inner = flags;
            const std::size_t condition = parse_group(inner, depth + 1);
            label = text(L("Conditional on a lookaround assertion"));
            const std::size_t content = parse_alternation(inner, depth + 1, false);
            return close_conditional(b, label, condition, content, depth);
        }
        const std::size_t close = p.find(L')', pos + 1);
        if (close == std::wstring::npos) {
            pos = n;
            add_token(TokenKind::Error, b, n, depth, text(L("Unclosed group")));
            a.complete = false;
            return npos;
        }
        const std::wstring condition = p.substr(pos + 1, close - pos - 1);
        pos = close + 1;
        bool digits = !condition.empty();
        for (wchar_t c : condition)
            digits = digits && detail::is_digit(c);
        if (digits)
            label = text(L("Conditional on group %s"), {condition});
        else if (condition.size() >= 2 && ((condition.front() == L'<' && condition.back() == L'>') ||
                                           (condition.front() == L'\'' && condition.back() == L'\'')))
            label = text(L("Conditional on group \"%s\""), {condition.substr(1, condition.size() - 2)});
        else if (condition == L"DEFINE")
            label = text(L("DEFINE block: definitions for subroutine calls, never matched directly"));
        else if (!condition.empty() && condition[0] == L'R')
            label = text(L("Conditional on recursion"));
        else {
            add_token(TokenKind::Error, b, pos, depth, text(L("Not recognised by this engine: %s"), {slice(b, pos)}));
            label = text(L("Not recognised by this engine: %s"), {slice(b, pos)});
        }
        add_token(TokenKind::Condition, b, pos, depth, label);
        Flags inner = flags;
        const std::size_t content = parse_alternation(inner, depth + 1, false);
        return close_conditional(b, label, npos, content, depth);
    }

    std::size_t close_conditional(std::size_t b, Text label, std::size_t condition, std::size_t content, int depth)
    {
        if (pos < n && p[pos] == L')') {
            add_token(TokenKind::GroupClose, pos, pos + 1, depth, text(L("End of group")));
            ++pos;
        } else {
            add_token(TokenKind::Error, n, n, depth, text(L("Unclosed group opened at offset %s"), {number(b)}));
            a.complete = false;
        }
        const std::size_t id = new_node(NodeKind::Group, b, pos);
        a.nodes[id].group = GroupKind::Conditional;
        a.nodes[id].label = std::move(label);
        if (condition != npos)
            adopt(id, condition);
        adopt(id, content);
        return id;
    }

    // (?imsx-imsx) or (?imsx-imsx:...)
    std::size_t parse_modifiers(Flags &flags, int depth)
    {
        const std::size_t b = pos;
        std::size_t j = pos + 2;
        bool on = true, valid = true;
        Flags changed = flags;
        std::wstring set_on, set_off;
        while (j < n && p[j] != L')' && p[j] != L':') {
            const wchar_t c = p[j];
            if (c == L'-') {
                on = false;
            } else if (c == L'i' || c == L'm' || c == L's' || c == L'x') {
                (on ? set_on : set_off) += c;
                if (c == L'i') changed.icase = on;
                if (c == L'm') changed.multiline = on;
                if (c == L's') changed.dotall = on;
                if (c == L'x') changed.extended = on;
            } else {
                valid = false;
            }
            ++j;
        }
        if (j >= n) {
            pos = n;
            add_token(TokenKind::Error, b, n, depth, text(L("Unclosed group")));
            a.complete = false;
            return npos;
        }
        const bool scoped = p[j] == L':';
        pos = j + 1;
        std::wstring summary;
        if (!set_on.empty())
            summary = L"+" + set_on;
        if (!set_off.empty())
            summary += (summary.empty() ? L"-" : L" -") + set_off;
        if (!valid) {
            note("unsupported-modifier", Severity::Warning, b, pos,
                 text(L("%s is rejected: only the modifiers i, m, s and x exist in this engine."), {slice(b, pos)}),
                 false);
            add_token(TokenKind::Error, b, pos, depth, text(L("Not recognised by this engine: %s"), {slice(b, pos)}));
            if (!scoped)
                return npos;
            return group_body(b, GroupKind::Unsupported, text(L("Not recognised by this engine: %s"), {slice(b, pos)}),
                              flags, depth, 0, false);
        }
        portability(L"(?i) (?m) (?s) (?x)", b, pos);
        if (scoped) {
            Flags inner = changed;
            const Text label = text(L("Group with modifiers %s"), {summary});
            add_token(TokenKind::GroupOpen, b, pos, depth, label);
            const std::size_t content = parse_alternation(inner, depth + 1, false);
            if (pos < n && p[pos] == L')') {
                add_token(TokenKind::GroupClose, pos, pos + 1, depth, text(L("End of group")));
                ++pos;
            } else {
                add_token(TokenKind::Error, n, n, depth, text(L("Unclosed group opened at offset %s"), {number(b)}));
                a.complete = false;
            }
            const std::size_t id = new_node(NodeKind::Group, b, pos);
            a.nodes[id].group = GroupKind::Modifiers;
            a.nodes[id].label = label;
            adopt(id, content);
            return id;
        }
        flags = changed;
        add_token(TokenKind::Modifier, b, pos, depth, text(L("Modifiers for the rest of the group: %s"), {summary}));
        return npos;
    }

    // --- character sets ---------------------------------------------------------

    struct SetItem
    {
        bool          ok = true;
        bool          single = false;
        std::uint32_t value = 0;
        CharSet       chars;
        std::size_t   begin = 0, end = 0;
        Text          label;
        TokenKind     kind = TokenKind::SetMember;
    };

    SetItem set_item(const Flags &flags)
    {
        SetItem item;
        item.begin = pos;
        const wchar_t c = p[pos];
        if (c == L'[' && pos + 1 < n && (p[pos + 1] == L':' || p[pos + 1] == L'.' || p[pos + 1] == L'=')) {
            const wchar_t kind = p[pos + 1];
            const wchar_t closer[3] = {kind, L']', 0};
            const std::size_t close = p.find(closer, pos + 2);
            if (close != std::wstring::npos) {
                std::wstring name = p.substr(pos + 2, close - pos - 2);
                pos = close + 2;
                item.end = pos;
                item.kind = TokenKind::PosixClass;
                if (kind == L':') {
                    bool negated = !name.empty() && name[0] == L'^';
                    if (negated)
                        name.erase(0, 1);
                    if (!detail::known_class_name(name)) {
                        item.ok = false;
                        item.label = text(L("Unknown class name %s"), {name});
                        return item;
                    }
                    item.chars = detail::class_chars(name, flags.icase);
                    if (negated)
                        item.chars = item.chars.complement();
                    item.label = negated ? text(L("Not in POSIX class %s"), {name}) : text(L("POSIX class %s"), {name});
                    return item;
                }
                item.chars = CharSet::all();
                item.label = kind == L'.' ? text(L("Collating element %s"), {name})
                                          : text(L("Equivalence class %s"), {name});
                if (name.size() == 1) {
                    item.single = true;
                    item.value = static_cast<std::uint32_t>(name[0]);
                    item.chars = char_of(name[0], flags.icase);
                }
                return item;
            }
        }
        if (c == L'\\' && pos + 1 < n) {
            const wchar_t e = p[pos + 1];
            switch (e) {
            case L'd': case L'D': case L'w': case L'W': case L's': case L'S':
            case L'h': case L'H': case L'l': case L'L': case L'u': case L'U':
                pos += 2;
                item.end = pos;
                item.kind = TokenKind::Shorthand;
                item.chars = detail::shorthand_chars(e, flags.icase);
                item.label = shorthand_text(e);
                shorthand_notes(e, item.begin, flags);
                return item;
            case L'b': pos += 2; return set_single(item, 0x08, text(L("Backspace U+%s"), {hex_code(0x08)}), flags);
            case L'v': pos += 2; return set_single(item, 0x0B, text(L("Vertical tab U+%s"), {hex_code(0x0B)}), flags);
            case L'a': pos += 2; return set_single(item, 0x07, text(L("Bell character U+%s"), {hex_code(0x07)}), flags);
            case L'e': pos += 2; return set_single(item, 0x1B, text(L("Escape character U+%s"), {hex_code(0x1B)}), flags);
            case L'f': pos += 2; return set_single(item, 0x0C, text(L("Form feed U+%s"), {hex_code(0x0C)}), flags);
            case L'n': pos += 2; return set_single(item, 0x0A, text(L("Newline (line feed) U+%s"), {hex_code(0x0A)}), flags);
            case L'r': pos += 2; return set_single(item, 0x0D, text(L("Carriage return U+%s"), {hex_code(0x0D)}), flags);
            case L't': pos += 2; return set_single(item, 0x09, text(L("Tab character U+%s"), {hex_code(0x09)}), flags);
            case L'c':
                if (pos + 2 < n) {
                    const std::uint32_t v = static_cast<std::uint32_t>(p[pos + 2]) % 32;
                    pos += 3;
                    return set_single(item, v, text(L("Control character U+%s"), {hex_code(v)}), flags);
                }
                pos = n;
                item.ok = false;
                item.end = n;
                item.label = text(L("Incomplete control escape"));
                return item;
            case L'x': {
                std::size_t value = 0;
                if (!hex_escape(value)) {
                    pos = std::max(pos, item.begin + 2);
                    item.ok = false;
                    item.end = pos;
                    item.label = text(L("Incomplete hexadecimal escape"));
                    return item;
                }
                if (value > 0xFFFF && o.code_unit_bits == 16)
                    note("hex-above-ffff", Severity::Warning, item.begin, pos,
                         text(L("%s is above FFFF, which 16-bit code units cannot hold, so the Windows engine rejects it. Type the character itself."),
                              {slice(item.begin, pos)}));
                const std::uint32_t v = static_cast<std::uint32_t>(std::min<std::size_t>(value, 0x10FFFF));
                return set_single(item, v, text(L("Code unit U+%s by hexadecimal value"), {hex_code(v)}), flags);
            }
            case L'0': {
                std::size_t j = pos + 2;
                std::uint32_t v = 0;
                while (j < n && j < pos + 4 && p[j] >= L'0' && p[j] <= L'7') {
                    v = v * 8 + static_cast<std::uint32_t>(p[j] - L'0');
                    ++j;
                }
                pos = j;
                return set_single(item, v, text(L("Octal escape for code unit U+%s"), {hex_code(v)}), flags);
            }
            case L'p': case L'P':
                pos += 2;
                note("p-inside-brackets", Severity::Warning, item.begin, pos,
                     text(L("Inside [ ], %s is just the letter %s. Use a POSIX class such as [[:alpha:]] instead."),
                          {slice(item.begin, pos), std::wstring(1, e)}));
                return set_single(item, static_cast<std::uint32_t>(e), text(L("Escaped %s: matches the character itself"), {std::wstring(1, e)}), flags);
            case L'N':
                pos += 2;
                item.ok = false;
                item.end = pos;
                item.label = text(L("Not recognised by this engine: %s"), {slice(item.begin, pos)});
                return item;
            default:
                break;
            }
            if (e >= L'1' && e <= L'9') {
                pos += 2;
                item.ok = false;
                item.end = pos;
                item.label = text(L("Backreferences are not allowed inside [ ]; %s is rejected"), {slice(item.begin, pos)});
                return item;
            }
            std::size_t end = pos + 2;
            if (detail::is_high_surrogate(e) && end < n && detail::is_low_surrogate(p[end]))
                ++end;
            const std::wstring value = p.substr(pos + 1, end - pos - 1);
            pos = end;
            return set_single(item, static_cast<std::uint32_t>(e), text(L("Escaped %s: matches the character itself"), {value}), flags);
        }
        std::size_t end = pos + 1;
        if (detail::is_high_surrogate(c) && end < n && detail::is_low_surrogate(p[end])) {
            ++end;
            supplementary(pos, end);
        }
        const std::wstring value = p.substr(pos, end - pos);
        pos = end;
        if (c == L'&' && pos < n && p[pos] == L'&')
            note("set-operation", Severity::Warning, item.begin, pos + 1,
                 text(L("[...&&...] is not an intersection here: && matches literal ampersands. This engine has no set operations.")));
        return set_single(item, static_cast<std::uint32_t>(c), text(L("Set member %s"), {value}), flags);
    }

    SetItem set_single(SetItem &item, std::uint32_t value, Text label, const Flags &flags)
    {
        item.end = pos;
        item.single = true;
        item.value = value;
        item.chars = value < 0x110000 ? char_of(static_cast<wchar_t>(value), flags.icase) : CharSet::all();
        item.label = std::move(label);
        return item;
    }

    std::size_t parse_set(Flags &flags, int depth)
    {
        const std::size_t b = pos;
        ++pos;
        bool negated = false;
        if (pos < n && p[pos] == L'^') {
            negated = true;
            ++pos;
        }
        const Text label = negated ? text(L("Negated set — matches any character not listed inside"))
                                   : text(L("Character set — matches any one character listed inside"));
        const std::size_t open = add_token(TokenKind::SetOpen, b, pos, depth, label);
        CharSet chars;
        bool first = true;
        bool closed_set = false;
        while (pos < n) {
            if (p[pos] == L']' && !first) {
                add_token(TokenKind::SetClose, pos, pos + 1, depth, text(L("End of character set")));
                ++pos;
                closed_set = true;
                break;
            }
            if (!first && p[pos] == L'-' && pos + 1 < n && p[pos + 1] == L'-')
                note("set-operation", Severity::Warning, pos, pos + 2,
                     text(L("[...--...] is not a subtraction here. This engine has no set operations and rejects -- after a range.")));
            first = false;
            SetItem item = set_item(flags);
            if (item.ok && item.single && pos + 1 < n && p[pos] == L'-' && p[pos + 1] != L']') {
                const std::size_t dash = pos;
                ++pos;
                SetItem last = set_item(flags);
                if (last.ok && last.single && last.value >= item.value) {
                    CharSet range;
                    range.add_range(item.value, last.value);
                    if (flags.icase) {
                        for (std::uint32_t v = item.value; v <= last.value && v < 128; ++v)
                            if (detail::is_ascii_letter(static_cast<wchar_t>(v)))
                                range.add(static_cast<wchar_t>(v ^ 0x20));
                    }
                    chars.merge(range);
                    add_token(TokenKind::SetRange, item.begin, last.end, depth,
                              text(L("Range %s to %s"), {slice(item.begin, dash), slice(dash + 1, last.end)}));
                } else {
                    add_token(TokenKind::Error, item.begin, last.end, depth,
                              text(L("Invalid range %s: the engine rejects it"), {slice(item.begin, last.end)}));
                }
                continue;
            }
            if (!item.ok) {
                add_token(TokenKind::Error, item.begin, std::max(item.end, item.begin + 1), depth, item.label);
                if (pos <= item.begin)
                    pos = item.begin + 1;
                continue;
            }
            chars.merge(item.chars);
            add_token(item.kind, item.begin, item.end, depth, item.label);
        }
        if (!closed_set) {
            add_token(TokenKind::Error, n, n, depth, text(L("Unterminated character set opened at offset %s"), {number(b)}));
            a.complete = false;
        }
        (void) open;
        if (negated)
            chars = chars.complement();
        const std::size_t id = new_node(NodeKind::Set, b, pos);
        Node &node = a.nodes[id];
        node.label = label;
        node.chars = chars;
        node.first = chars;
        node.last = chars;
        node.single = true;
        node.min_len = node.max_len = 1;
        return id;
    }

    // --- post-processing ----------------------------------------------------------

    static bool is_repeating(const Node &node)
    {
        return node.kind == NodeKind::Quantified && (node.unbounded || node.max > 1);
    }

    static std::size_t add_len(std::size_t x, std::size_t y)
    {
        const std::size_t cap = std::numeric_limits<std::size_t>::max() / 4;
        return std::min(cap, x + y);
    }
    static std::size_t mul_len(std::size_t x, std::size_t y)
    {
        const std::size_t cap = std::numeric_limits<std::size_t>::max() / 4;
        if (x == 0 || y == 0)
            return 0;
        if (x > cap / y)
            return cap;
        return x * y;
    }

    void compute(std::size_t id)
    {
        Node &node = a.nodes[id];
        switch (node.kind) {
        case NodeKind::Atom:
        case NodeKind::Set:
        case NodeKind::Empty:
            return;
        case NodeKind::Sequence: {
            node.min_len = node.max_len = 0;
            node.max_infinite = false;
            node.zero_width = true;
            node.first = CharSet();
            node.last = CharSet();
            node.chars = CharSet();
            bool leading = true;
            for (std::size_t child : node.children) {
                const Node &c = a.nodes[child];
                node.min_len = add_len(node.min_len, c.min_len);
                node.max_len = add_len(node.max_len, c.max_len);
                node.max_infinite = node.max_infinite || c.max_infinite;
                node.zero_width = node.zero_width && c.zero_width;
                node.chars.merge(c.chars);
                node.backref = node.backref || c.backref;
                node.recursion = node.recursion || c.recursion;
                if (leading)
                    node.first.merge(c.first);
                if (c.min_len > 0)
                    leading = false;
            }
            for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
                node.last.merge(a.nodes[*it].last);
                if (a.nodes[*it].min_len > 0)
                    break;
            }
            return;
        }
        case NodeKind::Alternation: {
            bool init = false;
            for (std::size_t child : node.children) {
                const Node &c = a.nodes[child];
                node.min_len = init ? std::min(node.min_len, c.min_len) : c.min_len;
                node.max_len = init ? std::max(node.max_len, c.max_len) : c.max_len;
                init = true;
                node.max_infinite = node.max_infinite || c.max_infinite;
                node.first.merge(c.first);
                node.last.merge(c.last);
                node.chars.merge(c.chars);
                node.backref = node.backref || c.backref;
                node.recursion = node.recursion || c.recursion;
            }
            return;
        }
        case NodeKind::Group: {
            if (node.children.empty())
                return;
            const Node &c = a.nodes[node.children.back()];
            node.chars = c.chars;
            node.backref = c.backref;
            node.recursion = c.recursion;
            if (node.children.size() > 1) {
                node.chars.merge(a.nodes[node.children.front()].chars);
                node.backref = node.backref || a.nodes[node.children.front()].backref;
            }
            switch (node.group) {
            case GroupKind::LookAhead:
            case GroupKind::NegativeLookAhead:
            case GroupKind::LookBehind:
            case GroupKind::NegativeLookBehind:
                node.zero_width = true;
                node.min_len = node.max_len = 0;
                return;
            case GroupKind::Conditional: {
                if (c.kind == NodeKind::Alternation) {
                    node.min_len = c.min_len;
                    node.max_len = c.max_len;
                } else {
                    node.min_len = 0; // the missing "no" branch matches empty
                    node.max_len = c.max_len;
                }
                node.max_infinite = c.max_infinite;
                node.first = c.first;
                node.last = c.last;
                return;
            }
            default:
                node.min_len = c.min_len;
                node.max_len = c.max_len;
                node.max_infinite = c.max_infinite;
                node.zero_width = c.zero_width;
                node.first = c.first;
                node.last = c.last;
                node.single = c.single && c.min_len == 1 && c.max_len == 1;
                return;
            }
        }
        case NodeKind::Quantified: {
            const Node &c = a.nodes[node.children.front()];
            node.min_len = mul_len(c.min_len, node.min);
            node.max_infinite = c.max_infinite || (node.unbounded && c.max_len > 0);
            node.max_len = node.unbounded ? c.max_len : mul_len(c.max_len, node.max);
            node.first = c.first;
            node.last = c.last;
            node.chars = c.chars;
            node.zero_width = c.zero_width;
            node.backref = c.backref;
            node.recursion = c.recursion;
            return;
        }
        }
    }

    bool inside_atomic_between(std::size_t inner, std::size_t outer) const
    {
        for (std::size_t at = a.nodes[inner].parent; at != npos && at != outer; at = a.nodes[at].parent)
            if (a.nodes[at].kind == NodeKind::Group && a.nodes[at].group == GroupKind::Atomic)
                return true;
        return false;
    }

    // An outer repeat that is the whole tail of an atomic group cannot be
    // re-entered after the group commits.
    bool mitigated(std::size_t q) const
    {
        const Node &node = a.nodes[q];
        if (node.mode == QuantMode::Possessive)
            return true;
        std::size_t child = q;
        for (std::size_t at = node.parent; at != npos; child = at, at = a.nodes[at].parent) {
            const Node &up = a.nodes[at];
            if (up.kind == NodeKind::Group && up.group == GroupKind::Atomic)
                return true;
            if (up.kind == NodeKind::Sequence && up.children.back() != child)
                return false;
            if (up.kind != NodeKind::Sequence && !(up.kind == NodeKind::Group && up.group != GroupKind::Atomic &&
                                                   up.group != GroupKind::LookAhead && up.group != GroupKind::LookBehind &&
                                                   up.group != GroupKind::NegativeLookAhead &&
                                                   up.group != GroupKind::NegativeLookBehind))
                return false;
        }
        return false;
    }

    // Characters that can follow `inner` inside the body of `outer` before the
    // next iteration of `outer`; `wraps` is set when everything after `inner`
    // up to the end of the body can match empty.
    CharSet follow_within(std::size_t inner, std::size_t outer, bool &wraps) const
    {
        CharSet follow;
        wraps = true;
        std::size_t child = inner;
        for (std::size_t at = a.nodes[inner].parent; at != npos && at != outer; child = at, at = a.nodes[at].parent) {
            const Node &up = a.nodes[at];
            if (up.kind != NodeKind::Sequence)
                continue;
            bool after = false;
            for (std::size_t sibling : up.children) {
                if (sibling == child) {
                    after = true;
                    continue;
                }
                if (!after)
                    continue;
                follow.merge(a.nodes[sibling].first);
                if (a.nodes[sibling].min_len > 0) {
                    wraps = false;
                    return follow;
                }
            }
        }
        return follow;
    }

    void collect_quantified(std::size_t id, std::size_t stop, std::vector<std::size_t> &out) const
    {
        for (std::size_t child : a.nodes[id].children) {
            const Node &c = a.nodes[child];
            if (c.kind == NodeKind::Group && (c.group == GroupKind::Atomic))
                continue;
            if (c.kind == NodeKind::Quantified && child != stop)
                out.push_back(child);
            collect_quantified(child, stop, out);
        }
    }

    std::wstring adversarial(const CharSet &repeat, const CharSet &avoid) const
    {
        const wchar_t prefer[] = L"a0A_ x.-";
        wchar_t pick = 0;
        for (const wchar_t *c = prefer; *c; ++c)
            if (repeat.ascii.test(static_cast<std::size_t>(*c))) {
                pick = *c;
                break;
            }
        if (pick == 0)
            for (std::size_t c = 0x21; c < 0x7F; ++c)
                if (repeat.ascii.test(c)) {
                    pick = static_cast<wchar_t>(c);
                    break;
                }
        if (pick == 0)
            return {};
        std::wstring out(28, pick);
        for (const wchar_t *c = L"!#%~;"; *c; ++c)
            if (!avoid.ascii.test(static_cast<std::size_t>(*c))) {
                out += *c;
                break;
            }
        return out;
    }

    void risk(const char *id, Severity severity, std::size_t node, Text message, std::wstring example = {})
    {
        Finding f;
        f.id = id;
        f.severity = severity;
        f.begin = a.nodes[node].begin;
        f.end = a.nodes[node].end;
        f.message = std::move(message);
        f.example = std::move(example);
        a.risks.push_back(std::move(f));
    }

    void analyse_risks()
    {
        CharSet everything;
        if (a.root != npos)
            everything = a.nodes[a.root].chars;
        for (std::size_t id = 0; id < a.nodes.size(); ++id) {
            const Node &q = a.nodes[id];
            if (!is_reachable(id))
                continue;
            if (is_repeating(q) && !mitigated(id)) {
                const std::size_t body = q.children.front();
                // Nested variable repeats that can split the same text between
                // iterations: exponential backtracking on a failing input.
                std::vector<std::size_t> inner;
                if (a.nodes[body].kind == NodeKind::Quantified)
                    inner.push_back(body);
                collect_quantified(body, npos, inner);
                bool flagged = false;
                for (std::size_t in : inner) {
                    const Node &i = a.nodes[in];
                    if (i.mode == QuantMode::Possessive || inside_atomic_between(in, id))
                        continue;
                    if (!(i.unbounded || i.max > i.min) || i.chars.empty())
                        continue;
                    bool wraps = false;
                    const CharSet follow = follow_within(in, id, wraps);
                    const bool ambiguous = follow.intersects(i.chars) ||
                                           (wraps && a.nodes[body].first.intersects(i.chars));
                    if (!ambiguous)
                        continue;
                    const Severity severity = (q.unbounded || q.max >= 16) ? Severity::Danger : Severity::Warning;
                    risk("nested-quantifier", severity, id,
                         text(L("Nested repetition %s can split the same text in many ways, so a failing match may backtrack exponentially. Make the inner or outer repeat possessive (++) or atomic (?>...), or remove the overlap."),
                              {fragment(p, q.begin, q.end)}),
                         adversarial(i.chars, everything));
                    flagged = true;
                    break;
                }
                // Overlapping alternatives inside a repeat.
                std::size_t alt = body;
                if (a.nodes[alt].kind == NodeKind::Group && !a.nodes[alt].children.empty())
                    alt = a.nodes[alt].children.back();
                if (!flagged && a.nodes[alt].kind == NodeKind::Alternation) {
                    const auto &branches = a.nodes[alt].children;
                    for (std::size_t x = 0; x < branches.size() && !flagged; ++x)
                        for (std::size_t y = x + 1; y < branches.size() && !flagged; ++y) {
                            const Node &bx = a.nodes[branches[x]];
                            const Node &by = a.nodes[branches[y]];
                            if (!bx.first.intersects(by.first))
                                continue;
                            const bool same_text = bx.single && by.single;
                            const CharSet both = bx.first.intersection(by.first);
                            risk("overlapping-alternatives", same_text && (q.unbounded || q.max >= 16) ? Severity::Danger : Severity::Warning, id,
                                 text(L("Alternatives %s and %s inside the repeat %s can start with the same characters, so the engine may retry many combinations. Make the alternatives start differently or use an atomic group."),
                                      {fragment(p, bx.begin, bx.end), fragment(p, by.begin, by.end), fragment(p, q.begin, q.end)}),
                                 same_text ? adversarial(both, everything) : std::wstring());
                            flagged = true;
                        }
                }
                if (q.backref)
                    risk("repeated-backreference", Severity::Warning, id,
                         text(L("The repeat %s contains a backreference, which the engine cannot optimise; long inputs can be slow."),
                              {fragment(p, q.begin, q.end)}));
                if (q.recursion)
                    risk("repeated-recursion", Severity::Warning, id,
                         text(L("The repeat %s contains recursion or a subroutine call; each iteration can re-run large parts of the pattern."),
                              {fragment(p, q.begin, q.end)}));
            }
            if (q.kind == NodeKind::Quantified && !q.unbounded && q.max > 1000)
                risk("large-count", Severity::Warning, id,
                     text(L("The count in %s is large; the engine's state limit or the time limit may stop it on long input."),
                          {fragment(p, q.begin, q.end)}));
            if (q.kind == NodeKind::Sequence)
                adjacent_repeats(id);
        }
        // Leading unanchored wildcard.
        if (a.root != npos) {
            std::size_t first = a.root;
            while (a.nodes[first].kind == NodeKind::Sequence && !a.nodes[first].children.empty())
                first = a.nodes[first].children.front();
            const Node &f = a.nodes[first];
            if (f.kind == NodeKind::Quantified && f.unbounded) {
                const Node &body = a.nodes[f.children.front()];
                if (body.any_char)
                    risk("leading-wildcard", Severity::Info, first,
                         text(L("The pattern starts with %s, so every start position rescans the rest of the text. Anchor it with ^ or drop the leading wildcard; a search already looks anywhere."),
                              {fragment(p, f.begin, f.end)}));
            }
        }
        std::sort(a.risks.begin(), a.risks.end(),
                  [](const Finding &x, const Finding &y) { return x.begin < y.begin; });
        a.risk = RiskLevel::Low;
        for (const Finding &f : a.risks) {
            if (f.severity == Severity::Danger)
                a.risk = RiskLevel::High;
            else if (f.severity == Severity::Warning && a.risk == RiskLevel::Low)
                a.risk = RiskLevel::Moderate;
        }
    }

    void adjacent_repeats(std::size_t seq)
    {
        const auto &items = a.nodes[seq].children;
        for (std::size_t x = 0; x < items.size(); ++x) {
            const Node &left = a.nodes[items[x]];
            if (left.kind != NodeKind::Quantified || !left.unbounded || left.mode == QuantMode::Possessive)
                continue;
            for (std::size_t y = x + 1; y < items.size(); ++y) {
                const Node &right = a.nodes[items[y]];
                if (right.kind == NodeKind::Quantified && right.unbounded && right.mode != QuantMode::Possessive &&
                    left.last.intersects(right.first)) {
                    Finding f;
                    f.id = "adjacent-repeats";
                    f.severity = Severity::Warning;
                    f.begin = left.begin;
                    f.end = right.end;
                    f.message = text(L("%s and %s can match the same characters next to each other, so a failing match tries every split point (polynomial backtracking)."),
                                     {fragment(p, left.begin, left.end), fragment(p, right.begin, right.end)});
                    a.risks.push_back(std::move(f));
                    break;
                }
                if (right.min_len > 0)
                    break;
            }
        }
    }

    bool is_reachable(std::size_t id) const
    {
        std::size_t at = id;
        while (a.nodes[at].parent != npos)
            at = a.nodes[at].parent;
        return at == a.root;
    }

    void lookbehind_checks()
    {
        for (std::size_t id = 0; id < a.nodes.size(); ++id) {
            const Node &g = a.nodes[id];
            if (g.kind != NodeKind::Group ||
                (g.group != GroupKind::LookBehind && g.group != GroupKind::NegativeLookBehind) || g.children.empty() ||
                !is_reachable(id))
                continue;
            const Node &body = a.nodes[g.children.front()];
            bool fixed = !body.max_infinite && body.min_len == body.max_len;
            bool repeated_group = false;
            std::vector<std::size_t> stack{g.children.front()};
            while (!stack.empty()) {
                const std::size_t at = stack.back();
                stack.pop_back();
                const Node &node = a.nodes[at];
                if (node.variable_atom)
                    fixed = false;
                if (node.kind == NodeKind::Quantified && !a.nodes[node.children.front()].single)
                    repeated_group = true;
                if (node.kind == NodeKind::Alternation) {
                    // Alternatives must share one constant width.
                    for (std::size_t child : node.children)
                        if (a.nodes[child].min_len != node.min_len || a.nodes[child].max_len != node.min_len)
                            fixed = false;
                }
                for (std::size_t child : node.children)
                    stack.push_back(child);
            }
            if (!fixed || repeated_group)
                note("variable-lookbehind", Severity::Warning, g.begin, g.end,
                     text(L("The lookbehind %s is not fixed width (or repeats a group), which this engine rejects. JavaScript and .NET accept it; rewrite it with one constant length or use \\K."),
                          {fragment(p, g.begin, g.end)}),
                     false);
        }
    }

    void reference_checks()
    {
        for (const Ref &ref : refs) {
            const bool early = ref.early;
            if (ref.call) {
                bool exists = ref.name.empty() ? ref.number <= captures : false;
                for (const auto &entry : names)
                    if (entry.first == ref.name)
                        exists = true;
                if (!exists)
                    note("missing-group", Severity::Warning, ref.begin, ref.end,
                         text(L("%s refers to a group that does not exist, so the engine rejects the pattern."), {slice(ref.begin, ref.end)}),
                         false);
                continue;
            }
            bool exists = false;
            if (ref.name.empty())
                exists = ref.number >= 1 && ref.number <= captures;
            else
                for (const auto &entry : names)
                    if (entry.first == ref.name)
                        exists = true;
            if (!exists)
                note("missing-group", Severity::Warning, ref.begin, ref.end,
                     text(L("%s refers to a group that does not exist, so the engine rejects the pattern."), {slice(ref.begin, ref.end)}),
                     false);
            else if (early)
                note("early-backreference", Severity::Warning, ref.begin, ref.end,
                     text(L("%s refers to a group that has not finished matching at this point, so it can never match here."),
                          {slice(ref.begin, ref.end)}),
                     false);
        }
    }

    void finish()
    {
        a.capture_groups = captures;
        for (std::size_t id = 0; id < a.nodes.size(); ++id)
            compute(id);
        lookbehind_checks();
        reference_checks();
        a.can_match_empty = a.root != npos && a.nodes[a.root].min_len == 0;
        if (!p.empty() && a.can_match_empty)
            note("matches-empty", Severity::Warning, 0, p.size(),
                 text(L("This pattern can succeed without consuming any text, so rows can match with nothing highlighted; a pattern such as a* matches every row.")));
        if (!o.case_sensitive) {
            for (std::size_t i = 0; i < p.size(); ++i)
                if (static_cast<std::uint32_t>(p[i]) >= 0x80 && !detail::is_low_surrogate(p[i])) {
                    note("case-folding", Severity::Info, i, i + 1,
                         text(L("Case-insensitive matching of letters outside ASCII follows the engine's locale traits shown under Engine.")));
                    break;
                }
        }
        if (o.whole_word && o.regex_mode)
            note("whole-word-ignored", Severity::Info, 0, p.size(),
                 text(L("Whole word is ignored in regex mode. Put \\b around the word instead.")));
        analyse_risks();
        std::stable_sort(a.compatibility.begin(), a.compatibility.end(),
                         [](const Finding &x, const Finding &y) { return x.begin < y.begin; });
    }
};

inline Analysis analyze(const std::wstring &pattern, const Options &options = {})
{
    Analysis out;
    Parser parser(pattern, options, out);
    parser.run();
    return out;
}

// Pre-order walk of the structure tree for display.
struct TreeLine
{
    std::size_t node;
    int         depth;
};

inline std::vector<TreeLine> tree_lines(const Analysis &a)
{
    std::vector<TreeLine> out;
    if (a.root == npos)
        return out;
    std::vector<TreeLine> stack{TreeLine{a.root, 0}};
    while (!stack.empty()) {
        const TreeLine line = stack.back();
        stack.pop_back();
        out.push_back(line);
        const auto &children = a.nodes[line.node].children;
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            stack.push_back(TreeLine{*it, line.depth + 1});
    }
    return out;
}

} // namespace Slic3r::GUI::RegexAnalysis

#pragma pop_macro("L")

#endif // slic3r_GUI_Widgets_RegexAnalysis_hpp_
