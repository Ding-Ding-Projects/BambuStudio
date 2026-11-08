#include "PreferenceFormats.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <vector>

namespace Slic3r::GUI::PreferenceFormats {

namespace {

using json = nlohmann::json;

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
    return s;
}

std::vector<std::string_view> lines_of(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t                   start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        if (end == text.size()) break;
        start = end + 1;
    }
    return lines;
}

// A double-quoted string with the JSON escape set, which the YAML and TOML
// writers use, from s[at] == '"'. `end` is just past the closing quote.
bool double_quoted(std::string_view s, std::size_t at, std::string &out, std::size_t &end)
{
    std::size_t i = at + 1;
    while (i < s.size() && s[i] != '"') i += s[i] == '\\' ? 2 : 1;
    if (i >= s.size()) return false;
    const json j = json::parse(s.substr(at, i - at + 1), nullptr, false);
    if (!j.is_string()) return false;
    out = j.get<std::string>();
    end = i + 1;
    return true;
}

// 'it''s': a single-quoted string, where '' stands for one quote.
bool single_quoted(std::string_view s, std::size_t at, std::string &out, std::size_t &end)
{
    out.clear();
    for (std::size_t i = at + 1; i < s.size(); ++i) {
        if (s[i] != '\'') {
            out += s[i];
            continue;
        }
        if (i + 1 < s.size() && s[i + 1] == '\'') {
            out += '\'';
            ++i;
            continue;
        }
        end = i + 1;
        return true;
    }
    return false;
}

// Cuts a "# comment" that is outside quotes and follows a space.
std::string_view without_comment(std::string_view s)
{
    char quote = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (quote == '"' && c == '\\') {
            ++i;
            continue;
        }
        if (quote != 0) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == '#' && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t')) return trim(s.substr(0, i));
    }
    return trim(s);
}

void skip_space(std::string_view s, std::size_t &i, bool newlines)
{
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || (newlines && (s[i] == '\n' || s[i] == '\r')))) ++i;
}

// ---------------------------------------------------------------- YAML

struct Frame
{
    int   indent;
    json *node;
};

bool yaml_scalar(std::string_view text, json &out)
{
    text = trim(text);
    if (text.empty()) return false;
    std::string s;
    std::size_t end = 0;
    if (text.front() == '"') {
        if (!double_quoted(text, 0, s, end) || !trim(text.substr(end)).empty()) return false;
        out = s;
        return true;
    }
    if (text.front() == '\'') {
        if (!single_quoted(text, 0, s, end) || !trim(text.substr(end)).empty()) return false;
        out = s;
        return true;
    }
    if (text == "null" || text == "~") {
        out = nullptr;
        return true;
    }
    if (text == "true" || text == "false") {
        out = text == "true";
        return true;
    }
    if (text == "[]" || text == "{}") {
        out = text == "[]" ? json::array() : json::object();
        return true;
    }
    // Flow collections, anchors, aliases, tags and block scalars are never
    // written by the exporter; refuse rather than misread them.
    if (std::string_view("[{&*!|>%@`").find(text.front()) != std::string_view::npos) return false;
    if (text.find(": ") != std::string_view::npos) return false;
    const json number = json::parse(text, nullptr, false);
    if (number.is_number()) {
        out = number;
        return true;
    }
    out = std::string(text); // a plain word
    return true;
}

// "key: rest", the key plain or quoted. False when the line is not a mapping entry.
bool yaml_key(std::string_view content, std::string &key, std::string_view &rest)
{
    std::size_t colon = 0;
    if (content.front() == '"' || content.front() == '\'') {
        std::size_t end = 0;
        if (!(content.front() == '"' ? double_quoted(content, 0, key, end) : single_quoted(content, 0, key, end))) return false;
        colon = end;
        while (colon < content.size() && content[colon] == ' ') ++colon;
        if (colon >= content.size() || content[colon] != ':') return false;
    } else {
        colon = content.find(':');
        // A colon inside a plain key or value ("C:\x") is not the separator.
        while (colon != std::string_view::npos && colon + 1 < content.size() && content[colon + 1] != ' ' && content[colon + 1] != '\t')
            colon = content.find(':', colon + 1);
        if (colon == std::string_view::npos || colon == 0) return false;
        key = std::string(trim(content.substr(0, colon)));
    }
    if (colon + 1 < content.size() && content[colon + 1] != ' ' && content[colon + 1] != '\t') return false;
    rest = trim(content.substr(colon + 1));
    return !key.empty();
}

