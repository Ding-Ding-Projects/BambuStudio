#ifndef SLIC3R_FARM_DEVICE_POLICY_HPP
#define SLIC3R_FARM_DEVICE_POLICY_HPP

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Slic3r {

// Use device IDs as the identity across account and local discovery sources.
template <class Device, class IsPairedLan>
std::map<std::string, Device*> merge_farm_devices(
    const std::map<std::string, Device*>& account,
    const std::map<std::string, Device*>& local,
    IsPairedLan is_paired_lan)
{
    std::map<std::string, Device*> result;
    for (const auto& entry : account)
        if (!entry.first.empty() && entry.second) result.emplace(entry);
    for (const auto& entry : local) {
        if (entry.first.empty() || !entry.second) continue;
        if (is_paired_lan(entry.second)) result[entry.first] = entry.second;
        else result.emplace(entry);
    }
    return result;
}

inline std::vector<std::string> selected_farm_ids(const std::vector<std::string>& saved_ids,
                                                  size_t max_devices = 6)
{
    std::vector<std::string> result;
    for (const auto& id : saved_ids) {
        if (id.empty()) continue;
        bool duplicate = false;
        for (const auto& selected : result)
            if (selected == id) { duplicate = true; break; }
        if (!duplicate && result.size() < max_devices) result.push_back(id);
    }
    return result;
}

enum class FarmLanReadiness { Ready, PairingRequired, AddressMissing, TransportUnavailable };

inline FarmLanReadiness farm_lan_readiness(bool paired, bool has_code, bool has_address, bool has_transport)
{
    if (!paired || !has_code) return FarmLanReadiness::PairingRequired;
    if (!has_address) return FarmLanReadiness::AddressMissing;
    if (!has_transport) return FarmLanReadiness::TransportUnavailable;
    return FarmLanReadiness::Ready;
}

// Multi-nozzle devices need an explicit, validated payload before dispatch.
inline bool farm_requires_nozzle_mapping(int nozzle_count, bool dual_nozzle_model)
{
    return nozzle_count > 1 || dual_nozzle_model;
}

inline bool farm_plate_nozzles_valid(const std::vector<int>& maps, const std::vector<int>& used_filaments)
{
    if (used_filaments.empty()) return false;
    for (int filament : used_filaments)
        if (filament < 1 || static_cast<size_t>(filament) > maps.size() ||
            (maps[filament - 1] != 1 && maps[filament - 1] != 2)) return false;
    return true;
}

inline bool send_completed_before_cancellation(int transport_result, bool was_cancelled)
{
    return transport_result == 0 && !was_cancelled;
}

template <class WasCancelled, class Transport>
int dispatch_farm_lan(std::mutex& transfer_mutex, WasCancelled was_cancelled,
                      Transport transport, int cancelled_result)
{
    std::unique_lock<std::mutex> lock(transfer_mutex);
    return was_cancelled() ? cancelled_result : transport();
}

} // namespace Slic3r

#endif
