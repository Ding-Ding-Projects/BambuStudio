// Standalone assertions for src/slic3r/GUI/DoorChime/DoorChime.hpp: the chime
// library, the G-code each chime sends, the door edge detector and the rule
// that keeps chimes away from a running print. Build without NDEBUG (this file
// undefines it), for example:
//   g++ -std=c++17 -Wall -Wextra -Werror tests/door_chime_test.cpp -o dct && ./dct
// tests/door_chime.test.mjs compiles and runs it.
#undef NDEBUG
#include "../src/slic3r/GUI/DoorChime/DoorChime.hpp"
#include <cassert>
#include <iostream>
#include <set>
#include <string>

using namespace Slic3r::GUI::DoorChime;
static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

static size_t count(const std::string &text, const std::string &what)
{
    size_t n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) ++n;
    return n;
}

static void library_assertions(const std::vector<Chime> &list)
{
    CHECK(list.size() >= 6);
    std::set<std::string> ids, names;
    size_t silent = 0;
    for (const Chime &chime : list) {
        CHECK(ids.insert(chime.id).second);     // ids are unique: they are stored in the config
        CHECK(names.insert(chime.name).second); // names are unique: they are what the user picks from
        if (is_silent(chime)) { ++silent; CHECK(std::string(chime.id) == SILENT_ID); continue; }
        CHECK(chime.notes.size() <= 8); // a chime, not a song
        for (const Note &note : chime.notes) {
            CHECK(note.pitch >= 0 && note.pitch <= 127);
            CHECK(note.ticks > 0 && note.ticks <= 40);
        }
        CHECK(chime.notes.front().pitch > 0); // a chime starts on a sound, not a rest
    }
    CHECK(silent == 1);
    CHECK(!is_silent(list.front())); // the default plays something
    CHECK(&find_chime(list, "") == &list.front());
    CHECK(&find_chime(list, "no_such_chime") == &list.front());
    CHECK(&find_chime(list, list.back().id) == &list.back());
}

static void gcode_assertions()
{
    const Chime &ding = find_chime(open_chimes(), "ding_dong");
    const std::string gcode = chime_gcode(ding);
    // Motors on, queue drained, the tune between S1 and W, motors released.
    CHECK(gcode.rfind("M17\nM400 S1\nM1006 S1\n", 0) == 0);
    CHECK(gcode.find("M1006 W\nM18\n") != std::string::npos);
    CHECK(gcode.find("M1006 A76 B25 L100 C76 D25 M100 E76 F25 N100\n") != std::string::npos);
    CHECK(gcode.find("M1006 A72 B35 L100 C72 D35 M100 E72 F35 N100\n") != std::string::npos);
    CHECK(count(gcode, "M1006 A") == ding.notes.size());
    CHECK(gcode.find(';') == std::string::npos); // live commands carry no comments

    // A rest keeps every voice quiet for its length.
    const std::string shop = chime_gcode(find_chime(open_chimes(), "shop_bell"));
    CHECK(shop.find("M1006 A0 B4 L0 C0 D4 M0 E0 F4 N0\n") != std::string::npos);

    CHECK(chime_gcode(find_chime(open_chimes(), SILENT_ID)).empty());
    CHECK(chime_gcode(find_chime(close_chimes(), SILENT_ID)).empty());
    for (const Chime &chime : close_chimes())
        if (!is_silent(chime)) CHECK(count(chime_gcode(chime), "M1006 A") == chime.notes.size());
}

static void edge_assertions()
{
    EdgeDetector d;
    // The first reading only records the state: an open door at connection is quiet.
    CHECK(d.observe("p1", 1) == Edge::None);
    CHECK(d.observe("p1", 1) == Edge::None);
    CHECK(d.observe("p1", 0) == Edge::Closed);
    CHECK(d.observe("p1", 0) == Edge::None);
    CHECK(d.observe("p1", 1) == Edge::Opened);
    // An unknown reading neither fires nor forgets.
    CHECK(d.observe("p1", -1) == Edge::None);
    CHECK(d.observe("p1", 0) == Edge::Closed);
    // Switching printers starts over.
    CHECK(d.observe("p2", 1) == Edge::None);
    CHECK(d.observe("p2", 0) == Edge::Closed);
    CHECK(d.observe("p1", 1) == Edge::None);

    EdgeDetector unknown_first;
    CHECK(unknown_first.observe("p1", -1) == Edge::None);
    CHECK(unknown_first.observe("p1", 0) == Edge::None);
    CHECK(unknown_first.observe("p1", 1) == Edge::Opened);
}

static void status_assertions()
{
    for (const char *busy : {"RUNNING", "PAUSE", "PREPARE", "SLICING"}) CHECK(!may_play(busy));
    for (const char *idle : {"IDLE", "FINISH", "FAILED", "INIT", ""}) CHECK(may_play(idle));
}

int main()
{
    library_assertions(open_chimes());
    library_assertions(close_chimes());
    gcode_assertions();
    edge_assertions();
    status_assertions();
    std::cout << "door chime: " << assertions << " assertions passed\n";
    return 0;
}