bool yaml_entry(json &object, const std::string &key, std::string_view value, int level, std::vector<Frame> &stack)
{
    if (object.contains(key)) return false; // a key twice is not YAML
    if (value.empty()) {
        object[key] = nullptr; // a block follows, or the value is empty
        stack.push_back({level, &object[key]});
        return true;
    }
    json v;
    if (!yaml_scalar(value, v)) return false;
    object[key] = std::move(v);
    return true;
}

bool yaml_to_json(std::string_view text, json &root)
{
    root = json();
    std::vector<Frame> stack{{-1, &root}};
    bool               content_seen = false;
    for (const std::string_view raw : lines_of(text)) {
        std::size_t indent = 0;
        while (indent < raw.size() && raw[indent] == ' ') ++indent;
        if (indent < raw.size() && raw[indent] == '\t') return false; // tabs never indent YAML
        const std::string_view content = without_comment(raw.substr(indent));
        if (content.empty()) continue;
        if (indent == 0 && content.front() == '%') continue; // %YAML 1.2
        if (indent == 0 && (content == "---" || content == "...")) {
            if (content == "---" && content_seen) return false; // one document only
            continue;
        }
        content_seen = true;
        const int level = int(indent);
        while (stack.back().indent >= level) stack.pop_back();
        json *parent = stack.back().node;

        if (content == "-" || content.rfind("- ", 0) == 0) {
            if (parent->is_null()) *parent = json::array();
            if (!parent->is_array()) return false;
            const std::string_view rest = content == "-" ? std::string_view() : trim(content.substr(2));
            if (rest.empty()) {
                parent->push_back(nullptr);
                stack.push_back({level, &parent->back()});
                continue;
            }
            std::string      key;
            std::string_view value;
            if (yaml_key(rest, key, value)) {
                // "- key: value": an object whose entries sit two columns in.
                parent->push_back(json::object());
                json *item = &parent->back();
                stack.push_back({level, item});
                if (!yaml_entry(*item, key, value, level + 2, stack)) return false;
                continue;
            }
            json v;
            if (!yaml_scalar(rest, v)) return false;
            parent->push_back(std::move(v));
            continue;
        }

        std::string      key;
        std::string_view value;
        if (!yaml_key(content, key, value)) return false;
        if (parent->is_null()) *parent = json::object();
        if (!parent->is_object()) return false;
        if (!yaml_entry(*parent, key, value, level, stack)) return false;
    }
    return true;
}

// ---------------------------------------------------------------- TOML

bool toml_key_at(std::string_view s, std::size_t &i, std::vector<std::string> &path)
{
    path.clear();
    for (;;) {
        skip_space(s, i, false);
        if (i >= s.size()) return false;
        std::string part;
        std::size_t end = 0;
        if (s[i] == '"') {
            if (!double_quoted(s, i, part, end)) return false;
            i = end;
        } else if (s[i] == '\'') {
            if (!single_quoted(s, i, part, end)) return false;
            i = end;
        } else {
            const std::size_t start = i;
            while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '-')) ++i;
            if (i == start) return false;
            part = std::string(s.substr(start, i - start));
        }
        path.push_back(std::move(part));
        std::size_t j = i;
        skip_space(s, j, false);
        if (j < s.size() && s[j] == '.') {
            i = j + 1;
            continue;
        }
        return true;
    }
}

bool assign(json &table, const std::vector<std::string> &path, json value)
{
    json *node = &table;
    for (std::size_t k = 0; k + 1 < path.size(); ++k) {
        if (!node->is_object()) return false;
        json &child = (*node)[path[k]];
        if (child.is_null()) child = json::object();
        node = &child;
    }
    if (!node->is_object() || node->contains(path.back())) return false;
    (*node)[path.back()] = std::move(value);
    return true;
}

