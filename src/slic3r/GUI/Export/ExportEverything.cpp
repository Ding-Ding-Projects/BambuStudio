#include "ExportEverything.hpp"

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Slic3r::GUI::Export {

namespace fs = std::filesystem;

// ===========================================================================
// Small helpers
// ===========================================================================

namespace {

const char *kind_name(DatasetKind kind)
{
    switch (kind) {
    case DatasetKind::Tabular: return "tabular";
    case DatasetKind::Structured: return "structured";
    case DatasetKind::Prose: return "prose";
    }
    return "?";
}

const char *type_name(Value::Type type)
{
    switch (type) {
    case Value::Type::Null: return "null";
    case Value::Type::Bool: return "boolean";
    case Value::Type::Integer: return "integer";
    case Value::Type::Number: return "number";
    case Value::Type::String: return "string";
    case Value::Type::Array: return "array";
    case Value::Type::Object: return "object";
    }
    return "?";
}

// Shortest decimal that round-trips, JSON-compatible (no NaN/Inf: those are
// spelled out by the caller per format).
std::string format_number(double v)
{
    char buf[64];
    for (int precision = 15; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof buf, "%.*g", precision, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    std::string s(buf);
    // "1e+20" style is fine for every format we emit; make sure an integral
    // double still reads as a float in TOML/YAML by keeping a fraction marker.
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    return s;
}

std::string scalar_text(const Value &v)
{
    switch (v.type) {
    case Value::Type::Null: return "";
    case Value::Type::Bool: return v.boolean ? "true" : "false";
    case Value::Type::Integer: return std::to_string(v.integer);
    case Value::Type::Number:
        if (std::isnan(v.number)) return "NaN";
        if (std::isinf(v.number)) return v.number > 0 ? "Infinity" : "-Infinity";
        return format_number(v.number);
    case Value::Type::String: return v.string;
    case Value::Type::Array: return "[array]";
    case Value::Type::Object: return "{object}";
    }
    return "";
}

void walk(const Value &v, const std::function<void(const Value &)> &fn)
{
    fn(v);
    for (const Value &child : v.array) walk(child, fn);
    for (const auto &kv : v.object) walk(kv.second, fn);
}

std::size_t count_nulls(const Dataset &d)
{
    std::size_t n = 0;
    if (d.kind == DatasetKind::Tabular) {
        for (const auto &row : d.rows)
            for (const auto &cell : row)
                if (cell.is_null()) ++n;
    } else if (d.kind == DatasetKind::Structured) {
        walk(d.root, [&n](const Value &v) { if (v.is_null()) ++n; });
    }
    return n;
}

std::size_t count_non_finite(const Dataset &d)
{
    std::size_t n = 0;
    auto check = [&n](const Value &v) {
        if (v.type == Value::Type::Number && !std::isfinite(v.number)) ++n;
    };
    if (d.kind == DatasetKind::Tabular) {
        for (const auto &row : d.rows)
            for (const auto &cell : row) check(cell);
    } else if (d.kind == DatasetKind::Structured) {
        walk(d.root, check);
    }
    return n;
}

std::size_t count_multiline_cells(const Dataset &d)
{
    std::size_t n = 0;
    for (const auto &row : d.rows)
        for (const auto &cell : row)
            if (cell.type == Value::Type::String && cell.string.find('\n') != std::string::npos) ++n;
    return n;
}

bool is_bare_key(const std::string &key)
{
    if (key.empty()) return false;
    for (unsigned char c : key)
        if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    return true;
}

std::string indent_of(int n) { return std::string(static_cast<std::size_t>(n) * 2, ' '); }

// RFC 6901 JSON-pointer segment escaping so a flattened path stays reversible
// even when a key contains "/" or "~".
std::string pointer_segment(const std::string &key)
{
    std::string out;
    for (char c : key) {
        if (c == '~') out += "~0";
        else if (c == '/') out += "~1";
        else out += c;
    }
    return out;
}

std::string header_comment_line(const Dataset &d, const SerializeOptions &o)
{
    std::string s = "schema=" + d.schema_id + " schemaVersion=" + std::to_string(d.schema_version) + " dataset=\"" + d.name +
                    "\" kind=" + kind_name(d.kind) + " encoding=UTF-8 lineEnding=" + line_ending_name(o.line_ending) +
                    " generator=\"" + o.generator + "\"";
    if (d.kind == DatasetKind::Tabular && !d.columns.empty()) {
        s += " columns=";
        for (std::size_t i = 0; i < d.columns.size(); ++i) {
            if (i) s += ",";
            s += d.columns[i].name + ":" + type_name(d.columns[i].type);
        }
    }
    return s;
}

Value header_value(const Dataset &d, const SerializeOptions &o)
{
    Value h = Value::make_object();
    h.set("schema", Value::from_string(d.schema_id));
    h.set("schemaVersion", Value::from_int(d.schema_version));
    h.set("dataset", Value::from_string(d.name));
    h.set("kind", Value::from_string(kind_name(d.kind)));
    h.set("encoding", Value::from_string("UTF-8"));
    h.set("lineEnding", Value::from_string(line_ending_name(o.line_ending)));
    h.set("generator", Value::from_string(o.generator));
    if (d.kind == DatasetKind::Tabular) {
        Value cols = Value::make_array();
        for (const Column &c : d.columns) {
            Value col = Value::make_object();
            col.set("name", Value::from_string(c.name));
            col.set("type", Value::from_string(type_name(c.type)));
            cols.push(std::move(col));
        }
        h.set("columns", std::move(cols));
    }
    return h;
}

Value row_object(const Dataset &d, const std::vector<Value> &row)
{
    Value obj = Value::make_object();
    for (std::size_t i = 0; i < d.columns.size(); ++i)
        obj.set(d.columns[i].name, i < row.size() ? row[i] : Value::null());
    return obj;
}

Value prose_object(const Dataset &d)
{
    Value obj = Value::make_object();
    obj.set("title", Value::from_string(d.prose_title));
    obj.set("text", Value::from_string(d.prose));
    return obj;
}

// The payload every format wraps: rows as objects, the root, or the prose.
Value data_value(const Dataset &d)
{
    switch (d.kind) {
    case DatasetKind::Tabular: {
        Value arr = Value::make_array();
        for (const auto &row : d.rows) arr.push(row_object(d, row));
        return arr;
    }
    case DatasetKind::Structured: return d.root;
    case DatasetKind::Prose: return prose_object(d);
    }
    return Value::null();
}

} // namespace

// ===========================================================================
// Escapers
// ===========================================================================

std::string json_escape(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

std::string csv_quote(const std::string &cell, char separator)
{
    const bool needs_quote = cell.find_first_of(std::string("\"\r\n") + separator) != std::string::npos ||
                             (!cell.empty() && (cell.front() == ' ' || cell.back() == ' '));
    if (!needs_quote) return cell;
    std::string out = "\"";
    for (char c : cell) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

std::string tsv_escape(const std::string &cell)
{
    std::string out;
    for (char c : cell) {
        switch (c) {
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\\': out += "\\\\"; break;
        default: out += c;
        }
    }
    return out;
}

std::string xml_escape(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:
            // XML 1.0 forbids most C0 controls entirely; drop them rather than
            // emit a document no parser accepts (tab, LF, CR are allowed).
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') break;
            out += static_cast<char>(c);
        }
    }
    return out;
}

std::string toml_quote(const std::string &text)
{
    // TOML basic strings share JSON's escape set (\uXXXX for controls).
    return "\"" + json_escape(text) + "\"";
}

std::string yaml_quote(const std::string &text)
{
    // A double-quoted YAML scalar accepts the JSON escape set, so always quoting
    // sidesteps every "yes"/"no"/"1e3"/":"-style misparse.
    return "\"" + json_escape(text) + "\"";
}

std::string html_escape(const std::string &text)
{
    std::string out;
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

std::string markdown_cell(const std::string &text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '|') out += "\\|";
        else if (c == '\r') { /* CRLF or lone CR: collapse into the LF handling */ if (i + 1 < text.size() && text[i + 1] == '\n') continue; out += "<br>"; }
        else if (c == '\n') out += "<br>";
        else out += c;
    }
    return out;
}

std::string apply_line_ending(const std::string &lf_text, LineEnding le)
{
    if (le == LineEnding::LF) return lf_text;
    std::string out;
    out.reserve(lf_text.size() + lf_text.size() / 16);
    for (char c : lf_text) {
        if (c == '\n') out += "\r\n";
        else out += c;
    }
    return out;
}

// ===========================================================================
// Loss report
// ===========================================================================

LossReport compute_loss_report(const Dataset &d, Format f)
{
    LossReport r;
    const std::size_t nulls      = count_nulls(d);
    const std::size_t non_finite = count_non_finite(d);

    auto lose = [&r](std::string s) { r.losses.push_back(std::move(s)); r.lossless = false; };
    auto note = [&r](std::string s) { r.notes.push_back(std::move(s)); };

    switch (f) {
    case Format::JSON:
    case Format::JSONL:
        if (non_finite > 0)
            lose(std::to_string(non_finite) + " non-finite number(s) (NaN/Infinity) have no JSON form and are written as null.");
        break;
    case Format::YAML:
        if (non_finite > 0) note("Non-finite numbers are written as .nan/.inf.");
        break;
    case Format::TOML:
        if (nulls > 0)
            lose(std::to_string(nulls) + " empty (null) value(s) have no TOML form and are omitted from their tables.");
        if (non_finite > 0) note("Non-finite numbers are written as nan/inf.");
        if (d.kind == DatasetKind::Structured && d.root.type != Value::Type::Object && d.root.type != Value::Type::Array)
            note("A scalar root is written under the key \"data\".");
        break;
    case Format::XML: {
        std::size_t controls = 0;
        auto check = [&controls](const Value &v) {
            if (v.type != Value::Type::String) return;
            for (unsigned char c : v.string)
                if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') ++controls;
        };
        if (d.kind == DatasetKind::Tabular) {
            for (const auto &row : d.rows)
                for (const auto &cell : row) check(cell);
        } else if (d.kind == DatasetKind::Structured) {
            walk(d.root, check);
        } else {
            check(Value::from_string(d.prose));
        }
        if (controls > 0) lose(std::to_string(controls) + " control character(s) are illegal in XML 1.0 and are dropped.");
        break;
    }
    case Format::CSV:
    case Format::TSV:
        if (d.kind == DatasetKind::Tabular) {
            note("Numbers and booleans become text; the .meta.json sidecar records each column's type.");
            if (nulls > 0) note(std::to_string(nulls) + " empty (null) cell(s) are written as empty fields, the same as empty text.");
            if (f == Format::TSV && count_multiline_cells(d) > 0)
                note("Line breaks and tabs inside cells are escaped as \\n and \\t.");
        } else if (d.kind == DatasetKind::Structured) {
            note("Reshaped: the tree is flattened to path/type/value rows (JSON-pointer paths), so nesting is reconstructable.");
            if (nulls > 0) note(std::to_string(nulls) + " null value(s) keep a row with type \"null\" and an empty value.");
        } else {
            note("Written as one row with the columns title and text.");
        }
        break;
    case Format::Markdown:
    case Format::HTML:
        if (d.kind == DatasetKind::Tabular) {
            note("Numbers and booleans become text; the header comment records each column's type.");
            if (count_multiline_cells(d) > 0) note("Line breaks inside cells are rendered as <br>.");
            if (nulls > 0) note(std::to_string(nulls) + " empty (null) cell(s) render as empty cells.");
        } else if (d.kind == DatasetKind::Structured) {
            lose("Value types are not preserved: numbers, booleans, null and text all render as text in a nested list.");
            std::size_t empties = 0;
            walk(d.root, [&empties](const Value &v) { if (v.is_container() && v.array.empty() && v.object.empty()) ++empties; });
            if (empties > 0) note(std::to_string(empties) + " empty array(s)/object(s) render as \"(empty)\".");
        }
        break;
    }
    return r;
}

// ===========================================================================
// JSON / JSONL
// ===========================================================================

namespace {

void emit_json(const Value &v, std::string &out, int indent, bool pretty)
{
    const std::string nl = pretty ? "\n" : "";
    switch (v.type) {
    case Value::Type::Null: out += "null"; break;
    case Value::Type::Bool: out += v.boolean ? "true" : "false"; break;
    case Value::Type::Integer: out += std::to_string(v.integer); break;
    case Value::Type::Number: out += std::isfinite(v.number) ? format_number(v.number) : "null"; break;
    case Value::Type::String: out += "\"" + json_escape(v.string) + "\""; break;
    case Value::Type::Array:
        if (v.array.empty()) { out += "[]"; break; }
        out += "[" + nl;
        for (std::size_t i = 0; i < v.array.size(); ++i) {
            if (pretty) out += indent_of(indent + 1);
            emit_json(v.array[i], out, indent + 1, pretty);
            if (i + 1 < v.array.size()) out += ",";
            out += nl;
        }
        if (pretty) out += indent_of(indent);
        out += "]";
        break;
    case Value::Type::Object:
        if (v.object.empty()) { out += "{}"; break; }
        out += "{" + nl;
        for (std::size_t i = 0; i < v.object.size(); ++i) {
            if (pretty) out += indent_of(indent + 1);
            out += "\"" + json_escape(v.object[i].first) + "\":" + (pretty ? " " : "");
            emit_json(v.object[i].second, out, indent + 1, pretty);
            if (i + 1 < v.object.size()) out += ",";
            out += nl;
        }
        if (pretty) out += indent_of(indent);
        out += "}";
        break;
    }
}

std::string serialize_json(const Dataset &d, const SerializeOptions &o)
{
    std::string out;
    if (o.include_schema_header) {
        Value envelope = header_value(d, o);
        envelope.set("data", data_value(d));
        emit_json(envelope, out, 0, true);
    } else {
        emit_json(data_value(d), out, 0, true);
    }
    out += "\n";
    return out;
}

std::string serialize_jsonl(const Dataset &d, const SerializeOptions &o)
{
    std::string out;
    auto line = [&out](const Value &v) { emit_json(v, out, 0, false); out += "\n"; };
    if (o.include_schema_header) {
        Value h = header_value(d, o);
        h.object.insert(h.object.begin(), std::make_pair(std::string("_type"), Value::from_string("header")));
        line(h);
    }
    switch (d.kind) {
    case DatasetKind::Tabular:
        for (const auto &row : d.rows) line(row_object(d, row));
        break;
    case DatasetKind::Structured:
        if (d.root.type == Value::Type::Array) {
            for (const Value &item : d.root.array) line(item);
        } else {
            line(d.root);
        }
        break;
    case DatasetKind::Prose: line(prose_object(d)); break;
    }
    return out;
}

// ===========================================================================
// YAML
// ===========================================================================

std::string yaml_key(const std::string &k) { return is_bare_key(k) ? k : yaml_quote(k); }

std::string yaml_scalar(const Value &v)
{
    switch (v.type) {
    case Value::Type::Null: return "null";
    case Value::Type::Bool: return v.boolean ? "true" : "false";
    case Value::Type::Integer: return std::to_string(v.integer);
    case Value::Type::Number:
        if (std::isnan(v.number)) return ".nan";
        if (std::isinf(v.number)) return v.number > 0 ? ".inf" : "-.inf";
        return format_number(v.number);
    case Value::Type::String: return yaml_quote(v.string);
    default: return "";
    }
}

// Emits `v` where the caller has already written "key:" or "- " on the current
// line; `indent` is the indentation of child lines.
void emit_yaml(const Value &v, std::string &out, int indent)
{
    if (v.is_scalar()) { out += " " + yaml_scalar(v) + "\n"; return; }
    if (v.type == Value::Type::Array) {
        if (v.array.empty()) { out += " []\n"; return; }
        out += "\n";
        for (const Value &item : v.array) {
            out += indent_of(indent) + "-";
            emit_yaml(item, out, indent + 1);
        }
        return;
    }
    if (v.object.empty()) { out += " {}\n"; return; }
    out += "\n";
    for (const auto &kv : v.object) {
        out += indent_of(indent) + yaml_key(kv.first) + ":";
        emit_yaml(kv.second, out, indent + 1);
    }
}

std::string serialize_yaml(const Dataset &d, const SerializeOptions &o)
{
    std::string out;
    if (o.include_schema_header) {
        out += "# " + header_comment_line(d, o) + "\n";
        out += "%YAML 1.2\n---\n";
        Value h = header_value(d, o);
        for (const auto &kv : h.object) {
            out += yaml_key(kv.first) + ":";
            emit_yaml(kv.second, out, 1);
        }
        out += "data:";
        emit_yaml(data_value(d), out, 1);
    } else {
        Value data = data_value(d);
        if (data.is_scalar()) {
            out += yaml_scalar(data) + "\n";
        } else {
            // Top-level containers start at column 0.
            std::string body;
            emit_yaml(data, body, 0);
            if (!body.empty() && body.front() == '\n') body.erase(0, 1);
            out += body;
        }
    }
    return out;
}

// ===========================================================================
// TOML
// ===========================================================================

std::string toml_key(const std::string &k) { return is_bare_key(k) ? k : toml_quote(k); }

bool toml_all_objects(const Value &arr)
{
    if (arr.array.empty()) return false;
    for (const Value &item : arr.array)
        if (item.type != Value::Type::Object) return false;
    return true;
}

void emit_toml_inline(const Value &v, std::string &out);

void emit_toml_inline_table(const Value &obj, std::string &out)
{
    out += "{";
    bool first = true;
    for (const auto &kv : obj.object) {
        if (kv.second.is_null()) continue; // no TOML null: reported by the loss report
        if (!first) out += ", ";
        first = false;
        out += toml_key(kv.first) + " = ";
        emit_toml_inline(kv.second, out);
    }
    out += "}";
}

void emit_toml_inline(const Value &v, std::string &out)
{
    switch (v.type) {
    case Value::Type::Null: out += "\"\""; break; // only reachable inside arrays; the report names it
    case Value::Type::Bool: out += v.boolean ? "true" : "false"; break;
    case Value::Type::Integer: out += std::to_string(v.integer); break;
    case Value::Type::Number:
        if (std::isnan(v.number)) out += "nan";
        else if (std::isinf(v.number)) out += v.number > 0 ? "inf" : "-inf";
        else out += format_number(v.number);
        break;
    case Value::Type::String: out += toml_quote(v.string); break;
    case Value::Type::Array:
        out += "[";
        for (std::size_t i = 0; i < v.array.size(); ++i) {
            if (i) out += ", ";
            emit_toml_inline(v.array[i], out);
        }
        out += "]";
        break;
    case Value::Type::Object: emit_toml_inline_table(v, out); break;
    }
}

// Emit an object as a TOML table body: scalars and inline arrays first, then
// sub-tables and arrays-of-tables with dotted headers.
void emit_toml_table(const Value &obj, const std::string &path, std::string &out)
{
    for (const auto &kv : obj.object) {
        const Value &v = kv.second;
        if (v.is_null()) continue;
        if (v.type == Value::Type::Object) continue;
        if (v.type == Value::Type::Array && toml_all_objects(v)) continue;
        out += toml_key(kv.first) + " = ";
        emit_toml_inline(v, out);
        out += "\n";
    }
    for (const auto &kv : obj.object) {
        const Value &v = kv.second;
        const std::string child = path.empty() ? toml_key(kv.first) : path + "." + toml_key(kv.first);
        if (v.type == Value::Type::Object) {
            out += "\n[" + child + "]\n";
            emit_toml_table(v, child, out);
        } else if (v.type == Value::Type::Array && toml_all_objects(v)) {
            for (const Value &item : v.array) {
                out += "\n[[" + child + "]]\n";
                emit_toml_table(item, child, out);
            }
        }
    }
}

std::string serialize_toml(const Dataset &d, const SerializeOptions &o)
{
    std::string out;
    if (o.include_schema_header) {
        out += "# " + header_comment_line(d, o) + "\n";
        Value h = header_value(d, o);
        emit_toml_table(h, "", out);
    }
    Value data = data_value(d);
    if (data.type == Value::Type::Object) {
        out += "\n[data]\n";
        emit_toml_table(data, "data", out);
    } else if (data.type == Value::Type::Array && toml_all_objects(data)) {
        for (const Value &item : data.array) {
            out += "\n[[data]]\n";
            emit_toml_table(item, "data", out);
        }
    } else if (data.type == Value::Type::Array && data.array.empty()) {
        out += "\ndata = []\n";
    } else if (!data.is_null()) {
        out += "\ndata = ";
        emit_toml_inline(data, out);
        out += "\n";
    }
    return out;
}

// ===========================================================================
// XML
// ===========================================================================

void emit_xml_value(const Value &v, const std::string *key, std::string &out, int indent)
{
    out += indent_of(indent) + "<item";
    if (key != nullptr) out += " key=\"" + xml_escape(*key) + "\"";
    out += std::string(" type=\"") + type_name(v.type) + "\"";
    if (v.is_null()) { out += "/>\n"; return; }
    if (v.is_scalar()) { out += ">" + xml_escape(scalar_text(v)) + "</item>\n"; return; }
    if (v.array.empty() && v.object.empty()) { out += "/>\n"; return; }
    out += ">\n";
    for (const Value &child : v.array) emit_xml_value(child, nullptr, out, indent + 1);
    for (const auto &kv : v.object) emit_xml_value(kv.second, &kv.first, out, indent + 1);
    out += indent_of(indent) + "</item>\n";
}

std::string serialize_xml(const Dataset &d, const SerializeOptions &o)
{
    std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out += "<export";
    if (o.include_schema_header) {
        out += " schema=\"" + xml_escape(d.schema_id) + "\" schemaVersion=\"" + std::to_string(d.schema_version) + "\"";
        out += " dataset=\"" + xml_escape(d.name) + "\" kind=\"" + kind_name(d.kind) + "\"";
        out += " encoding=\"UTF-8\" lineEnding=\"" + std::string(line_ending_name(o.line_ending)) + "\"";
        out += " generator=\"" + xml_escape(o.generator) + "\"";
    }
    out += ">\n";
    switch (d.kind) {
    case DatasetKind::Tabular:
        out += "  <columns>\n";
        for (const Column &c : d.columns)
            out += "    <column name=\"" + xml_escape(c.name) + "\" type=\"" + type_name(c.type) + "\"/>\n";
        out += "  </columns>\n  <rows>\n";
        for (const auto &row : d.rows) {
            out += "    <row>\n";
            for (std::size_t i = 0; i < d.columns.size(); ++i) {
                const Value &cell = i < row.size() ? row[i] : Value();
                out += "      <field name=\"" + xml_escape(d.columns[i].name) + "\" type=\"" + type_name(cell.type) + "\"";
                if (cell.is_null()) out += "/>\n";
                else out += ">" + xml_escape(scalar_text(cell)) + "</field>\n";
            }
            out += "    </row>\n";
        }
        out += "  </rows>\n";
        break;
    case DatasetKind::Structured:
        out += "  <data>\n";
        emit_xml_value(d.root, nullptr, out, 2);
        out += "  </data>\n";
        break;
    case DatasetKind::Prose:
        out += "  <data>\n    <title>" + xml_escape(d.prose_title) + "</title>\n    <text>" + xml_escape(d.prose) +
               "</text>\n  </data>\n";
        break;
    }
    out += "</export>\n";
    return out;
}

// ===========================================================================
// CSV / TSV
// ===========================================================================

std::string delimited_cell(const std::string &text, Format f)
{
    return f == Format::CSV ? csv_quote(text, ',') : tsv_escape(text);
}

void flatten(const Value &v, const std::string &path, std::vector<std::vector<Value>> &rows)
{
    if (v.is_scalar()) {
        rows.push_back({Value::from_string(path), Value::from_string(type_name(v.type)), Value::from_string(scalar_text(v))});
        return;
    }
    // A container row records its type so an empty container survives.
    rows.push_back({Value::from_string(path), Value::from_string(type_name(v.type)), Value::from_string("")});
    for (std::size_t i = 0; i < v.array.size(); ++i) flatten(v.array[i], path + "/" + std::to_string(i), rows);
    for (const auto &kv : v.object) flatten(kv.second, path + "/" + pointer_segment(kv.first), rows);
}

Serialized serialize_delimited(const Dataset &d, Format f, const SerializeOptions &o)
{
    const char sep = f == Format::CSV ? ',' : '\t';
    std::vector<std::string>        columns;
    std::vector<std::vector<Value>> rows;
    switch (d.kind) {
    case DatasetKind::Tabular:
        for (const Column &c : d.columns) columns.push_back(c.name);
        rows = d.rows;
        break;
    case DatasetKind::Structured:
        columns = {"path", "type", "value"};
        flatten(d.root, "", rows);
        break;
    case DatasetKind::Prose:
        columns = {"title", "text"};
        rows.push_back({Value::from_string(d.prose_title), Value::from_string(d.prose)});
        break;
    }

    Serialized s;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i) s.body += sep;
        s.body += delimited_cell(columns[i], f);
    }
    s.body += "\n";
    for (const auto &row : rows) {
        for (std::size_t i = 0; i < columns.size(); ++i) {
            if (i) s.body += sep;
            s.body += delimited_cell(i < row.size() ? scalar_text(row[i]) : std::string(), f);
        }
        s.body += "\n";
    }
    if (o.include_schema_header) {
        Value h = header_value(d, o);
        h.set("format", Value::from_string(format_name(f)));
        h.set("separator", Value::from_string(f == Format::CSV ? "," : "\\t"));
        h.set("quoting", Value::from_string(f == Format::CSV ? "RFC 4180" : "backslash escapes for \\t \\n \\r \\\\"));
        if (d.kind != DatasetKind::Tabular) {
            Value cols = Value::make_array();
            for (const std::string &c : columns) {
                Value col = Value::make_object();
                col.set("name", Value::from_string(c));
                col.set("type", Value::from_string("string"));
                cols.push(std::move(col));
            }
            h.set("columns", std::move(cols));
        }
        std::string meta;
        emit_json(h, meta, 0, true);
        s.sidecar_name = std::string("meta.json");
        s.sidecar_body = meta + "\n";
    }
    return s;
}

