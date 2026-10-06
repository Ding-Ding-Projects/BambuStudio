#include <catch_main.hpp>

#include "slic3r/GUI/Export/ExportEverything.hpp"

#include <miniz.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace Slic3r::GUI::Export;
namespace fs = std::filesystem;

namespace {

Dataset tabular_fixture()
{
    Dataset d;
    d.name           = "Sample rows";
    d.schema_id      = "test.rows";
    d.schema_version = 2;
    d.kind           = DatasetKind::Tabular;
    d.file_stem      = "rows";
    d.columns        = {{"id", Value::Type::Integer}, {"label", Value::Type::String}, {"ratio", Value::Type::Number}, {"on", Value::Type::Bool}};
    d.rows.push_back({Value::from_int(1), Value::from_string("plain"), Value::from_number(0.5), Value::from_bool(true)});
    d.rows.push_back({Value::from_int(2), Value::from_string("comma, \"quote\" and\nnewline"), Value::from_number(1e21), Value::from_bool(false)});
    d.rows.push_back({Value::from_int(3), Value::from_string("tab\there | pipe"), Value::null(), Value::from_bool(true)});
    return d;
}

Dataset structured_fixture()
{
    Dataset d;
    d.name      = "Nested";
    d.schema_id = "test.nested";
    d.kind      = DatasetKind::Structured;
    d.file_stem = "nested";
    d.root      = Value::make_object();
    d.root.set("name", Value::from_string("a/b~c \"q\""));
    d.root.set("count", Value::from_int(3));
    Value inner = Value::make_object();
    inner.set("deep", Value::from_bool(true));
    inner.set("empty list", Value::make_array());
    inner.set("nothing", Value::null());
    d.root.set("inner", std::move(inner));
    Value list = Value::make_array();
    list.push(Value::from_int(1));
    list.push(Value::from_string("two"));
    d.root.set("list", std::move(list));
    Value tables = Value::make_array();
    Value t1 = Value::make_object();
    t1.set("k", Value::from_string("v1"));
    Value t2 = Value::make_object();
    t2.set("k", Value::from_string("v2"));
    tables.push(std::move(t1));
    tables.push(std::move(t2));
    d.root.set("tables", std::move(tables));
    return d;
}

Dataset prose_fixture()
{
    Dataset d;
    d.name        = "Notes";
    d.schema_id   = "test.prose";
    d.kind        = DatasetKind::Prose;
    d.file_stem   = "notes";
    d.prose_title = "Release <notes>";
    d.prose       = "First paragraph & stuff.\nsecond line\n\nSecond paragraph.";
    return d;
}

std::size_t count_occurrences(const std::string &haystack, const std::string &needle)
{
    std::size_t n = 0;
    for (std::size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + needle.size())) ++n;
    return n;
}

class TempDir
{
public:
    TempDir()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = fs::temp_directory_path() / ("bambu-export-test-" + std::to_string(stamp));
        fs::create_directories(m_path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(m_path, ec); }
    const fs::path &path() const { return m_path; }

private:
    fs::path m_path;
};

} // namespace

TEST_CASE("Escapers", "[export][escape]")
{
    CHECK(json_escape("a\"b\\c\n\t\x01") == "a\\\"b\\\\c\\n\\t\\u0001");
    CHECK(csv_quote("plain", ',') == "plain");
    CHECK(csv_quote("has,comma", ',') == "\"has,comma\"");
    CHECK(csv_quote("say \"hi\"", ',') == "\"say \"\"hi\"\"\"");
    CHECK(csv_quote("multi\nline", ',') == "\"multi\nline\"");
    CHECK(csv_quote(" padded", ',') == "\" padded\"");
    CHECK(tsv_escape("a\tb\nc\\d") == "a\\tb\\nc\\\\d");
    CHECK(xml_escape("<a href=\"x\">&'</a>") == "&lt;a href=&quot;x&quot;&gt;&amp;&apos;&lt;/a&gt;");
    CHECK(xml_escape(std::string("bad\x01ok")) == "badok");
    CHECK(toml_quote("tab\there \"q\"") == "\"tab\\there \\\"q\\\"\"");
    CHECK(yaml_quote("yes") == "\"yes\"");
    CHECK(html_escape("<b>&\"</b>") == "&lt;b&gt;&amp;&quot;&lt;/b&gt;");
    CHECK(markdown_cell("a|b\r\nc") == "a\\|b<br>c");
    CHECK(apply_line_ending("a\nb\n", LineEnding::CRLF) == "a\r\nb\r\n");
    CHECK(apply_line_ending("a\nb\n", LineEnding::LF) == "a\nb\n");
}

