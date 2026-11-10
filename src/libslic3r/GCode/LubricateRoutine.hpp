#ifndef slic3r_GCode_LubricateRoutine_hpp_
#define slic3r_GCode_LubricateRoutine_hpp_

#include <string>
#include <vector>

namespace Slic3r {

// A tune for the buzzer while the bed moves: one note before each bed segment,
// repeating from the start when the travel is longer than the tune. Notes are
// MIDI note numbers (69 = A4, 440 Hz); 0 is a rest. Every tune is either a
// public-domain melody or a plain original pattern.
struct LubricateMelody
{
    const char      *id;   // stored in the `lubricate_melody` option
    const char      *name; // English display name, translated by the GUI
    std::vector<int> notes;
};

// The melody library, the default (truck backing up) first.
const std::vector<LubricateMelody> &lubricate_melodies();
// The melody with this id, or the default when the id is unknown or empty.
const LubricateMelody &lubricate_melody(const std::string &id);

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
    // Id of the tune played while the bed moves (see lubricate_melodies()).
    std::string melody;
};

// The whole melody once, for a print start without lubricate mode, so the
// chosen tune plays on every print. Empty for Silent.
std::string start_melody_gcode(const std::string &melody_id);

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
