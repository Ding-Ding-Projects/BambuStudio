#include <catch_main.hpp>

#include "slic3r/GUI/HomeAssistantSharingService.hpp"
#include "slic3r/GUI/HomeAssistantTaskExecutor.hpp"
#include "slic3r/GUI/HomeAssistantTransportPolicy.hpp"

#include "nlohmann/json.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = asio::ip::tcp;
using udp       = asio::ip::udp;

using Slic3r::GUI::HomeAssistant::CredentialTransportSafety;
using Slic3r::GUI::HomeAssistant::SharingService;
using Slic3r::GUI::HomeAssistant::SharingStartErrorCode;
using Slic3r::GUI::HomeAssistant::credential_transport_safety_for_url;
namespace Execution = Slic3r::GUI::HomeAssistant::Execution;

namespace {

struct RunningShare
{
    SharingService             service;
    SharingService::StartResult result;
    std::string                token;
    std::shared_ptr<std::atomic<int>> supplier_calls;

    explicit RunningShare(std::string payload)
        : token(SharingService::make_pairing_token())
        , supplier_calls(std::make_shared<std::atomic<int>>(0))
    {
        SharingService::Options options;
        options.pairing_token = token;
        options.display_name  = "Bambu Studio test";
        result = service.start(
            std::move(options),
            [payload = std::move(payload), calls = supplier_calls]() {
                ++(*calls);
                return payload;
            });
    }
};

http::response<http::string_body>
request(const SharingService::StartResult &endpoint,
        http::verb                        method,
        const std::string &               target,
        const std::vector<std::string> &  authorization_values = {})
{
    asio::io_context io;
    tcp::resolver resolver(io);
    beast::tcp_stream stream(io);
    stream.expires_after(std::chrono::seconds(5));
    const auto resolved = resolver.resolve(endpoint.advertised_ipv4, std::to_string(endpoint.port));
    stream.connect(resolved);

    http::request<http::empty_body> outgoing(method, target, 11);
    outgoing.set(http::field::host, endpoint.advertised_ipv4);
    outgoing.set(http::field::user_agent, "BambuStudio-home-assistant-test");
    for (const std::string &authorization : authorization_values)
        outgoing.insert(http::field::authorization, authorization);

    http::write(stream, outgoing);
    beast::flat_buffer buffer;
    http::response<http::string_body> incoming;
    http::read(stream, buffer, incoming);
    return incoming;
}

std::string valid_payload()
{
    return nlohmann::json{
        {"printers", nlohmann::json::array({
            {
                {"serial", "SYNTHETIC-SERIAL"},
                {"host", "192.0.2.1"},
                {"access_code", "synthetic-test-value"},
                {"name", "Verification printer"},
                {"ignored", "must not leave the process"},
            },
        })},
    }.dump();
}

std::optional<asio::ip::address_v4> default_route_test_address()
{
    boost::system::error_code ec;
    asio::io_context          io;
    udp::socket               socket(io);
    socket.open(udp::v4(), ec);
    if (ec)
        return std::nullopt;
    socket.connect(
        udp::endpoint(asio::ip::make_address_v4("192.0.2.1"), 9),
        ec);
    if (ec)
        return std::nullopt;
    const auto endpoint = socket.local_endpoint(ec);
    if (ec || !endpoint.address().is_v4())
        return std::nullopt;
    const auto address = endpoint.address().to_v4();
    const auto bytes   = address.to_bytes();
    const bool allowed =
        bytes[0] == 10 ||
        (bytes[0] == 100 && bytes[1] >= 64 && bytes[1] <= 127) ||
        (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) ||
        (bytes[0] == 192 && bytes[1] == 168);
    return allowed ? std::optional<asio::ip::address_v4>(address) : std::nullopt;
}

void append_test_u16(std::vector<std::uint8_t> &packet, std::uint16_t value)
{
    packet.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    packet.push_back(static_cast<std::uint8_t>(value & 0xff));
}

std::vector<std::uint8_t> mdns_service_query(std::uint16_t transaction_id)
{
    std::vector<std::uint8_t> packet;
    packet.reserve(64);
    append_test_u16(packet, transaction_id);
    append_test_u16(packet, 0);
    append_test_u16(packet, 1);
    append_test_u16(packet, 0);
    append_test_u16(packet, 0);
    append_test_u16(packet, 0);

    for (const std::string label : {"_bambu-slicer", "_tcp", "local"}) {
        packet.push_back(static_cast<std::uint8_t>(label.size()));
        packet.insert(packet.end(), label.begin(), label.end());
    }
    packet.push_back(0);
    append_test_u16(packet, 12);     // PTR
    append_test_u16(packet, 0x8001); // IN with the unicast-response bit
    return packet;
}

void send_mdns_flood(const SharingService::StartResult &endpoint,
                     std::size_t                        query_count,
                     bool                               unique_transaction_ids)
{
    asio::io_context io;
    udp::socket socket(io, udp::endpoint(udp::v4(), 0));
    const auto interface_address =
        asio::ip::make_address_v4(endpoint.advertised_ipv4);
    socket.set_option(asio::ip::multicast::outbound_interface(interface_address));
    socket.set_option(asio::ip::multicast::enable_loopback(true));
    const udp::endpoint destination(
        asio::ip::make_address_v4("224.0.0.251"),
        5353);
    for (std::size_t index = 0; index < query_count; ++index) {
        const auto transaction_id = unique_transaction_ids
            ? static_cast<std::uint16_t>(0x4000u + index)
            : static_cast<std::uint16_t>(0x4242u);
        const auto query = mdns_service_query(transaction_id);
        socket.send_to(asio::buffer(query), destination);
    }
}

std::size_t count_mdns_flood_responses(
    const SharingService::StartResult &endpoint,
    std::size_t query_count,
    std::chrono::milliseconds observation_window)
{
    asio::io_context io;
    udp::socket socket(io, udp::endpoint(udp::v4(), 0));
    socket.non_blocking(true);
    const auto interface_address =
        asio::ip::make_address_v4(endpoint.advertised_ipv4);
    socket.set_option(asio::ip::multicast::outbound_interface(interface_address));
    socket.set_option(asio::ip::multicast::enable_loopback(true));
    const udp::endpoint destination(
        asio::ip::make_address_v4("224.0.0.251"),
        5353);
    const auto query = mdns_service_query(0x4242);
    for (std::size_t index = 0; index < query_count; ++index)
        socket.send_to(asio::buffer(query), destination);

    std::size_t response_count = 0;
    const auto deadline = std::chrono::steady_clock::now() + observation_window;
    std::array<std::uint8_t, 2048> response{};
    while (std::chrono::steady_clock::now() < deadline) {
        boost::system::error_code ec;
        udp::endpoint sender;
        const std::size_t received =
            socket.receive_from(asio::buffer(response), sender, 0, ec);
        if (!ec) {
            if (received >= 2 && response[0] == 0x42 && response[1] == 0x42)
                ++response_count;
            continue;
        }
        if (ec != asio::error::would_block && ec != asio::error::try_again)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return response_count;
}

std::size_t count_unique_mdns_flood_responses(
    const SharingService::StartResult &endpoint,
    std::size_t query_count,
    std::chrono::milliseconds observation_window,
    std::uint16_t first_transaction_id)
{
    asio::io_context io;
    udp::socket socket(io, udp::endpoint(udp::v4(), 0));
    socket.non_blocking(true);
    const auto interface_address =
        asio::ip::make_address_v4(endpoint.advertised_ipv4);
    socket.set_option(
        asio::ip::multicast::outbound_interface(interface_address));
    socket.set_option(asio::ip::multicast::enable_loopback(true));
    const udp::endpoint destination(
        asio::ip::make_address_v4("224.0.0.251"),
        5353);

    for (std::size_t index = 0; index < query_count; ++index) {
        const std::uint16_t transaction_id =
            static_cast<std::uint16_t>(first_transaction_id + index);
        const auto query = mdns_service_query(transaction_id);
        socket.send_to(asio::buffer(query), destination);
    }

    std::set<std::uint16_t> response_ids;
    const auto deadline =
        std::chrono::steady_clock::now() + observation_window;
    std::array<std::uint8_t, 2048> response{};
    while (std::chrono::steady_clock::now() < deadline) {
        boost::system::error_code ec;
        udp::endpoint sender;
        const std::size_t received =
            socket.receive_from(asio::buffer(response), sender, 0, ec);
        if (!ec) {
            if (received >= 2) {
                const auto transaction_id = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(response[0]) << 8) |
                    response[1]);
                const std::size_t offset =
                    static_cast<std::uint16_t>(
                        transaction_id - first_transaction_id);
                if (offset < query_count)
                    response_ids.insert(transaction_id);
            }
            continue;
        }
        if (ec != asio::error::would_block &&
            ec != asio::error::try_again)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return response_ids.size();
}

} // namespace