// ===========================================================================
// Markdown / HTML
// ===========================================================================

void emit_markdown_tree(const Value &v, std::string &out, int indent)
{
    auto leaf = [](const Value &x) -> std::string {
        if (x.is_null()) return "_(empty)_";
        if (x.type == Value::Type::String) return markdown_cell(x.string);
        return "`" + scalar_text(x) + "`";
    };
    if (v.type == Value::Type::Array) {
        if (v.array.empty()) { out += indent_of(indent) + "- _(empty)_\n"; return; }
        for (const Value &item : v.array) {
            if (item.is_scalar()) out += indent_of(indent) + "- " + leaf(item) + "\n";
            else { out += indent_of(indent) + "-\n"; emit_markdown_tree(item, out, indent + 1); }
        }
        return;
    }
    if (v.object.empty()) { out += indent_of(indent) + "- _(empty)_\n"; return; }
    for (const auto &kv : v.object) {
        if (kv.second.is_scalar()) {
            out += indent_of(indent) + "- **" + markdown_cell(kv.first) + "**: " + leaf(kv.second) + "\n";
        } else {
            out += indent_of(indent) + "- **" + markdown_cell(kv.first) + "**:\n";
            emit_markdown_tree(kv.second, out, indent + 1);
        }
    }
}

std::string serialize_markdown(const Dataset &d, const SerializeOptions &o)
{
    std::string out;
    if (o.include_schema_header) out += "<!-- " + header_comment_line(d, o) + " -->\n\n";
    switch (d.kind) {
    case DatasetKind::Tabular: {
        out += "# " + d.name + "\n\n";
        out += "|";
        for (const Column &c : d.columns) out += " " + markdown_cell(c.name) + " |";
        out += "\n|";
        for (std::size_t i = 0; i < d.columns.size(); ++i) out += " --- |";
        out += "\n";
        for (const auto &row : d.rows) {
            out += "|";
            for (std::size_t i = 0; i < d.columns.size(); ++i)
                out += " " + markdown_cell(i < row.size() ? scalar_text(row[i]) : std::string()) + " |";
            out += "\n";
        }
        break;
    }
    case DatasetKind::Structured:
        out += "# " + d.name + "\n\n";
        if (d.root.is_scalar()) out += scalar_text(d.root) + "\n";
        else emit_markdown_tree(d.root, out, 0);
        break;
    case DatasetKind::Prose:
        if (!d.prose_title.empty()) out += "# " + d.prose_title + "\n\n";
        out += d.prose;
        if (out.empty() || out.back() != '\n') out += "\n";
        break;
    }
    return out;
}

