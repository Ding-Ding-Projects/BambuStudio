#include "Markdown.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <vector>

namespace Slic3r::Markdown {

namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

bool is_space(char c) { return c == ' ' || c == '\t'; }
bool is_blank(const std::string &line)
{
    return std::all_of(line.begin(), line.end(), [](char c) { return is_space(c) || c == '\r'; });
}
bool is_ascii_punct(char c)
{
    return std::ispunct(static_cast<unsigned char>(c)) != 0;
}
bool is_alnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

std::string trim(const std::string &s)
{
    size_t b = 0, e = s.size();
    while (b < e && is_space(s[b])) ++b;
    while (e > b && is_space(s[e - 1])) --e;
    return s.substr(b, e - b);
}

// Leading indentation width (tabs count as 4 columns).
size_t indent_of(const std::string &line)
{
    size_t width = 0;
    for (char c : line) {
        if (c == ' ') ++width;
        else if (c == '\t') width += 4 - (width % 4);
        else break;
    }
    return width;
}

// Remove up to `columns` columns of leading indentation.
std::string strip_indent(const std::string &line, size_t columns)
{
    size_t width = 0, i = 0;
    while (i < line.size() && width < columns) {
        if (line[i] == ' ') { ++width; ++i; }
        else if (line[i] == '\t') { width += 4 - (width % 4); ++i; }
        else break;
    }
    return line.substr(i);
}

std::vector<std::string> split_lines(const std::string &text)
{
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') { lines.push_back(current); current.clear(); }
        else if (c != '\r') current.push_back(c);
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

// ---------------------------------------------------------------------------
// Inline rendering
// ---------------------------------------------------------------------------

struct InlineContext
{
    const RenderOptions *options;
};

std::string render_inline(const std::string &text, const InlineContext &ctx);

// Position just after the code span that starts at `pos` (a backtick run),
// or std::string::npos when the run has no matching closer.
size_t code_span_end(const std::string &s, size_t pos)
{
    size_t run = 0;
    while (pos + run < s.size() && s[pos + run] == '`') ++run;
    size_t i = pos + run;
    while (i < s.size()) {
        if (s[i] == '`') {
            size_t r = 0;
            while (i + r < s.size() && s[i + r] == '`') ++r;
            if (r == run) return i + r;
            i += r;
        } else {
            ++i;
        }
    }
    return std::string::npos;
}

// Find the closing delimiter `delim` at or after `from`, skipping backslash
// escapes and code spans. The closer must be right-flanking (not preceded by
// whitespace) and, for '_', not followed by an alphanumeric.
size_t find_closer(const std::string &s, size_t from, const std::string &delim)
{
    size_t i = from;
    while (i < s.size()) {
        if (s[i] == '\\') { i += 2; continue; }
        if (s[i] == '`') {
            const size_t end = code_span_end(s, i);
            if (end == std::string::npos) { ++i; continue; }
            i = end;
            continue;
        }
        if (s.compare(i, delim.size(), delim) == 0) {
            const bool preceded_by_space = i == 0 || is_space(s[i - 1]) || s[i - 1] == '\n';
            // A longer run of the same character is not this delimiter.
            const bool longer_run = i + delim.size() < s.size() && s[i + delim.size()] == delim[0];
            const bool shorter_ok = !(delim.size() == 1 && i > 0 && s[i - 1] == delim[0]);
            bool word_ok = true;
            if (delim[0] == '_' && i + delim.size() < s.size() && is_alnum(s[i + delim.size()]))
                word_ok = false;
            if (!preceded_by_space && !longer_run && shorter_ok && word_ok && i > from)
                return i;
        }
        ++i;
    }
    return std::string::npos;
}

// Parse a link/image destination + optional title starting at s[pos] == '('.
// On success writes dest/title and returns the index after ')'.
size_t parse_destination(const std::string &s, size_t pos, std::string &dest, std::string &title)
{
    if (pos >= s.size() || s[pos] != '(') return std::string::npos;
    size_t i = pos + 1;
    while (i < s.size() && is_space(s[i])) ++i;
    dest.clear();
    title.clear();
    if (i < s.size() && s[i] == '<') {
        ++i;
        while (i < s.size() && s[i] != '>' && s[i] != '\n') dest.push_back(s[i++]);
        if (i >= s.size() || s[i] != '>') return std::string::npos;
        ++i;
    } else {
        int depth = 0;
        while (i < s.size()) {
            const char c = s[i];
            if (c == '\\' && i + 1 < s.size()) { dest.push_back(s[i + 1]); i += 2; continue; }
            if (c == '(') ++depth;
            if (c == ')') { if (depth == 0) break; --depth; }
            if (is_space(c) || c == '\n') break;
            dest.push_back(c);
            ++i;
        }
    }
    while (i < s.size() && (is_space(s[i]) || s[i] == '\n')) ++i;
    if (i < s.size() && (s[i] == '"' || s[i] == '\'')) {
        const char quote = s[i++];
        while (i < s.size() && s[i] != quote) {
            if (s[i] == '\\' && i + 1 < s.size()) { title.push_back(s[i + 1]); i += 2; continue; }
            title.push_back(s[i++]);
        }
        if (i >= s.size()) return std::string::npos;
        ++i;
        while (i < s.size() && is_space(s[i])) ++i;
    }
    if (i >= s.size() || s[i] != ')') return std::string::npos;
    return i + 1;
}

// Find the ']' matching the '[' at `open`, honouring nesting, escapes and
// code spans.
size_t matching_bracket(const std::string &s, size_t open)
{
    int depth = 0;
    for (size_t i = open; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\\') { ++i; continue; }
        if (c == '`') {
            const size_t end = code_span_end(s, i);
            if (end != std::string::npos) { i = end - 1; continue; }
        }
        if (c == '[') ++depth;
        else if (c == ']') { if (--depth == 0) return i; }
    }
    return std::string::npos;
}

bool looks_like_uri(const std::string &s)
{
    // scheme ":" rest, scheme = [A-Za-z][A-Za-z0-9+.-]{1,31}, no spaces or '<'.
    size_t i = 0;
    if (i >= s.size() || !std::isalpha(static_cast<unsigned char>(s[i]))) return false;
    while (i < s.size() && (is_alnum(s[i]) || s[i] == '+' || s[i] == '.' || s[i] == '-')) ++i;
    if (i < 2 || i > 32 || i >= s.size() || s[i] != ':') return false;
    return s.find_first_of(" \t\n<>") == std::string::npos;
}

bool looks_like_email(const std::string &s)
{
    const size_t at = s.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= s.size()) return false;
    if (s.find_first_of(" \t\n<>") != std::string::npos) return false;
    return s.find('.', at) != std::string::npos;
}

std::string apply_hook(const std::function<std::string(const std::string &)> &hook, const std::string &dest)
{
    if (!hook) return dest;
    const std::string rewritten = hook(dest);
    return rewritten.empty() ? dest : rewritten;
}

std::string render_inline(const std::string &text, const InlineContext &ctx)
{
    std::string out;
    out.reserve(text.size() + 32);
    const std::string &s = text;
    size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];