TEST_CASE("Bounded Home Assistant executor cancels active work and drops inert pending work",
          "[home-assistant][performance][executor]")
{
    Execution::BoundedTaskExecutor executor(1, 2);
    std::promise<void> started;
    auto started_future = started.get_future();
    std::atomic<int> pending_runs{0};

    REQUIRE(executor.submit([&started](const std::atomic_bool &cancel_requested) {
        started.set_value();
        while (!cancel_requested.load())
            std::this_thread::yield();
    }));
    REQUIRE(started_future.wait_for(std::chrono::seconds(2)) ==
            std::future_status::ready);
    REQUIRE(executor.submit([&pending_runs](const std::atomic_bool &) {
        ++pending_runs;
    }));
    REQUIRE(executor.submit([&pending_runs](const std::atomic_bool &) {
        ++pending_runs;
    }));
    CHECK_FALSE(executor.submit([](const std::atomic_bool &) {}));
    CHECK(executor.pending() == 2);

    executor.shutdown();
    CHECK(pending_runs.load() == 0);
    CHECK_FALSE(executor.submit([](const std::atomic_bool &) {}));
}

TEST_CASE("Bounded Home Assistant executor contains task exceptions",
          "[home-assistant][performance][executor][exceptions]")
{
    Execution::BoundedTaskExecutor executor(1, 2);
    std::promise<void> survivor_ran;
    auto survivor_future = survivor_ran.get_future();

    REQUIRE(executor.submit([](const std::atomic_bool &) {
        throw std::runtime_error("synthetic task failure");
    }));
    REQUIRE(executor.submit([&survivor_ran](const std::atomic_bool &) {
        survivor_ran.set_value();
    }));

    CHECK(survivor_future.wait_for(std::chrono::seconds(2)) ==
          std::future_status::ready);
    executor.shutdown();
}

TEST_CASE("Home Assistant UI completion posts exactly once",
          "[home-assistant][entities][executor]")
{
    std::mutex queue_mutex;
    std::vector<std::function<void()>> ui_queue;
    std::atomic<int> callback_count{0};
    int delivered_value = 0;

    Execution::OnceUiCompletion<int> completion(
        [&](std::function<void()> callback) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            ui_queue.push_back(std::move(callback));
            return true;
        },
        [&](int value) {
            delivered_value = value;
            ++callback_count;
        });

    REQUIRE(completion.dispatch(42));
    std::vector<std::thread> contenders;
    for (int value = 0; value < 8; ++value) {
        contenders.emplace_back([completion, value] {
            completion.dispatch(value);
        });
    }
    for (std::thread &contender : contenders)
        contender.join();

    REQUIRE(ui_queue.size() == 1);
    CHECK(callback_count.load() == 0);
    ui_queue.front()();
    CHECK(callback_count.load() == 1);
    CHECK(delivered_value == 42);
}

TEST_CASE("Discarded Home Assistant work posts one shutdown completion",
          "[home-assistant][entities][executor][shutdown]")
{
    std::mutex queue_mutex;
    std::vector<std::function<void()>> ui_queue;
    std::atomic<int> callback_count{0};

    Execution::OnceUiCompletion<int> completion(
        [&](std::function<void()> callback) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            ui_queue.push_back(std::move(callback));
            return true;
        },
        [&](int value) {
            CHECK(value == 7);
            ++callback_count;
        });

    Execution::BoundedTaskExecutor executor(1, 1);
    std::promise<void> active_started;
    auto active_started_future = active_started.get_future();
    REQUIRE(executor.submit(
        [&active_started](const std::atomic_bool &cancel_requested) {
            active_started.set_value();
            while (!cancel_requested.load())
                std::this_thread::yield();
        }));
    REQUIRE(active_started_future.wait_for(std::chrono::seconds(2)) ==
            std::future_status::ready);
    REQUIRE(
        executor.submit_with_discard(
            [](const std::atomic_bool &) {},
            [completion] { completion.dispatch(7); }) ==
        Execution::BoundedTaskExecutor::SubmitStatus::Accepted);

    executor.shutdown();
    REQUIRE(ui_queue.size() == 1);
    CHECK(callback_count.load() == 0);
    ui_queue.front()();
    CHECK(callback_count.load() == 1);
}