void emit_html_tree(const Value &v, std::string &out, int indent)
{
    auto leaf = [](const Value &x) -> std::string {
        if (x.is_null()) return "<em>(empty)</em>";
        return html_escape(scalar_text(x));
    };
    if (v.type == Value::Type::Array) {
        if (v.array.empty()) { out += indent_of(indent) + "<em>(empty)</em>\n"; return; }
        out += indent_of(indent) + "<ol start=\"0\">\n";
        for (const Value &item : v.array) {
            if (item.is_scalar()) out += indent_of(indent + 1) + "<li>" + leaf(item) + "</li>\n";
            else { out += indent_of(indent + 1) + "<li>\n"; emit_html_tree(item, out, indent + 2); out += indent_of(indent + 1) + "</li>\n"; }
        }
        out += indent_of(indent) + "</ol>\n";
        return;
    }
    if (v.object.empty()) { out += indent_of(indent) + "<em>(empty)</em>\n"; return; }
    out += indent_of(indent) + "<dl>\n";
    for (const auto &kv : v.object) {
        out += indent_of(indent + 1) + "<dt>" + html_escape(kv.first) + "</dt>\n";
        if (kv.second.is_scalar()) out += indent_of(indent + 1) + "<dd>" + leaf(kv.second) + "</dd>\n";
        else { out += indent_of(indent + 1) + "<dd>\n"; emit_html_tree(kv.second, out, indent + 2); out += indent_of(indent + 1) + "</dd>\n"; }
    }
    out += indent_of(indent) + "</dl>\n";
}

