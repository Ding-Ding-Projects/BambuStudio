// Standalone assertions for src/slic3r/GUI/Extensions/ExtensionContext.cpp:
// the slice, printer, model and send sections an extension receives (grams,
// metres, cost, warnings, only the file's name, every plate added up, where
// each filament goes), the sample requests, and adding
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
    f.warnings   = {{2, "1000C001", "bad", 0}, {0, "", "tip", 0}};
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

    // model.imported: only the objects that arrived, the files' names (never a folder), millimetres.
    std::vector<std::string> files;
    for (int i = 0; i < 55; ++i) files.push_back("C:/Users/someone/private folder/part" + std::to_string(i) + ".stl");
    const nlohmann::json imported = imported_section({{"part 0", 1, 120, {{10, 20, 30}}, false}}, 2, files);
    CHECK(imported["imported"] == true && imported["units"] == "mm" && imported["plate"] == 2 && imported["objectCount"] == 1);
    CHECK(imported["files"].size() == 50 && imported["files"][0] == "part0.stl");
    CHECK(imported.dump().find("someone") == std::string::npos);

    // print.finished: how a print ended, never which printer it was.
    nlohmann::json ended = print_section(PrintEndFacts{"failed", "Bambu Lab X1 Carbon", 3723.6, true, 245, 0x0300400C});
    CHECK(ended["result"] == "failed" && ended["seconds"] == 3724 && ended["seenFromStart"] == true && ended["layers"] == 245);
    CHECK(ended["error"] == "0300400C" && ended["printerModel"] == "Bambu Lab X1 Carbon");
    ended = print_section(PrintEndFacts{"finished", "X", -1, true, -3, 0x0300400C});
    CHECK(ended["seconds"].is_null() && ended["seenFromStart"] == false && ended["layers"] == 0 && !ended.contains("error"));
    CHECK(print_section(PrintEndFacts{"exploded", "X", 1, false, 1, 0})["result"] == "");
    std::vector<std::string> ended_keys;
    for (auto it = ended.begin(); it != ended.end(); ++it) ended_keys.push_back(it.key());
    CHECK(ended_keys == std::vector<std::string>({"layers", "printerModel", "result", "seconds", "seenFromStart"}));

    // Samples are marked in every section, so an extension can tell.
    for (Hook h : {Hook::PlateSliced, Hook::GcodeExported, Hook::Command, Hook::PrintBeforeSend, Hook::ModelImported, Hook::PrintFinished}) {
        const nlohmann::json c = sample_context(h);
        CHECK(c["slice"]["sample"] == true && c["printer"]["sample"] == true && c["model"]["sample"] == true);
        CHECK((h == Hook::GcodeExported) == c["slice"].contains("file"));
        CHECK((h == Hook::PrintBeforeSend) == c.contains("send"));
        if (c.contains("send")) CHECK(c["send"]["sample"] == true && c["send"]["filaments"].size() == 2);
        CHECK((h == Hook::ModelImported) == c["model"].contains("imported"));
        CHECK((h == Hook::PrintFinished) == c.contains("print"));
        if (c.contains("print")) CHECK(c["print"]["sample"] == true && c["print"]["result"] == "finished");
    }
}

