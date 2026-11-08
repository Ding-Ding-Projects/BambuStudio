// Standalone assertions for src/slic3r/GUI/Extensions/ExtensionContext.cpp:
// the slice, printer and model sections an extension receives (grams, metres,
// cost, warnings, only the file's name), the sample requests, and adding
// comment lines to an exported G-code file after its header block, keeping
// the file's line endings, refusing anything but comments and leaving the
// original untouched when something goes wrong. Compiled and run by
// tests/extension_context.test.mjs.
#include "src/slic3r/GUI/Extensions/ExtensionCore.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionContext.cpp"

#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

using namespace Slic3r::GUI::Extensions;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static void write(const fs::path &path, const std::string &text) { std::ofstream(path, std::ios::binary | std::ios::trunc) << text; }
static std::string read(const fs::path &path)
{
    std::ifstream      in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

static void sections()
{
    SliceFacts f;
    f.plate      = 2;
    f.plate_name = "Brackets";
    f.seconds    = 3723.4;
    f.layers     = 245;
    f.bed_type   = "textured_plate";
    f.filaments  = {{1, "PLA", "#00AE42", 10000.0, 1.24, 1.75, 20.0}, {3, "PETG", "", 2405.28, 0, 1.75, 0}};
    f.warnings   = {{2, "1000C001", "bad"}, {0, "", "tip"}};
    f.file       = "C:/Users/someone/Desktop/private folder/bracket.gcode";
    const nlohmann::json s = slice_section(f);
    CHECK(s["plate"] == 2 && s["plateName"] == "Brackets" && s["seconds"] == 3723 && s["layers"] == 245);
    CHECK(s["filaments"].size() == 2 && s["filaments"][0]["grams"] == 12.4 && s["filaments"][0]["cost"] == 0.25);
    // 10000 mm3 of 1.75 mm filament is 4.16 m.
    CHECK(s["filaments"][0]["meters"] == 4.16 && s["filaments"][0]["color"] == "#00AE42");
    // A preset without density weighs nothing it cannot know, and has no cost.
    CHECK(s["filaments"][1]["grams"] == 0.0 && !s["filaments"][1].contains("cost") && !s["filaments"][1].contains("color"));
    CHECK(s["totalCost"] == 0.25 && s["warnings"][0]["level"] == "error" && s["warnings"][1]["level"] == "tip");
    // Only the file's name, never the folder it went to.
    CHECK(s["file"] == "bracket.gcode");
    CHECK(s.dump().find("someone") == std::string::npos);

    const nlohmann::json p = printer_section({"Bambu Lab X1 Carbon 0.4 nozzle", "Bambu Lab X1 Carbon", {0.4}, {"Bambu PLA Basic"}});
    CHECK(p["model"] == "Bambu Lab X1 Carbon" && p["nozzles"][0] == 0.4 && p["filamentPresets"][0] == "Bambu PLA Basic");

    std::vector<ModelObjectFacts> objects;
    for (int i = 0; i < 205; ++i) objects.push_back({"part " + std::to_string(i), 1, 10, {{1, 2, 3}}, i < 2});
    const nlohmann::json m = model_section(objects, 1);
    CHECK(m["objects"].size() == 200 && m["objectCount"] == 205 && m["selectedCount"] == 2 && m["triangles"] == 2000);

    // Samples are marked in every section, so an extension can tell.
    for (Hook h : {Hook::PlateSliced, Hook::GcodeExported, Hook::Command}) {
        const nlohmann::json c = sample_context(h);
        CHECK(c["slice"]["sample"] == true && c["printer"]["sample"] == true && c["model"]["sample"] == true);
        CHECK((h == Hook::GcodeExported) == c["slice"].contains("file"));
    }
}

static void comment_lines(const fs::path &dir)
{
    const fs::path file = dir / "plate.gcode";
    // Bambu Studio's header block stays first; the start lines follow it.
    write(file, "; HEADER_BLOCK_START\n; total layer number: 3\n; HEADER_BLOCK_END\n\nG28\nG1 X1\n");
    std::string problem;
    CHECK(add_comment_lines(file, {{"com.example.stamp", "1.0.0", {"; stamped"}, {"; end of stamp"}}}, problem));
    CHECK(read(file) == "; HEADER_BLOCK_START\n; total layer number: 3\n; HEADER_BLOCK_END\n"
                        "; added by extension com.example.stamp 1.0.0\n; stamped\n"
                        "\nG28\nG1 X1\n"
                        "; added by extension com.example.stamp 1.0.0\n; end of stamp\n");
    CHECK(!fs::exists(dir / "plate.gcode.partial"));

    // No header: the very start. Windows line endings are kept, and a last line without one is ended first.
    write(file, "G28\r\nG1 X1");
    CHECK(add_comment_lines(file, {{"a.b", "2.0.0", {"; one", "; two"}, {}}, {"c.d", "0.1.0", {}, {"; tail"}}}, problem));
    CHECK(read(file) == "; added by extension a.b 2.0.0\r\n; one\r\n; two\r\nG28\r\nG1 X1\r\n; added by extension c.d 0.1.0\r\n; tail\r\n");

    // Nothing to add changes nothing.
    write(file, "G28\n");
    CHECK(add_comment_lines(file, {{"a.b", "1.0.0", {}, {}}}, problem) && read(file) == "G28\n");

    // Anything but a comment line is refused, and the file stays as it was.
    CHECK(!add_comment_lines(file, {{"a.b", "1.0.0", {"G1 Z50"}, {}}}, problem) && problem == "not-comment");
    CHECK(!add_comment_lines(file, {{"a.b", "1.0.0", {}, {"; ok\nG1 Z50"}}}, problem) && problem == "not-comment");
    CHECK(read(file) == "G28\n");

    CHECK(!add_comment_lines(dir / "missing.gcode", {{"a.b", "1.0.0", {"; x"}, {}}}, problem) && problem == "unreadable");

    // A large file is copied whole.
    std::string big = "; HEADER_BLOCK_START\n; HEADER_BLOCK_END\n";
    while (big.size() < (std::size_t(3) << 20)) big += "G1 X10.000 Y10.000 E0.01234\n";
    write(file, big);
    CHECK(add_comment_lines(file, {{"a.b", "1.0.0", {"; s"}, {"; e"}}}, problem));
    const std::string after = read(file);
    CHECK(after.size() == big.size() + 2 * std::string("; added by extension a.b 1.0.0\n").size() + 8);
    CHECK(after.compare(0, 40, "; HEADER_BLOCK_START\n; HEADER_BLOCK_END\n") == 0);
    CHECK(after.compare(after.size() - 4, 4, "; e\n") == 0);
}

int main()
{
    const fs::path dir = fs::temp_directory_path() / ("ext-context-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    sections();
    comment_lines(dir);
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("extension context: all checks passed\n");
    return 0;
}
