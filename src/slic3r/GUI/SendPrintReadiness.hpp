#ifndef slic3r_GUI_SendPrintReadiness_hpp_
#define slic3r_GUI_SendPrintReadiness_hpp_

// Decisions of the Send print job dialog (SelectMachineDialog) that do not
// need wxWidgets or a printer, kept here so tests/send_print_readiness_test.cpp
// can execute them:
//
// - what the dialog may say about the plate or plates it is about to send;
// - whether a nozzle move made in the dialog is still waiting to be applied;
// - which nozzle volume type a quick swap checks and then stores per material;
// - the exact snapshot of the plate keys that the swap check writes for a
//   moment, so that opening the dialog never changes the plate.

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI {

// One plate that the dialog would send.
struct SendPlateFacts
{
    int  plate_index{-1};
    bool has_printable_objects{false}; // PartPlate::has_printable_instances()
    bool has_valid_slice{false};       // PartPlate::is_valid_gcode_file()
};

enum class SendReadiness
{
    Ready,      // every plate that carries objects has a slice that can be sent
    EmptyPlate, // nothing to send: no plate has printable objects or a slice
    NeedsSlice, // a plate with printable objects has no valid slice any more
};

struct SendReadinessVerdict
{
    SendReadiness readiness{SendReadiness::EmptyPlate};
    int           plate_index{-1}; // the first plate that needs slicing, or -1
};

// One entry for "Print plate", one per plate for "Print all". A plate without
// printable objects is skipped when other plates are sent, as
// PartPlateList::is_all_slice_results_ready_for_print() does. A plate that has
// a valid slice can be sent whatever the object check says, as before.
inline SendReadinessVerdict classify_send_plates(const std::vector<SendPlateFacts> &plates)
{
    bool any_ready = false;
    for (const SendPlateFacts &plate : plates) {
        if (plate.has_valid_slice) {
            any_ready = true;
            continue;
        }
        if (plate.has_printable_objects)
            return {SendReadiness::NeedsSlice, plate.plate_index};
    }
    return {any_ready ? SendReadiness::Ready : SendReadiness::EmptyPlate, -1};
}

// A nozzle move made in print setup that Swap has not applied yet. While one
// is pending the dialog must not send: the slice it would send still uses the
// old assignment.
inline bool quick_swap_pending(const std::vector<int> &pending_maps, const std::vector<int> &applied_maps)
{
    return !pending_maps.empty() && pending_maps != applied_maps;
}

// The nozzle volume type each material needs once `maps` (1 = left, 2 = right,
// 0 = no nozzle) is applied. A material on a nozzle of one fixed type takes
// that type; on a hybrid nozzle it keeps the type it already had, or
// `standard_type` when it had none. Materials without a nozzle keep theirs.
// The swap check tests nozzle capacity for exactly these types and the swap
// stores exactly these types.
inline std::vector<int> quick_swap_volume_types(const std::vector<int> &maps,
                                                const std::vector<int> &current_volume_types,
                                                const std::vector<int> &extruder_volume_types,
                                                int                     hybrid_type,
                                                int                     standard_type)
{
    std::vector<int> types(maps.size(), standard_type);
    for (std::size_t i = 0; i < maps.size(); ++i) {
        const bool has_current = i < current_volume_types.size();
        const int  nozzle      = maps[i];
        if (nozzle < 1 || static_cast<std::size_t>(nozzle) > extruder_volume_types.size()) {
            if (has_current)
                types[i] = current_volume_types[i];
            continue;
        }
        const int extruder_type = extruder_volume_types[nozzle - 1];
        if (extruder_type != hybrid_type)
            types[i] = extruder_type;
        else if (has_current)
            types[i] = current_volume_types[i];
    }
    return types;
}

// The exact state of some keys of a configuration: for each key, whether it
// was present and, if so, a copy of its value. restore() puts every key back
// as it was, erasing the keys that were absent, without going through any
// setter that could clear related keys. Config needs has(key), a const
// optptr(key) returning const Option*, set_key_value(key, Option*) taking
// ownership and erase(key); Option needs clone() and operator==.
template<class Config, class Option>
class ConfigKeySnapshot
{
public:
    ConfigKeySnapshot(const Config &config, const std::vector<std::string> &keys)
    {
        m_entries.reserve(keys.size());
        for (const std::string &key : keys) {
            const Option *value = config.has(key) ? config.optptr(key) : nullptr;
            m_entries.push_back({key, std::unique_ptr<Option>(value ? value->clone() : nullptr)});
        }
    }

    void restore(Config &config) const
    {
        for (const Entry &entry : m_entries) {
            if (entry.value)
                config.set_key_value(entry.key, entry.value->clone());
            else if (config.has(entry.key))
                config.erase(entry.key);
        }
    }

    bool matches(const Config &config) const
    {
        for (const Entry &entry : m_entries) {
            if (config.has(entry.key) != static_cast<bool>(entry.value))
                return false;
            if (entry.value) {
                const Option *live = config.optptr(entry.key);
                if (!live || !(*live == *entry.value))
                    return false;
            }
        }
        return true;
    }

private:
    struct Entry
    {
        std::string             key;
        std::unique_ptr<Option> value;
    };
    std::vector<Entry> m_entries;
};

}} // namespace Slic3r::GUI

#endif