TEST_CASE("Entity state parsing validates domains and caps worker results",
          "[home-assistant][entities][bounds]")
{
    nlohmann::json states = nlohmann::json::array({
        {
            {"entity_id", "sensor.unrelated"},
            {"state", "ignored"},
            {"attributes", {{"friendly_name", "Unrelated"}}},
        },
        {
            {"entity_id", 17},
            {"state", "malformed"},
        },
    });
    for (std::size_t index = 0;
         index < Execution::kMaxEntityFetchResults + 8;
         ++index) {
        const std::string domain =
            index % 2 == 0 ? "light" : "media_player";
        states.push_back({
            {"entity_id", domain + ".entity_" + std::to_string(index)},
            {"state", index % 2 == 0 ? "on" : "playing"},
            {"attributes",
             {{"friendly_name", "Entity " + std::to_string(index)}}},
        });
    }

    const auto filtered = Execution::parse_filtered_entity_states(
        states.dump(),
        {"media_player", "light"});
    REQUIRE(filtered);
    CHECK(filtered.truncated);
    REQUIRE(
        filtered.entities.size() ==
        Execution::kMaxEntityFetchResults);
    CHECK(std::all_of(
        filtered.entities.begin(),
        filtered.entities.end(),
        [](const Execution::FilteredEntityState &entity) {
            return entity.entity_id.rfind("light.", 0) == 0 ||
                   entity.entity_id.rfind("media_player.", 0) == 0;
        }));
    CHECK(filtered.entities.front().friendly_name == "Entity 0");

    const auto hard_capped = Execution::parse_filtered_entity_states(
        states.dump(),
        {"light", "media_player"},
        Execution::kMaxEntityFetchResults + 100);
    REQUIRE(hard_capped);
    CHECK(hard_capped.truncated);
    CHECK(
        hard_capped.entities.size() ==
        Execution::kMaxEntityFetchResults);

    const auto empty_object_id = Execution::parse_filtered_entity_states(
        R"([{"entity_id":"light.","state":"on"}])",
        {"light"});
    REQUIRE(empty_object_id);
    CHECK(empty_object_id.entities.empty());

    CHECK(Execution::valid_entity_domains({"light"}));
    CHECK(Execution::valid_entity_domains({"media_player", "light"}));
    CHECK_FALSE(Execution::valid_entity_domains({}));
    CHECK_FALSE(Execution::valid_entity_domains(
        {"one", "two", "three", "four", "five"}));
    CHECK_FALSE(Execution::valid_entity_domains({"light", "light"}));
    CHECK_FALSE(Execution::valid_entity_domains({"light.bad"}));
    CHECK_FALSE(Execution::valid_entity_domains({"Light"}));
    CHECK_FALSE(Execution::valid_entity_domains(
        {std::string(Execution::kMaxEntityDomainBytes + 1, 'a')}));

    const auto malformed =
        Execution::parse_filtered_entity_states("{", {"light"});
    CHECK(
        malformed.status ==
        Execution::FilteredEntityParseStatus::InvalidResponse);
    CHECK(malformed.entities.empty());

    const auto wrong_root =
        Execution::parse_filtered_entity_states("{}", {"light"});
    CHECK(
        wrong_root.status ==
        Execution::FilteredEntityParseStatus::InvalidResponse);

    const auto invalid_filter =
        Execution::parse_filtered_entity_states("[]", {});
    CHECK(
        invalid_filter.status ==
        Execution::FilteredEntityParseStatus::InvalidFilter);
}

TEST_CASE("Configured Home Assistant entities stop at thirty two unique values",
          "[home-assistant][entities][config][bounds]")
{
    std::string raw = ";;light.duplicate;light.duplicate;";
    for (std::size_t index = 0; index < 40; ++index) {
        raw += "light.entity_" + std::to_string(index) + ";";
    }
    raw += std::string(Execution::kMaxEntityIdBytes + 1, 'x');

    const auto values = Execution::bounded_semicolon_config_values(
        raw,
        100);
    REQUIRE(
        values.size() ==
        Execution::kMaxConfiguredEntityValues);
    CHECK(values.front() == "light.duplicate");
    CHECK(values[1] == "light.entity_0");
    CHECK(values.back() == "light.entity_30");
    CHECK(
        std::count(
            values.begin(),
            values.end(),
            "light.duplicate") == 1);
}

TEST_CASE("Configured Home Assistant entity parsing stops after its scan budget",
          "[home-assistant][entities][config][performance]")
{
    const std::string raw =
        "light.duplicate;light.duplicate;light.duplicate;light.after_budget";
    const auto values = Execution::bounded_semicolon_config_values(
        raw,
        Execution::kMaxConfiguredEntityValues,
        Execution::kMaxEntityIdBytes,
        3,
        raw.size());

    REQUIRE(values.size() == 1);
    CHECK(values.front() == "light.duplicate");
}