TEST_CASE("JSON envelope and JSONL", "[export][json]")
{
    Dataset d = tabular_fixture();
    SerializeOptions o;
    o.generator = "test-gen";
    Serialized s = serialize(d, Format::JSON, o);
    CHECK(s.body.find("\"schema\": \"test.rows\"") != std::string::npos);
    CHECK(s.body.find("\"schemaVersion\": 2") != std::string::npos);
    CHECK(s.body.find("\"encoding\": \"UTF-8\"") != std::string::npos);
    CHECK(s.body.find("\"lineEnding\": \"LF\"") != std::string::npos);
    CHECK(s.body.find("\"generator\": \"test-gen\"") != std::string::npos);
    CHECK(s.body.find("\"ratio\": 0.5") != std::string::npos);
    CHECK(s.body.find("\"ratio\": null") != std::string::npos);
    CHECK(s.body.find("\"label\": \"comma, \\\"quote\\\" and\\nnewline\"") != std::string::npos);
    CHECK(!s.sidecar_name.has_value());

    Serialized jl = serialize(d, Format::JSONL, o);
    CHECK(count_occurrences(jl.body, "\n") == 4); // header + 3 rows
    CHECK(jl.body.rfind("{\"_type\":\"header\"", 0) == 0);
    CHECK(jl.body.find("{\"id\":1,\"label\":\"plain\",\"ratio\":0.5,\"on\":true}") != std::string::npos);

    o.line_ending = LineEnding::CRLF;
    Serialized crlf = serialize(d, Format::JSONL, o);
    CHECK(count_occurrences(crlf.body, "\r\n") == 4);
    CHECK(crlf.body.find("\"lineEnding\":\"CRLF\"") != std::string::npos);

    o.include_schema_header = false;
    o.line_ending = LineEnding::LF;
    Serialized bare = serialize(d, Format::JSON, o);
    CHECK(bare.body.rfind("[", 0) == 0);
    CHECK(bare.body.find("schemaVersion") == std::string::npos);
}

TEST_CASE("YAML always quotes strings and nests correctly", "[export][yaml]")
{
    Serialized s = serialize(structured_fixture(), Format::YAML);
    CHECK(s.body.rfind("# schema=test.nested", 0) == 0);
    CHECK(s.body.find("%YAML 1.2\n---\n") != std::string::npos);
    CHECK(s.body.find("  name: \"a/b~c \\\"q\\\"\"\n") != std::string::npos);
    CHECK(s.body.find("  count: 3\n") != std::string::npos);
    CHECK(s.body.find("  inner:\n    deep: true\n    \"empty list\": []\n    nothing: null\n") != std::string::npos);
    CHECK(s.body.find("  list:\n    - 1\n    - \"two\"\n") != std::string::npos);
    CHECK(s.body.find("  tables:\n    -\n      k: \"v1\"\n    -\n      k: \"v2\"\n") != std::string::npos);

    Dataset yes;
    yes.kind = DatasetKind::Structured;
    yes.root = Value::make_object();
    yes.root.set("flag", Value::from_string("yes"));
    yes.root.set("num", Value::from_string("1e3"));
    SerializeOptions o;
    o.include_schema_header = false;
    Serialized bare = serialize(yes, Format::YAML, o);
    CHECK(bare.body == "flag: \"yes\"\nnum: \"1e3\"\n");
}

TEST_CASE("TOML tables, arrays of tables and null omission", "[export][toml]")
{
    Dataset d = structured_fixture();
    Serialized s = serialize(d, Format::TOML);
    CHECK(s.body.find("[data]\nname = \"a/b~c \\\"q\\\"\"\ncount = 3\nlist = [1, \"two\"]\n") != std::string::npos);
    CHECK(s.body.find("\n[data.inner]\ndeep = true\n\"empty list\" = []\n") != std::string::npos);
    CHECK(s.body.find("nothing") == std::string::npos); // null omitted
    CHECK(count_occurrences(s.body, "[[data.tables]]") == 2);
    LossReport r = compute_loss_report(d, Format::TOML);
    CHECK(!r.lossless);
    REQUIRE(r.losses.size() == 1);
    CHECK(r.losses[0].find("1 empty (null)") != std::string::npos);

    Serialized rows = serialize(tabular_fixture(), Format::TOML);
    CHECK(count_occurrences(rows.body, "[[data]]") == 3);
    CHECK(rows.body.find("label = \"tab\\there | pipe\"") != std::string::npos);
    CHECK(rows.body.find("ratio = 1e+21") != std::string::npos);
}

