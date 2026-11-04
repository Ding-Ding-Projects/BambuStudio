#ifndef slic3r_GUI_DeviceLiveReadings_hpp_
#define slic3r_GUI_DeviceLiveReadings_hpp_

// Pure presentation rules for the live readings strip on the print monitor and for
// the visibility of the Print Options and Safety Options entries.
//
// This header deliberately depends on the standard library only, so the rules can be
// compiled and exercised without wxWidgets or a printer. Every function takes values
// that the application already parses from the printer; nothing here reads a field the
// protocol layer does not provide. Where a value is missing or out of range the
// function returns std::nullopt (or false) and the caller hides the reading instead of
// showing a guess.

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

namespace Slic3r { namespace GUI { namespace DeviceLive {

// ---------------------------------------------------------------------------------
// Wi-Fi signal. MachineObject::wifi_signal is stored verbatim from the "wifi_signal"
// field of the print status message (DeviceManager.cpp), for example "-61dBm".
// ---------------------------------------------------------------------------------

enum class WifiQuality { Strong, Fair, Weak };

// Parses "-61dBm", "-61 dBm" or "-61". Returns nullopt for an empty or malformed text and
// for any value outside the plausible received-signal range [-120, 0] dBm.
inline std::optional<int> parse_wifi_dbm(const std::string &text)
{
    std::size_t i = 0;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    const std::size_t number_start = i;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) ++i;
    const std::size_t digits_start = i;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
    const std::size_t digit_count = i - digits_start;
    if (digit_count == 0 || digit_count > 4) return std::nullopt;
    const std::string number = text.substr(number_start, i - number_start);

    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    if (i < text.size()) {
        static const char unit[] = "dbm";
        for (std::size_t k = 0; k < 3; ++k) {
            if (i >= text.size() || std::tolower(static_cast<unsigned char>(text[i])) != unit[k]) return std::nullopt;
            ++i;
        }
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        if (i != text.size()) return std::nullopt;
    }

    const int value = std::atoi(number.c_str());
    if (value > 0 || value < -120) return std::nullopt;
    return value;
}

// Display heuristic only (not a protocol value): at or above -60 dBm is strong, at or
// above -75 dBm is fair, anything lower is weak.
inline WifiQuality classify_wifi_dbm(int dbm)
{
    if (dbm >= -60) return WifiQuality::Strong;
    if (dbm >= -75) return WifiQuality::Fair;
    return WifiQuality::Weak;
}

// ---------------------------------------------------------------------------------
// Air duct mode. DevFan parses device.airduct.modeCur into AirDuctData::curren_mode
// using the AIR_DUCT enumeration of DeviceCore/DevFan.h.
// ---------------------------------------------------------------------------------

enum class AirDuctModeLabel { Cooling, Heating, Exhaust, FullCooling };

// Mirrors AIR_DUCT_COOLING_FILT (0), AIR_DUCT_HEATING_INTERNAL_FILT (1), AIR_DUCT_EXHAUST (2)
// and AIR_DUCT_FULL_COOLING (3). -1 (not reported) and 0xFF (initial mode inside the printer
// controller) are not modes the user can act on, so they map to nullopt.
inline std::optional<AirDuctModeLabel> air_duct_mode_label(int mode_id)
{
    switch (mode_id) {
    case 0: return AirDuctModeLabel::Cooling;
    case 1: return AirDuctModeLabel::Heating;
    case 2: return AirDuctModeLabel::Exhaust;
    case 3: return AirDuctModeLabel::FullCooling;
    default: return std::nullopt;
    }
}

// ---------------------------------------------------------------------------------
// Door-open check setting. This is the configured reaction to an open door, parsed from
// the printer configuration (MachineObject::get_door_open_check_state). It is a setting,
// not a live reading of the door, and the label shown for it must say so.
// ---------------------------------------------------------------------------------

enum class DoorCheckSetting { Off, Warning, PausePrint };

// Mirrors MachineObject::DoorOpenCheckState (0 disabled, 1 warning, 2 pause print).
inline std::optional<DoorCheckSetting> door_check_setting(int state)
{
    switch (state) {
    case 0: return DoorCheckSetting::Off;
    case 1: return DoorCheckSetting::Warning;
    case 2: return DoorCheckSetting::PausePrint;
    default: return std::nullopt;
    }
}

// ---------------------------------------------------------------------------------
// Installed nozzle. DevNozzle carries diameter, material and flow type per extruder.
// ---------------------------------------------------------------------------------

// A nozzle is shown only when the printer reported a positive diameter and the entry is
// flagged reliable, not unknown and not abnormal.
inline bool nozzle_reading_usable(float diameter_mm, bool material_known, bool reliable, bool unknown_or_abnormal)
{
    return diameter_mm > 0.0f && diameter_mm < 10.0f && material_known && reliable && !unknown_or_abnormal;
}

inline std::string format_nozzle_diameter(float diameter_mm)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f mm", static_cast<double>(diameter_mm));
    return buffer;
}

// ---------------------------------------------------------------------------------
// Entry visibility on the print monitor.
// ---------------------------------------------------------------------------------

// The Safety Options dialog holds the door-open check and the idle heating protection.
// The door-open check is routed here only for printers whose configuration enables the
// safety dialog; the idle heating protection is offered whenever the printer reports it.
// Printers that report neither never see the entry.
inline bool safety_entry_visible(bool config_enables_safety, bool door_check_supported, bool idle_heating_supported)
{
    return (config_enables_safety && door_check_supported) || idle_heating_supported;
}

// Printers whose configuration does not enable the safety dialog keep the door-open check
// in Print Options, so the safety dialog must not duplicate it.
inline bool safety_dialog_shows_door_check(bool config_enables_safety, bool door_check_supported)
{
    return config_enables_safety && door_check_supported;
}

}}} // namespace Slic3r::GUI::DeviceLive

#endif // slic3r_GUI_DeviceLiveReadings_hpp_