        // Backslash escapes.
        if (c == '\\') {
            if (i + 1 < s.size() && s[i + 1] == '\n') { out += "<br>\n"; i += 2; continue; }
            if (i + 1 < s.size() && is_ascii_punct(s[i + 1])) { out += escape_html(std::string(1, s[i + 1])); i += 2; continue; }
            out += "\\";
            ++i;
            continue;
        }

        // Code spans.
        if (c == '`') {
            const size_t end = code_span_end(s, i);
            if (end == std::string::npos) { out += "`"; ++i; continue; }
            size_t run = 0;
            while (s[i + run] == '`') ++run;
            std::string code = s.substr(i + run, end - run - (i + run));
            std::replace(code.begin(), code.end(), '\n', ' ');
            if (code.size() >= 2 && code.front() == ' ' && code.back() == ' ' &&
                !std::all_of(code.begin(), code.end(), [](char ch) { return ch == ' '; }))
                code = code.substr(1, code.size() - 2);
            out += "<code>" + escape_html(code) + "</code>";
            i = end;
            continue;
        }

        // Hard line break: two or more trailing spaces before a newline.
        if (c == '\n') {
            size_t spaces = 0;
            while (!out.empty() && out.back() == ' ') { out.pop_back(); ++spaces; }
            out += spaces >= 2 ? "<br>\n" : "\n";
            ++i;
            continue;
        }

