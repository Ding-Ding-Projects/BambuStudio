#ifndef slic3r_GUI_PrintCooldown_hpp_
#define slic3r_GUI_PrintCooldown_hpp_

// Cool down after print: when a print ends, every fan is turned to full and
// stays there until the bed and the chamber are cool enough to open the door
// and take the part out, or until five minutes have passed. This header is the
// part without wxWidgets or the device layer (the rule for when a print has
// ended, the temperature rule, the G-code and the session that decides when
// the fans go on and off), so that tests/print_cooldown_test.cpp can run it on
// its own.

#include <string>

namespace Slic3r { namespace GUI { namespace PrintCooldown {

// AppConfig key. The option is off until the user turns it on.
inline constexpr const char *ENABLED_KEY = "print_cooldown_enabled";

// Cool enough to handle: a bed at or under 40 C lets most parts release and is
// safe to touch, and the same limit is used for the chamber air.
inline constexpr float kBedTargetC     = 40.0f;
inline constexpr float kChamberTargetC = 40.0f;

// The fans never run longer than this, whatever the temperatures say.
inline constexpr long long kTimeoutMs = 5LL * 60LL * 1000LL;

inline bool is_printing(const std::string &print_status)
{
    return print_status == "RUNNING" || print_status == "PAUSE" || print_status == "PREPARE" || print_status == "SLICING";
}

// A print that finished and one that failed or was stopped both leave a hot
// printer behind.
inline bool has_ended(const std::string &print_status) { return print_status == "FINISH" || print_status == "FAILED"; }

// A chamber reading of zero or less means the printer has no chamber sensor
// (or has not reported it yet), so only the bed decides.
inline bool cool_enough(float bed_c, float chamber_c)
{
    return bed_c <= kBedTargetC && (chamber_c <= 0.0f || chamber_c <= kChamberTargetC);
}

// What to send to the printer as a live `gcode_line` command: the part cooling
// fan (P1), the auxiliary fan (P2) and the chamber fan (P3), all to full or all
// off. A printer without one of these fans ignores its line.
inline std::string fans_gcode(bool on)
{
    const char *speed = on ? "255" : "0";
    std::string gcode;
    for (int fan = 1; fan <= 3; ++fan) gcode += "M106 P" + std::to_string(fan) + " S" + speed + "\n";
    return gcode;
}

enum class Action { None, FansOn, FansOff };
enum class Phase { Idle, Cooling, Cooled, TimedOut };

// Follows one printer's status reports and says when to turn the fans on and
// off. A cool-down starts only on the change from printing to ended that this
// session saw happen, so opening the app beside a printer that finished hours
// ago, or switching to another printer, starts nothing.
class Session
{
public:
    Action observe(const std::string &machine, const std::string &print_status, float bed_c, float chamber_c, long long now_ms, bool enabled)
    {
        if (machine != m_machine) {
            // Another printer: the fans of the one left behind can no longer be
            // reached from here, so the session simply starts over.
            m_machine      = machine;
            m_was_printing = false;
            m_phase        = Phase::Idle;
        }

        if (m_phase == Phase::Cooling) {
            if (is_printing(print_status)) {
                // A new print owns the fans from here on; leave them alone.
                m_was_printing = true;
                m_phase        = Phase::Idle;
                return Action::None;
            }
            if (!enabled) {
                m_phase = Phase::Idle;
                return Action::FansOff;
            }
            if (cool_enough(bed_c, chamber_c)) {
                m_phase = Phase::Cooled;
                return Action::FansOff;
            }
            if (now_ms - m_started_ms >= kTimeoutMs) {
                m_phase = Phase::TimedOut;
                return Action::FansOff;
            }
            return Action::None;
        }

        if (is_printing(print_status)) {
            m_was_printing = true;
            m_phase        = Phase::Idle;
            return Action::None;
        }

        const bool just_ended = m_was_printing && has_ended(print_status);
        m_was_printing        = false;
        if (!just_ended || !enabled) return Action::None;
        if (cool_enough(bed_c, chamber_c)) {
            // Already cool (a short print on a cold bed): nothing to blow at.
            m_phase = Phase::Cooled;
            return Action::None;
        }
        m_phase      = Phase::Cooling;
        m_started_ms = now_ms;
        return Action::FansOn;
    }

    Phase phase() const { return m_phase; }

    // Milliseconds left before the timeout while cooling, otherwise zero.
    long long remaining_ms(long long now_ms) const
    {
        if (m_phase != Phase::Cooling) return 0;
        const long long left = kTimeoutMs - (now_ms - m_started_ms);
        return left > 0 ? left : 0;
    }

private:
    std::string m_machine;
    bool        m_was_printing = false;
    Phase       m_phase        = Phase::Idle;
    long long   m_started_ms   = 0;
};

}}} // namespace Slic3r::GUI::PrintCooldown

#endif // slic3r_GUI_PrintCooldown_hpp_