TEST_CASE("Light alert transaction is snapshot flash restore ordered",
          "[home-assistant][performance][lights]")
{
    Execution::LightAlertTransaction transaction;
    transaction.target_url       = "https://ha.example.test";
    transaction.authorization    = "Bearer synthetic";
    transaction.light_entity_ids = {"light.one", "light.two"};
    transaction.red              = 12;
    transaction.green            = 34;
    transaction.blue             = 56;
    transaction.flashes          = 3;
    transaction.generation       = 0x2a;

    std::vector<std::string> paths;
    std::vector<std::string> bodies;
    std::vector<long> timeouts;
    std::vector<bool> shutdown_allowed;
    std::vector<std::chrono::milliseconds> waits;
    std::atomic_bool cancelled{false};
    Execution::run_light_alert_transaction(
        transaction,
        [&](const Execution::LightAlertTransaction &observed,
            const std::string &path,
            const std::string &body,
            long timeout_seconds,
            bool allow_during_shutdown,
            const std::atomic_bool &) {
            CHECK(observed.target_url == transaction.target_url);
            CHECK(observed.authorization == transaction.authorization);
            CHECK(observed.generation == transaction.generation);
            paths.push_back(path);
            bodies.push_back(body);
            timeouts.push_back(timeout_seconds);
            shutdown_allowed.push_back(allow_during_shutdown);
            return true;
        },
        [&](std::chrono::milliseconds delay) {
            waits.push_back(delay);
            return true;
        },
        cancelled);

    const std::vector<std::string> expected_paths{
        "/api/services/scene/create",
        "/api/services/light/turn_on",
        "/api/services/scene/turn_on",
        "/api/services/scene/turn_on",
        "/api/services/scene/delete",
    };
    const std::vector<bool> expected_shutdown_allowed{
        false, false, true, true, true};
    const std::vector<long> expected_timeouts{
        Execution::kNormalLightRequestTimeoutSeconds,
        Execution::kNormalLightRequestTimeoutSeconds,
        Execution::kShutdownRecoveryTimeoutSeconds,
        Execution::kShutdownRecoveryTimeoutSeconds,
        Execution::kShutdownRecoveryTimeoutSeconds,
    };
    const std::vector<std::chrono::milliseconds> expected_waits{
        std::chrono::seconds(4),
        std::chrono::seconds(2),
    };
    REQUIRE(paths == expected_paths);
    CHECK(timeouts == expected_timeouts);
    CHECK(shutdown_allowed == expected_shutdown_allowed);
    CHECK(waits == expected_waits);

    const auto snapshot = nlohmann::json::parse(bodies.front());
    CHECK(snapshot["scene_id"] == "bambustudio_light_restore_2a");
    CHECK(
        snapshot["snapshot_entities"] ==
        nlohmann::json::array({"light.one", "light.two"}));
    const auto flash = nlohmann::json::parse(bodies[1]);
    CHECK(
        flash["entity_id"] ==
        nlohmann::json::array({"light.one", "light.two"}));
    CHECK(flash["rgb_color"] == nlohmann::json::array({12, 34, 56}));
    CHECK(flash["flash"] == "long");
    const auto restore = nlohmann::json::parse(bodies[2]);
    CHECK(restore["entity_id"] == "scene.bambustudio_light_restore_2a");
}

TEST_CASE("Light alert transaction batches and caps thirty two entities",
          "[home-assistant][performance][lights][bounds]")
{
    Execution::LightAlertTransaction transaction;
    transaction.target_url = "https://ha.example.test";
    transaction.authorization = "Bearer synthetic";
    transaction.generation = 99;
    for (std::size_t index = 0; index < 40; ++index)
        transaction.light_entity_ids.push_back(
            "light.entity_" + std::to_string(index));
    transaction.light_entity_ids.push_back("light.entity_0");
    transaction.light_entity_ids.push_back("");

    std::vector<std::string> paths;
    std::vector<std::string> bodies;
    std::atomic_bool cancelled{false};
    Execution::run_light_alert_transaction(
        transaction,
        [&](const Execution::LightAlertTransaction &,
            const std::string &path,
            const std::string &body,
            long,
            bool,
            const std::atomic_bool &) {
            paths.push_back(path);
            bodies.push_back(body);
            return true;
        },
        [](std::chrono::milliseconds) { return true; },
        cancelled);

    REQUIRE(
        std::count(
            paths.begin(),
            paths.end(),
            "/api/services/light/turn_on") == 1);
    const auto snapshot = nlohmann::json::parse(bodies[0]);
    const auto flash = nlohmann::json::parse(bodies[1]);
    REQUIRE(
        snapshot["snapshot_entities"].size() ==
        Execution::kMaxLightEntityIds);
    REQUIRE(
        flash["entity_id"].size() ==
        Execution::kMaxLightEntityIds);
    CHECK(snapshot["snapshot_entities"] == flash["entity_id"]);
    CHECK(snapshot["snapshot_entities"].front() == "light.entity_0");
    CHECK(snapshot["snapshot_entities"].back() == "light.entity_31");
}

TEST_CASE("Interrupted light alert restores its own generation immediately",
          "[home-assistant][performance][lights][shutdown]")
{
    Execution::LightAlertTransaction transaction;
    transaction.target_url       = "https://ha.example.test";
    transaction.authorization    = "Bearer synthetic";
    transaction.light_entity_ids = {"light.one", "light.two"};
    transaction.generation       = 7;

    std::vector<std::string> paths;
    std::vector<long> timeouts;
    std::vector<bool> shutdown_allowed;
    int waits = 0;
    std::atomic_bool cancelled{false};
    Execution::run_light_alert_transaction(
        transaction,
        [&](const Execution::LightAlertTransaction &,
            const std::string &path,
            const std::string &,
            long timeout_seconds,
            bool allow_during_shutdown,
            const std::atomic_bool &) {
            paths.push_back(path);
            timeouts.push_back(timeout_seconds);
            shutdown_allowed.push_back(allow_during_shutdown);
            if (path == "/api/services/light/turn_on")
                cancelled.store(true);
            return path != "/api/services/light/turn_on";
        },
        [&](std::chrono::milliseconds) {
            ++waits;
            return false;
        },
        cancelled);

    const std::vector<std::string> expected_paths{
        "/api/services/scene/create",
        "/api/services/light/turn_on",
        "/api/services/scene/turn_on",
        "/api/services/scene/delete",
    };
    const std::vector<long> expected_timeouts{
        Execution::kNormalLightRequestTimeoutSeconds,
        Execution::kNormalLightRequestTimeoutSeconds,
        Execution::kShutdownRecoveryTimeoutSeconds,
        Execution::kShutdownRecoveryTimeoutSeconds,
    };
    const std::vector<bool> expected_shutdown_allowed{false, false, true, true};
    CHECK(paths == expected_paths);
    CHECK(timeouts == expected_timeouts);
    CHECK(shutdown_allowed == expected_shutdown_allowed);
    CHECK(waits == 0);
}