TEST_CASE("XML entities and structure", "[export][xml]")
{
    Serialized s = serialize(prose_fixture(), Format::XML);
    CHECK(s.body.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0) == 0);
    CHECK(s.body.find("<title>Release &lt;notes&gt;</title>") != std::string::npos);
    CHECK(s.body.find("First paragraph &amp; stuff.") != std::string::npos);

    Serialized t = serialize(tabular_fixture(), Format::XML);
    CHECK(t.body.find("<column name=\"ratio\" type=\"number\"/>") != std::string::npos);
    CHECK(t.body.find("<field name=\"ratio\" type=\"null\"/>") != std::string::npos);
    CHECK(t.body.find("<field name=\"label\" type=\"string\">comma, &quot;quote&quot; and\nnewline</field>") != std::string::npos);

    Serialized n = serialize(structured_fixture(), Format::XML);
    CHECK(n.body.find("<item key=\"empty list\" type=\"array\"/>") != std::string::npos);
    CHECK(n.body.find("<item key=\"nothing\" type=\"null\"/>") != std::string::npos);
}

TEST_CASE("CSV and TSV quoting with sidecar schema", "[export][csv]")
{
    Dataset    d = tabular_fixture();
    Serialized s = serialize(d, Format::CSV);
    CHECK(s.body.rfind("id,label,ratio,on\n", 0) == 0);
    CHECK(s.body.find("2,\"comma, \"\"quote\"\" and\nnewline\",1e+21,false\n") != std::string::npos);
    CHECK(s.body.find("3,tab\there | pipe,,true\n") != std::string::npos);
    REQUIRE(s.sidecar_name.has_value());
    CHECK(*s.sidecar_name == "meta.json");
    CHECK(s.sidecar_body.find("\"quoting\": \"RFC 4180\"") != std::string::npos);
    CHECK(s.sidecar_body.find("\"type\": \"integer\"") != std::string::npos);

    Serialized t = serialize(d, Format::TSV);
    CHECK(t.body.find("3\ttab\\there | pipe\t\ttrue\n") != std::string::npos);
    CHECK(t.body.find("2\tcomma, \"quote\" and\\nnewline\t1e+21\tfalse\n") != std::string::npos);

    LossReport r = compute_loss_report(d, Format::CSV);
    CHECK(r.lossless);
    CHECK(!r.notes.empty());
}

TEST_CASE("CSV flattening of nested data is reversible and reported", "[export][csv][loss]")
{
    Dataset    d = structured_fixture();
    Serialized s = serialize(d, Format::CSV);
    CHECK(s.body.rfind("path,type,value\n", 0) == 0);
    CHECK(s.body.find(",object,\n") != std::string::npos);
    CHECK(s.body.find("/name,string,\"a/b~c \"\"q\"\"\"\n") != std::string::npos);
    // Key "empty list" keeps an explicit row so the empty array survives.
    CHECK(s.body.find("/inner/empty list,array,\n") != std::string::npos);
    CHECK(s.body.find("/inner/nothing,null,\n") != std::string::npos);
    CHECK(s.body.find("/tables/1/k,string,v2\n") != std::string::npos);

    LossReport r = compute_loss_report(d, Format::CSV);
    CHECK(r.lossless);
    REQUIRE(!r.notes.empty());
    CHECK(r.notes[0].find("flattened") != std::string::npos);

    // Keys with "/" or "~" escape per RFC 6901 so the path stays unambiguous.
    Dataset tricky;
    tricky.kind = DatasetKind::Structured;
    tricky.root = Value::make_object();
    tricky.root.set("a/b", Value::from_int(1));
    tricky.root.set("c~d", Value::from_int(2));
    Serialized ts = serialize(tricky, Format::TSV);
    CHECK(ts.body.find("/a~1b\tinteger\t1\n") != std::string::npos);
    CHECK(ts.body.find("/c~0d\tinteger\t2\n") != std::string::npos);
}