bool toml_value(std::string_view s, std::size_t &i, json &out)
{
    skip_space(s, i, false);
    if (i >= s.size()) return false;
    std::string text;
    std::size_t end = 0;
    if (s.compare(i, 3, "\"\"\"") == 0 || s.compare(i, 3, "'''") == 0) return false; // multi-line strings are never written
    if (s[i] == '"' || s[i] == '\'') {
        if (!(s[i] == '"' ? double_quoted(s, i, text, end) : single_quoted(s, i, text, end))) return false;
        i   = end;
        out = text;
        return true;
    }
    if (s[i] == '[') {
        out = json::array();
        ++i;
        for (;;) {
            skip_space(s, i, true);
            if (i < s.size() && s[i] == ']') {
                ++i;
                return true;
            }
            json item;
            if (!toml_value(s, i, item)) return false;
            out.push_back(std::move(item));
            skip_space(s, i, true);
            if (i < s.size() && s[i] == ',') {
                ++i;
                continue;
            }
            if (i < s.size() && s[i] == ']') {
                ++i;
                return true;
            }
            return false;
        }
    }
    if (s[i] == '{') {
        out = json::object();
        ++i;
        skip_space(s, i, false);
        if (i < s.size() && s[i] == '}') {
            ++i;
            return true;
        }
        for (;;) {
            std::vector<std::string> path;
            if (!toml_key_at(s, i, path)) return false;
            skip_space(s, i, false);
            if (i >= s.size() || s[i] != '=') return false;
            ++i;
            json value;
            if (!toml_value(s, i, value) || !assign(out, path, std::move(value))) return false;
            skip_space(s, i, false);
            if (i < s.size() && s[i] == ',') {
                ++i;
                continue;
            }
            if (i < s.size() && s[i] == '}') {
                ++i;
                return true;
            }
            return false;
        }
    }
    const std::size_t start = i;
    while (i < s.size() && std::string_view(",]} \t\r\n").find(s[i]) == std::string_view::npos) ++i;
    std::string word(s.substr(start, i - start));
    if (word.empty()) return false;
    if (word == "true" || word == "false") {
        out = word == "true";
        return true;
    }
    if (word == "nan" || word == "+nan" || word == "-nan" || word == "inf" || word == "+inf" || word == "-inf") {
        out = nullptr; // no JSON form; the import counts it as not text
        return true;
    }
    word.erase(std::remove(word.begin(), word.end(), '_'), word.end());
    const json number = json::parse(word.front() == '+' ? word.substr(1) : word, nullptr, false);
    if (number.is_number()) {
        out = number;
        return true;
    }
    if (std::isdigit(static_cast<unsigned char>(word.front()))) {
        out = word; // a date, a time or 0x1F: kept as text
        return true;
    }
    return false;
}

// Brackets opened and not closed on this line, outside quotes.
int open_brackets(std::string_view s)
{
    int  depth = 0;
    char quote = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (quote == '"' && c == '\\') {
            ++i;
            continue;
        }
        if (quote != 0) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') quote = c;
        else if (c == '[' || c == '{') ++depth;
        else if (c == ']' || c == '}') --depth;
    }
    return depth;
}

// Lines, with an array or inline table that spans lines joined into one.
std::vector<std::string> toml_lines(std::string_view text)
{
    std::vector<std::string> out;
    std::string              current;
    int                      depth = 0;
    for (const std::string_view line : lines_of(text)) {
        const std::string_view content = without_comment(line);
        if (depth > 0) current += '\n';
        else current.clear();
        current += content;
        depth += open_brackets(content);
        if (depth <= 0) {
            out.push_back(current);
            depth = 0;
        }
    }
    if (depth > 0) out.push_back(current); // never closed: refused below
    return out;
}

json *table_at(json &root, const std::vector<std::string> &path, bool array_of_tables)
{
    json *node = &root;
    for (std::size_t k = 0; k < path.size(); ++k) {
        if (!node->is_object()) return nullptr;
        json      &child = (*node)[path[k]];
        const bool last  = k + 1 == path.size();
        if (last && array_of_tables) {
            if (child.is_null()) child = json::array();
            if (!child.is_array()) return nullptr;
            child.push_back(json::object());
            return &child.back();
        }
        if (child.is_null()) child = json::object();
        if (child.is_array()) {
            if (child.empty() || !child.back().is_object()) return nullptr;
            node = &child.back();
            continue;
        }
        if (!child.is_object()) return nullptr;
        node = &child;
    }
    return node;
}

bool toml_to_json(std::string_view text, json &root)
{
    root        = json::object();
    json *table = &root;
    for (const std::string &line : toml_lines(text)) {
        const std::string_view content = trim(line);
        if (content.empty()) continue;
        if (content.front() == '[') {
            const bool               array = content.size() > 1 && content[1] == '[';
            std::size_t              i     = array ? 2 : 1;
            std::vector<std::string> path;
            if (!toml_key_at(content, i, path)) return false;
            skip_space(content, i, false);
            const std::string_view close = array ? "]]" : "]";
            if (content.compare(i, close.size(), close) != 0 || !trim(content.substr(i + close.size())).empty()) return false;
            table = table_at(root, path, array);
            if (table == nullptr) return false;
            continue;
        }
        std::size_t              i = 0;
        std::vector<std::string> path;
        if (!toml_key_at(content, i, path)) return false;
        skip_space(content, i, false);
        if (i >= content.size() || content[i] != '=') return false;
        ++i;
        json value;
        if (!toml_value(content, i, value)) return false;
        skip_space(content, i, true);
        if (i != content.size() || !assign(*table, path, std::move(value))) return false;
    }
    return true;
}