std::string serialize_html(const Dataset &d, const SerializeOptions &o)
{
    std::string out = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n";
    if (o.include_schema_header) {
        out += "<meta name=\"schema\" content=\"" + html_escape(d.schema_id) + "\">\n";
        out += "<meta name=\"schemaVersion\" content=\"" + std::to_string(d.schema_version) + "\">\n";
        out += "<meta name=\"dataset\" content=\"" + html_escape(d.name) + "\">\n";
        out += std::string("<meta name=\"kind\" content=\"") + kind_name(d.kind) + "\">\n";
        out += std::string("<meta name=\"lineEnding\" content=\"") + line_ending_name(o.line_ending) + "\">\n";
        out += "<meta name=\"generator\" content=\"" + html_escape(o.generator) + "\">\n";
        if (d.kind == DatasetKind::Tabular) {
            std::string cols;
            for (std::size_t i = 0; i < d.columns.size(); ++i) {
                if (i) cols += ",";
                cols += d.columns[i].name + ":" + type_name(d.columns[i].type);
            }
            out += "<meta name=\"columns\" content=\"" + html_escape(cols) + "\">\n";
        }
    }
    const std::string title = d.kind == DatasetKind::Prose && !d.prose_title.empty() ? d.prose_title : d.name;
    out += "<title>" + html_escape(title) + "</title>\n</head>\n<body>\n<h1>" + html_escape(title) + "</h1>\n";
    switch (d.kind) {
    case DatasetKind::Tabular:
        out += "<table>\n<thead><tr>";
        for (const Column &c : d.columns) out += "<th>" + html_escape(c.name) + "</th>";
        out += "</tr></thead>\n<tbody>\n";
        for (const auto &row : d.rows) {
            out += "<tr>";
            for (std::size_t i = 0; i < d.columns.size(); ++i) {
                std::string cell = html_escape(i < row.size() ? scalar_text(row[i]) : std::string());
                std::string with_breaks;
                for (char c : cell) { if (c == '\n') with_breaks += "<br>"; else if (c != '\r') with_breaks += c; }
                out += "<td>" + with_breaks + "</td>";
            }
            out += "</tr>\n";
        }
        out += "</tbody>\n</table>\n";
        break;
    case DatasetKind::Structured:
        if (d.root.is_scalar()) out += "<p>" + html_escape(scalar_text(d.root)) + "</p>\n";
        else emit_html_tree(d.root, out, 0);
        break;
    case DatasetKind::Prose: {
        // Paragraphs split on blank lines; single line breaks become <br>.
        std::string paragraph;
        auto flush = [&]() {
            if (paragraph.empty()) return;
            out += "<p>" + paragraph + "</p>\n";
            paragraph.clear();
        };
        std::istringstream in(d.prose);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) { flush(); continue; }
            if (!paragraph.empty()) paragraph += "<br>\n";
            paragraph += html_escape(line);
        }
        flush();
        break;
    }
    }
    out += "</body>\n</html>\n";
    return out;
}

} // namespace