TEST_CASE("Markdown and HTML rendering", "[export][markdown][html]")
{
    Dataset    d = tabular_fixture();
    Serialized m = serialize(d, Format::Markdown);
    CHECK(m.body.rfind("<!-- schema=test.rows", 0) == 0);
    CHECK(m.body.find("columns=id:integer,label:string,ratio:number,on:boolean") != std::string::npos);
    CHECK(m.body.find("| id | label | ratio | on |\n| --- | --- | --- | --- |\n") != std::string::npos);
    CHECK(m.body.find("| 3 | tab\there \\| pipe |  | true |") != std::string::npos);
    CHECK(m.body.find("and<br>newline") != std::string::npos);
    CHECK(compute_loss_report(d, Format::Markdown).lossless);

    Serialized h = serialize(d, Format::HTML);
    CHECK(h.body.find("<meta charset=\"utf-8\">") != std::string::npos);
    CHECK(h.body.find("<meta name=\"schema\" content=\"test.rows\">") != std::string::npos);
    CHECK(h.body.find("<td>comma, &quot;quote&quot; and<br>newline</td>") != std::string::npos);

    Dataset n = structured_fixture();
    LossReport r = compute_loss_report(n, Format::Markdown);
    CHECK(!r.lossless);
    CHECK(r.losses[0].find("types are not preserved") != std::string::npos);
    Serialized nm = serialize(n, Format::Markdown);
    CHECK(nm.body.find("- **inner**:\n  - **deep**: `true`\n  - **empty list**:\n    - _(empty)_\n  - **nothing**: _(empty)_\n") != std::string::npos);

    Serialized p = serialize(prose_fixture(), Format::HTML);
    CHECK(p.body.find("<h1>Release &lt;notes&gt;</h1>") != std::string::npos);
    CHECK(p.body.find("<p>First paragraph &amp; stuff.<br>\nsecond line</p>\n<p>Second paragraph.</p>") != std::string::npos);
    Serialized pm = serialize(prose_fixture(), Format::Markdown);
    CHECK(pm.body.find("# Release <notes>\n\nFirst paragraph & stuff.") != std::string::npos);
    CHECK(compute_loss_report(prose_fixture(), Format::Markdown).lossless);
}

TEST_CASE("Loss report edge cases", "[export][loss]")
{
    Dataset d;
    d.kind = DatasetKind::Structured;
    d.root = Value::make_object();
    d.root.set("nan", Value::from_number(std::nan("")));
    d.root.set("ctl", Value::from_string(std::string("x\x02y")));
    CHECK(!compute_loss_report(d, Format::JSON).lossless);
    CHECK(compute_loss_report(d, Format::YAML).lossless);
    CHECK(!compute_loss_report(d, Format::XML).lossless);
    CHECK(serialize(d, Format::JSON).body.find("\"nan\": null") != std::string::npos);
    CHECK(serialize(d, Format::YAML).body.find("nan: .nan") != std::string::npos);
    CHECK(serialize(d, Format::TOML).body.find("nan = nan") != std::string::npos);

    for (Format f : all_formats()) {
        // Every format is offered for every kind; only the report changes.
        CHECK(!serialize(tabular_fixture(), f).body.empty());
        CHECK(!serialize(structured_fixture(), f).body.empty());
        CHECK(!serialize(prose_fixture(), f).body.empty());
    }
    CHECK(format_is_natural_for(Format::CSV, DatasetKind::Tabular));
    CHECK(format_is_natural_for(Format::TOML, DatasetKind::Structured));
    CHECK(format_is_natural_for(Format::HTML, DatasetKind::Prose));
    CHECK(!format_is_natural_for(Format::CSV, DatasetKind::Prose));
    CHECK(format_from_extension("yml") == Format::YAML);
    CHECK(!format_from_extension("exe").has_value());
}