        // Images.
        if (c == '!' && i + 1 < s.size() && s[i + 1] == '[') {
            const size_t close = matching_bracket(s, i + 1);
            std::string dest, title;
            size_t after = close == std::string::npos ? std::string::npos : parse_destination(s, close + 1, dest, title);
            if (after != std::string::npos) {
                const std::string alt = s.substr(i + 2, close - (i + 2));
                out += "<img src=\"" + escape_html(apply_hook(ctx.options->resolve_image, dest)) + "\" alt=\"" +
                       escape_html(plain_text(alt)) + "\"";
                if (!title.empty()) out += " title=\"" + escape_html(title) + "\"";
                out += ">";
                i = after;
                continue;
            }
            out += "!";
            ++i;
            continue;
        }

        // Links.
        if (c == '[') {
            const size_t close = matching_bracket(s, i);
            std::string dest, title;
            size_t after = close == std::string::npos ? std::string::npos : parse_destination(s, close + 1, dest, title);
            if (after != std::string::npos) {
                const std::string label = s.substr(i + 1, close - (i + 1));
                out += "<a href=\"" + escape_html(apply_hook(ctx.options->resolve_link, dest)) + "\"";
                if (!title.empty()) out += " title=\"" + escape_html(title) + "\"";
                out += ">" + render_inline(label, ctx) + "</a>";
                i = after;
                continue;
            }
            out += "[";
            ++i;
            continue;
        }

        // Autolinks.
        if (c == '<') {
            const size_t close = s.find('>', i + 1);
            if (close != std::string::npos) {
                const std::string body = s.substr(i + 1, close - i - 1);
                if (looks_like_uri(body)) {
                    out += "<a href=\"" + escape_html(apply_hook(ctx.options->resolve_link, body)) + "\">" + escape_html(body) + "</a>";
                    i = close + 1;
                    continue;
                }
                if (looks_like_email(body)) {
                    out += "<a href=\"mailto:" + escape_html(body) + "\">" + escape_html(body) + "</a>";
                    i = close + 1;
                    continue;
                }
            }
            out += "&lt;";
            ++i;
            continue;
        }

        // Emphasis / strong / strikethrough.
        if (c == '*' || c == '_' || c == '~') {
            size_t run = 0;
            while (i + run < s.size() && s[i + run] == c) ++run;
            const bool next_is_space   = i + run >= s.size() || is_space(s[i + run]) || s[i + run] == '\n';
            const bool prev_is_alnum   = i > 0 && is_alnum(s[i - 1]);
            const bool can_open        = !next_is_space && !(c == '_' && prev_is_alnum);
            if (can_open && run >= 1 && run <= 3 && !(c == '~' && run != 2)) {
                std::string open_tag, close_tag, delim;
                size_t consumed = run;
                if (c == '~') { open_tag = "<del>"; close_tag = "</del>"; delim = "~~"; }
                else if (run == 1) { open_tag = "<em>"; close_tag = "</em>"; delim = std::string(1, c); }
                else if (run == 2) { open_tag = "<strong>"; close_tag = "</strong>"; delim = std::string(2, c); }
                else { open_tag = "<em><strong>"; close_tag = "</strong></em>"; delim = std::string(3, c); }
                const size_t closer = find_closer(s, i + consumed, delim);
                if (closer != std::string::npos) {
                    out += open_tag + render_inline(s.substr(i + consumed, closer - (i + consumed)), ctx) + close_tag;
                    i = closer + delim.size();
                    continue;
                }
            }
            out += escape_html(std::string(run, c));
            i += run;
            continue;
        }