Serialized serialize(const Dataset &d, Format f, const SerializeOptions &o)
{
    Serialized s;
    switch (f) {
    case Format::JSON: s.body = serialize_json(d, o); break;
    case Format::JSONL: s.body = serialize_jsonl(d, o); break;
    case Format::YAML: s.body = serialize_yaml(d, o); break;
    case Format::TOML: s.body = serialize_toml(d, o); break;
    case Format::XML: s.body = serialize_xml(d, o); break;
    case Format::CSV:
    case Format::TSV: s = serialize_delimited(d, f, o); break;
    case Format::Markdown: s.body = serialize_markdown(d, o); break;
    case Format::HTML: s.body = serialize_html(d, o); break;
    }
    s.body = apply_line_ending(s.body, o.line_ending);
    if (s.sidecar_name) s.sidecar_body = apply_line_ending(s.sidecar_body, o.line_ending);
    if (o.write_bom) s.body.insert(0, "\xEF\xBB\xBF");
    return s;
}

// ===========================================================================
// Archives
// ===========================================================================

std::optional<std::string> sanitize_archive_path(const std::string &candidate)
{
    std::string p = candidate;
    std::replace(p.begin(), p.end(), '\\', '/');
    // Drive letter or UNC.
    if (p.size() >= 2 && p[1] == ':') return std::nullopt;
    if (p.rfind("//", 0) == 0) return std::nullopt;
    while (p.rfind("./", 0) == 0) p.erase(0, 2);
    while (!p.empty() && p.front() == '/') p.erase(0, 1);
    if (p.empty()) return std::nullopt;
    // Reject any ".." segment and any embedded NUL / control character.
    std::string segment;
    for (std::size_t i = 0; i <= p.size(); ++i) {
        if (i == p.size() || p[i] == '/') {
            if (segment == ".." || segment.empty()) return std::nullopt;
            segment.clear();
        } else {
            if (static_cast<unsigned char>(p[i]) < 0x20) return std::nullopt;
            segment += p[i];
        }
    }
    return p;
}