TEST_CASE("Archive member paths are sanitized", "[export][archive][safety]")
{
    CHECK(sanitize_archive_path("rows.csv") == "rows.csv");
    CHECK(sanitize_archive_path("./sub\\rows.csv") == "sub/rows.csv");
    CHECK(sanitize_archive_path("/abs/rows.csv") == "abs/rows.csv");
    CHECK(!sanitize_archive_path("../rows.csv").has_value());
    CHECK(!sanitize_archive_path("sub/../../rows.csv").has_value());
    CHECK(!sanitize_archive_path("C:\\rows.csv").has_value());
    CHECK(!sanitize_archive_path("\\\\server\\share\\rows.csv").has_value());
    CHECK(!sanitize_archive_path("a//b").has_value());
    CHECK(!sanitize_archive_path("").has_value());
    CHECK(!sanitize_archive_path(std::string("bad\x01name")).has_value());

    TempDir       tmp;
    ArchiveResult refused = write_zip(tmp.path() / "x.zip", {{"../escape.txt", "x"}});
    CHECK(!refused.ok);
    CHECK(refused.error.find("unsafe") != std::string::npos);
}

TEST_CASE("ZIP round trip through miniz", "[export][archive][zip]")
{
    TempDir  tmp;
    fs::path archive = tmp.path() / "out.zip";
    ArchiveResult r = write_zip(archive, {{"rows.csv", "id\n1\n"}, {"rows.csv.meta.json", "{}\n"}, {"nested/dir/x.txt", "deep"}});
    REQUIRE(r.ok);
    REQUIRE(fs::exists(archive));

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof zip);
    REQUIRE(mz_zip_reader_init_file(&zip, archive.string().c_str(), 0));
    CHECK(mz_zip_reader_get_num_files(&zip) == 3);
    size_t size = 0;
    void  *data = mz_zip_reader_extract_file_to_heap(&zip, "nested/dir/x.txt", &size, 0);
    REQUIRE(data != nullptr);
    CHECK(std::string(static_cast<const char *>(data), size) == "deep");
    mz_free(data);
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i) {
        mz_zip_archive_file_stat st;
        REQUIRE(mz_zip_reader_file_stat(&zip, i, &st));
        std::string name = st.m_filename;
        CHECK(name.find("..") == std::string::npos);
        CHECK(name.front() != '/');
    }
    mz_zip_reader_end(&zip);
}

TEST_CASE("7-Zip switch mapping covers every option", "[export][archive][7z]")
{
    ArchiveOptions o;
    o.format = ArchiveFormat::SevenZip;
    std::vector<std::string> sw = seven_zip_switches(o);
    auto has = [&sw](const std::string &s) { return std::find(sw.begin(), sw.end(), s) != sw.end(); };
    CHECK(has("-t7z"));
    CHECK(has("-m0=LZMA2"));
    CHECK(has("-mx=5"));
    CHECK(has("-ms=on"));
    CHECK(has("-mmt=on"));
    CHECK(!has("-mhe=on"));
    for (const std::string &s : sw) CHECK(s.rfind("-p", 0) != 0);

    o.method          = SevenZipMethod::LZMA;
    o.level           = SevenZipLevel::Ultra;
    o.dictionary_mib  = 128;
    o.word_size       = 273;
    o.solid           = true;
    o.solid_block_mib = 512;
    o.threads         = 4;
    o.split_volume    = "100m";
    o.password        = "s3cret pass";
    o.encrypt_headers = true;
    sw = seven_zip_switches(o);
    CHECK(has("-m0=LZMA"));
    CHECK(has("-mx=9"));
    CHECK(has("-md=128m"));
    CHECK(has("-mfb=273"));
    CHECK(has("-ms=512m"));
    CHECK(has("-mmt=4"));
    CHECK(has("-v100m"));
    CHECK(has("-p"));
    CHECK(has("-mhe=on"));
    CHECK(!seven_zip_filenames_visible(o));

    std::vector<std::string> redacted = seven_zip_switches(o, true);
    CHECK(std::find(redacted.begin(), redacted.end(), "-p***") != redacted.end());
    for (const std::string &s : redacted) CHECK(s.find("s3cret") == std::string::npos);

    o.encrypt_headers = false;
    sw = seven_zip_switches(o);
    CHECK(has("-mhe=off"));
    CHECK(seven_zip_filenames_visible(o));
    std::vector<std::string> hints = seven_zip_cost_hints(o);
    bool warned = false;
    for (const std::string &h : hints) warned = warned || h.find("file names are visible") != std::string::npos;
    CHECK(warned);

    o.method         = SevenZipMethod::PPMd;
    o.dictionary_mib = 64;
    o.word_size      = 8;
    o.solid          = false;
    sw               = seven_zip_switches(o);
    CHECK(has("-m0=PPMd"));
    CHECK(has("-mmem=64m"));
    CHECK(has("-mo=8"));
    CHECK(has("-ms=off"));
    CHECK(!has("-md=64m"));

    o.method = SevenZipMethod::BZip2;
    sw       = seven_zip_switches(o);
    CHECK(has("-m0=BZip2"));
    for (const std::string &s : sw) {
        CHECK(s.rfind("-md=", 0) != 0);
        CHECK(s.rfind("-mfb=", 0) != 0);
    }

    o.method = SevenZipMethod::Deflate;
    o.level  = SevenZipLevel::Store;
    sw       = seven_zip_switches(o);
    CHECK(has("-m0=Deflate"));
    CHECK(has("-mx=0"));
    CHECK(has("-mfb=8"));
}

