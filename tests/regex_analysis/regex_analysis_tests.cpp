#include <catch_main.hpp>

#include "slic3r/GUI/Widgets/BoundedRegex.hpp"
#include "slic3r/GUI/Widgets/RegexAnalysis.hpp"

#include <set>
#include <string>
#include <vector>

// Pure-model tests for the regex builder's structure, capability and risk
// analysis. The worker-backed half (every capability probe against the real
// Boost.Regex engine, the engine descriptor and error offsets) lives in
// tests/bounded_regex/bounded_regex_tests.cpp under [regex_workbench].

using namespace Slic3r::GUI::RegexAnalysis;

namespace {

struct Expected
{
    TokenKind    kind;
    std::wstring text;        // pattern slice the token covers
    std::wstring explanation; // English explanation
};

std::string narrow(const std::wstring &w)
{
    std::string out;
    for (wchar_t c : w)
        out += (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '?';
    return out;
}

void require_tokens(const Analysis &a, const std::vector<Expected> &expected)
{
    INFO("pattern: " << narrow(a.pattern));
    REQUIRE(a.tokens.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const Token &tok = a.tokens[i];
        INFO("token " << i << ": " << narrow(english(tok.explanation)));
        CHECK(tok.kind == expected[i].kind);
        CHECK(a.pattern.substr(tok.begin, tok.end - tok.begin) == expected[i].text);
        CHECK(english(tok.explanation) == expected[i].explanation);
    }
}

std::vector<std::wstring> tree_text(const Analysis &a)
{
    std::vector<std::wstring> out;
    for (const TreeLine &line : tree_lines(a))
        out.push_back(std::wstring(static_cast<std::size_t>(line.depth) * 2, L' ') +
                      english(a.nodes[line.node].label));
    return out;
}

const Finding *find(const std::vector<Finding> &list, const char *id)
{
    for (const Finding &f : list)
        if (std::string(f.id) == id)
            return &f;
    return nullptr;
}

Options with_bits(unsigned bits)
{
    Options options;
    options.code_unit_bits = bits;
    return options;
}

bool has_error_token(const Analysis &a)
{
    for (const Token &tok : a.tokens)
        if (tok.kind == TokenKind::Error)
            return true;
    return false;
}

} // namespace

TEST_CASE("token annotation names every token of a dated pattern", "[regex_analysis]")
{
    const Analysis a = analyze(L"^(?<year>\\d{4})-(\\d\\d)?$");
    require_tokens(a, {
        {TokenKind::Anchor, L"^", L"Start of the text"},
        {TokenKind::GroupOpen, L"(?<year>", L"Capture group 1 named \"year\""},
        {TokenKind::Shorthand, L"\\d", L"Digit 0-9"},
        {TokenKind::Quantifier, L"{4}", L"Repeat exactly 4 times"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::Literal, L"-", L"Literal text: -"},
        {TokenKind::GroupOpen, L"(", L"Capture group 2"},
        {TokenKind::Shorthand, L"\\d", L"Digit 0-9"},
        {TokenKind::Shorthand, L"\\d", L"Digit 0-9"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::Quantifier, L"?", L"Repeat 0 to 1 times (greedy: as many as possible)"},
        {TokenKind::Anchor, L"$", L"End of the text"},
    });
    CHECK(a.capture_groups == 2);
    CHECK(a.named_groups == 1);
    CHECK(a.complete);
    CHECK(a.risk == RiskLevel::Low);
    CHECK(a.has_compatibility("dollar-end"));
}

TEST_CASE("structure tree nests groups, alternatives and literals", "[regex_analysis]")
{
    const Analysis a = analyze(L"(red|blue)\\b");
    CHECK(tree_text(a) == std::vector<std::wstring>{
        L"Sequence of 2 items",
        L"  Capture group 1",
        L"    One of 2 alternatives",
        L"      Literal text: red",
        L"      Literal text: blue",
        L"  Word boundary",
    });
    const Node &root = a.nodes[a.root];
    CHECK(root.min_len == 3);
    CHECK(root.max_len == 4);
    CHECK_FALSE(root.max_infinite);
    CHECK_FALSE(a.can_match_empty);
}

TEST_CASE("literals merge until a quantifier claims the last character", "[regex_analysis]")
{
    require_tokens(analyze(L"abc+"), {
        {TokenKind::Literal, L"ab", L"Literal text: ab"},
        {TokenKind::Literal, L"c", L"Literal text: c"},
        {TokenKind::Quantifier, L"+", L"Repeat 1 or more times (greedy: as many as possible)"},
    });
    require_tokens(analyze(L"a\\.b"), {{TokenKind::Literal, L"a\\.b", L"Literal text: a.b"}});

    const Analysis quoted = analyze(L"\\Qa.b\\E*");
    require_tokens(quoted, {
        {TokenKind::Quoted, L"\\Qa.b\\E", L"Quoted literal text: a.b"},
        {TokenKind::Quantifier, L"*", L"Repeat 0 or more times (greedy: as many as possible)"},
    });
    CHECK(tree_text(quoted) == std::vector<std::wstring>{
        L"Sequence of 2 items",
        L"  Quoted literal text: a.",
        L"  Repeat 0 or more times (greedy: as many as possible)",
        L"    Literal text: b",
    });
}

TEST_CASE("flags and inline modifiers change the explanation", "[regex_analysis]")
{
    Options multiline;
    multiline.multiline = true;
    require_tokens(analyze(L"^a$", multiline), {
        {TokenKind::Anchor, L"^", L"Start of a line"},
        {TokenKind::Literal, L"a", L"Literal text: a"},
        {TokenKind::Anchor, L"$", L"End of a line"},
    });
    CHECK_FALSE(analyze(L"^a$", multiline).has_compatibility("dollar-end"));

    require_tokens(analyze(L"(?s)."), {
        {TokenKind::Modifier, L"(?s)", L"Modifiers for the rest of the group: +s"},
        {TokenKind::AnyChar, L".", L"Any character, including line breaks"},
    });
    require_tokens(analyze(L"(?-s:.)"), {
        {TokenKind::GroupOpen, L"(?-s:", L"Group with modifiers -s"},
        {TokenKind::AnyChar, L".", L"Any single character except newline"},
        {TokenKind::GroupClose, L")", L"End of group"},
    });
    require_tokens(analyze(L"(?x) a b # note"), {
        {TokenKind::Modifier, L"(?x)", L"Modifiers for the rest of the group: +x"},
        {TokenKind::Ignored, L" ", L"Ignored whitespace (free-spacing mode)"},
        {TokenKind::Literal, L"a", L"Literal text: a"},
        {TokenKind::Ignored, L" ", L"Ignored whitespace (free-spacing mode)"},
        {TokenKind::Literal, L"b", L"Literal text: b"},
        {TokenKind::Ignored, L" ", L"Ignored whitespace (free-spacing mode)"},
        {TokenKind::Comment, L"# note", L"Comment, ignored by the engine"},
    });
}

TEST_CASE("character sets list members, ranges and classes", "[regex_analysis]")
{
    require_tokens(analyze(L"[^a-z\\d_]"), {
        {TokenKind::SetOpen, L"[^", L"Negated set — matches any character not listed inside"},
        {TokenKind::SetRange, L"a-z", L"Range a to z"},
        {TokenKind::Shorthand, L"\\d", L"Digit 0-9"},
        {TokenKind::SetMember, L"_", L"Set member _"},
        {TokenKind::SetClose, L"]", L"End of character set"},
    });
    require_tokens(analyze(L"[]a[:alpha:][:^digit:]]"), {
        {TokenKind::SetOpen, L"[", L"Character set — matches any one character listed inside"},
        {TokenKind::SetMember, L"]", L"Set member ]"},
        {TokenKind::SetMember, L"a", L"Set member a"},
        {TokenKind::PosixClass, L"[:alpha:]", L"POSIX class alpha"},
        {TokenKind::PosixClass, L"[:^digit:]", L"Not in POSIX class digit"},
        {TokenKind::SetClose, L"]", L"End of character set"},
    });
    const Analysis open = analyze(L"[ab");
    CHECK_FALSE(open.complete);
    CHECK(english(open.tokens.back().explanation) == L"Unterminated character set opened at offset 0");
}

TEST_CASE("groups, references, conditionals and verbs are named", "[regex_analysis]")
{
    require_tokens(analyze(L"(?:a)(?>b)(?=c)(?!d)(?<=e)(?<!f)"), {
        {TokenKind::GroupOpen, L"(?:", L"Non-capturing group — groups without remembering"},
        {TokenKind::Literal, L"a", L"Literal text: a"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::GroupOpen, L"(?>", L"Atomic group: never backtracks into its content"},
        {TokenKind::Literal, L"b", L"Literal text: b"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::GroupOpen, L"(?=", L"Lookahead: must be followed by this"},
        {TokenKind::Literal, L"c", L"Literal text: c"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::GroupOpen, L"(?!", L"Negative lookahead: must not be followed by this"},
        {TokenKind::Literal, L"d", L"Literal text: d"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::GroupOpen, L"(?<=", L"Lookbehind: must be preceded by this"},
        {TokenKind::Literal, L"e", L"Literal text: e"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::GroupOpen, L"(?<!", L"Negative lookbehind: must not be preceded by this"},
        {TokenKind::Literal, L"f", L"Literal text: f"},
        {TokenKind::GroupClose, L")", L"End of group"},
    });
    require_tokens(analyze(L"(a)\\g{-1}\\k<x>(?<x>b)(?1)(?R)"), {
        {TokenKind::GroupOpen, L"(", L"Capture group 1"},
        {TokenKind::Literal, L"a", L"Literal text: a"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::Backreference, L"\\g{-1}", L"Backreference to group 1"},
        {TokenKind::Backreference, L"\\k<x>", L"Backreference to group \"x\""},
        {TokenKind::GroupOpen, L"(?<x>", L"Capture group 2 named \"x\""},
        {TokenKind::Literal, L"b", L"Literal text: b"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::Subroutine, L"(?1)", L"Call group 1 as a subroutine"},
        {TokenKind::Subroutine, L"(?R)", L"Recurse into the whole pattern"},
    });
    require_tokens(analyze(L"(a)?(?(1)b|c)(*FAIL)"), {
        {TokenKind::GroupOpen, L"(", L"Capture group 1"},
        {TokenKind::Literal, L"a", L"Literal text: a"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::Quantifier, L"?", L"Repeat 0 to 1 times (greedy: as many as possible)"},
        {TokenKind::Condition, L"(?(1)", L"Conditional on group 1"},
        {TokenKind::Literal, L"b", L"Literal text: b"},
        {TokenKind::Alternation, L"|", L"Alternation — matches either side"},
        {TokenKind::Literal, L"c", L"Literal text: c"},
        {TokenKind::GroupClose, L")", L"End of group"},
        {TokenKind::Verb, L"(*FAIL)", L"Backtracking control verb FAIL"},
    });
    const Analysis reset = analyze(L"(?|(a)|(b)(c))");
    CHECK(reset.capture_groups == 2);
}

TEST_CASE("quantifier forms and escapes are explained", "[regex_analysis]")
{
    require_tokens(analyze(L"a*?b++c{2,}d{1,3}+e{,3}"), {
        {TokenKind::Literal, L"a", L"Literal text: a"},
        {TokenKind::Quantifier, L"*?", L"Repeat 0 or more times (lazy: as few as possible)"},
        {TokenKind::Literal, L"b", L"Literal text: b"},
        {TokenKind::Quantifier, L"++", L"Repeat 1 or more times (possessive: never gives back)"},
        {TokenKind::Literal, L"c", L"Literal text: c"},
        {TokenKind::Quantifier, L"{2,}", L"Repeat 2 or more times (greedy: as many as possible)"},
        {TokenKind::Literal, L"d", L"Literal text: d"},
        {TokenKind::Quantifier, L"{1,3}+", L"Repeat 1 to 3 times (possessive: never gives back)"},
        {TokenKind::Literal, L"e{,3}", L"Literal text: e{,3}"},
    });
    require_tokens(analyze(L"\\x41\\x{263A}\\t\\cA\\012"), {
        {TokenKind::Control, L"\\x41", L"Code unit U+0041 by hexadecimal value"},
        {TokenKind::Control, L"\\x{263A}", L"Code unit U+263A by hexadecimal value"},
        {TokenKind::Control, L"\\t", L"Tab character U+0009"},
        {TokenKind::Control, L"\\cA", L"Control character U+0001"},
        {TokenKind::Control, L"\\012", L"Octal escape for code unit U+000A"},
    });
    require_tokens(analyze(L"\\A\\G\\K\\R\\X\\C\\z"), {
        {TokenKind::Anchor, L"\\A", L"Start of the text, even with multiline anchors"},
        {TokenKind::Anchor, L"\\G", L"Where the previous match ended"},
        {TokenKind::Keep, L"\\K", L"Reported match starts here; earlier text must still match"},
        {TokenKind::Special, L"\\R", L"Any line break sequence, CRLF counted as one"},
        {TokenKind::Special, L"\\X", L"One grapheme cluster: a character with its combining marks"},
        {TokenKind::Special, L"\\C", L"Exactly one code unit"},
        {TokenKind::Anchor, L"\\z", L"End of the text, even with multiline anchors"},
    });
    const Analysis nothing = analyze(L"*a");
    REQUIRE_FALSE(nothing.tokens.empty());
    CHECK(nothing.tokens.front().kind == TokenKind::Error);
    CHECK(english(nothing.tokens.front().explanation) == L"Quantifier with nothing to repeat");
    CHECK(has_error_token(analyze(L"^*")));
    CHECK(has_error_token(analyze(L"a)")));
    CHECK_FALSE(analyze(L"(a").complete);
}

TEST_CASE("compatibility warnings flag the engine's dialect traps", "[regex_analysis]")
{
    CHECK(analyze(L"\\u0041").has_compatibility("u-escape"));
    CHECK(analyze(L"\\u").has_compatibility("u-class"));
    CHECK(analyze(L"\\l").has_compatibility("l-class"));
    CHECK(analyze(L"\\h\\v").has_compatibility("hv-class"));
    CHECK(analyze(L"\\p{L}").has_compatibility("p-letter-class"));
    CHECK(analyze(L"\\p{Lu}").has_compatibility("p-unknown-class"));
    CHECK_FALSE(analyze(L"\\p{alpha}").has_compatibility("p-unknown-class"));
    CHECK(analyze(L"[\\pL]").has_compatibility("p-inside-brackets"));
    CHECK(analyze(L"(a)\\10").has_compatibility("single-digit-backreference"));
    CHECK(analyze(L"(a)\\g<1>").has_compatibility("g-angle-reference"));
    CHECK(analyze(L"\\'").has_compatibility("buffer-anchors"));
    CHECK(analyze(L"\\<a\\>").has_compatibility("word-anchors"));
    CHECK(analyze(L"\\o{12}").has_compatibility("octal-brace"));
    CHECK(analyze(L"\\y").has_compatibility("unknown-escape"));
    CHECK_FALSE(analyze(L"\\.").has_compatibility("unknown-escape"));
    CHECK(analyze(L"a\\E").has_compatibility("stray-quote-end"));
    CHECK(analyze(L"a(?#open").has_compatibility("open-comment"));
    CHECK(analyze(L"(?P<n>a)").has_compatibility("python-named-group"));
    CHECK(analyze(L"(?U)a").has_compatibility("unsupported-modifier"));
    CHECK(analyze(L"(*MARK:x)a").has_compatibility("unsupported-verb"));
    CHECK(analyze(L"[a-z&&[aeiou]]").has_compatibility("set-operation"));
    CHECK(analyze(L"[a-z--[aeiou]]").has_compatibility("set-operation"));
    CHECK(analyze(L"\\1(a)").has_compatibility("early-backreference"));
    CHECK(analyze(L"(a\\1)").has_compatibility("early-backreference"));
    CHECK_FALSE(analyze(L"(a)\\1").has_compatibility("early-backreference"));
    CHECK(analyze(L"(a)\\2").has_compatibility("missing-group"));
    CHECK(analyze(L"\\k<nope>").has_compatibility("missing-group"));
    CHECK(analyze(L"a{,3}").has_compatibility("literal-brace"));
    CHECK(analyze(L"a*").has_compatibility("matches-empty"));
    CHECK_FALSE(analyze(L"a+").has_compatibility("matches-empty"));
    CHECK(analyze(L"(?>a)").has_compatibility("javascript-portability"));

    CHECK(analyze(L"(?<=a+)b").has_compatibility("variable-lookbehind"));
    CHECK(analyze(L"(?<=a|bc)d").has_compatibility("variable-lookbehind"));
    CHECK(analyze(L"(?<=(?:ab){2})c").has_compatibility("variable-lookbehind"));
    CHECK(analyze(L"(?<=\\R)x").has_compatibility("variable-lookbehind"));
    CHECK_FALSE(analyze(L"(?<=a{2})b").has_compatibility("variable-lookbehind"));
    CHECK_FALSE(analyze(L"(?<=ab|cd)x").has_compatibility("variable-lookbehind"));

    Options whole;
    whole.whole_word = true;
    CHECK(analyze(L"nozzle", whole).has_compatibility("whole-word-ignored"));
    Options insensitive;
    CHECK(analyze(L"été", insensitive).has_compatibility("case-folding"));
    insensitive.case_sensitive = true;
    CHECK_FALSE(analyze(L"été", insensitive).has_compatibility("case-folding"));

    // A supplementary-plane character is two code units only for 16-bit
    // engines (the Windows build); the warning follows the reported width.
    std::wstring emoji = L"x";
    if (sizeof(wchar_t) == 2) {
        emoji += static_cast<wchar_t>(0xD83D);
        emoji += static_cast<wchar_t>(0xDE00);
    } else {
        emoji += static_cast<wchar_t>(0x1F600);
    }
    const Analysis sixteen = analyze(emoji + L"+", with_bits(16));
    if (sizeof(wchar_t) == 2) {
        const Finding *f = find(sixteen.compatibility, "supplementary-character");
        REQUIRE(f);
        CHECK(f->severity == Severity::Warning);
    }
    CHECK(analyze(L"\\x{1F600}", with_bits(16)).has_compatibility("hex-above-ffff"));
    CHECK_FALSE(analyze(L"\\x{1F600}", with_bits(32)).has_compatibility("hex-above-ffff"));
    const Analysis wide_analysis = analyze(L"\\x{1F600}", with_bits(32));
    const Finding *wide = find(wide_analysis.compatibility, "supplementary-character");
    REQUIRE(wide);
    CHECK(wide->severity == Severity::Info);

    const Analysis u_analysis = analyze(L"\\u0041");
    const Finding *u = find(u_analysis.compatibility, "u-escape");
    REQUIRE(u);
    CHECK(english(u->message) ==
          L"\\u0041 is not a code point here: \\u is the uppercase-letter class, followed by the literal text 0041. Write \\x{0041}.");
    const Analysis ten_analysis = analyze(L"(a)\\10");
    const Finding *ten = find(ten_analysis.compatibility, "single-digit-backreference");
    REQUIRE(ten);
    CHECK(english(ten->message) ==
          L"A backslash takes only one digit here: \\10 is group 1 followed by the digit 0. Write \\g{10} for a higher group.");
}

TEST_CASE("backtracking risk finds nested and overlapping repeats", "[regex_analysis]")
{
    const Analysis nested = analyze(L"(a+)+$");
    CHECK(nested.risk == RiskLevel::High);
    const Finding *f = find(nested.risks, "nested-quantifier");
    REQUIRE(f);
    CHECK(f->severity == Severity::Danger);
    CHECK(f->begin == 0);
    CHECK(f->end == 5);
    CHECK(f->example == std::wstring(28, L'a') + L"!");

    CHECK(analyze(L"(\\w+\\s?)+$").risk == RiskLevel::High);
    CHECK(analyze(L"(x+x+)+y").risk == RiskLevel::High);
    CHECK(analyze(L"(a*)*b").risk == RiskLevel::High);

    // Separated iterations, possessive or atomic repeats are not ambiguous.
    CHECK(analyze(L"(a+b)+").risk == RiskLevel::Low);
    CHECK(analyze(L"(\\d+,)*\\d+").risk == RiskLevel::Low);
    CHECK(analyze(L"(\\w+\\s)+").risk == RiskLevel::Low);
    CHECK(analyze(L"(a++)+").risks.empty());
    CHECK(analyze(L"(a+)++").risks.empty());
    CHECK(analyze(L"(?>(a+)+)").risks.empty());
    CHECK(analyze(L"((?>a+))+").risks.empty());

    const Analysis same = analyze(L"(a|a)*");
    const Finding *overlap = find(same.risks, "overlapping-alternatives");
    REQUIRE(overlap);
    CHECK(overlap->severity == Severity::Danger);
    const Analysis prefix_analysis = analyze(L"(a|ab)*c");
    const Finding *prefix = find(prefix_analysis.risks, "overlapping-alternatives");
    REQUIRE(prefix);
    CHECK(prefix->severity == Severity::Warning);
    CHECK(analyze(L"(red|blue)+").risks.empty());

    const Analysis adjacent = analyze(L"\\d+\\d+");
    CHECK(adjacent.risk == RiskLevel::Moderate);
    CHECK(adjacent.has_risk("adjacent-repeats"));
    CHECK(analyze(L"\\w*\\s*\\w*").has_risk("adjacent-repeats"));
    CHECK_FALSE(analyze(L"\\d+-\\d+").has_risk("adjacent-repeats"));

    CHECK(analyze(L".*foo").has_risk("leading-wildcard"));
    CHECK_FALSE(analyze(L"^.*foo").has_risk("leading-wildcard"));
    CHECK(analyze(L"((a)\\2)+").has_risk("repeated-backreference"));
    CHECK(analyze(L"(a(?1)?b)+").has_risk("repeated-recursion"));
    CHECK(analyze(L"(ab){2000}").has_risk("large-count"));
    CHECK(analyze(L"^\\w+@\\w+\\.com$").risk == RiskLevel::Low);
}

TEST_CASE("capability matrix keeps unsupported constructs visible", "[regex_analysis]")
{
    const auto &rows = capabilities();
    REQUIRE(rows.size() >= 35);
    std::set<std::string> ids;
    for (const Capability &row : rows) {
        INFO(row.id);
        CHECK(ids.insert(row.id).second);
        CHECK(std::string(row.feature).size() > 3);
        CHECK(std::string(row.explanation).size() > 10);
        CHECK(std::wstring(row.syntax).size() > 0);
        CHECK(std::wstring(row.probe).size() > 0);
        if (row.expect == ProbeExpect::Match)
            CHECK(std::wstring(row.expected_match).size() > 0);
        if (row.expect == ProbeExpect::Rejected)
            CHECK(row.support == Support::Unsupported);
        // The structure model must read every supported probe without
        // inventing a syntax error, and must explain every rejected one.
        const Analysis a = analyze(row.probe);
        if (row.support != Support::Unsupported) {
            CHECK_FALSE(has_error_token(a));
            CHECK(a.complete);
        }
        if (row.expect == ProbeExpect::Rejected)
            CHECK((has_error_token(a) || !a.compatibility.empty()));
    }
    for (const char *id : {"unicode-escape", "octal-brace", "named-characters", "unicode-categories",
                           "set-operations", "python-named-groups", "variable-lookbehind", "other-modifiers",
                           "marked-verbs"}) {
        bool listed = false;
        for (const Capability &row : rows)
            if (std::string(row.id) == id) {
                listed = true;
                CHECK(row.support == Support::Unsupported);
            }
        INFO(id);
        CHECK(listed);
    }
    for (const char *id : {"literals", "quoted-literals", "hex-escapes", "character-sets", "class-names", "anchors",
                           "word-boundaries", "numbered-groups", "named-groups", "non-capturing-groups",
                           "atomic-groups", "alternation", "greedy-quantifiers", "lazy-quantifiers",
                           "possessive-quantifiers", "lookahead", "lookbehind", "backreferences", "conditionals",
                           "subroutines", "inline-modifiers", "supplementary-characters"}) {
        bool listed = false;
        for (const Capability &row : rows)
            listed = listed || std::string(row.id) == id;
        INFO(id);
        CHECK(listed);
    }
    ProbeObservation seen;
    seen.answered = true;
    seen.matched = true;
    seen.whole = L"a.b";
    CHECK(confirms(rows.front(), seen));
    seen.whole = L"axb";
    CHECK_FALSE(confirms(rows.front(), seen));
    seen.answered = false;
    CHECK_FALSE(confirms(rows.front(), seen));
}

TEST_CASE("flag and escaping tables document every builder flag", "[regex_analysis]")
{
    const auto &flags = flag_rows();
    REQUIRE(flags.size() == 6);
    CHECK(std::string(flags[0].name) == "Regex mode");
    CHECK(std::string(flags[1].name) == "Case sensitive");
    CHECK(std::wstring(flags[1].engine) == L"icase (added when off)");
    CHECK(std::wstring(flags[2].engine) == L"no_mod_m (cleared when on)");
    CHECK(std::wstring(flags[3].engine) == L"no_mod_s (always set)");
    CHECK(std::string(flags[5].name) == "Whole word");
    REQUIRE(escape_rules().size() >= 6);
    for (const EscapeRule &rule : escape_rules())
        CHECK(std::string(rule.meaning).size() > 10);
}

TEST_CASE("engine descriptor round-trips the worker report", "[regex_analysis]")
{
    const EngineInfo info = parse_engine_descriptor(
        "engine=boost.regex;version=108400;syntax=perl;code_unit_bits=16;traits=win32;max_states=1000000");
    CHECK(info.reported);
    CHECK(info.version == 108400);
    CHECK(version_text(info.version) == L"1.84.0");
    CHECK(info.code_unit_bits == 16);
    CHECK(info.traits == "win32");
    CHECK(info.syntax == "perl");
    CHECK(info.max_states == 1000000);
    CHECK(std::string(traits_label(info.traits)) == "Windows locale (w32_regex_traits)");

    CHECK_FALSE(parse_engine_descriptor("").reported);
    CHECK_FALSE(parse_engine_descriptor("engine=other;version=1").reported);
    CHECK_FALSE(parse_engine_descriptor("engine=boost.regex;version=abc").reported);
    CHECK(parse_engine_descriptor("engine=boost.regex;version=108300;code_unit_bits=7").code_unit_bits ==
          sizeof(wchar_t) * 8);
}

TEST_CASE("text substitution keeps placeholders in order", "[regex_analysis]")
{
    CHECK(substitute(L"a %s b %s %%", {L"1", L"2"}) == L"a 1 b 2 %");
    CHECK(substitute(L"%s", {}) == L"");
    CHECK(english(text("Range %s to %s", {L"a", L"z"})) == L"Range a to z");
    CHECK(from_utf8("\xE2\x80\x94") == L"—");
    CHECK(fragment(L"a\nb", 0, 3) == L"a\\nb");
    CHECK(fragment(std::wstring(100, L'x'), 0, 100, 10) == std::wstring(10, L'x') + L"...");
}

TEST_CASE("deep nesting stops at the analysis depth instead of exhausting the stack", "[regex_analysis]")
{
    // Every nesting the worker can accept is analysed in full.
    STATIC_REQUIRE(static_cast<std::size_t>(kMaxAnalysisDepth) >= 2 * Slic3r::GUI::BoundedRegex::kMaxNestingDepth);
    const std::size_t cap = static_cast<std::size_t>(kMaxAnalysisDepth);
    const Analysis at_cap = analyze(std::wstring(cap, L'(') + L"a" + std::wstring(cap, L')'));
    CHECK(at_cap.complete);
    CHECK(at_cap.capture_groups == cap);
    CHECK_FALSE(has_error_token(at_cap));

    const Analysis beyond = analyze(std::wstring(cap + 1, L'(') + L"a" + std::wstring(cap + 1, L')'));
    CHECK_FALSE(beyond.complete);
    CHECK(has_error_token(beyond));

    // A 512-parenthesis pattern (the field's whole budget) stays bounded.
    const Analysis hostile = analyze(std::wstring(512, L'('));
    CHECK_FALSE(hostile.complete);
    for (const Token &tok : hostile.tokens)
        CHECK(tok.end <= 512);
}

TEST_CASE("analysis never runs past the pattern on hostile input", "[regex_analysis]")
{
    const std::vector<std::wstring> seeds{
        L"(?<=(?(?=a)b|c)(?|(x)|(y)))\\k<n>[[:alpha:]-z]\\Q(\\E{2,1}",
        L"((((((((((a))))))))))\\g{-11}(?&q)(?P=x)(*",
        L"[\\x{110000}-\\x{0}]\\c\\x\\p{\\N{}(?#",
        L"(?x)  # comment\n [ ] \\  a{ 2 , 3 }+?",
        L"a{1,2,3}{}{,}{a}\\",
        L"(?i-msx:(?im)(?-i)a)|(?)(?:)()",
        L"[]]][^]][\\]-a]",
    };
    for (const std::wstring &seed : seeds) {
        for (std::size_t len = 0; len <= seed.size(); ++len) {
            const std::wstring pattern = seed.substr(0, len);
            for (unsigned bits : {16u, 32u}) {
                const Analysis a = analyze(pattern, with_bits(bits));
                for (const Token &tok : a.tokens) {
                    CHECK(tok.begin <= tok.end);
                    CHECK(tok.end <= pattern.size());
                }
                for (const Node &node : a.nodes)
                    CHECK(node.end <= pattern.size());
                for (const Finding &f : a.compatibility)
                    CHECK(f.end <= pattern.size());
                for (const Finding &f : a.risks)
                    CHECK(f.end <= pattern.size());
                (void) tree_lines(a);
            }
        }
    }
}