ArchiveResult write_zip(const fs::path &archive_path, const std::vector<ArchiveEntry> &entries)
{
    ArchiveResult r;
    r.archive_path = archive_path;
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof zip);
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        r.error = "miniz: could not initialise the ZIP writer";
        return r;
    }
    for (const ArchiveEntry &e : entries) {
        std::optional<std::string> name = sanitize_archive_path(e.relative_path);
        if (!name) {
            mz_zip_writer_end(&zip);
            r.error = "Refused unsafe archive member path: " + e.relative_path;
            return r;
        }
        if (!mz_zip_writer_add_mem(&zip, name->c_str(), e.data.data(), e.data.size(), MZ_DEFAULT_LEVEL)) {
            mz_zip_writer_end(&zip);
            r.error = "miniz: could not add " + *name;
            return r;
        }
    }
    void  *buffer = nullptr;
    size_t size   = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size)) {
        mz_zip_writer_end(&zip);
        r.error = "miniz: could not finalise the archive";
        return r;
    }
    std::ofstream out(archive_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        mz_zip_writer_end(&zip);
        r.error = "Cannot write " + archive_path.string();
        return r;
    }
    out.write(static_cast<const char *>(buffer), static_cast<std::streamsize>(size));
    out.close();
    mz_zip_writer_end(&zip); // also frees `buffer`
    if (!out) { r.error = "Write failed for " + archive_path.string(); return r; }
    r.ok = true;
    return r;
}

std::vector<std::string> seven_zip_switches(const ArchiveOptions &o, bool redact_password)
{
    std::vector<std::string> sw;
    sw.push_back("-t7z");
    sw.push_back("-y");
    sw.push_back("-bso0");
    sw.push_back("-bsp0");
    sw.push_back(std::string("-m0=") + seven_zip_method_switch(o.method));
    sw.push_back("-mx=" + std::to_string(static_cast<int>(o.level)));
    const bool lzma = o.method == SevenZipMethod::LZMA || o.method == SevenZipMethod::LZMA2;
    if (o.dictionary_mib > 0) {
        if (lzma) sw.push_back("-md=" + std::to_string(o.dictionary_mib) + "m");
        else if (o.method == SevenZipMethod::PPMd) sw.push_back("-mmem=" + std::to_string(o.dictionary_mib) + "m");
        // BZip2 / Deflate have no dictionary switch.
    }
    if (o.word_size > 0) {
        if (lzma || o.method == SevenZipMethod::Deflate) sw.push_back("-mfb=" + std::to_string(o.word_size));
        else if (o.method == SevenZipMethod::PPMd) sw.push_back("-mo=" + std::to_string(o.word_size));
    }
    if (!o.solid) sw.push_back("-ms=off");
    else if (o.solid_block_mib > 0) sw.push_back("-ms=" + std::to_string(o.solid_block_mib) + "m");
    else sw.push_back("-ms=on");
    sw.push_back(o.threads > 0 ? "-mmt=" + std::to_string(o.threads) : std::string("-mmt=on"));
    if (!o.split_volume.empty()) sw.push_back("-v" + o.split_volume);
    if (!o.password.empty()) {
        sw.push_back("-p" + (redact_password ? std::string("***") : o.password));
        sw.push_back(o.encrypt_headers ? "-mhe=on" : "-mhe=off");
    }
    return sw;
}