TEST_CASE("run_export writes a data file, a sidecar and a ZIP", "[export][job]")
{
    TempDir   tmp;
    ExportJob job;
    job.dataset     = tabular_fixture();
    job.format      = Format::CSV;
    job.output_path = tmp.path() / "rows.csv";
    ExportOutcome out = run_export(job);
    REQUIRE(out.ok);
    CHECK(fs::exists(tmp.path() / "rows.csv"));
    CHECK(fs::exists(tmp.path() / "rows.csv.meta.json"));
    REQUIRE(out.members.size() == 2);
    CHECK(out.members[1] == "rows.csv.meta.json");

    job.format         = Format::JSON;
    job.archive.format = ArchiveFormat::Zip;
    job.output_path    = tmp.path() / "rows.zip";
    out                = run_export(job);
    REQUIRE(out.ok);
    REQUIRE(out.members.size() == 1);
    CHECK(out.members[0] == "rows.json");
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof zip);
    REQUIRE(mz_zip_reader_init_file(&zip, job.output_path.string().c_str(), 0));
    CHECK(mz_zip_reader_locate_file(&zip, "rows.json", nullptr, 0) >= 0);
    mz_zip_reader_end(&zip);

    // A missing 7-Zip is an honest error, never a silent fallback to ZIP.
    job.archive.format    = ArchiveFormat::SevenZip;
    job.seven_zip_override = tmp.path() / "definitely-missing" / "7z.exe";
    job.output_path       = tmp.path() / "rows.7z";
    SevenZipLocation loc  = find_seven_zip(job.seven_zip_override);
    if (!loc.found) {
        out = run_export(job);
        CHECK(!out.ok);
        CHECK(out.error.find("7-Zip not found") != std::string::npos);
        CHECK(!fs::exists(job.output_path));
    } else {
        job.archive.password        = "pw";
        job.archive.encrypt_headers = true;
        out                         = run_export(job);
        CHECK(out.ok);
        CHECK(out.command_line.find("pw") == std::string::npos);
        CHECK(out.command_line.find("-p***") != std::string::npos);
        CHECK(fs::exists(job.output_path));
    }
}


TEST_CASE("Preference export excludes unknown sections keys and invalid safe values", "[export][privacy]")
{
    CHECK(preference_export_allowed("", "dark_color_mode", "1"));
    CHECK(preference_export_allowed("", "motion_preference", "reduced"));
    CHECK_FALSE(preference_export_allowed("", "access_token", "example"));
    CHECK_FALSE(preference_export_allowed("custom", "dark_color_mode", "1"));
    CHECK_FALSE(preference_export_allowed("", "future_setting", "1"));
    CHECK_FALSE(preference_export_allowed("", "dark_color_mode", "unexpected private text"));
    CHECK_FALSE(preference_export_allowed("", "personal_vocabulary", "{}"));
}

TEST_CASE("Invalid encrypted output is rejected before staging spawning or overwriting", "[export][privacy]")
{
    TempDir tmp;
    const auto target = tmp.path() / "keep.7z";
    { std::ofstream out(target); out << "existing output"; }
    ArchiveOptions options;
    options.format = ArchiveFormat::SevenZip;
    options.password.assign(1025, 'x'); // Synthetic in-memory marker, never a credential.
    SevenZipLocation location;
    location.found = true;
    location.executable = tmp.path() / "must-not-run.exe";
    const auto result = write_seven_zip(target, {{"data.json", "{}"}}, options, location);
    CHECK_FALSE(result.ok);
    CHECK(result.command_line.empty());
    CHECK_FALSE(result.error.empty());
    std::ifstream in(target); std::string text; std::getline(in, text);
    CHECK(text == "existing output");
    for (bool display : {false, true})
        for (const auto &arg : seven_zip_switches(options, display))
            CHECK(arg.find(options.password) == std::string::npos);
}

