// Standalone assertions for src/slic3r/GUI/PreferencesRemake/PreferenceFormats.cpp:
// Import preferences... reads the YAML, TOML and XML that Export
// preferences... writes, with and without the schema header, and the plain
// hand edits people make to them, and refuses anything else with the format
// named. The exported files below are the exporter's real output
// (src/slic3r/GUI/Export/ExportEverything.cpp) for a small preferences set;
// tests/preference_formats.test.mjs checks that the writer still writes them
// this way. Each converted file then goes through plan_import, as in the app.
// Built and run by tests/preference_formats.test.mjs, which also builds it
// with SLIC3R_PREFERENCE_FORMATS_NEGATIVE_FIXTURE: in that build a file that
// could not be read is passed on as if it had been, and the checks below must
// fail.
#include "src/slic3r/GUI/PreferencesRemake/PreferenceFormats.cpp"
#include "src/slic3r/GUI/PreferencesRemake/PreferenceHistory.cpp"
#include "src/slic3r/GUI/PreferencesRemake/PreferencePlans.cpp"

#include <cstdio>

using namespace Slic3r::GUI;
using namespace Slic3r::GUI::PreferenceFormats;

static int g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

static const char *kYamlHeader = R"EXPORT(# schema=bambustudio.app-config schemaVersion=1 dataset="Preferences" kind=structured encoding=UTF-8 lineEnding=LF generator="BambuStudio 2.8"
%YAML 1.2
---
schema: "bambustudio.app-config"
schemaVersion: 1
dataset: "Preferences"
kind: "structured"
encoding: "UTF-8"
lineEnding: "LF"
generator: "BambuStudio 2.8"
exclusions:
  - "3 settings omitted: credentials, paths."
data:
  app:
    dark_color_mode: "1"
    ui_density: "compact"
    default_page: "0"
    download_path: "D:\\Models\\Incoming"
    "note with space": "quote \" and \\ and # hash"
)EXPORT";
static const char *kTomlHeader = R"EXPORT(# schema=bambustudio.app-config schemaVersion=1 dataset="Preferences" kind=structured encoding=UTF-8 lineEnding=LF generator="BambuStudio 2.8"
schema = "bambustudio.app-config"
schemaVersion = 1
dataset = "Preferences"
kind = "structured"
encoding = "UTF-8"
lineEnding = "LF"
generator = "BambuStudio 2.8"
exclusions = ["3 settings omitted: credentials, paths."]

[data]

[data.app]
dark_color_mode = "1"
ui_density = "compact"
default_page = "0"
download_path = "D:\\Models\\Incoming"
"note with space" = "quote \" and \\ and # hash"
)EXPORT";
static const char *kXmlHeader = R"EXPORT(<?xml version="1.0" encoding="UTF-8"?>
<export schema="bambustudio.app-config" schemaVersion="1" dataset="Preferences" kind="structured" encoding="UTF-8" lineEnding="LF" generator="BambuStudio 2.8">
  <data>
    <item type="object">
      <item key="app" type="object">
        <item key="dark_color_mode" type="string">1</item>
        <item key="ui_density" type="string">compact</item>
        <item key="default_page" type="string">0</item>
        <item key="download_path" type="string">D:\Models\Incoming</item>
        <item key="note with space" type="string">quote &quot; and \ and # hash</item>
      </item>
    </item>
  </data>
</export>
)EXPORT";
static const char *kYamlBare = R"EXPORT(app:
  dark_color_mode: "1"
  ui_density: "compact"
  default_page: "0"
  download_path: "D:\\Models\\Incoming"
  "note with space": "quote \" and \\ and # hash"
)EXPORT";
static const char *kTomlBare = R"EXPORT(
[data]

[data.app]
dark_color_mode = "1"
ui_density = "compact"
default_page = "0"
download_path = "D:\\Models\\Incoming"
"note with space" = "quote \" and \\ and # hash"
)EXPORT";
static const char *kXmlBare = R"EXPORT(<?xml version="1.0" encoding="UTF-8"?>
<export>
  <data>
    <item type="object">
      <item key="app" type="object">
        <item key="dark_color_mode" type="string">1</item>
        <item key="ui_density" type="string">compact</item>
        <item key="default_page" type="string">0</item>
        <item key="download_path" type="string">D:\Models\Incoming</item>
        <item key="note with space" type="string">quote &quot; and \ and # hash</item>
      </item>
    </item>
  </data>