TEST_CASE("Cancelled light alert deletes a snapshot that was never flashed",
          "[home-assistant][performance][lights][shutdown][cleanup]")
{
    Execution::LightAlertTransaction transaction;
    transaction.target_url       = "https://ha.example.test";
    transaction.authorization    = "Bearer synthetic";
    transaction.light_entity_ids = {"light.one"};
    transaction.generation       = 0x2c;

    std::vector<std::string> paths;
    std::vector<std::string> bodies;
    std::vector<long> timeouts;
    std::vector<bool> shutdown_allowed;
    int waits = 0;
    std::atomic_bool cancelled{false};
    Execution::run_light_alert_transaction(
        transaction,
        [&](const Execution::LightAlertTransaction &,
            const std::string &path,
            const std::string &body,
            long timeout_seconds,
            bool allow_during_shutdown,
            const std::atomic_bool &) {
            paths.push_back(path);
            bodies.push_back(body);
            timeouts.push_back(timeout_seconds);
            shutdown_allowed.push_back(allow_during_shutdown);
            if (path == "/api/services/scene/create")
                cancelled.store(true);
            return true;
        },
        [&](std::chrono::milliseconds) {
            ++waits;
            return true;
        },
        cancelled);

    const std::vector<std::string> expected_paths{
        "/api/services/scene/create",
        "/api/services/scene/delete",
    };
    const std::vector<long> expected_timeouts{
        Execution::kNormalLightRequestTimeoutSeconds,
        Execution::kShutdownRecoveryTimeoutSeconds,
    };
    const std::vector<bool> expected_shutdown_allowed{false, true};
    REQUIRE(paths == expected_paths);
    CHECK(timeouts == expected_timeouts);
    CHECK(shutdown_allowed == expected_shutdown_allowed);
    CHECK(waits == 0);
    REQUIRE(bodies.size() == 2);
    const auto cleanup = nlohmann::json::parse(bodies.back());
    CHECK(cleanup["entity_id"] == "scene.bambustudio_light_restore_2c");
}

TEST_CASE("Failed light restoration keeps its recovery scene",
          "[home-assistant][performance][lights][recovery]")
{
    Execution::LightAlertTransaction transaction;
    transaction.target_url       = "https://ha.example.test";
    transaction.authorization    = "Bearer synthetic";
    transaction.light_entity_ids = {"light.one"};
    transaction.generation       = 0x2b;

    std::vector<std::string> paths;
    std::atomic_bool cancel_requested{false};
    Execution::run_light_alert_transaction(
        transaction,
        [&paths](
            const Execution::LightAlertTransaction &,
            const std::string &path,
            const std::string &,
            long,
            bool,
            const std::atomic_bool &) {
            paths.push_back(path);
            return path != "/api/services/scene/turn_on";
        },
        [](std::chrono::milliseconds) { return true; },
        cancel_requested);

    REQUIRE(paths.size() == 4);
    CHECK(paths[0] == "/api/services/scene/create");
    CHECK(paths[1] == "/api/services/light/turn_on");
    CHECK(paths[2] == "/api/services/scene/turn_on");
    CHECK(paths[3] == "/api/services/scene/turn_on");
    CHECK(
        std::find(
            paths.begin(),
            paths.end(),
            "/api/services/scene/delete") == paths.end());
}

TEST_CASE("Light alert executor supersedes a pending target with one immutable newer generation",
          "[home-assistant][performance][lights][executor]")
{
    std::promise<void> first_started;
    auto first_started_future = first_started.get_future();
    std::promise<void> latest_seen;
    auto latest_seen_future = latest_seen.get_future();
    std::atomic_bool release_first{false};
    std::mutex observed_mutex;
    std::vector<std::uint64_t> observed_generations;

    Execution::LightAlertExecutor executor(
        [&](const Execution::LightAlertTransaction &transaction,
            const std::string &path,
            const std::string &,
            long,
            bool,
            const std::atomic_bool &cancel_requested) {
            if (path == "/api/services/scene/create") {
                {
                    std::lock_guard<std::mutex> lock(observed_mutex);
                    observed_generations.push_back(transaction.generation);
                }
                if (transaction.generation == 1) {
                    first_started.set_value();
                    while (!release_first.load() && !cancel_requested.load())
                        std::this_thread::yield();
                } else if (transaction.generation == 3) {
                    latest_seen.set_value();
                }
            }
            return false; // End each transaction after its snapshot attempt.
        },
        2);

    const auto transaction = [](std::uint64_t generation) {
        Execution::LightAlertTransaction value;
        value.target_url = "https://ha.example.test";
        value.authorization = "Bearer generation-" + std::to_string(generation);
        value.light_entity_ids = {"light.one"};
        value.generation = generation;
        return value;
    };

    REQUIRE(executor.submit(transaction(1)));
    REQUIRE(first_started_future.wait_for(std::chrono::seconds(2)) ==
            std::future_status::ready);
    REQUIRE(executor.submit(transaction(2)));
    REQUIRE(executor.submit(transaction(3)));
    release_first.store(true);
    REQUIRE(latest_seen_future.wait_for(std::chrono::seconds(2)) ==
            std::future_status::ready);
    executor.shutdown();

    std::lock_guard<std::mutex> lock(observed_mutex);
    const std::vector<std::uint64_t> expected_generations{1, 3};
    CHECK(observed_generations == expected_generations);
}

TEST_CASE("Light alert executor contains request exceptions",
          "[home-assistant][performance][lights][executor][exceptions]")
{
    std::promise<void> failing_request_started;
    auto failing_request_future = failing_request_started.get_future();
    std::promise<void> survivor_seen;
    auto survivor_future = survivor_seen.get_future();

    Execution::LightAlertExecutor executor(
        [&](const Execution::LightAlertTransaction &transaction,
            const std::string &path,
            const std::string &,
            long,
            bool,
            const std::atomic_bool &) {
            if (path != "/api/services/scene/create")
                return false;
            if (transaction.generation == 1) {
                failing_request_started.set_value();
                throw std::runtime_error("synthetic request failure");
            }
            survivor_seen.set_value();
            return false;
        },
        1);

    const auto transaction = [](std::uint64_t generation) {
        Execution::LightAlertTransaction value;
        value.target_url = "https://ha.example.test";
        value.authorization = "Bearer synthetic";
        value.light_entity_ids = {"light.one"};
        value.generation = generation;
        return value;
    };

    REQUIRE(executor.submit(transaction(1)));
    REQUIRE(failing_request_future.wait_for(std::chrono::seconds(2)) ==
            std::future_status::ready);
    REQUIRE(executor.submit(transaction(2)));
    CHECK(survivor_future.wait_for(std::chrono::seconds(2)) ==
          std::future_status::ready);
    executor.shutdown();
}