        out += escape_html(std::string(1, c));
        ++i;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Block rendering
// ---------------------------------------------------------------------------

struct BlockContext
{
    const RenderOptions *options;
    std::map<std::string, int> *slugs; // heading slug -> occurrences, for unique ids
};

void render_blocks(const std::vector<std::string> &lines, size_t begin, size_t end, std::string &out, BlockContext &ctx);

bool fence_start(const std::string &line, char &fence_char, size_t &fence_len, std::string &info)
{
    if (indent_of(line) > 3) return false;
    const std::string t = strip_indent(line, 3);
    if (t.empty() || (t[0] != '`' && t[0] != '~')) return false;
    fence_char = t[0];
    fence_len  = 0;
    while (fence_len < t.size() && t[fence_len] == fence_char) ++fence_len;
    if (fence_len < 3) return false;
    info = trim(t.substr(fence_len));
    if (fence_char == '`' && info.find('`') != std::string::npos) return false;
    return true;
}

bool fence_end(const std::string &line, char fence_char, size_t fence_len)
{
    if (indent_of(line) > 3) return false;
    const std::string t = trim(strip_indent(line, 3));
    if (t.size() < fence_len) return false;
    return std::all_of(t.begin(), t.end(), [fence_char](char c) { return c == fence_char; });
}

bool atx_heading(const std::string &line, int &level, std::string &text)
{
    if (indent_of(line) > 3) return false;
    const std::string t = strip_indent(line, 3);
    size_t hashes = 0;
    while (hashes < t.size() && t[hashes] == '#') ++hashes;
    if (hashes == 0 || hashes > 6) return false;
    if (hashes < t.size() && !is_space(t[hashes])) return false;
    level = static_cast<int>(hashes);
    std::string rest = trim(t.substr(hashes));
    // Optional closing hashes.
    size_t e = rest.size();
    while (e > 0 && rest[e - 1] == '#') --e;
    if (e < rest.size() && (e == 0 || is_space(rest[e - 1]))) rest = trim(rest.substr(0, e));
    text = rest;
    return true;
}

bool thematic_break(const std::string &line)
{
    if (indent_of(line) > 3) return false;
    const std::string t = strip_indent(line, 3);
    char marker = 0;
    size_t count = 0;
    for (char c : t) {
        if (is_space(c)) continue;
        if (c != '-' && c != '*' && c != '_') return false;
        if (marker == 0) marker = c;
        else if (c != marker) return false;
        ++count;
    }
    return count >= 3;
}

bool blockquote_line(const std::string &line)
{
    return indent_of(line) <= 3 && !strip_indent(line, 3).empty() && strip_indent(line, 3)[0] == '>';
}

std::string blockquote_strip(const std::string &line)
{
    std::string t = strip_indent(line, 3);
    if (t.empty() || t[0] != '>') return line;
    t = t.substr(1);
    if (!t.empty() && t[0] == ' ') t = t.substr(1);
    return t;
}

struct ListMarker
{
    bool        ordered { false };
    int         start { 1 };
    char        bullet { 0 };      // '-', '*', '+', '.' or ')'
    size_t      indent { 0 };      // columns before the marker
    size_t      content_indent { 0 }; // columns before the content
    std::string content;
};

bool list_item_start(const std::string &line, ListMarker &m)
{
    const size_t indent = indent_of(line);
    const std::string t = strip_indent(line, indent);
    if (t.empty()) return false;
    size_t marker_len = 0;
    if (t[0] == '-' || t[0] == '*' || t[0] == '+') {
        m.ordered = false;
        m.bullet  = t[0];
        marker_len = 1;
    } else {
        size_t d = 0;
        while (d < t.size() && d < 9 && std::isdigit(static_cast<unsigned char>(t[d]))) ++d;
        if (d == 0 || d >= t.size() || (t[d] != '.' && t[d] != ')')) return false;
        m.ordered = true;
        m.start   = std::stoi(t.substr(0, d));
        m.bullet  = t[d];
        marker_len = d + 1;
    }
    if (marker_len < t.size() && !is_space(t[marker_len])) return false;
    if (marker_len >= t.size()) { // empty item
        m.indent = indent;
        m.content_indent = indent + marker_len + 1;
        m.content.clear();
        return !thematic_break(line);
    }
    // Content starts after 1..4 spaces; more than 4 means an indented code
    // block inside the item, which CommonMark treats as one space + code.
    size_t spaces = 0;
    while (marker_len + spaces < t.size() && is_space(t[marker_len + spaces])) ++spaces;
    if (spaces > 4) spaces = 1;
    m.indent         = indent;
    m.content_indent = indent + marker_len + spaces;
    m.content        = t.substr(marker_len + spaces);
    return !thematic_break(line);
}

bool table_delimiter_row(const std::string &line, std::vector<std::string> &aligns)
{
    std::string t = trim(line);
    if (t.empty() || t.find('-') == std::string::npos) return false;
    if (!t.empty() && t.front() == '|') t = t.substr(1);
    if (!t.empty() && t.back() == '|') t.pop_back();
    aligns.clear();
    std::stringstream ss(t);
    std::string cell;
    while (std::getline(ss, cell, '|')) {
        cell = trim(cell);
        if (cell.empty()) return false;
        const bool left = cell.front() == ':', right = cell.back() == ':';
        std::string dashes = cell.substr(left ? 1 : 0, cell.size() - (left ? 1 : 0) - (right ? 1 : 0));
        if (dashes.empty() || !std::all_of(dashes.begin(), dashes.end(), [](char c) { return c == '-'; })) return false;
        aligns.push_back(left && right ? "center" : right ? "right" : left ? "left" : "");
    }
    return !aligns.empty();
}

std::vector<std::string> table_cells(const std::string &line)
{
    std::string t = trim(line);
    if (!t.empty() && t.front() == '|') t = t.substr(1);
    if (!t.empty() && t.back() == '|' && (t.size() < 2 || t[t.size() - 2] != '\\')) t.pop_back();
    std::vector<std::string> cells;
    std::string cell;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '\\' && i + 1 < t.size() && t[i + 1] == '|') { cell.push_back('|'); ++i; continue; }
        if (t[i] == '`') {
            const size_t end = code_span_end(t, i);
            if (end != std::string::npos) { cell += t.substr(i, end - i); i = end - 1; continue; }
        }
        if (t[i] == '|') { cells.push_back(trim(cell)); cell.clear(); continue; }
        cell.push_back(t[i]);
    }
    cells.push_back(trim(cell));
    return cells;
}

