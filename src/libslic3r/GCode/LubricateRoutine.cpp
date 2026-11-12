#include "LubricateRoutine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace Slic3r {

namespace {

// Distance kept from every edge of the bed and from the nozzle, in mm.
constexpr double edge_margin   = 5.;
constexpr double z_near_nozzle = 5.;
// Bed segment between two beeps, in mm, and the bed speed (mm/min): 25 mm at
// 25 mm/s gives roughly one beep a second, the cadence of a reversing truck.
constexpr double z_segment = 25.;
constexpr int    z_feed    = 1500;
constexpr int    xy_feed   = 12000;
constexpr int    cycles    = 2;

std::string fmt(double v)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << v;
    return out.str();
}

// One note on all three buzzer channels, the same channel layout as the start
// tune of the Bambu Lab profiles. A rest plays nothing.
std::string note(int midi)
{
    if (midi <= 0)
        return std::string();
    const std::string n = std::to_string(midi);
    return "M1006 S1\n"
           "M1006 A" + n + " B20 L99 C" + n + " D20 M99 E" + n + " F20 N99\n"
           "M1006 W\n";
}

// Move the bed from `from` to `to` in segments, playing the next note of the
// tune before each one. `next_note` carries the position across calls.
void bed_travel(std::ostringstream &out, double from, double to, const std::vector<int> *tune, size_t &next_note)
{
    const double length = std::abs(to - from);
    const int    steps  = std::max(1, int(std::ceil(length / z_segment)));
    for (int i = 1; i <= steps; ++i) {
        if (tune != nullptr && !tune->empty())
            out << note((*tune)[next_note++ % tune->size()]);
        out << "G1 Z" << fmt(from + (to - from) * i / steps) << " F" << z_feed << "\n";
    }
}

bool is_full_homing_line(const std::string &line)
{
    // G28 followed by the end of the line, a space, a comment or axis letters;
    // G28.140 and the like are calibration commands, not homing.
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos || line.compare(start, 3, "G28") != 0)
        return false;
    const size_t next = start + 3;
    return next == line.size() || line[next] == ' ' || line[next] == '\t' || line[next] == ';' || line[next] == '\r';
}

} // namespace

const std::vector<LubricateMelody> &lubricate_melodies()
{
    // MIDI notes: C5 = 72, D5 = 74, E5 = 76, F5 = 77, G5 = 79, A5 = 81, B5 = 83.
    static const std::vector<LubricateMelody> melodies{
        // About 1 kHz, the beeper of a reversing truck.
        {"truck_backup", "Truck backing up", {83}},
        {"two_tone_siren", "Two-tone siren", {81, 76}},
        {"rising_chirp", "Rising chirp", {72, 76, 79, 84}},
        // Beethoven, Symphony No. 9 (public domain).
        {"ode_to_joy", "Ode to Joy", {76, 76, 77, 79, 79, 77, 76, 74, 72, 72, 74, 76, 76, 74, 74, 0}},
        // Traditional (public domain).
        {"twinkle", "Twinkle, Twinkle, Little Star", {72, 72, 79, 79, 81, 81, 79, 0, 77, 77, 76, 76, 74, 74, 72, 0}},
        // Beethoven, Bagatelle No. 25 (public domain).
        {"fur_elise", "Fur Elise", {76, 75, 76, 75, 76, 71, 74, 72, 69, 0}},
        // The Westminster Quarters (public domain).
        {"westminster", "Westminster chimes", {76, 72, 74, 67, 0, 67, 74, 76, 72, 0}},
        {"silent", "Silent", {}},
    };
    return melodies;
}

const LubricateMelody &lubricate_melody(const std::string &id)
{
    for (const LubricateMelody &m : lubricate_melodies())
        if (id == m.id)
            return m;
    return lubricate_melodies().front();
}

std::string start_melody_gcode(const std::string &melody_id)
{
    const LubricateMelody &melody = lubricate_melody(melody_id);
    if (melody.notes.empty())
        return std::string();
    std::string out = "; start melody: " + std::string(melody.id) + "\n";
    for (int midi : melody.notes)
        out += note(midi);
    return out;
}

