#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace Slic3r {

// This projection deliberately contains no endpoint, credential, request body,
// response body, local file path, or session key.
enum class HubState { stopped, not_configured, enrollment_required, invalid_configuration,
                      pending, delivered, delivery_failed };
struct HubSnapshot {
    HubState state = HubState::stopped;
    std::string result_code;
    int http_status = 0;
    std::uint64_t attempts = 0;
    std::uint64_t accepted = 0;
    std::uint64_t replies_received = 0;
    std::uint64_t revision = 0;
    bool busy = false;
    bool retry_available = false;
    bool endpoint_present = false;
    bool enrollment_present = false;
    bool inventory_applicable = false;
    std::chrono::system_clock::time_point updated_at{};
    std::chrono::system_clock::time_point last_accepted_at{};
};

bool validStatusHubEndpoint(const std::string& endpoint) noexcept;
HubState statusHubConfigurationState(const std::string& endpoint, bool enrolled) noexcept;

class StatusHubService final {
public:
    static StatusHubService& instance();
    StatusHubService() = default;
    ~StatusHubService();
    StatusHubService(const StatusHubService&) = delete;
    StatusHubService& operator=(const StatusHubService&) = delete;

    // No endpoint default. Reads owner-provided process configuration only.
    // An optional repository path is supplied explicitly by a development host;
    // installed applications must leave it empty, never scan the open project.
    void start(std::string repository_path = {});
    void checkpoint();
    void retry();
    void stop();
    HubSnapshot snapshot() const;

private:
    void run(std::string repository_path);
    void publish(HubState state, const std::string& code, int http = 0,
                 bool accepted = false, std::uint64_t replies = 0);
    mutable std::mutex m_mutex;
    std::mutex m_lifecycle;
    std::condition_variable m_condition;
    std::thread m_worker;
    bool m_stop = false;
    bool m_requested = false;
    HubSnapshot m_snapshot;
};
}