TEST_CASE("Home Assistant URL policy protects every bearer request", "[home-assistant][transport]")
{
    const auto safe = CredentialTransportSafety::Safe;
    const auto missing = CredentialTransportSafety::NotConfigured;
    const auto insecure = CredentialTransportSafety::Insecure;

    CHECK(credential_transport_safety_for_url("") == missing);
    CHECK(credential_transport_safety_for_url("https://homeassistant.local:8123") == safe);
    CHECK(credential_transport_safety_for_url("HTTPS://192.168.50.10") == safe);
    CHECK(credential_transport_safety_for_url("http://localhost:8123") == safe);
    CHECK(credential_transport_safety_for_url("http://LOCALHOST.:8123") == safe);
    CHECK(credential_transport_safety_for_url("http://127.0.0.2:8123") == safe);

    CHECK(credential_transport_safety_for_url("http://192.168.50.10:8123") == insecure);
    CHECK(credential_transport_safety_for_url("http://homeassistant.local:8123") == insecure);
    CHECK(credential_transport_safety_for_url("http://[::1]:8123") == insecure);
    CHECK(credential_transport_safety_for_url("ftp://localhost") == insecure);
    CHECK(credential_transport_safety_for_url("localhost:8123") == insecure);
    CHECK(credential_transport_safety_for_url("https://user@example.test") == insecure);
    CHECK(credential_transport_safety_for_url("http://::1:8123") == insecure);
    CHECK(credential_transport_safety_for_url(" http://localhost:8123") == insecure);
    CHECK(credential_transport_safety_for_url("https://example.test:") == insecure);
    CHECK(credential_transport_safety_for_url("https://example.test:70000") == insecure);
    CHECK(credential_transport_safety_for_url("https://-example.test") == insecure);
    CHECK(credential_transport_safety_for_url("https://example.test?token=nope") == insecure);
    CHECK(credential_transport_safety_for_url("https://example.test/#fragment") == insecure);
    CHECK(credential_transport_safety_for_url("https://[not-ipv6]") == insecure);
}

TEST_CASE("Pairing tokens are fresh and URL safe", "[home-assistant][sharing]")
{
    const std::string first = SharingService::make_pairing_token();
    const std::string second = SharingService::make_pairing_token();

    REQUIRE(first.size() >= 32);
    REQUIRE(first.size() <= 128);
    CHECK(first != second);
    CHECK(std::all_of(first.begin(), first.end(), [](unsigned char value) {
        return std::isalnum(value) != 0 || value == '-' || value == '_' || value == '.' || value == '~';
    }));
}

TEST_CASE("Sharing start failures use stable structured error codes",
          "[home-assistant][sharing][errors]")
{
    {
        SharingService service;
        SharingService::Options options;
        options.pairing_token = SharingService::make_pairing_token();
        const auto result = service.start(std::move(options), {});
        CHECK_FALSE(result);
        CHECK(
            result.error_code ==
            SharingStartErrorCode::MissingSupplier);
        CHECK(result.native_error == 0);
    }
    {
        SharingService service;
        SharingService::Options options;
        options.pairing_token = "too-short";
        const auto result =
            service.start(std::move(options), [] { return valid_payload(); });
        CHECK_FALSE(result);
        CHECK(
            result.error_code ==
            SharingStartErrorCode::InvalidPairingToken);
        CHECK(result.native_error == 0);
    }
    {
        SharingService service;
        SharingService::Options options;
        options.pairing_token = SharingService::make_pairing_token();
        options.display_name.clear();
        const auto result =
            service.start(std::move(options), [] { return valid_payload(); });
        CHECK_FALSE(result);
        CHECK(
            result.error_code ==
            SharingStartErrorCode::InvalidDisplayName);
        CHECK(result.native_error == 0);
    }

    RunningShare share(valid_payload());
    REQUIRE(share.result);
    CHECK(share.result.error_code == SharingStartErrorCode::None);
    CHECK(share.result.native_error == 0);

    SharingService::Options duplicate_options;
    duplicate_options.pairing_token = SharingService::make_pairing_token();
    const auto duplicate = share.service.start(
        std::move(duplicate_options),
        [] { return valid_payload(); });
    CHECK_FALSE(duplicate);
    CHECK(
        duplicate.error_code ==
        SharingStartErrorCode::AlreadyRunning);
    CHECK(duplicate.native_error == 0);
}

TEST_CASE("Sharing rejects loopback and public advertised addresses", "[home-assistant][sharing][security]")
{
    const auto rejected = [](const std::string &address) {
        SharingService service;
        SharingService::Options options;
        options.pairing_token   = SharingService::make_pairing_token();
        options.display_name    = "Bambu Studio address policy test";
        options.advertised_ipv4 = address;
        const auto result = service.start(std::move(options), [] { return valid_payload(); });
        CHECK_FALSE(result);
        CHECK(
            result.error_code ==
            SharingStartErrorCode::InvalidAdvertisedAddress);
        CHECK(result.native_error == 0);
        CHECK_FALSE(service.is_running());
    };

    rejected("127.0.0.1");
    rejected("169.254.1.1");
    rejected("8.8.8.8");
    rejected("192.0.2.1");
    rejected("255.255.255.255");
}

