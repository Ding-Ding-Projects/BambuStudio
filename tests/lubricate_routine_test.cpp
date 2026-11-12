// Standalone assertions for src/libslic3r/GCode/LubricateRoutine.cpp: where the
// lubricate routine goes in every Bambu Lab start G-code template, the H2C
// homing window it must stay out of, and the moves and notes it writes. Build
// without NDEBUG (this file undefines it) and run it from the repository root:
//   g++ -std=c++17 -I. tests/lubricate_routine_test.cpp -o lrt && ./lrt
// tests/lubricate_routine.test.mjs compiles and runs it.
#undef NDEBUG
#include "../src/libslic3r/GCode/LubricateRoutine.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace Slic3r;
static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

// The machine_start_gcode string of a template file, unescaped. Enough JSON for
// these files: \n, \t, \", \\ and \/.
static std::string start_gcode_of(const std::filesystem::path &file)
{
    std::ifstream in(file, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const size_t key = text.find("\"machine_start_gcode\"");
    if (key == std::string::npos) return std::string();
    size_t i = text.find('"', text.find(':', key)) + 1;
    std::string out;
    for (; i < text.size() && text[i] != '"'; ++i) {
        if (text[i] != '\\') { out += text[i]; continue; }
        const char c = text[++i];
        out += c == 'n' ? '\n' : c == 't' ? '\t' : c;
    }
    return out;
}

static size_t line_of(const std::string &text, const std::string &what, size_t from = 0)
{
    const size_t at = text.find(what, from);
    return at == std::string::npos ? std::string::npos : size_t(std::count(text.begin(), text.begin() + at, '\n'));
}

static LubricateArea h2c_area()
{
    LubricateArea area;
    area.max_x  = 325;
    area.max_y  = 320;
    area.max_z  = 325;
    area.buzzer = true;
    return area;
}

static void h2c_assertions(const std::string &start)
{
    const std::string out = insert_lubricate_routine(start, h2c_area());
    const size_t routine  = line_of(out, ";===== lubricate mode: full travel of every axis =====");
    const size_t open     = line_of(out, "M1009 Q1 L1");
    const size_t home     = line_of(out, "G28 Z P0 T250");
    const size_t close    = line_of(out, "M1009 Q1 L0");
    const size_t end      = line_of(out, ";===== first homing end =====");
    CHECK(routine != std::string::npos);
    // The printer's own homing window closes before the routine starts...
    CHECK(open < home && home < close && close < routine && routine < end);
    // ...and the routine homes again inside the same window before the rest of the start.
    const size_t reopen  = line_of(out, "M1009 Q1 L1", out.find("; lubricate melody"));
    const size_t rehome  = line_of(out, "G28 Z P0 T250 ; home again after lubricate mode");
    const size_t reclose = line_of(out, "M1009 Q1 L0", out.find("home again after lubricate mode"));
    const size_t done    = line_of(out, ";===== lubricate mode end =====");
    CHECK(routine < reopen && reopen + 1 == rehome && rehome + 1 == reclose && reclose + 1 == done && done < end);
    // The bed moves with a beep before each segment, the toolhead covers X, Y and both diagonals.
    CHECK(out.find("G1 Z5.00 F1500") != std::string::npos);
    CHECK(out.find("G1 Z320.00 F1500") != std::string::npos);
    CHECK(out.find("M1006 A83 B20 L99 C83 D20 M99 E83 F20 N99") != std::string::npos);
    CHECK(out.find("G1 X320.00 Y315.00") != std::string::npos);
    // Nothing of the template is lost, and no homing window is left open.
    CHECK(out.size() > start.size());
    size_t opens = 0, closes = 0;
    for (size_t at = out.find("M1009 Q1 L1"); at != std::string::npos; at = out.find("M1009 Q1 L1", at + 1)) ++opens;
    for (size_t at = out.find("M1009 Q1 L0"); at != std::string::npos; at = out.find("M1009 Q1 L0", at + 1)) ++closes;
    CHECK(opens == closes);
}

static void plain_assertions()
{
    // Without a homing window the routine follows the last homing line, as before.
    const std::string start = "M17\nG28 ; home\nM104 S200\n";
    const std::string out = insert_lubricate_routine(start, h2c_area());
    CHECK(out.rfind("M17\nG28 ; home\n;===== lubricate mode", 0) == 0);
    CHECK(out.find("M400\nG28 ; home again after lubricate mode\n;===== lubricate mode end =====\nM104 S200\n") != std::string::npos);

    // A conditional homing line is skipped.
    const std::string conditional = "G28 X\nM622 J0\n    G28 R\nM623\nM104 S200\n";
    const std::string out2 = insert_lubricate_routine(conditional, h2c_area());
    CHECK(out2.rfind("G28 X\n;===== lubricate mode", 0) == 0);

    // No homing at all: the routine homes first.
    const std::string out3 = insert_lubricate_routine("M104 S200\n", h2c_area());
    CHECK(out3.rfind("G28 ; home before lubricate mode\n", 0) == 0);
}

int main(int argc, char **argv)
{
    const std::filesystem::path root = argc > 1 ? argv[1] : ".";
    const std::filesystem::path machines = root / "resources" / "profiles" / "BBL" / "machine";
    size_t templates = 0;
    for (const auto &entry : std::filesystem::directory_iterator(machines)) {
        const std::string name = entry.path().filename().string();
        if (name.find("template machine_start_gcode") == std::string::npos) continue;
        const std::string start = start_gcode_of(entry.path());
        if (start.empty()) continue;
        ++templates;
        const std::string out = insert_lubricate_routine(start, h2c_area());
        // Every template gets exactly one routine and keeps its own text.
        CHECK(out.find(";===== lubricate mode: full travel of every axis =====") != std::string::npos);
        CHECK(out.find(";===== lubricate mode end =====") != std::string::npos);
        CHECK(out.size() > start.size());
        if (name.find("H2C") != std::string::npos) h2c_assertions(start);
    }
    CHECK(templates >= 10);
    plain_assertions();
    std::cout << "lubricate routine: " << templates << " templates, " << assertions << " assertions passed\n";
    return 0;
}
