#include "../../src/slic3r/GUI/DeviceCore/FarmDevicePolicy.hpp"

#include <atomic>
#include <cassert>
#include <thread>

struct Device {
    bool paired_lan;
    bool online;
};

int main()
{
    Device account{false, true};
    Device local{true, false};
    Device discovery{false, false};
    std::map<std::string, Device*> accounts{{"same", &account}};
    std::map<std::string, Device*> locals{{"same", &local}, {"offline", &discovery}};
    const auto inventory = Slic3r::merge_farm_devices(accounts, locals,
        [](Device* device) { return device->paired_lan; });
    assert(inventory.size() == 2);
    assert(inventory.at("same") == &local);
    assert(inventory.at("offline") == &discovery && !inventory.at("offline")->online);

    const auto selected = Slic3r::selected_farm_ids({"a", "a", "b", "c", "d", "e", "f", "g"});
    assert(selected.size() == 6 && selected.back() == "f");

    using Slic3r::FarmLanReadiness;
    assert(Slic3r::farm_lan_readiness(false, false, true, true) == FarmLanReadiness::PairingRequired);
    assert(Slic3r::farm_lan_readiness(true, true, false, true) == FarmLanReadiness::AddressMissing);
    assert(Slic3r::farm_lan_readiness(true, true, true, false) == FarmLanReadiness::TransportUnavailable);
    assert(Slic3r::farm_lan_readiness(true, true, true, true) == FarmLanReadiness::Ready);

    assert(!Slic3r::farm_requires_nozzle_mapping(1, false));
    assert(Slic3r::farm_requires_nozzle_mapping(2, false));
    assert(Slic3r::farm_requires_nozzle_mapping(1, true));
    assert(Slic3r::farm_plate_nozzles_valid({1, 2}, {1, 2}));
    assert(Slic3r::farm_plate_nozzles_valid({2, 1}, {1}));
    assert(!Slic3r::farm_plate_nozzles_valid({1, 0}, {2}));
    assert(!Slic3r::farm_plate_nozzles_valid({1}, {2}));
    assert(!Slic3r::farm_plate_nozzles_valid({1}, {}));
    assert(Slic3r::send_completed_before_cancellation(0, false));
    assert(!Slic3r::send_completed_before_cancellation(0, true));
    assert(!Slic3r::send_completed_before_cancellation(-7, false));

    std::mutex mutex;
    int calls = 0;
    const int cancelled = Slic3r::dispatch_farm_lan(mutex, [] { return true; }, [&] { ++calls; return 0; }, -18);
    assert(cancelled == -18 && calls == 0);
    const int success = Slic3r::dispatch_farm_lan(mutex, [] { return false; }, [&] { ++calls; return 0; }, -18);
    const int failure = Slic3r::dispatch_farm_lan(mutex, [] { return false; }, [&] { ++calls; return -7; }, -18);
    assert(success == 0 && failure == -7 && calls == 2);

    std::atomic<int> active{0};
    std::atomic<int> largest{0};
    auto send = [&] {
        return Slic3r::dispatch_farm_lan(mutex, [] { return false; }, [&] {
            const int now = ++active;
            if (now > largest) largest = now;
            std::this_thread::yield();
            --active;
            return 0;
        }, -18);
    };
    std::thread first(send), second(send);
    first.join();
    second.join();
    assert(largest == 1);
}