TEST_CASE("mDNS sender policy accepts only usable hosts on the advertised link",
          "[home-assistant][sharing][security][mdns]")
{
    using Slic3r::GUI::HomeAssistant::SharingNetworkPolicy::
        mdns_sender_is_eligible_ipv4;

    constexpr std::uint32_t advertised = 0xc0a8320a; // 192.168.50.10
    CHECK(mdns_sender_is_eligible_ipv4(advertised, 0xc0a83214, 24));
    CHECK(mdns_sender_is_eligible_ipv4(advertised, advertised, 24));
    CHECK_FALSE(mdns_sender_is_eligible_ipv4(
        advertised,
        0xc0a83314,
        24));
    CHECK_FALSE(mdns_sender_is_eligible_ipv4(
        advertised,
        0xc0a83200,
        24));
    CHECK_FALSE(mdns_sender_is_eligible_ipv4(
        advertised,
        0xc0a832ff,
        24));
    CHECK_FALSE(mdns_sender_is_eligible_ipv4(
        advertised,
        0x08080808,
        24));
    CHECK_FALSE(mdns_sender_is_eligible_ipv4(
        advertised,
        0xa9fe0102,
        16));
    CHECK_FALSE(mdns_sender_is_eligible_ipv4(
        advertised,
        0xc0a83214,
        0));
    CHECK(mdns_sender_is_eligible_ipv4(
        0x64400102,
        0x64400103,
        24));
}

TEST_CASE("Sharing auto-detection follows the ordinary default route",
          "[home-assistant][sharing][network]")
{
    const auto expected = default_route_test_address();
    if (!expected) {
        WARN("The host has no RFC1918/shared-space IPv4 default route to compare");
        return;
    }

    RunningShare share(valid_payload());
    REQUIRE(share.result);
    CHECK(share.result.error_code == SharingStartErrorCode::None);
    CHECK(share.result.native_error == 0);
    CHECK(share.result.advertised_ipv4 == expected->to_string());
}

TEST_CASE("Sharing rejects privileged explicit ports", "[home-assistant][sharing][security]")
{
    SharingService service;
    SharingService::Options options;
    options.pairing_token = SharingService::make_pairing_token();
    options.display_name  = "Bambu Studio port policy test";
    options.port          = 80;
    const auto result = service.start(std::move(options), [] { return valid_payload(); });
    CHECK_FALSE(result);
    CHECK(result.error_code == SharingStartErrorCode::InvalidPort);
    CHECK(result.native_error == 0);
    CHECK_FALSE(service.is_running());
}

TEST_CASE("Sharing endpoint authenticates before reading printer data", "[home-assistant][sharing]")
{
    RunningShare share(valid_payload());
    REQUIRE(share.result);

    const auto missing = request(share.result, http::verb::get, "/bambustudio/printers");
    CHECK(missing.result() == http::status::unauthorized);
    CHECK(missing.body() == "Authentication required.\n");
    CHECK(share.supplier_calls->load() == 0);

    const auto wrong = request(
        share.result,
        http::verb::get,
        "/bambustudio/printers",
        {"Bearer definitely-not-the-pairing-value"});
    CHECK(wrong.result() == http::status::unauthorized);
    CHECK(wrong.body() == missing.body());
    CHECK(share.supplier_calls->load() == 0);

    const auto duplicate = request(
        share.result,
        http::verb::get,
        "/bambustudio/printers",
        {"Bearer " + share.token, "Bearer " + share.token});
    CHECK(duplicate.result() == http::status::unauthorized);
    CHECK(share.supplier_calls->load() == 0);

    const auto accepted = request(
        share.result,
        http::verb::get,
        "/bambustudio/printers",
        {"Bearer " + share.token});
    REQUIRE(accepted.result() == http::status::ok);
    CHECK(accepted[http::field::cache_control] == "no-store");
    CHECK(accepted[http::field::content_type] == "application/json; charset=utf-8");
    REQUIRE(share.supplier_calls->load() == 1);

    const auto payload = nlohmann::json::parse(accepted.body());
    REQUIRE(payload.size() == 1);
    REQUIRE(payload.contains("printers"));
    REQUIRE(payload["printers"].size() == 1);
    const auto &printer = payload["printers"][0];
    CHECK(printer.size() == 4);
    CHECK(printer.contains("serial"));
    CHECK(printer.contains("host"));
    CHECK(printer.contains("access_code"));
    CHECK(printer.contains("name"));
    CHECK_FALSE(printer.contains("ignored"));
}

TEST_CASE("Sharing endpoint has one exact method and path", "[home-assistant][sharing]")
{
    RunningShare share(valid_payload());
    REQUIRE(share.result);

    const std::vector<std::string> auth{"Bearer " + share.token};
    CHECK(request(share.result, http::verb::post, "/bambustudio/printers", auth).result() ==
          http::status::method_not_allowed);
    CHECK(request(share.result, http::verb::get, "/bambustudio/printers?all=true", auth).result() ==
          http::status::not_found);
    CHECK(request(share.result, http::verb::get, "/bambustudio/printer", auth).result() ==
          http::status::not_found);
    CHECK(share.supplier_calls->load() == 0);
}

TEST_CASE("Sharing endpoint rejects malformed and oversized offers generically", "[home-assistant][sharing]")
{
    const auto rejected = [](std::string payload) {
        RunningShare share(std::move(payload));
        REQUIRE(share.result);
        const auto response = request(
            share.result,
            http::verb::get,
            "/bambustudio/printers",
            {"Bearer " + share.token});
        CHECK(response.result() == http::status::service_unavailable);
        CHECK(response.body() == "Printer data is temporarily unavailable.\n");
    };

    rejected("not json");
    rejected(std::string(SharingService::max_offer_response_bytes() + 1, 'x'));
    nlohmann::json too_many = nlohmann::json::array();
    for (std::size_t index = 0; index <= SharingService::max_printer_count(); ++index)
        too_many.push_back({{"serial", "s"}, {"host", "h"}, {"access_code", "a"}});
    {
        RunningShare share(nlohmann::json{{"printers", std::move(too_many)}}.dump());
        REQUIRE(share.result);
        const auto response = request(
            share.result,
            http::verb::get,
            "/bambustudio/printers",
            {"Bearer " + share.token});
        REQUIRE(response.result() == http::status::ok);
        CHECK(nlohmann::json::parse(response.body())["printers"].size() ==
              SharingService::max_printer_count());
    }
    rejected(nlohmann::json{
        {"printers", nlohmann::json::array({
            {{"serial", std::string(257, 's')}, {"host", "h"}, {"access_code", "a"}},
        })},
    }.dump());
}

