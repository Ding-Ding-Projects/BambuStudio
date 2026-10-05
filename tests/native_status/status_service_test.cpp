#include "libslic3r/StatusHub/StatusHubService.hpp"
#include <status_hub_client.hpp>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace Slic3r;
static void require(bool pass, const char* label)
{
    if (!pass) throw std::runtime_error(label);
}
static void environment(const char* key, const char* value)
{
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}
static HubSnapshot wait_for_result(StatusHubService& service, std::uint64_t revision = 0)
{
    for (int i = 0; i < 300; ++i) {
        auto state = service.snapshot();
        if (state.revision > revision && !state.busy) return state;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("service did not publish within three seconds");
}
class CountingTransport final : public status_hub::HttpTransport {
public:
    int calls = 0;
    status_hub::TransportResult send(const status_hub::HttpRequest&) noexcept override {
        ++calls;
        status_hub::TransportResult result;
        result.code = status_hub::ResultCode::unreachable;
        return result;
    }
};
int main()
{
    try {
        require(std::string(status_hub::kDefaultBaseUrl).empty(), "no implicit destination");
        require(validStatusHubEndpoint("https://example.test"), "HTTPS origin");
        require(validStatusHubEndpoint("https://example.test:443/"), "explicit HTTPS port");
        for (const auto* bad : {"", "http://example.test", "https://name:secret@example.test",
             "https://example.test/private", "https://example.test?key=x", "https://example.test#x",
             "https://example.test:8080", "https://example.test\\x", "https://example.test\n",
             "https://example..test", "https://"})
            require(!validStatusHubEndpoint(bad), "unsafe origin rejected");
        require(statusHubConfigurationState("", true) == HubState::not_configured, "missing endpoint closes writes");
        require(statusHubConfigurationState("https://example.test", false) == HubState::enrollment_required,
                "missing enrollment closes writes");
        environment("STATUS_HUB_URL", "");
        environment("AGENT_INGEST_TOKEN", "");
        StatusHubService service;
        service.start();
        service.start(); // Idempotent startup.
        auto snapshot = wait_for_result(service);
        require(snapshot.state == HubState::not_configured && snapshot.accepted == 0, "disconnected is not delivered");
        service.retry();
        snapshot = wait_for_result(service, snapshot.revision);
        require(snapshot.accepted == 0 && snapshot.attempts >= 2, "retry preserves no-delivery truth");
        const auto before = std::chrono::steady_clock::now();
        service.stop();
        service.stop();
        require(std::chrono::steady_clock::now() - before < std::chrono::seconds(1), "offline stop interrupts heartbeat");
        require(service.snapshot().result_code == "stopped_without_delivery", "offline exit is honest");
        environment("STATUS_HUB_URL", "https://example.test");
        service.start();
        snapshot = wait_for_result(service);
        require(snapshot.state == HubState::enrollment_required && snapshot.accepted == 0, "enrollment state");
        service.stop();
        environment("STATUS_HUB_URL", "http://example.test");
        service.start();
        require(wait_for_result(service).state == HubState::invalid_configuration, "invalid configuration state");
        service.stop();
        // The real client must also refuse before transport with no enrollment.
        CountingTransport transport;
        status_hub::ClientOptions options;
        options.sessionId = "contract-session";
        options.baseUrl = "https://example.test";
        options.startHeartbeat = false;
        status_hub::StatusHubClient client(transport, options);
        auto update = client.update();
        require(!update.ok && transport.calls == 0, "missing enrollment makes zero transport calls");
        client.close();
        std::cout << "PASS native status configuration, lifecycle, retry and zero-transport boundaries\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