bool table_start(const std::vector<std::string> &lines, size_t i, size_t end)
{
    if (i + 1 >= end) return false;
    if (lines[i].find('|') == std::string::npos) return false;
    std::vector<std::string> aligns;
    if (!table_delimiter_row(lines[i + 1], aligns)) return false;
    return table_cells(lines[i]).size() == aligns.size();
}

// Does this line interrupt a paragraph?
bool interrupts_paragraph(const std::vector<std::string> &lines, size_t i, size_t end)
{
    const std::string &line = lines[i];
    if (is_blank(line)) return true;
    char fc; size_t fl; std::string info; int lvl; std::string txt; ListMarker m;
    if (fence_start(line, fc, fl, info)) return true;
    if (atx_heading(line, lvl, txt)) return true;
    if (thematic_break(line)) return true;
    if (blockquote_line(line)) return true;
    if (list_item_start(line, m) && !m.content.empty() && (!m.ordered || m.start == 1)) return true;
    if (table_start(lines, i, end)) return true;
    return false;
}

std::string unique_slug(const std::string &text, BlockContext &ctx)
{
    std::string slug = heading_slug(text);
    if (slug.empty()) slug = "section";
    int &n = (*ctx.slugs)[slug];
    std::string id = n == 0 ? slug : slug + "-" + std::to_string(n);
    ++n;
    return id;
}