TEST_CASE("Authorized offer responses are cached and rate bounded",
          "[home-assistant][sharing][amplification]")
{
    RunningShare share(valid_payload());
    REQUIRE(share.result);
    const std::vector<std::string> auth{"Bearer " + share.token};

    for (std::size_t index = 0; index < SharingService::offer_response_burst(); ++index)
        REQUIRE(request(share.result, http::verb::get, "/bambustudio/printers", auth).result() ==
                http::status::ok);
    CHECK(share.supplier_calls->load() == 1);

    const auto limited =
        request(share.result, http::verb::get, "/bambustudio/printers", auth);
    CHECK(limited.result() == http::status::too_many_requests);
    CHECK(limited["Retry-After"] == "1");
    CHECK(limited.body() == "Too many requests; retry later.\n");
    CHECK(share.supplier_calls->load() == 1);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(SharingService::offer_response_refill_ms() + 100));
    CHECK(request(share.result, http::verb::get, "/bambustudio/printers", auth).result() ==
          http::status::ok);
    CHECK(share.supplier_calls->load() == 1);
}

TEST_CASE("Rate-limited offers do not invoke the printer supplier",
          "[home-assistant][sharing][amplification]")
{
    SharingService service;
    SharingService::Options options;
    options.pairing_token = SharingService::make_pairing_token();
    options.display_name  = "Bambu Studio supplier budget test";
    const std::string token = options.pairing_token;
    auto supplier_calls = std::make_shared<std::atomic<int>>(0);
    const auto endpoint = service.start(
        std::move(options),
        [supplier_calls] {
            const int call = ++(*supplier_calls);
            return call <= static_cast<int>(SharingService::offer_response_burst())
                ? std::string("not json")
                : valid_payload();
        });
    REQUIRE(endpoint);
    const std::vector<std::string> auth{"Bearer " + token};

    for (std::size_t index = 0; index < SharingService::offer_response_burst(); ++index)
        CHECK(request(endpoint, http::verb::get, "/bambustudio/printers", auth).result() ==
              http::status::service_unavailable);
    REQUIRE(supplier_calls->load() ==
            static_cast<int>(SharingService::offer_response_burst()));

    const auto limited =
        request(endpoint, http::verb::get, "/bambustudio/printers", auth);
    CHECK(limited.result() == http::status::too_many_requests);
    CHECK(limited["Retry-After"] == "1");
    CHECK(supplier_calls->load() ==
          static_cast<int>(SharingService::offer_response_burst()));

    std::this_thread::sleep_for(
        std::chrono::milliseconds(SharingService::offer_response_refill_ms() + 100));
    CHECK(request(endpoint, http::verb::get, "/bambustudio/printers", auth).result() ==
          http::status::ok);
    CHECK(supplier_calls->load() ==
          static_cast<int>(SharingService::offer_response_burst() + 1));
}

TEST_CASE("mDNS response scheduling coalesces and paces a query flood",
          "[home-assistant][sharing][amplification][mdns]")
{
    RunningShare share(valid_payload());
    REQUIRE(share.result);

    const auto observation_window = std::chrono::milliseconds(600);
    const auto responses =
        count_mdns_flood_responses(share.result, 512, observation_window);
    const auto maximum_paced_responses =
        2 + observation_window.count() /
                static_cast<std::int64_t>(SharingService::mdns_response_interval_ms());

    CHECK(responses >= 1);
    CHECK(responses <= static_cast<std::size_t>(maximum_paced_responses));
}

TEST_CASE("mDNS query admission is globally token bounded before refill",
          "[home-assistant][sharing][amplification][mdns][tokens]")
{
    RunningShare share(valid_payload());
    REQUIRE(share.result);

    const auto responses = count_unique_mdns_flood_responses(
        share.result,
        SharingService::mdns_query_burst() + 32,
        std::chrono::milliseconds(600),
        0x5000);
    CHECK(responses >= 1);
    CHECK(responses <= SharingService::mdns_query_burst());

    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            SharingService::mdns_query_refill_ms() + 100));
    const auto after_refill = count_unique_mdns_flood_responses(
        share.result,
        1,
        std::chrono::milliseconds(300),
        0x6000);
    CHECK(after_refill == 1);
}

TEST_CASE("Sharing lifecycle is idempotent and restartable", "[home-assistant][sharing]")
{
    SharingService service;
    SharingService::Options first_options;
    first_options.pairing_token = SharingService::make_pairing_token();
    first_options.display_name  = "Bambu Studio restart test";
    const auto first = service.start(first_options, [] { return valid_payload(); });
    REQUIRE(first);
    CHECK(service.is_running());
    CHECK(service.port() == first.port);

    // Stop with more unique responses arriving than the bounded queue can
    // retain. Restarting must not inherit a send timer, packet, or queue item.
    send_mdns_flood(first, SharingService::max_pending_mdns_responses() * 4, true);
    service.stop();
    service.stop();
    CHECK_FALSE(service.is_running());
    CHECK(service.port() == 0);

    SharingService::Options second_options;
    second_options.pairing_token = SharingService::make_pairing_token();
    second_options.display_name  = "Bambu Studio restart test";
    const auto second = service.start(second_options, [] { return valid_payload(); });
    REQUIRE(second);
    CHECK(service.is_running());
    const auto responses = count_mdns_flood_responses(
        second,
        64,
        std::chrono::milliseconds(400));
    CHECK(responses >= 1);
    CHECK(responses <= 2 + 400 / SharingService::mdns_response_interval_ms());
    service.stop();
}

TEST_CASE("Concurrent sharing stops serialize join and cleanup",
          "[home-assistant][sharing][lifecycle][concurrency]")
{
    SharingService service;
    SharingService::Options options;
    options.pairing_token = SharingService::make_pairing_token();
    options.display_name = "Bambu Studio concurrent stop test";
    REQUIRE(service.start(options, [] { return valid_payload(); }));

    std::promise<void> release_stops;
    const std::shared_future<void> release = release_stops.get_future().share();
    std::thread first([&service, release] {
        release.wait();
        service.stop();
    });
    std::thread second([&service, release] {
        release.wait();
        service.stop();
    });
    release_stops.set_value();
    first.join();
    second.join();

    CHECK_FALSE(service.is_running());
    CHECK(service.port() == 0);

    SharingService::Options restart_options;
    restart_options.pairing_token = SharingService::make_pairing_token();
    restart_options.display_name = "Bambu Studio post-race restart test";
    REQUIRE(service.start(
        std::move(restart_options),
        [] { return valid_payload(); }));
    service.stop();
}
