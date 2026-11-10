#ifndef slic3r_GCode_LubricateRoutine_hpp_
#define slic3r_GCode_LubricateRoutine_hpp_

#include <string>

namespace Slic3r {

// Lubricate mode: run the motion system through its whole travel so fresh grease
// spreads along the rails, lead screws and bearings. The bed travels from near the
// nozzle to the bottom and back, the toolhead runs the full length of X and of Y
// and the diagonals, and on Bambu Lab printers the buzzer beeps like a truck
// backing up while the bed moves.
struct LubricateArea
{
    double min_x = 0.;
    double min_y = 0.;
    double max_x = 0.;
    double max_y = 0.;
    double max_z = 0.;
    // Bambu Lab printers play notes with M1006; other firmware gets no sound.
    bool buzzer = false;
};

// The routine on its own. It assumes a homed machine and leaves it homed again
// with `rehome` (a whole G28 line), or with a plain G28 when that is empty.
std::string lubricate_routine_gcode(const LubricateArea &area, const std::string &rehome = std::string());

// The start G-code with the routine inserted after its last full homing line
// (G28, not a G28.<n> calibration variant, and not inside an M622/M623 firmware
// conditional that may be skipped), so the routine starts from a homed
// machine and the rest of the start G-code finds the state it expects. Without a
// homing line the routine homes first and is placed at the top.
std::string insert_lubricate_routine(const std::string &start_gcode, const LubricateArea &area);

} // namespace Slic3r

#endif // slic3r_GCode_LubricateRoutine_hpp_