bool seven_zip_filenames_visible(const ArchiveOptions &o) { return !o.password.empty() && !o.encrypt_headers; }

std::vector<std::string> seven_zip_cost_hints(const ArchiveOptions &o)
{
    std::vector<std::string> hints;
    if (o.format != ArchiveFormat::SevenZip) return hints;
    unsigned default_dict = 16;
    switch (o.level) {
    case SevenZipLevel::Store: default_dict = 0; break;
    case SevenZipLevel::Fastest: default_dict = 1; break;
    case SevenZipLevel::Fast: default_dict = 4; break;
    case SevenZipLevel::Normal: default_dict = 16; break;
    case SevenZipLevel::Maximum: default_dict = 32; break;
    case SevenZipLevel::Ultra: default_dict = 64; break;
    }
    const unsigned dict = o.dictionary_mib > 0 ? o.dictionary_mib : default_dict;
    const unsigned threads = o.threads > 0 ? o.threads : 2;
    if (o.level == SevenZipLevel::Store) {
        hints.push_back("Store: no compression, fastest, the archive is as large as its contents.");
    } else if (o.method == SevenZipMethod::LZMA2 || o.method == SevenZipMethod::LZMA) {
        // LZMA needs roughly 10-11 times the dictionary to compress; decompression needs the dictionary itself.
        hints.push_back("Dictionary " + std::to_string(dict) + " MiB: about " + std::to_string(dict * 11 * (o.method == SevenZipMethod::LZMA2 ? threads : 1)) +
                        " MiB RAM to compress, " + std::to_string(dict) + " MiB to extract.");
        if (o.method == SevenZipMethod::LZMA) hints.push_back("LZMA compresses on one thread; LZMA2 uses all of them.");
    } else if (o.method == SevenZipMethod::PPMd) {
        hints.push_back("PPMd: best for text, slow and single-threaded; uses " + std::to_string(dict) + " MiB RAM both to compress and to extract.");
    } else if (o.method == SevenZipMethod::BZip2) {
        hints.push_back("BZip2: moderate ratio, multi-threaded, about 10 MiB RAM; no dictionary setting.");
    } else {
        hints.push_back("Deflate: the ZIP method, widest compatibility, weakest ratio; no dictionary setting.");
    }
    if (o.solid) hints.push_back("Solid: better ratio, but extracting one file decompresses its whole block.");
    else hints.push_back("Non-solid: any single file extracts alone at the cost of a larger archive.");
    if (!o.split_volume.empty()) hints.push_back("Split volumes of " + o.split_volume + ": every part is needed to extract.");
    if (!o.password.empty()) {
        if (o.encrypt_headers) hints.push_back("AES-256 with encrypted headers: file names are hidden; listing needs the password.");
        else hints.push_back("Warning: content is encrypted but the file names are visible to anyone.");
    }
    return hints;
}

namespace {

std::vector<fs::path> path_entries()
{
    std::vector<fs::path> dirs;
    const char *path = std::getenv("PATH");
    if (path == nullptr) return dirs;
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    std::string current;
    for (const char *p = path;; ++p) {
        if (*p == sep || *p == '\0') {
            if (!current.empty()) dirs.emplace_back(current);
            current.clear();
            if (*p == '\0') break;
        } else {
            current += *p;
        }
    }
    return dirs;
}

fs::path env_path(const char *name)
{
    const char *v = std::getenv(name);
    return v != nullptr ? fs::path(v) : fs::path();
}

std::string quote_argument(const std::string &arg)
{
#ifdef _WIN32
    // CommandLineToArgvW rules: quote when needed, double backslashes before a quote.
    if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos) return arg;
    std::string out = "\"";
    unsigned    backslashes = 0;
    for (char c : arg) {
        if (c == '\\') { ++backslashes; continue; }
        if (c == '"') { out.append(backslashes * 2 + 1, '\\'); out += '"'; backslashes = 0; continue; }
        out.append(backslashes, '\\');
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, '\\');
    out += '"';
    return out;
#else
    std::string out = "'";
    for (char c : arg) { if (c == '\'') out += "'\\''"; else out += c; }
    out += "'";
    return out;
#endif
}

std::string join_command(const std::vector<std::string> &argv)
{
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        s += quote_argument(argv[i]);
    }
    return s;
}

#ifdef _WIN32
std::wstring widen(const std::string &utf8)
{
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
    return w;
}
#endif

// Run argv with the given working directory, hidden, and return the exit code
// (or -1 when the process could not be started).
int run_process(const std::vector<std::string> &argv, const fs::path &working_dir, std::string &error)
{
#ifdef _WIN32
    std::wstring cmd = widen(join_command(argv));
    STARTUPINFOW        si{};
    PROCESS_INFORMATION pi{};
    si.cb          = sizeof si;
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::wstring app = widen(argv.front());
    std::wstring cwd = working_dir.wstring();
    if (!CreateProcessW(app.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi)) {
        error = "CreateProcess failed with error " + std::to_string(GetLastError());
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(code);
#else
    std::string cmd = "cd " + quote_argument(working_dir.string()) + " && " + join_command(argv) + " >/dev/null 2>&1";
    const int   rc  = std::system(cmd.c_str());
    if (rc == -1) { error = "system() failed"; return -1; }
    return WEXITSTATUS(rc);
#endif
}

fs::path make_staging_dir(std::string &error)
{
    std::random_device rd;
    std::mt19937_64    gen(rd() ^ static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    for (int attempt = 0; attempt < 8; ++attempt) {
        char name[64];
        std::snprintf(name, sizeof name, "bambustudio-export-%016llx", static_cast<unsigned long long>(gen()));
        std::error_code ec;
        fs::path        dir = fs::temp_directory_path(ec) / name;
        if (ec) { error = "No temporary directory: " + ec.message(); return {}; }
        if (fs::create_directories(dir, ec) && !ec) return dir;
    }
    error = "Could not create a staging directory";
    return {};
}

const char *seven_zip_exit_text(int code)
{
    switch (code) {
    case 0: return "ok";
    case 1: return "warning (some files were locked or missing)";
    case 2: return "fatal error";
    case 7: return "command line error";
    case 8: return "not enough memory";
    case 255: return "user stopped the process";
    default: return "unknown exit code";
    }
}

} // namespace

SevenZipLocation find_seven_zip(const fs::path &override)
{
    SevenZipLocation loc;
#ifdef _WIN32
    const std::vector<std::string> names{"7z.exe", "7za.exe"};
#else
    const std::vector<std::string> names{"7z", "7za", "7zz"};
#endif
    std::vector<fs::path> candidates;
    if (!override.empty()) candidates.push_back(override);
    for (const fs::path &dir : path_entries())
        for (const std::string &n : names) candidates.push_back(dir / n);
    for (const char *env : {"ProgramFiles", "ProgramW6432", "ProgramFiles(x86)"}) {
        fs::path base = env_path(env);
        if (!base.empty())
            for (const std::string &n : names) candidates.push_back(base / "7-Zip" / n);
    }
    {
        fs::path local = env_path("LOCALAPPDATA");
        if (!local.empty())
            for (const std::string &n : names) candidates.push_back(local / "Programs" / "7-Zip" / n);
    }
#ifndef _WIN32
    for (const char *dir : {"/usr/bin", "/usr/local/bin", "/opt/homebrew/bin"})
        for (const std::string &n : names) candidates.push_back(fs::path(dir) / n);
#endif
    std::string searched;
    for (const fs::path &c : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(c, ec)) {
            loc.found      = true;
            loc.executable = c;
            break;
        }
        fs::path dir = c.parent_path();
        if (searched.find(dir.string()) == std::string::npos) {
            if (!searched.empty()) searched += "; ";
            searched += dir.string();
        }
    }
    loc.searched = searched;
    return loc;
}

