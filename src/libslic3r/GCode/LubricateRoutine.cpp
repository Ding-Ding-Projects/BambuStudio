#include "LubricateRoutine.hpp"

#include <algorithm>
#include <cmath>
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

// One reversing-truck beep: about 1 kHz (MIDI note 83, B5) on all three buzzer
// channels, the same channel layout as the start tune of the Bambu Lab profiles.
std::string beep()
{
    return "M1006 S1\n"
           "M1006 A83 B20 L99 C83 D20 M99 E83 F20 N99\n"
           "M1006 W\n";
}

// Move the bed from `from` to `to` in segments, with a beep before each one.
void bed_travel(std::ostringstream &out, double from, double to, bool buzzer)
{
    const double length = std::abs(to - from);
    const int    steps  = std::max(1, int(std::ceil(length / z_segment)));
    for (int i = 1; i <= steps; ++i) {
        if (buzzer)
            out << beep();
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
    out << "G90\n";
    out << "G1 Z" << fmt(z0) << " F" << z_feed << "\n";
    for (int c = 0; c < cycles; ++c) {
        // Bed: all the way down and back up, beeping like a reversing truck.
        bed_travel(out, z0, z1, area.buzzer);
        bed_travel(out, z1, z0, area.buzzer);
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
    out << (rehome.empty() ? std::string("G28") : rehome) << " ; home again after lubricate mode\n";
    out << ";===== lubricate mode end =====\n";
    return out.str();
}

std::string insert_lubricate_routine(const std::string &start_gcode, const LubricateArea &area)
{
    std::vector<std::string> lines;
    std::istringstream       in(start_gcode);
    for (std::string line; std::getline(in, line);)
        lines.push_back(line);

    int last_home = -1;
    for (int i = 0; i < int(lines.size()); ++i)
        if (is_full_homing_line(lines[i]))
            last_home = i;

    if (last_home < 0)
        return "G28 ; home before lubricate mode\n" + lubricate_routine_gcode(area) + start_gcode;

    std::string rehome = lines[last_home];
    const size_t comment = rehome.find(';');
    if (comment != std::string::npos)
        rehome.erase(comment);
    while (!rehome.empty() && (rehome.back() == ' ' || rehome.back() == '\t' || rehome.back() == '\r'))
        rehome.pop_back();

    std::string result;
    for (int i = 0; i < int(lines.size()); ++i) {
        result += lines[i];
        result += '\n';
        if (i == last_home)
            result += lubricate_routine_gcode(area, rehome);
    }
    if (!start_gcode.empty() && start_gcode.back() != '\n' && !result.empty())
        result.pop_back();
    return result;
}

} // namespace Slic3r