TEST_CASE("Source formats preserve JSON payloads and disclose exclusions", "[export][formats]")
{
    auto dataset = structured_fixture();
    dataset.root.set("large", Value::from_int(9007199254740993LL));
    dataset.exclusions.push_back("One private field omitted");
    for (auto format : {Format::SQL, Format::JavaScript, Format::TypeScript, Format::Python, Format::Go, Format::Rust, Format::JSONSchema, Format::Protobuf}) {
        const auto result = serialize(dataset, format);
        CHECK_FALSE(result.body.empty());
        CHECK_FALSE(compute_loss_report(dataset, format).lossless);
        CHECK(format_from_extension(format_extension(format)) == format);
        if (format != Format::SQL) CHECK(result.body.find("9007199254740993") != std::string::npos);
    }
}


TEST_CASE("Encrypted archive verification uses real 7-Zip and rejects visible headers", "[export][privacy]")
{
#ifdef _WIN32
    const auto location = find_seven_zip();
    if (!location.found) { WARN("7-Zip integration unavailable on this host"); return; }
    TempDir tmp;
    ArchiveOptions options;
    options.format = ArchiveFormat::SevenZip;
    options.password = std::string(24, 'x') + "\xe6\xb8\xac";
    const auto archive = write_seven_zip(tmp.path() / "private.7z", {{"private-name.txt", "round trip"}}, options, location);
    INFO(archive.error);
    REQUIRE(archive.ok); // Production path tests content and rejects no-password header listing.
    CHECK(archive.command_line.find(options.password) == std::string::npos);
    CHECK(archive.command_line.find("-p***") != std::string::npos);
    options.encrypt_headers = false;
    CHECK_FALSE(write_seven_zip(tmp.path() / "visible.7z", {{"name", "content"}}, options, location).ok);
    CHECK_FALSE(fs::exists(tmp.path() / "visible.7z"));
    options.encrypt_headers = true;
    options.password += "\n";
    CHECK_FALSE(write_seven_zip(tmp.path() / "newline.7z", {{"name", "content"}}, options, location).ok);
    CHECK_FALSE(fs::exists(tmp.path() / "newline.7z"));
#endif
}


TEST_CASE("Preset export excludes connection credentials and free form fields", "[export][privacy]")
{
    CHECK(preset_export_allowed("layer_height", "0.2"));
    CHECK(preset_export_allowed("sparse_infill_density", "15%"));
    CHECK_FALSE(preset_export_allowed("printhost_apikey", "example"));
    CHECK_FALSE(preset_export_allowed("printhost_password", "example"));
    CHECK_FALSE(preset_export_allowed("print_host", "https://example.invalid"));
    CHECK_FALSE(preset_export_allowed("machine_start_gcode", "example"));
    CHECK_FALSE(preset_export_allowed("layer_height", "private text"));
}


TEST_CASE("Overwrite authorization is exact and includes sidecars", "[export][privacy]")
{
    TempDir tmp;
    ExportJob job;
    job.dataset = tabular_fixture();
    job.format = Format::CSV;
    job.output_path = tmp.path() / "rows.csv";
    REQUIRE(run_export(job).ok);
    CHECK_FALSE(run_export(job).ok);
    job.overwrite_approved_paths = {job.output_path};
    CHECK_FALSE(run_export(job).ok);
    job.overwrite_approved_paths = planned_output_paths(job);
    CHECK(run_export(job).ok);
    job.output_path = tmp.path() / "other.csv";
    { std::ofstream other(job.output_path); other << "preserve"; }
    CHECK_FALSE(run_export(job).ok);
    std::ifstream other(job.output_path); std::string text; std::getline(other, text);
    CHECK(text == "preserve");
}