void render_list(const std::vector<std::string> &lines, size_t &i, size_t end, std::string &out, BlockContext &ctx)
{
    ListMarker first;
    list_item_start(lines[i], first);
    const bool ordered = first.ordered;
    const char bullet  = first.bullet;

    out += ordered ? (first.start == 1 ? "<ol>\n" : "<ol start=\"" + std::to_string(first.start) + "\">\n") : "<ul>\n";

    while (i < end) {
        ListMarker m;
        if (!list_item_start(lines[i], m) || m.ordered != ordered || m.bullet != bullet || m.indent != first.indent) break;
        // Gather this item's lines.
        std::vector<std::string> item_lines;
        item_lines.push_back(m.content);
        ++i;
        while (i < end) {
            const std::string &line = lines[i];
            if (is_blank(line)) {
                // Blank lines belong to the item only when followed by more
                // indented content or another item of this list.
                size_t j = i;
                while (j < end && is_blank(lines[j])) ++j;
                if (j >= end) break;
                if (indent_of(lines[j]) >= m.content_indent) {
                    for (; i < j; ++i) item_lines.emplace_back();
                    continue;
                }
                ListMarker next;
                if (list_item_start(lines[j], next) && next.indent == first.indent && next.ordered == ordered &&
                    next.bullet == bullet) {
                    i = j; // the list continues after the gap
                }
                break;
            }
            if (indent_of(line) >= m.content_indent) {
                item_lines.push_back(strip_indent(line, m.content_indent));
                ++i;
                continue;
            }
            ListMarker next;
            if (list_item_start(line, next)) break; // sibling (or parent) item
            // Lazy continuation of a paragraph.
            if (!item_lines.empty() && !is_blank(item_lines.back()) && !interrupts_paragraph(lines, i, end)) {
                item_lines.push_back(trim(line));
                ++i;
                continue;
            }
            break;
        }
        // Task list checkbox.
        std::string checkbox;
        if (!item_lines.empty()) {
            std::string &c = item_lines.front();
            if (c.rfind("[ ] ", 0) == 0 || c == "[ ]") { checkbox = "<input type=\"checkbox\" disabled> "; c = c.size() > 3 ? c.substr(4) : ""; }
            else if (c.rfind("[x] ", 0) == 0 || c.rfind("[X] ", 0) == 0 || c == "[x]" || c == "[X]") { checkbox = "<input type=\"checkbox\" disabled checked> "; c = c.size() > 3 ? c.substr(4) : ""; }
        }
        // Tight rendering: an item with no blank line inside it renders its
        // leading paragraph without the <p> wrapper (nested blocks keep theirs).
        while (!item_lines.empty() && is_blank(item_lines.back())) item_lines.pop_back();
        const bool tight = std::none_of(item_lines.begin(), item_lines.end(), [](const std::string &l) { return is_blank(l); });
        std::string body;
        render_blocks(item_lines, 0, item_lines.size(), body, ctx);
        const std::string open = "<p>", close = "</p>\n";
        if (tight && body.rfind(open, 0) == 0) {
            const size_t close_at = body.find(close, open.size());
            if (close_at != std::string::npos)
                {
                const std::string rest = body.substr(close_at + close.size());
                body = body.substr(open.size(), close_at - open.size()) + (rest.empty() ? "" : "\n" + rest);
            }
        }
        if (!body.empty() && body.back() == '\n') body.pop_back();
        out += "<li>" + checkbox + body + "</li>\n";
    }
    out += ordered ? "</ol>\n" : "</ul>\n";
}

