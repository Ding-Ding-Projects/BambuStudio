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
// Bed and toolhead speeds (mm/min). Every stroke runs as one smooth move.
constexpr int z_feed  = 1200;
constexpr int xy_feed = 12000;
constexpr int cycles  = 2;
std::string fmt(double v)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << v;
    return out.str();
}

// One queued note on all three buzzer channels, the same line shape as the start
// tune of the Bambu Lab profiles. A rest queues silence of the same shape, so the
// rhythm of the tune survives.
std::string note(const LubricateNote &event)
{
    const std::string d = std::to_string(event.ticks);
    if (event.midi == 0) {
        return "M1006 A0 B" + d + " L0 C0 D" + d + " M0 E0 F" + d + " N0\n";
    }
    const std::string n = std::to_string(event.midi - 23);
    return "M1006 A" + n + " B" + d + " L99 C" + n + " D" + d + " M99 E" + n + " F" + d + " N99\n";
}

// The whole tune as one block: start the buffer once, queue every note, then play
// it and wait, exactly like the start tune in the Bambu Lab profiles. Queuing one
// note with a wait before every bed segment broke the tune into separate beeps and
// made the bed stop and start between segments.
std::string tune_block(const std::vector<LubricateNote> &notes)
{
    if (notes.empty())
        return std::string();
    std::string out = "M1006 S1\n";
    for (const auto &event : notes)
        out += note(event);
    out += "M1006 W\n";
    return out;
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
    // Explicit MIDI pitches and centisecond durations retain the score across both print paths.
    static const std::vector<LubricateMelody> melodies{
        // The beeper of a reversing truck: beep, pause, beep.
        {"truck_backup", "Truck backing up", {{71, 30}, {0, 45}, {71, 30}, {0, 45}, {71, 30}, {0, 45}, {71, 30}, {0, 45}}},
        {"two_tone_siren", "Two-tone siren", {{69, 30}, {64, 30}, {69, 30}, {64, 30}, {69, 30}, {64, 30}}},
        {"rising_chirp", "Rising chirp", {{60, 12}, {64, 12}, {67, 12}, {72, 24}, {0, 18}, {60, 12}, {64, 12}, {67, 12}, {72, 24}}},
        // Beethoven, Symphony No. 9 (public domain).
        {"ode_to_joy", "Ode to Joy", {{64, 40}, {64, 40}, {65, 40}, {67, 40}, {67, 40}, {65, 40}, {64, 40}, {62, 40}, {60, 40}, {60, 40}, {62, 40}, {64, 40}, {64, 60}, {62, 20}, {62, 80}}},
        // Traditional (public domain).
        {"twinkle", "Twinkle, Twinkle, Little Star", {{60, 40}, {60, 40}, {67, 40}, {67, 40}, {69, 40}, {69, 40}, {67, 80}, {65, 40}, {65, 40}, {64, 40}, {64, 40}, {62, 40}, {62, 40}, {60, 80}}},
        // Beethoven, Bagatelle No. 25 (public domain).
        {"fur_elise", "Fur Elise", {{76, 20}, {75, 20}, {76, 20}, {75, 20}, {76, 20}, {71, 20}, {74, 20}, {72, 20}, {69, 60}}},
        // The Westminster Quarters (public domain).
        {"westminster", "Westminster chimes", {{64, 55}, {60, 55}, {62, 55}, {55, 110}, {0, 55}, {55, 55}, {62, 55}, {64, 55}, {60, 110}}},
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
    return "; start melody: " + std::string(melody.id) + "\nM17\nM400 S1\n" + tune_block(melody.notes);
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
    const std::string      tune   = area.buzzer ? tune_block(melody.notes) : std::string();
    out << "; lubricate melody: " << melody.id << "\n";
    out << "G90\n";
    out << "G1 Z" << fmt(z0) << " F" << z_feed << "\n";
    for (int c = 0; c < cycles; ++c) {
        // The tune plays while the machine stands still, then every stroke runs in one move.
        out << tune;
        // Bed: all the way down and back up.
        out << "G1 Z" << fmt(z1) << " F" << z_feed << "\n";
        out << "G1 Z" << fmt(z0) << " F" << z_feed << "\n";
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
        out << "M400\n";
    }
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
    // the closing line, and homing again repeats the same window.
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
            // Repeat the whole window, not only its homing line: inside it the start
            // G-code lowers the bed and parks the toolhead over the bed before homing Z,
            // and homing Z from the corner where the routine ends misses the bed.
            rehome.clear();
            for (int i = open; i <= close; ++i) {
                const std::string code = code_of(lines[i]);
                if (code.empty())
                    continue;
                rehome += code;
                rehome += i == last_home ? " ; home again after lubricate mode\n" : "\n";
            }
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