// ---------------------------------------------------------------- XML

struct XmlReader
{
    std::string_view s;
    std::size_t      i = 0;

    void space()
    {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }

    // Whitespace, comments and <?...?> declarations.
    bool misc()
    {
        for (;;) {
            space();
            if (s.compare(i, 4, "<!--") == 0) {
                const std::size_t end = s.find("-->", i + 4);
                if (end == std::string_view::npos) return false;
                i = end + 3;
                continue;
            }
            if (s.compare(i, 2, "<?") == 0) {
                const std::size_t end = s.find("?>", i + 2);
                if (end == std::string_view::npos) return false;
                i = end + 2;
                continue;
            }
            if (s.compare(i, 2, "<!") == 0) return false; // DOCTYPE and CDATA are never written
            return true;
        }
    }

    static bool unescape(std::string_view text, std::string &out)
    {
        out.clear();
        for (std::size_t k = 0; k < text.size(); ++k) {
            if (text[k] != '&') {
                out += text[k];
                continue;
            }
            const std::size_t semi = text.find(';', k);
            if (semi == std::string_view::npos) return false;
            const std::string_view name = text.substr(k + 1, semi - k - 1);
            if (name == "amp") out += '&';
            else if (name == "lt") out += '<';
            else if (name == "gt") out += '>';
            else if (name == "quot") out += '"';
            else if (name == "apos") out += '\'';
            else if (name.size() > 1 && name[0] == '#') {
                const bool        hex  = name[1] == 'x' || name[1] == 'X';
                const std::string code(name.substr(hex ? 2 : 1));
                char             *end  = nullptr;
                const unsigned long cp = std::strtoul(code.c_str(), &end, hex ? 16 : 10);
                if (code.empty() || *end != '\0' || cp == 0 || cp > 0x10FFFF) return false;
                // UTF-8
                if (cp < 0x80) out += char(cp);
                else if (cp < 0x800) {
                    out += char(0xC0 | (cp >> 6));
                    out += char(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    out += char(0xE0 | (cp >> 12));
                    out += char(0x80 | ((cp >> 6) & 0x3F));
                    out += char(0x80 | (cp & 0x3F));
                } else {
                    out += char(0xF0 | (cp >> 18));
                    out += char(0x80 | ((cp >> 12) & 0x3F));
                    out += char(0x80 | ((cp >> 6) & 0x3F));
                    out += char(0x80 | (cp & 0x3F));
                }
            } else return false;
            k = semi;
        }
        return true;
    }

    // <name a="1" b='2'> or <name .../>; the reader is left after it.
    bool open(std::string &name, std::map<std::string, std::string> &attributes, bool &empty)
    {
        attributes.clear();
        if (i >= s.size() || s[i] != '<' || s.compare(i, 2, "</") == 0) return false;
        ++i;
        const std::size_t start = i;
        while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '-' || s[i] == ':')) ++i;
        if (i == start) return false;
        name = std::string(s.substr(start, i - start));
        for (;;) {
            space();
            if (i >= s.size()) return false;
            if (s.compare(i, 2, "/>") == 0) {
                i += 2;
                empty = true;
                return true;
            }
            if (s[i] == '>') {
                ++i;
                empty = false;
                return true;
            }
            const std::size_t key_start = i;
            while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '-' || s[i] == ':')) ++i;
            if (i == key_start) return false;
            const std::string key(s.substr(key_start, i - key_start));
            space();
            if (i >= s.size() || s[i] != '=') return false;
            ++i;
            space();
            if (i >= s.size() || (s[i] != '"' && s[i] != '\'')) return false;
            const char        quote = s[i];
            const std::size_t end   = s.find(quote, i + 1);
            if (end == std::string_view::npos) return false;
            std::string value;
            if (!unescape(s.substr(i + 1, end - i - 1), value) || attributes.count(key)) return false;
            attributes[key] = std::move(value);
            i = end + 1;
        }
    }

    bool close(std::string_view name)
    {
        space();
        if (s.compare(i, 2, "</") != 0) return false;
        i += 2;
        if (s.compare(i, name.size(), name) != 0) return false;
        i += name.size();
        space();
        if (i >= s.size() || s[i] != '>') return false;
        ++i;
        return true;
    }

    // <item key="..." type="..."> as the exporter writes it.
    bool item(json &out, std::string *key)
    {
        if (!misc()) return false;
        std::string                        name;
        std::map<std::string, std::string> attributes;
        bool                               empty = false;
        if (!open(name, attributes, empty) || name != "item" || !attributes.count("type")) return false;
        if (key != nullptr) {
            if (!attributes.count("key")) return false;
            *key = attributes["key"];
        }
        const std::string type = attributes["type"];
        if (type == "object" || type == "array") {
            out = type == "object" ? json::object() : json::array();
            if (empty) return true;
            for (;;) {
                if (!misc()) return false;
                if (s.compare(i, 2, "</") == 0) return close("item");
                json        child;
                std::string child_key;
                if (!item(child, type == "object" ? &child_key : nullptr)) return false;
                if (type == "array") out.push_back(std::move(child));
                else if (out.contains(child_key)) return false;
                else out[child_key] = std::move(child);
            }
        }
        std::string text;
        if (!empty) {
            const std::size_t end = s.find('<', i);
            if (end == std::string_view::npos || !unescape(s.substr(i, end - i), text)) return false;
            i = end;
            if (!close("item")) return false;
        }
        if (type == "null") out = nullptr;
        else if (type == "string") out = text;
        else if (type == "boolean") {
            if (text != "true" && text != "false") return false;
            out = text == "true";
        } else if (type == "integer" || type == "number") {
            const json number = json::parse(text, nullptr, false);
            out               = number.is_number() ? number : json(); // NaN and Infinity: not text
        } else return false;
        return true;
    }
};

