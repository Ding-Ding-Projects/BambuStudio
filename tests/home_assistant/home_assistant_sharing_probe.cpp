#include "slic3r/GUI/HomeAssistantSharingService.hpp"

#include "nlohmann/json.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

using Slic3r::GUI::HomeAssistant::SharingService;

namespace {

unsigned run_seconds(int argc, char **argv)
{
    if (argc != 2)
        return 180;
    try {
        const unsigned value = static_cast<unsigned>(std::stoul(argv[1]));
        return value >= 5 && value <= 600 ? value : 180;
    } catch (...) {
        return 180;
    }
}

} // namespace

int main(int argc, char **argv)
{
    std::atomic<int> authorized_fetches{0};
    const std::string payload = nlohmann::json{
        {"printers", nlohmann::json::array({
            {
                {"serial", "SYNTHETIC-VERIFICATION"},
                {"host", "192.0.2.1"},
                {"access_code", "synthetic-probe-value"},
                {"name", "Bambu Studio verification probe"},
            },
        })},
    }.dump();

    SharingService service;
    SharingService::Options options;
    options.pairing_token = SharingService::make_pairing_token();
    options.display_name  = "Bambu Studio verification probe";
    const auto result = service.start(
        std::move(options),
        [&authorized_fetches, payload] {
            ++authorized_fetches;
            return payload;
        });
    if (!result) {
        std::cerr << "START_FAILED\n";
        return 1;
    }

    std::cout << "READY " << result.advertised_ipv4 << ' ' << result.port << '\n' << std::flush;
    bool reported_fetch = false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(run_seconds(argc, argv));
    while (std::chrono::steady_clock::now() < deadline) {
        if (!reported_fetch && authorized_fetches.load() > 0) {
            std::cout << "AUTHORIZED_FETCH_SEEN\n" << std::flush;
            reported_fetch = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    service.stop();
    std::cout << "STOPPED\n";
    return reported_fetch ? 0 : 2;
}
