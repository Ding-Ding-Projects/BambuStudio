#include "StatusHubService.hpp"
#include <status_hub_client.hpp>
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace Slic3r {
namespace {
std::string configuration(const char* name)
{
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}
}

bool validStatusHubEndpoint(const std::string& value) noexcept
{
    // An origin only, HTTPS only, no credentials, query, fragment or path.
    // This consumer never guesses an endpoint or follows a user document URL.
    if (value.size() < 9 || value.size() > 800 || value.compare(0, 8, "https://") != 0)
        return false;
    auto authority = value.substr(8);
    if (!authority.empty() && authority.back() == '/') authority.pop_back();
    if (authority.empty() || authority.find_first_of("/@?#\\ \t\r\n") != std::string::npos)
        return false;
    // Only the default HTTPS port is supported. IPv6 literals are deliberately
    // unsupported until a proper URL parser is introduced at this boundary.
    const auto port = authority.find(':');
    if (port != std::string::npos) {
        if (authority.substr(port) != ":443") return false;
        authority.resize(port);
    }
    if (authority.empty() || authority.front() == '.' || authority.back() == '.') return false;
    for (unsigned char c : authority)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '.')) return false;
    return authority.find("..") == std::string::npos;
}

HubState statusHubConfigurationState(const std::string& endpoint, bool enrolled) noexcept
{
    if (endpoint.empty()) return HubState::not_configured;
    if (!validStatusHubEndpoint(endpoint)) return HubState::invalid_configuration;
    return enrolled ? HubState::pending : HubState::enrollment_required;
}

StatusHubService& StatusHubService::instance()
{
    static StatusHubService service;
    return service;
}
StatusHubService::~StatusHubService() { stop(); }

void StatusHubService::start(std::string repository_path)
{
    std::lock_guard<std::mutex> lifecycle(m_lifecycle);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_worker.joinable()) return;
    m_stop = false;
    m_requested = true;
    m_snapshot = HubSnapshot{};
    m_snapshot.state = HubState::pending;
    m_snapshot.retry_available = true;
    m_worker = std::thread([this, repository_path = std::move(repository_path)] {
        try { run(repository_path); }
        catch (...) { publish(HubState::delivery_failed, "internal_error"); }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.retry_available = false;
    });
}

void StatusHubService::checkpoint()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // stop() joins outside this mutex. Test its protected flag first so a
    // concurrent checkpoint never reads the thread object during join().
    if (m_stop || !m_worker.joinable()) return;
    m_requested = true; // One pending slot, not an unbounded queue.
    m_condition.notify_one();
}
void StatusHubService::retry() { checkpoint(); }
void StatusHubService::stop()
{
    std::lock_guard<std::mutex> lifecycle(m_lifecycle);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_worker.joinable()) return;
        m_stop = true;
        m_condition.notify_one();
    }
    m_worker.join();
}
HubSnapshot StatusHubService::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}
void StatusHubService::publish(HubState state, const std::string& code, int http,
                               bool accepted, std::uint64_t replies)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.state = state;
    m_snapshot.result_code = code;
    m_snapshot.http_status = http;
    m_snapshot.busy = false;
    m_snapshot.updated_at = std::chrono::system_clock::now();
    ++m_snapshot.revision;
    if (accepted) {
        ++m_snapshot.accepted;
        m_snapshot.last_accepted_at = m_snapshot.updated_at;
    }
    m_snapshot.replies_received += replies;
}

void StatusHubService::run(std::string repository_path)
{
    std::unique_ptr<status_hub::HttpTransport> transport;
    std::unique_ptr<status_hub::StatusHubClient> client;
    // Configuration and enrollment are immutable for one process lifetime.
    // Retry never replaces a session key after authentication rejection.
    const auto endpoint = configuration("STATUS_HUB_URL");
    const auto enrollment = configuration("AGENT_INGEST_TOKEN");
    const auto configured = statusHubConfigurationState(endpoint, !enrollment.empty());
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.endpoint_present = !endpoint.empty();
        m_snapshot.enrollment_present = !enrollment.empty();
        m_snapshot.inventory_applicable = !repository_path.empty();
    }
    if (configured == HubState::pending) {
        transport = status_hub::makeDefaultTransport();
        auto id = status_hub::generateSessionKey();
        if (!transport || !id.ok) {
            publish(HubState::delivery_failed, "initialization_failed");
            return;
        }
        status_hub::ClientOptions options;
        options.sessionId = "bambu-" + id.value;
        options.baseUrl = endpoint;
        options.ingestToken = enrollment;
        options.startHeartbeat = false; // This worker owns the quiet heartbeat.
        options.timeoutMs = 1200;
        options.finishDeadlineMs = 1200;
        options.maxSendAttempts = 1;
        options.backoffCapMs = 0;
        options.coalesceMs = 0;
        options.probeRetryMs = 15000;
        client = std::make_unique<status_hub::StatusHubClient>(*transport, options);
    }
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait_for(lock, std::chrono::seconds(60), [this] { return m_stop || m_requested; });
            if (m_stop) break;
            m_requested = false;
            m_snapshot.busy = true;
            ++m_snapshot.attempts;
        }
        if (!client) {
            publish(configured, configured == HubState::not_configured ? "endpoint_missing" :
                configured == HubState::enrollment_required ? "missing_ingest_token" : "invalid_url");
            continue;
        }
        status_hub::SessionPatch patch;
        patch.title = "Bambu Studio";
        patch.status = status_hub::Status::running;
        patch.summary = "Ready";
        patch.machine = status_hub::machineLabel();
        if (!repository_path.empty()) {
            status_hub::WorktreeReportOptions inventory;
            inventory.repositoryPath = repository_path;
            inventory.budgetMs = 500;
            auto rows = status_hub::collectWorktrees(inventory);
            if (!rows.ok) {
                publish(HubState::delivery_failed, "worktree_incomplete");
                continue; // Never publish a partial inventory as complete.
            }
            patch.worktrees = std::move(rows.value);
        }
        auto result = client->update(patch);
        const int http_status = result.ok ? result.value.httpStatus : result.httpStatus;
        const bool accepted = result.ok && !result.degraded && !result.coalesced &&
                              http_status >= 200 && http_status < 300;
        publish(accepted ? HubState::delivered : HubState::delivery_failed,
                status_hub::resultCodeName(result.code), http_status, accepted);
        if (accepted) {
            auto replies = client->pollReplies();
            if (!replies.ok) {
                publish(HubState::delivery_failed, status_hub::resultCodeName(replies.code), replies.httpStatus);
            } else if (!replies.value.replies.empty()) {
                // Owner inbox content is never copied into UI, logs or exports.
                publish(HubState::delivered, "replies_received", http_status, false,
                        replies.value.replies.size());
            }
        }
    }
    if (client) {
        auto result = client->finish(status_hub::Status::waiting);
        const int http_status = result.ok ? result.value.httpStatus : result.httpStatus;
        const bool accepted = result.ok && !result.degraded &&
                              http_status >= 200 && http_status < 300;
        publish(accepted ? HubState::stopped : HubState::delivery_failed,
                accepted ? "terminal_delivered" : status_hub::resultCodeName(result.code),
                http_status, accepted);
        client->close();
    } else publish(HubState::stopped, "stopped_without_delivery");
}
}