bool xml_to_json(std::string_view text, json &root)
{
    XmlReader r{text};
    if (!r.misc()) return false;
    std::string                        name;
    std::map<std::string, std::string> attributes;
    bool                               empty = false;
    if (!r.open(name, attributes, empty) || name != "export" || empty) return false;
    root = json::object();
    if (attributes.count("schema")) root["schema"] = attributes["schema"];
    if (attributes.count("schemaVersion")) {
        const json version = json::parse(attributes["schemaVersion"], nullptr, false);
        root["schemaVersion"] = version.is_number_integer() ? version : json(attributes["schemaVersion"]);
    }
    if (!r.misc()) return false;
    std::map<std::string, std::string> data_attributes;
    if (!r.open(name, data_attributes, empty) || name != "data" || empty) return false;
    json data;
    if (!r.item(data, nullptr) || !r.close("data") || !r.close("export") || !r.misc() || r.i != text.size()) return false;
    root["data"] = std::move(data);
    return true;
}

} // namespace

Format detect(std::string_view file_name, std::string_view text)
{
    std::string ext;
    if (const std::size_t dot = file_name.rfind('.'); dot != std::string_view::npos)
        for (char c : file_name.substr(dot + 1)) ext += char(std::tolower(static_cast<unsigned char>(c)));
    if (ext == "json") return Format::Json;
    if (ext == "yaml" || ext == "yml") return Format::Yaml;
    if (ext == "toml") return Format::Toml;
    if (ext == "xml") return Format::Xml;
    // Unknown extension: the first meaningful characters decide.
    for (const std::string_view line : lines_of(text)) {
        const std::string_view t = trim(line);
        if (t.empty() || t.front() == '#') continue;
        if (t.front() == '{') return Format::Json;
        if (t.front() == '<') return Format::Xml;
        if (t.front() == '%' || t == "---") return Format::Yaml;
        if (t.front() == '[' || t.find(" = ") != std::string_view::npos) return Format::Toml;
        if (t.find(':') != std::string_view::npos) return Format::Yaml;
        break;
    }
    return Format::Unknown;
}

const char *name(Format format)
{
    switch (format) {
    case Format::Json: return "JSON";
    case Format::Yaml: return "YAML";
    case Format::Toml: return "TOML";
    case Format::Xml: return "XML";
    case Format::Unknown: break;
    }
    return "";
}

bool to_json(Format format, std::string_view text, std::string &out, std::string &problem)
{
    problem.clear();
    if (format == Format::Json) {
        out = std::string(text);
        return true;
    }
    json root;
    bool read = false;
    switch (format) {
    case Format::Yaml: read = yaml_to_json(text, root); problem = "not-yaml"; break;
    case Format::Toml: read = toml_to_json(text, root); problem = "not-toml"; break;
    case Format::Xml: read = xml_to_json(text, root); problem = "not-xml"; break;
    default: problem = "unknown-format"; return false;
    }
#ifdef SLIC3R_PREFERENCE_FORMATS_NEGATIVE_FIXTURE
    // Test-only mutation (tests/preference_formats.test.mjs): the suite must fail.
    read = true;
#endif
    if (!read) return false;
    problem.clear();
    out = root.dump(-1, ' ', false, json::error_handler_t::replace);
    return true;
}

} // namespace Slic3r::GUI::PreferenceFormats