void render_blocks(const std::vector<std::string> &lines, size_t begin, size_t end, std::string &out, BlockContext &ctx)
{
    InlineContext ictx{ctx.options};
    size_t i = begin;
    while (i < end) {
        const std::string &line = lines[i];
        if (is_blank(line)) { ++i; continue; }

        char fence_char; size_t fence_len; std::string info;
        if (fence_start(line, fence_char, fence_len, info)) {
            const size_t fence_indent = indent_of(line);
            std::string code;
            ++i;
            while (i < end && !fence_end(lines[i], fence_char, fence_len)) {
                code += strip_indent(lines[i], fence_indent);
                code += '\n';
                ++i;
            }
            if (i < end) ++i; // closing fence
            std::string lang;
            const size_t sp = info.find_first_of(" \t");
            lang = sp == std::string::npos ? info : info.substr(0, sp);
            out += "<pre><code";
            if (!lang.empty()) out += " class=\"language-" + escape_html(lang) + "\"";
            out += ">" + escape_html(code) + "</code></pre>\n";
            continue;
        }

        int level; std::string heading;
        if (atx_heading(line, level, heading)) {
            const std::string id = unique_slug(heading, ctx);
            out += "<h" + std::to_string(level) + " id=\"" + escape_html(id) + "\">" + render_inline(heading, ictx) +
                   "</h" + std::to_string(level) + ">\n";
            ++i;
            continue;
        }

        if (thematic_break(line)) { out += "<hr>\n"; ++i; continue; }

        if (blockquote_line(line)) {
            std::vector<std::string> inner;
            while (i < end && blockquote_line(lines[i])) inner.push_back(blockquote_strip(lines[i++]));
            std::string body;
            render_blocks(inner, 0, inner.size(), body, ctx);
            out += "<blockquote>\n" + body + "</blockquote>\n";
            continue;
        }

        ListMarker marker;
        if (list_item_start(line, marker)) { render_list(lines, i, end, out, ctx); continue; }

        if (table_start(lines, i, end)) {
            std::vector<std::string> aligns;
            table_delimiter_row(lines[i + 1], aligns);
            const std::vector<std::string> head = table_cells(lines[i]);
            out += "<table>\n<thead>\n<tr>\n";
            for (size_t c = 0; c < aligns.size(); ++c) {
                out += "<th";
                if (!aligns[c].empty()) out += " align=\"" + aligns[c] + "\"";
                out += ">" + render_inline(c < head.size() ? head[c] : "", ictx) + "</th>\n";
            }
            out += "</tr>\n</thead>\n";
            i += 2;
            std::string body;
            // GFM: rows continue until a blank line or another block start;
            // a row without any pipe is still one (single) cell.
            while (i < end && !is_blank(lines[i]) && !table_start(lines, i, end) && !interrupts_paragraph(lines, i, end)) {
                const std::vector<std::string> cells = table_cells(lines[i]);
                body += "<tr>\n";
                for (size_t c = 0; c < aligns.size(); ++c) {
                    body += "<td";
                    if (!aligns[c].empty()) body += " align=\"" + aligns[c] + "\"";
                    body += ">" + render_inline(c < cells.size() ? cells[c] : "", ictx) + "</td>\n";
                }
                body += "</tr>\n";
                ++i;
            }
            if (!body.empty()) out += "<tbody>\n" + body + "</tbody>\n";
            out += "</table>\n";
            continue;
        }

        if (indent_of(line) >= 4) {
            std::string code;
            size_t last_nonblank = i;
            size_t j = i;
            while (j < end && (is_blank(lines[j]) || indent_of(lines[j]) >= 4)) {
                if (!is_blank(lines[j])) last_nonblank = j;
                ++j;
            }
            for (size_t k = i; k <= last_nonblank; ++k) code += (is_blank(lines[k]) ? std::string() : strip_indent(lines[k], 4)) + "\n";
            out += "<pre><code>" + escape_html(code) + "</code></pre>\n";
            i = last_nonblank + 1;
            continue;
        }

        // Paragraph.
        std::string para = trim(line);
        ++i;
        while (i < end && !interrupts_paragraph(lines, i, end)) {
            // Preserve trailing spaces of the previous line for hard breaks.
            const std::string &prev = lines[i - 1];
            size_t trailing = 0;
            while (trailing < prev.size() && prev[prev.size() - 1 - trailing] == ' ') ++trailing;
            if (trailing >= 2) para += "  ";
            para += "\n" + trim(lines[i]);
            ++i;
        }
        out += "<p>" + render_inline(para, ictx) + "</p>\n";
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string escape_html(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out.push_back(c);
        }
    }
    return out;
}