TEST_CASE("Cancellation before publication preserves primary and sidecar", "[export][cancellation]")
{
    TempDir tmp;
    ExportJob job;
    job.dataset = tabular_fixture(); job.format = Format::CSV;
    job.output_path = tmp.path() / "rows.csv";
    REQUIRE(run_export(job).ok);
    job.overwrite_approved_paths = planned_output_paths(job);
    const auto before = fs::file_size(job.output_path);
    job.control = std::make_shared<ExportControl>();
    REQUIRE(job.control->request_cancel());
    const auto result = run_export(job);
    CHECK_FALSE(result.ok); CHECK(result.cancelled);
    CHECK(fs::file_size(job.output_path) == before);
    CHECK(fs::exists(tmp.path() / "rows.csv.meta.json"));
    ExportControl publication;
    REQUIRE(publication.begin_publication());
    CHECK_FALSE(publication.request_cancel());
}

TEST_CASE("A changed destination is not overwritten by an asynchronous export", "[export][cancellation]")
{
    TempDir tmp;
    ExportJob job;
    job.dataset = structured_fixture();
    job.dataset.root.set("payload", Value::from_string(std::string(16000000, 'a')));
    job.output_path = tmp.path() / "output.json";
    { std::ofstream out(job.output_path); out << "before"; }
    job.overwrite_approved_paths = {job.output_path};
    job.control = std::make_shared<ExportControl>();
    ExportOutcome outcome;
    std::thread worker([&] { outcome = run_export(job); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (job.control->phase != ExportPhase::Serializing && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool observed = job.control->phase == ExportPhase::Serializing;
    if (observed) { std::ofstream out(job.output_path); out << "another writer"; }
    worker.join();
    REQUIRE(observed);
    CHECK_FALSE(outcome.ok);
    std::ifstream in(job.output_path); std::string text; std::getline(in, text);
    CHECK(text == "another writer");
}

TEST_CASE("Cancellation terminates only the owned archive child", "[export][cancellation]")
{
#ifdef _WIN32
    const char *helper = std::getenv("EXPORT_TEST_CHILD");
    if (!helper) { WARN("Set EXPORT_TEST_CHILD to the compiled archive_test_child.exe for child-process cancellation coverage"); return; }
    TempDir tmp;
    ExportJob job;
    job.dataset = structured_fixture(); job.archive.format = ArchiveFormat::SevenZip;
    job.output_path = tmp.path() / "cancelled.7z"; job.seven_zip_override = helper;
    job.control = std::make_shared<ExportControl>();
    ExportOutcome outcome;
    std::thread worker([&] { outcome = run_export(job); });
    DWORD pid = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!pid && std::chrono::steady_clock::now() < deadline) {
        for (const auto &entry : fs::recursive_directory_iterator(tmp.path()))
            if (entry.path().filename() == "child.pid") { std::ifstream in(entry.path()); in >> pid; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    HANDLE child = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    const auto start = std::chrono::steady_clock::now();
    const bool accepted = job.control->request_cancel();
    worker.join();
    CHECK(accepted); REQUIRE(pid != 0);
    CHECK(outcome.cancelled); CHECK_FALSE(outcome.ok);
    CHECK_FALSE(fs::exists(job.output_path));
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
    if (child) { CHECK(WaitForSingleObject(child, 0) == WAIT_OBJECT_0); CloseHandle(child); }
#endif
}


TEST_CASE("ZIP cancellation never publishes a partially compressed output", "[export][cancellation]")
{
    TempDir tmp;
    auto control = std::make_shared<ExportControl>();
    REQUIRE(control->request_cancel());
    const auto result = write_zip(tmp.path() / "partial.zip", {{"large", std::string(100000, 'a')}}, control);
    CHECK_FALSE(result.ok);
    CHECK_FALSE(fs::exists(tmp.path() / "partial.zip"));
}


TEST_CASE("Confirmation snapshot cannot overwrite a newly replaced destination", "[export][cancellation]")
{
    TempDir tmp;
    ExportJob job;
    job.dataset = structured_fixture(); job.output_path = tmp.path() / "output.json";
    { std::ofstream out(job.output_path); out << "confirmed original"; }
    job.overwrite_approved_paths = {job.output_path};
    const auto captured = inspect_output_target(job.output_path);
    REQUIRE(captured.has_value()); job.destination_snapshots = {*captured};
    { std::ofstream out(job.output_path); out << "new unconfirmed content"; }
    CHECK_FALSE(run_export(job).ok);
    std::ifstream in(job.output_path); std::string text; std::getline(in, text);
    CHECK(text == "new unconfirmed content");
}