</export>
)EXPORT";

static std::vector<PreferencePlans::Setting> shown()
{
    using PreferencePlans::Setting;
    return {
        Setting{"dark_color_mode", std::string("0"), std::string("0"), true},
        Setting{"ui_density", std::string("comfortable"), std::string("comfortable"), true},
        Setting{"default_page", std::string("0"), std::string("0"), true},
    };
}

// Converts, then plans the import exactly as the dialog does.
static PreferencePlans::ImportPlan import_as(Format format, const char *text)
{
    std::string json, problem;
    PreferencePlans::ImportPlan plan;
    if (!to_json(format, text, json, problem)) {
        plan.problem = problem;
        return plan;
    }
    return PreferencePlans::plan_import(json, shown());
}

// What the sample export changes: two settings differ, one is the same, two are not shown.
static void check_sample(const PreferencePlans::ImportPlan &plan)
{
    CHECK(plan.problem.empty());
    CHECK(plan.apply.size() == 2);
    bool dark = false, density = false;
    for (const auto &item : plan.apply) {
        if (item.key == "dark_color_mode" && item.value == "1") dark = true;
        if (item.key == "ui_density" && item.value == "compact") density = true;
    }
    CHECK(dark && density);
    CHECK(plan.same.size() == 1);
    CHECK(plan.not_here.size() == 2);
}

static void test_exports()
{
    check_sample(import_as(Format::Yaml, kYamlHeader));
    check_sample(import_as(Format::Yaml, kYamlBare));
    check_sample(import_as(Format::Toml, kTomlHeader));
    check_sample(import_as(Format::Toml, kTomlBare));
    check_sample(import_as(Format::Xml, kXmlHeader));
    check_sample(import_as(Format::Xml, kXmlBare));

    // The escapes come back exactly.
    for (const auto &[format, text] : std::vector<std::pair<Format, const char *>>{
             {Format::Yaml, kYamlHeader}, {Format::Toml, kTomlHeader}, {Format::Xml, kXmlHeader}}) {
        std::string json, problem;
        CHECK(to_json(format, text, json, problem));
        const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
        CHECK(j.is_object() && j.contains("data") && j["data"].contains("app"));
        if (!j.is_object() || !j.contains("data")) continue;
        CHECK(j["schema"] == "bambustudio.app-config");
        CHECK(j["schemaVersion"] == 1);
        CHECK(j["data"]["app"]["download_path"] == "D:\\Models\\Incoming");
        CHECK(j["data"]["app"]["note with space"] == "quote \" and \\ and # hash");
    }
}

static void test_hand_edits()
{
    // Comments, plain and single-quoted words, CRLF line ends.
    const char *yaml = "# mine\r\napp:\r\n  dark_color_mode: 1   # dark\r\n  ui_density: 'compact'\r\n  default_page: '0'\r\n";
    const PreferencePlans::ImportPlan y = import_as(Format::Yaml, yaml);
    CHECK(y.problem.empty() && y.apply.size() == 2);
    // A list over several lines and a comment in TOML.
    const char *toml = "schema = 'bambustudio.app-config' # from my export\nschemaVersion = 1\nexclusions = [\n  \"a\",\n  \"b\",\n]\n[data.app]\ndark_color_mode = \"1\"\nui_density = \"compact\"\n";
    const PreferencePlans::ImportPlan t = import_as(Format::Toml, toml);
    CHECK(t.problem.empty() && t.apply.size() == 2);
    // Escaped characters come back as UTF-8 in every format.
    std::string json, problem;
    CHECK(to_json(Format::Yaml, "app:\n  name: \"caf\\u00e9\"\n", json, problem) && json.find("caf\xC3\xA9") != std::string::npos);
    CHECK(to_json(Format::Toml, "[app]\nname = \"caf\\u00e9\"\n", json, problem) && json.find("caf\xC3\xA9") != std::string::npos);
    CHECK(to_json(Format::Xml, "<export><data><item type=\"object\"><item key=\"name\" type=\"string\">caf&#233; &#x2713;</item></item></data></export>", json,
                  problem) &&
          json.find("caf\xC3\xA9 \xE2\x9C\x93") != std::string::npos);
    // A file from a newer version is still refused once converted.
    const char *newer = "schema: \"bambustudio.app-config\"\nschemaVersion: 2\ndata:\n  app:\n    ui_density: \"compact\"\n";
    CHECK(import_as(Format::Yaml, newer).problem == "newer-version");
    // Some other schema is not preferences.
    CHECK(import_as(Format::Toml, "schema = \"bambustudio.preset\"\n[data.app]\nui_density = \"compact\"\n").problem == "not-preferences");
}