std::string lubricate_routine_gcode(const LubricateArea &area, const std::string &rehome)
{
    const double x0 = area.min_x + edge_margin, x1 = area.max_x - edge_margin;
    const double y0 = area.min_y + edge_margin, y1 = area.max_y - edge_margin;
    const double z0 = z_near_nozzle, z1 = std::max(z0, area.max_z - edge_margin);

    std::ostringstream out;
    out << ";===== lubricate mode: full travel of every axis =====\n";
    if (x1 <= x0 || y1 <= y0) {
        out << "; lubricate mode skipped: the printable area is too small\n";
        return out.str();
    }
    const LubricateMelody &melody = lubricate_melody(area.melody);
    const std::vector<int> *tune  = area.buzzer ? &melody.notes : nullptr;
    size_t                  next_note = 0;
    out << "; lubricate melody: " << melody.id << "\n";
    out << "G90\n";
    out << "G1 Z" << fmt(z0) << " F" << z_feed << "\n";
    for (int c = 0; c < cycles; ++c) {
        // Bed: all the way down and back up, with the melody playing as it moves.
        bed_travel(out, z0, z1, tune, next_note);
        bed_travel(out, z1, z0, tune, next_note);
        // Toolhead: the whole length of X and of Y, then both diagonals.
        out << "G1 X" << fmt(x0) << " Y" << fmt(y0) << " F" << xy_feed << "\n";
        out << "G1 X" << fmt(x1) << "\n";
        out << "G1 X" << fmt(x0) << "\n";
        out << "G1 Y" << fmt(y1) << "\n";
        out << "G1 Y" << fmt(y0) << "\n";
        out << "G1 X" << fmt(x1) << " Y" << fmt(y1) << "\n";
        out << "G1 X" << fmt(x0) << " Y" << fmt(y0) << "\n";
        out << "G1 X" << fmt(x1) << " Y" << fmt(y0) << "\n";
        out << "G1 X" << fmt(x0) << " Y" << fmt(y1) << "\n";
    }
    out << "M400\n";
    if (rehome.empty())
        out << "G28 ; home again after lubricate mode\n";
    else if (rehome.find('\n') != std::string::npos)
        out << rehome; // several lines, already commented and terminated
    else
        out << rehome << " ; home again after lubricate mode\n";
    out << ";===== lubricate mode end =====\n";
    return out.str();
}

std::string insert_lubricate_routine(const std::string &start_gcode, const LubricateArea &area)
{
    std::vector<std::string> lines;
    std::istringstream       in(start_gcode);
    for (std::string line; std::getline(in, line);)
        lines.push_back(line);

    // M622 ... M623 is a firmware conditional on Bambu Lab printers (the H2C start
    // G-code homes with `G28 R` inside one that only runs when bed leveling is
    // skipped), so a homing line inside it may never run and is not a place to start.
    int last_home = -1;
    int conditional_depth = 0;
    for (int i = 0; i < int(lines.size()); ++i) {
        const std::string &line  = lines[i];
        const size_t       start = line.find_first_not_of(" \t");
        if (start != std::string::npos && line.compare(start, 4, "M622") == 0)
            ++conditional_depth;
        else if (start != std::string::npos && line.compare(start, 4, "M623") == 0)
            conditional_depth = std::max(0, conditional_depth - 1);
        else if (conditional_depth == 0 && is_full_homing_line(line))
            last_home = i;
    }

    if (last_home < 0)
        return "G28 ; home before lubricate mode\n" + lubricate_routine_gcode(area) + start_gcode;

    // A line without its comment, its trailing blanks and its indentation.
    auto code_of = [](std::string line) {
        const size_t comment = line.find(';');
        if (comment != std::string::npos)
            line.erase(comment);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
            line.pop_back();
        const size_t start = line.find_first_not_of(" \t");
        return start == std::string::npos ? std::string() : line.substr(start);
    };
    std::string rehome       = code_of(lines[last_home]);
    int         insert_after = last_home;

    // The H2C homes Z inside `M1009 Q1 L1` ... `M1009 Q1 L0`. The command is not
    // documented, so the routine does not run inside that window: it goes after
    // the closing line, and homing again reopens the same window around the same
    // homing line, as the start G-code does.
    auto nearby_code = [&](int from, int step) {
        for (int i = from + step; i >= 0 && i < int(lines.size()) && std::abs(i - from) <= 8; i += step)
            if (!code_of(lines[i]).empty())
                return i;
        return -1;
    };
    const int close = nearby_code(last_home, 1);
    if (close >= 0 && code_of(lines[close]).rfind("M1009", 0) == 0) {
        int open = -1;
        for (int i = last_home - 1; i >= 0 && last_home - i <= 8; --i)
            if (code_of(lines[i]).rfind("M1009", 0) == 0) {
                open = i;
                break;
            }
        if (open >= 0) {
            rehome = code_of(lines[open]) + "\n" + rehome + " ; home again after lubricate mode\n" + code_of(lines[close]) + "\n";
            insert_after = close;
        }
    }

    std::string result;
    for (int i = 0; i < int(lines.size()); ++i) {
        result += lines[i];
        result += '\n';
        if (i == insert_after)
            result += lubricate_routine_gcode(area, rehome);
    }
    if (!start_gcode.empty() && start_gcode.back() != '\n' && !result.empty())
        result.pop_back();
    return result;
}

} // namespace Slic3r