std::string heading_slug(const std::string &heading_text)
{
    // Strip inline markup characters first so "**Bold** title" and
    // "`code` title" slug the same way GitHub does.
    std::string slug;
    for (char c : heading_text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= 0x80) { slug.push_back(c); continue; } // keep UTF-8 bytes
        if (is_alnum(c)) { slug.push_back(static_cast<char>(std::tolower(u))); continue; }
        if (c == ' ' || c == '-') { slug.push_back('-'); continue; }
        if (c == '_') { slug.push_back('_'); continue; }
        // other punctuation dropped
    }
    // Collapse runs of hyphens that came from " - " style separators.
    std::string collapsed;
    for (char c : slug) {
        if (c == '-' && !collapsed.empty() && collapsed.back() == '-') continue;
        collapsed.push_back(c);
    }
    while (!collapsed.empty() && collapsed.front() == '-') collapsed.erase(collapsed.begin());
    while (!collapsed.empty() && collapsed.back() == '-') collapsed.pop_back();
    return collapsed;
}

std::string render_to_html(const std::string &markdown, const RenderOptions &options)
{
    const std::vector<std::string> lines = split_lines(markdown);
    std::map<std::string, int> slugs;
    BlockContext ctx{&options, &slugs};
    std::string out;
    render_blocks(lines, 0, lines.size(), out, ctx);
    return out;
}

std::string first_heading(const std::string &markdown)
{
    for (const std::string &line : split_lines(markdown)) {
        int level; std::string text;
        if (atx_heading(line, level, text) && level == 1) return text;
    }
    return {};
}

std::string plain_text(const std::string &markdown)
{
    const std::string html = render_to_html(markdown);
    std::string out;
    out.reserve(html.size());
    bool in_tag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        const char c = html[i];
        if (c == '<') { in_tag = true; continue; }
        if (c == '>') { in_tag = false; if (!out.empty() && out.back() != ' ' && out.back() != '\n') out.push_back(' '); continue; }
        if (in_tag) continue;
        if (c == '&') {
            const size_t semi = html.find(';', i);
            if (semi != std::string::npos && semi - i <= 6) {
                const std::string ent = html.substr(i, semi - i + 1);
                if (ent == "&amp;") out.push_back('&');
                else if (ent == "&lt;") out.push_back('<');
                else if (ent == "&gt;") out.push_back('>');
                else if (ent == "&quot;") out.push_back('"');
                else if (ent == "&#39;") out.push_back('\'');
                else { out += ent; }
                i = semi;
                continue;
            }
        }
        out.push_back(c);
    }
    // Collapse whitespace runs.
    std::string collapsed;
    for (char c : out) {
        const bool ws = c == ' ' || c == '\n' || c == '\t';
        if (ws && !collapsed.empty() && collapsed.back() == ' ') continue;
        collapsed.push_back(ws ? ' ' : c);
    }
    return trim(collapsed);
}

} // namespace Slic3r::Markdown