static void test_refused()
{
    std::string json, problem;
    // YAML: tabs, flow collections, anchors, a key twice, two documents, unclosed quotes.
    for (const char *bad : {"app:\n\tui_density: \"compact\"\n", "app: {ui_density: compact}\n", "app: &a\n  x: 1\n",
                            "app:\n  ui_density: a\n  ui_density: b\n", "app:\n  x: 1\n---\napp:\n  x: 2\n", "app:\n  x: \"open\n",
                            "app:\n  - x\n  y: 1\n"}) {
        CHECK(!to_json(Format::Yaml, bad, json, problem));
        CHECK(problem == "not-yaml");
    }
    // TOML: a key twice, a table twice as a value, multi-line strings, an unclosed array, junk after a value.
    for (const char *bad : {"[data.app]\nx = \"1\"\nx = \"2\"\n", "a = 1\n[a]\n", "x = \"\"\"long\"\"\"\n", "x = [1, 2\n",
                            "x = \"1\" y\n", "[data\n", "= 1\n"}) {
        CHECK(!to_json(Format::Toml, bad, json, problem));
        CHECK(problem == "not-toml");
    }
    // XML: another root, a missing type, an unknown entity, text after the end, a CDATA section.
    for (const char *bad : {"<preset><data/></preset>", "<export><data><item>1</item></data></export>",
                            "<export><data><item type=\"string\">&nbsp;</item></data></export>",
                            "<export><data><item type=\"object\"></item></data></export>trailing",
                            "<export><data><item type=\"string\"><![CDATA[x]]></item></data></export>"}) {
        CHECK(!to_json(Format::Xml, bad, json, problem));
        CHECK(problem == "not-xml");
    }
    CHECK(!to_json(Format::Unknown, "anything", json, problem));
    CHECK(problem == "unknown-format");
    // JSON passes through untouched for plan_import to judge.
    CHECK(to_json(Format::Json, "{\"app\":{}}", json, problem) && json == "{\"app\":{}}");
}

static void test_detect()
{
    CHECK(detect("prefs.json", "") == Format::Json);
    CHECK(detect("prefs.YAML", "") == Format::Yaml);
    CHECK(detect("prefs.yml", "") == Format::Yaml);
    CHECK(detect("prefs.toml", "") == Format::Toml);
    CHECK(detect("prefs.Xml", "") == Format::Xml);
    CHECK(detect("prefs.txt", "  {\"app\": {}}") == Format::Json);
    CHECK(detect("prefs.txt", "<?xml version=\"1.0\"?>") == Format::Xml);
    CHECK(detect("prefs.txt", "# comment\n%YAML 1.2\n") == Format::Yaml);
    CHECK(detect("prefs.txt", "[data.app]\n") == Format::Toml);
    CHECK(detect("prefs.txt", "schema = \"x\"\n") == Format::Toml);
    CHECK(detect("prefs.txt", "app:\n  x: 1\n") == Format::Yaml);
    CHECK(detect("prefs", "hello") == Format::Unknown);
    CHECK(std::string(name(Format::Toml)) == "TOML");
}

int main()
{
    test_exports();
    test_hand_edits();
    test_refused();
    test_detect();
    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("preference formats: all checks passed\n");
    return 0;
}