static void send()
{
    CHECK(normalise_color("#00ae42") == "#00AE42" && normalise_color("00AE42FF") == "#00AE42" && normalise_color("#F4EE2A80") == "#F4EE2A");
    CHECK(normalise_color("").empty() && normalise_color("green").empty() && normalise_color("#12345").empty() && normalise_color("#GG0000").empty());

    SendFacts f;
    f.plate         = 2;
    f.printer_model = "Bambu Lab X1 Carbon";
    f.connection    = "lan";
    f.options       = {{"bed_leveling", "auto"}, {"timelapse", "on"}, {"Bad Id", "x"}, {"flow_cali", std::string(41, 'x')}};
    f.filaments     = {{1, "PLA", "00AE42FF", "A1", "PLA", "00AE42FF", 61}, {2, "PETG", "#F4EE2A", "", "", "", 40}, {3, "PLA", "#000000", "Ext", "PLA", "", 140}};
    const nlohmann::json s = send_section(f);
    CHECK(s["plate"] == 2 && s["allPlates"] == false && s["printerModel"] == "Bambu Lab X1 Carbon" && s["connection"] == "lan");
    CHECK(s["slicedForThisModel"] == true);
    // Option ids are plain, values short; anything else is left out.
    CHECK(s["options"].size() == 2 && s["options"]["timelapse"] == "on" && s["options"]["bed_leveling"] == "auto");
    CHECK(s["filaments"][0]["color"] == "#00AE42" && s["filaments"][0]["tray"] == "A1" && s["filaments"][0]["trayColor"] == "#00AE42");
    CHECK(s["filaments"][0]["remainPercent"] == 61);
    // Not mapped: no tray details, whatever was passed.
    CHECK(s["filaments"][1]["tray"] == "" && !s["filaments"][1].contains("trayType") && !s["filaments"][1].contains("remainPercent"));
    // A remain outside 0 to 100 is unknown.
    CHECK(s["filaments"][2]["remainPercent"] == -1 && s["filaments"][2]["trayColor"] == "");
    // Only the two connections there are.
    f.connection = "usb";
    CHECK(send_section(f)["connection"] == "");
    f.all_plates = true;
    CHECK(send_section(f)["plate"] == 0 && send_section(f)["allPlates"] == true);
    // Nothing names the printer itself: the section has exactly these keys.
    const std::vector<std::string> keys = {"allPlates", "connection", "filaments", "options", "plate", "printerModel", "slicedForThisModel"};
    std::vector<std::string>       found;
    for (auto it = s.begin(); it != s.end(); ++it) found.push_back(it.key());
    CHECK(found == keys);

    // Every plate as one.
    SliceFacts a, b;
    a.plate = 1; a.seconds = 3600; a.prepare_seconds = 300; a.layers = 100; a.bed_type = "textured_plate"; a.filament_changes = 2;
    a.filaments = {{1, "PLA", "#00AE42", 1000.0, 1.24, 1.75, 20.0}, {2, "PETG", "", 500.0, 1.27, 1.75, 0}};
    a.warnings  = {{1, "", "first", 0}};
    b.plate = 2; b.seconds = 1800; b.prepare_seconds = 200; b.layers = 50; b.bed_type = "cool_plate"; b.filament_changes = 1;
    b.filaments = {{1, "PLA", "#00AE42", 2000.0, 1.24, 1.75, 20.0}, {4, "TPU", "", 100.0, 1.2, 1.75, 0}};
    b.warnings  = {{2, "C1", "second", 0}};
    const SliceFacts all = combine_plates({a, b});
    CHECK(all.plate == 0 && all.plate_count == 2 && all.seconds == 5400 && all.prepare_seconds == 500 && all.layers == 150);
    CHECK(all.filament_changes == 3 && all.bed_type.empty());
    CHECK(all.filaments.size() == 3 && all.filaments[0].slot == 1 && all.filaments[0].volume_mm3 == 3000.0 && all.filaments[2].slot == 4);
    CHECK(all.warnings.size() == 2 && all.warnings[0].plate == 1 && all.warnings[1].plate == 2);
    const nlohmann::json js = slice_section(all);
    CHECK(js["allPlates"] == true && js["plateCount"] == 2 && js["warnings"][1]["plate"] == 2 && js["warnings"][1]["code"] == "C1");
    // One plate stays itself, without the all-plates fields.
    const nlohmann::json one = slice_section(combine_plates({a}));
    CHECK(one["plate"] == 1 && !one.contains("allPlates") && !one["warnings"][0].contains("plate"));
    b.bed_type = "textured_plate";
    CHECK(combine_plates({a, b}).bed_type == "textured_plate");
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
    send();
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
