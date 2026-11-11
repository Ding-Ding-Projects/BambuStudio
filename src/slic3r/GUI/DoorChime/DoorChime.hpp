#ifndef slic3r_GUI_DoorChime_hpp_
#define slic3r_GUI_DoorChime_hpp_

// Door chime: the printer plays a short tune through its M1006 buzzer when its
// door opens, and another when it closes. This header is the part without
// wxWidgets or the device layer (the chime library, the G-code, the door edge
// detector and the rule for when a chime may play), so that
// tests/door_chime_test.cpp can run it on its own.

#include "../Melody/MelodyModel.hpp"

#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace DoorChime {

// AppConfig keys. The feature is off until the user turns it on, because the
// door reading comes from a community-documented status bit.
inline constexpr const char *ENABLED_KEY = "door_chime_enabled";
inline constexpr const char *OPEN_KEY    = "door_chime_open";
inline constexpr const char *CLOSE_KEY   = "door_chime_close";

inline constexpr const char *SILENT_ID = "silent";

// One note of a chime: a MIDI-style pitch (69 = A4 = 440 Hz, 0 = rest) and its
// length in buzzer ticks.
struct Note
{
    int pitch;
    int ticks;
};

struct Chime
{
    const char       *id;   // stored in the AppConfig key
    const char       *name; // English display name, translated by the GUI
    std::vector<Note> notes;
};

// Chimes for the door opening, the default first. They tend to rise, a hello.
inline const std::vector<Chime> &open_chimes()
{
    // C5 = 72, D5 = 74, E5 = 76, F5 = 77, G5 = 79, A5 = 81, B5 = 83, C6 = 84, E6 = 88.
    static const std::vector<Chime> chimes{
        // The two-note doorbell, high then low.
        {"ding_dong", "Ding-dong", {{76, 25}, {72, 35}}},
        {"welcome_rise", "Welcome rise", {{72, 8}, {76, 8}, {79, 8}, {84, 20}}},
        // Two quick high pings, like the bell over a shop door.
        {"shop_bell", "Shop bell", {{88, 6}, {0, 4}, {88, 14}}},
        // Westminster Quarters, first phrase (public domain).
        {"westminster_open", "Westminster, first phrase", {{76, 15}, {72, 15}, {74, 15}, {67, 30}}},
        {"hello_chirp", "Hello chirp", {{79, 6}, {84, 12}}},
        {"marimba_up", "Marimba up", {{72, 6}, {74, 6}, {76, 6}, {79, 6}, {81, 16}}},
        {"single_ping", "Single ping", {{84, 12}}},
        {SILENT_ID, "Silent", {}},
    };
    return chimes;
}

// Chimes for the door closing, the default first. They tend to fall and settle.
inline const std::vector<Chime> &close_chimes()
{
    static const std::vector<Chime> chimes{
        // The doorbell turned around: low then high, then home.
        {"dong_ding", "Dong-ding", {{72, 20}, {76, 30}}},
        {"falling_arpeggio", "Falling arpeggio", {{84, 8}, {79, 8}, {76, 8}, {72, 20}}},
        // Westminster Quarters, the phrase that ends on the home note (public domain).
        {"westminster_close", "Westminster, closing phrase", {{67, 15}, {74, 15}, {76, 15}, {72, 30}}},
        {"goodnight", "Goodnight", {{76, 14}, {74, 14}, {72, 28}}},
        {"soft_latch", "Soft latch", {{60, 6}, {0, 3}, {55, 10}}},
        {"double_tap", "Double tap", {{72, 5}, {0, 4}, {72, 5}}},
        {"bye_chirp", "Bye chirp", {{84, 6}, {79, 12}}},
        {SILENT_ID, "Silent", {}},
    };
    return chimes;
}

// The chime with this id in `list`, or the first one (the default) when the id
// is unknown or empty.
inline const Chime &find_chime(const std::vector<Chime> &list, const std::string &id)
{
    for (const Chime &chime : list)
        if (id == chime.id) return chime;
    return list.front();
}

inline bool is_silent(const Chime &chime) { return chime.notes.empty(); }

// The chime as a melody for the Melody creator's G-code writer: every note on
// all three buzzer voices for a fuller sound, the layout the lubricate tunes use.
inline Melody chime_melody(const Chime &chime)
{
    Melody melody;
    melody.name = chime.name;
    for (const Note &note : chime.notes) {
        MelodyStep step;
        for (MelodyVoice &voice : step.voices) {
            voice.pitch  = note.pitch;
            voice.ticks  = note.ticks;
            voice.volume = note.pitch > 0 ? 100 : 0;
        }
        melody.steps.push_back(step);
    }
    return melody;
}

// What to send to the printer for this chime as a live `gcode_line` command,
// or an empty string for a silent chime.
inline std::string chime_gcode(const Chime &chime)
{
    return is_silent(chime) ? std::string() : melody_to_printer_gcode(chime_melody(chime));
}

// A chime only plays while the printer is not printing: the tune turns the
// motors on and releases them afterwards (M17 / M18) and waits for the motion
// queue, which must never happen in the middle of a print.
inline bool may_play(const std::string &print_status)
{
    return print_status != "RUNNING" && print_status != "PAUSE" && print_status != "PREPARE" &&
           print_status != "SLICING";
}

enum class Edge { None, Opened, Closed };

// Turns door readings into open and close edges. The first reading of a
// printer, and the first after switching printers, only records the state, so
// connecting to a printer whose door stands open stays quiet. A reading of -1
// (no sensor, or not reported yet) is ignored and keeps the last known state.
class EdgeDetector
{
public:
    Edge observe(const std::string &machine, int door_open)
    {
        if (machine != m_machine) {
            m_machine = machine;
            m_state   = -1;
        }
        if (door_open != 0 && door_open != 1) return Edge::None;
        const int was = m_state;
        m_state       = door_open;
        if (was == -1 || was == door_open) return Edge::None;
        return door_open == 1 ? Edge::Opened : Edge::Closed;
    }

private:
    std::string m_machine;
    int         m_state = -1;
};

}}} // namespace Slic3r::GUI::DoorChime

#endif // slic3r_GUI_DoorChime_hpp_