ArchiveResult write_seven_zip(const fs::path &archive_path, const std::vector<ArchiveEntry> &entries,
                              const ArchiveOptions &o, const SevenZipLocation &seven_zip)
{
    ArchiveResult r;
    r.archive_path = archive_path;
    if (!seven_zip.found) {
        r.error = "7-Zip not found. Searched: " + seven_zip.searched;
        return r;
    }
    std::string err;
    fs::path    staging = make_staging_dir(err);
    if (staging.empty()) { r.error = err; return r; }

    struct Cleanup
    {
        fs::path dir;
        ~Cleanup() { std::error_code ec; fs::remove_all(dir, ec); }
    } cleanup{staging};

    for (const ArchiveEntry &e : entries) {
        std::optional<std::string> name = sanitize_archive_path(e.relative_path);
        if (!name) { r.error = "Refused unsafe archive member path: " + e.relative_path; return r; }
        fs::path        target = staging / fs::path(*name);
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) { r.error = "Cannot stage " + *name; return r; }
        out.write(e.data.data(), static_cast<std::streamsize>(e.data.size()));
        if (!out) { r.error = "Write failed while staging " + *name; return r; }
    }

    // 7-Zip appends volume suffixes itself (.7z.001) and refuses to overwrite a
    // stale single-file archive with a split set, so clear the target first.
    {
        std::error_code ec;
        fs::remove(archive_path, ec);
    }

    std::vector<std::string> argv{seven_zip.executable.string(), "a"};
    for (const std::string &s : seven_zip_switches(o, false)) argv.push_back(s);
    argv.push_back("-r");
    argv.push_back(fs::absolute(archive_path).string());
    argv.push_back("*");

    std::vector<std::string> shown{seven_zip.executable.string(), "a"};
    for (const std::string &s : seven_zip_switches(o, true)) shown.push_back(s);
    shown.push_back("-r");
    shown.push_back(fs::absolute(archive_path).string());
    shown.push_back("*");
    r.command_line = join_command(shown);

    const int code = run_process(argv, staging, err);
    if (code < 0) { r.error = "Could not start 7-Zip: " + err; return r; }
    if (code != 0 && code != 1) {
        r.error = "7-Zip exited with code " + std::to_string(code) + " (" + seven_zip_exit_text(code) + ")";
        return r;
    }
    if (code == 1) r.error = "7-Zip reported a warning (exit code 1); the archive was written.";
    r.ok = true;
    return r;
}

// ===========================================================================
// One-shot export
// ===========================================================================

ExportOutcome run_export(const ExportJob &job)
{
    ExportOutcome outcome;
    outcome.loss = compute_loss_report(job.dataset, job.format);
    if (job.output_path.empty()) { outcome.error = "No output path"; return outcome; }

    Serialized s = serialize(job.dataset, job.format, job.serialize_options);

    const std::string stem = !job.dataset.file_stem.empty() ? job.dataset.file_stem : std::string("export");
    const std::string data_name = job.archive.format == ArchiveFormat::None ? job.output_path.filename().string()
                                                                             : stem + "." + format_extension(job.format);
    const std::string sidecar_name = data_name + "." + (s.sidecar_name ? *s.sidecar_name : std::string("meta.json"));

    if (job.archive.format == ArchiveFormat::None) {
        std::error_code ec;
        fs::create_directories(job.output_path.parent_path(), ec);
        std::ofstream out(job.output_path, std::ios::binary | std::ios::trunc);
        if (!out) { outcome.error = "Cannot write " + job.output_path.string(); return outcome; }
        out.write(s.body.data(), static_cast<std::streamsize>(s.body.size()));
        out.close();
        if (!out) { outcome.error = "Write failed for " + job.output_path.string(); return outcome; }
        outcome.members.push_back(data_name);
        if (s.sidecar_name) {
            fs::path      side = job.output_path.parent_path() / sidecar_name;
            std::ofstream so(side, std::ios::binary | std::ios::trunc);
            if (!so) { outcome.error = "Cannot write " + side.string(); return outcome; }
            so.write(s.sidecar_body.data(), static_cast<std::streamsize>(s.sidecar_body.size()));
            outcome.members.push_back(sidecar_name);
        }
        outcome.ok           = true;
        outcome.written_path = job.output_path;
        return outcome;
    }

    std::vector<ArchiveEntry> entries;
    entries.push_back({data_name, s.body});
    if (s.sidecar_name) entries.push_back({sidecar_name, s.sidecar_body});
    for (const ArchiveEntry &e : entries) outcome.members.push_back(e.relative_path);

    ArchiveResult ar;
    if (job.archive.format == ArchiveFormat::Zip) {
        ar = write_zip(job.output_path, entries);
    } else {
        ar = write_seven_zip(job.output_path, entries, job.archive, find_seven_zip(job.seven_zip_override));
        outcome.command_line = ar.command_line;
    }
    outcome.ok           = ar.ok;
    outcome.error        = ar.error;
    outcome.written_path = ar.archive_path;
    return outcome;
}

} // namespace Slic3r::GUI::Export
