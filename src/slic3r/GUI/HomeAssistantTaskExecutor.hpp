#ifndef slic3r_GUI_HomeAssistantTaskExecutor_hpp_
#define slic3r_GUI_HomeAssistantTaskExecutor_hpp_

#include "nlohmann/json.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI { namespace HomeAssistant { namespace Execution {

template <typename Result>
class OnceUiCompletion
{
public:
    using Callback = std::function<void(Result)>;
    using Post = std::function<bool(std::function<void()>)>;

    OnceUiCompletion(Post post, Callback callback)
        : m_state(std::make_shared<State>(std::move(post), std::move(callback)))
    {}

    bool dispatch(Result result) const noexcept
    {
        const std::shared_ptr<State> state = m_state;
        if (!state || !state->post || !state->callback ||
            state->claimed.exchange(true))
            return false;

        try {
            return state->post(
                [state, result = std::move(result)]() mutable {
                    try {
                        Callback callback = std::move(state->callback);
                        if (callback)
                            callback(std::move(result));
                    } catch (...) {
                        // Consumer callbacks must not escape into an event
                        // dispatcher or terminate a worker.
                    }
                });
        } catch (...) {
            // Scheduling can allocate. The completion remains claimed so a
            // second producer cannot deliver a conflicting terminal result.
            return false;
        }
    }

    bool claimed() const noexcept
    {
        return m_state && m_state->claimed.load();
    }

private:
    struct State
    {
        State(Post post_value, Callback callback_value)
            : post(std::move(post_value))
            , callback(std::move(callback_value))
        {}

        Post             post;
        Callback         callback;
        std::atomic_bool claimed{false};
    };

    std::shared_ptr<State> m_state;
};

// Executes whole, independently safe transactions. A task is either waiting
// or active; callers never enqueue individual phases of one operation.
class BoundedTaskExecutor
{
public:
    using Task = std::function<void(const std::atomic_bool &cancel_requested)>;
    using DiscardHandler = std::function<void()>;

    enum class SubmitStatus : std::uint8_t
    {
        Accepted,
        QueueFull,
        Stopping,
        InvalidTask,
        AllocationFailure,
    };

    BoundedTaskExecutor(std::size_t worker_count, std::size_t max_pending)
        : m_max_pending(max_pending)
    {
        if (worker_count == 0 || max_pending == 0)
            throw std::invalid_argument("A bounded executor needs workers and queue capacity");

        try {
            m_workers.reserve(worker_count);
            for (std::size_t index = 0; index < worker_count; ++index)
                m_workers.emplace_back([this] { worker_loop(); });
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopping = true;
                m_cancel_requested.store(true);
            }
            m_condition.notify_all();
            for (std::thread &worker : m_workers) {
                if (worker.joinable())
                    worker.join();
            }
            throw;
        }
    }

    ~BoundedTaskExecutor()
    {
        shutdown();
    }

    BoundedTaskExecutor(const BoundedTaskExecutor &) = delete;
    BoundedTaskExecutor &operator=(const BoundedTaskExecutor &) = delete;

    bool submit(Task task) noexcept
    {
        return submit_with_discard(std::move(task), {}) == SubmitStatus::Accepted;
    }

    SubmitStatus submit_with_discard(
        Task task,
        DiscardHandler on_discard) noexcept
    {
        if (!task)
            return SubmitStatus::InvalidTask;
        try {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping)
                return SubmitStatus::Stopping;
            if (m_tasks.size() >= m_max_pending)
                return SubmitStatus::QueueFull;
            m_tasks.push_back({std::move(task), std::move(on_discard)});
        } catch (...) {
            return SubmitStatus::AllocationFailure;
        }
        m_condition.notify_one();
        return SubmitStatus::Accepted;
    }

    void shutdown() noexcept
    {
        std::call_once(m_shutdown_once, [this] {
            std::deque<PendingTask> discarded;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopping = true;
                m_cancel_requested.store(true);
                // Waiting tasks have not performed any phase and are safe to
                // discard. Notify their owners after releasing the executor
                // lock. Active whole transactions observe cancellation.
                discarded.swap(m_tasks);
            }
            for (PendingTask &pending : discarded) {
                if (!pending.on_discard)
                    continue;
                try {
                    pending.on_discard();
                } catch (...) {
                    // A discard observer cannot prevent deterministic worker
                    // shutdown.
                }
            }
            m_condition.notify_all();
            for (std::thread &worker : m_workers) {
                if (worker.joinable())
                    worker.join();
            }
        });
    }

    std::size_t pending() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

private:
    struct PendingTask
    {
        Task           task;
        DiscardHandler on_discard;
    };

    void worker_loop()
    {
        while (true) {
            PendingTask pending;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
                if (m_stopping)
                    return;
                pending = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            try {
                pending.task(m_cancel_requested);
            } catch (...) {
                // One malformed transaction must not terminate an owned
                // worker or prevent deterministic shutdown.
            }
        }
    }

    const std::size_t          m_max_pending;
    mutable std::mutex         m_mutex;
    std::condition_variable    m_condition;
    std::deque<PendingTask>    m_tasks;
    std::vector<std::thread>   m_workers;
    std::atomic_bool           m_cancel_requested{false};
    bool                       m_stopping = false;
    std::once_flag             m_shutdown_once;
};

inline constexpr std::size_t kMaxEntityQueryDomains = 4;
inline constexpr std::size_t kMaxEntityFetchResults = 512;
inline constexpr std::size_t kMaxConfiguredEntityValues = 32;
inline constexpr std::size_t kMaxConfiguredEntitySegments = 256;
inline constexpr std::size_t kMaxConfiguredEntityScanBytes = 64 * 1024;
inline constexpr std::size_t kMaxEntityDomainBytes = 64;
inline constexpr std::size_t kMaxEntityIdBytes = 256;
inline constexpr std::size_t kMaxEntityStateBytes = 256;
inline constexpr std::size_t kMaxFriendlyNameBytes = 512;

inline std::vector<std::string> bounded_semicolon_config_values(
    const std::string &raw,
    std::size_t max_values,
    std::size_t max_value_bytes = kMaxEntityIdBytes,
    std::size_t max_segments = kMaxConfiguredEntitySegments,
    std::size_t max_scan_bytes = kMaxConfiguredEntityScanBytes)
{
    std::vector<std::string> values;
    if (raw.empty() || max_values == 0 || max_value_bytes == 0 ||
        max_segments == 0 || max_scan_bytes == 0)
        return values;

    const std::size_t value_limit =
        std::min(max_values, kMaxConfiguredEntityValues);
    const std::size_t value_byte_limit =
        std::min(max_value_bytes, kMaxEntityIdBytes);
    const std::size_t segment_limit =
        std::min(max_segments, kMaxConfiguredEntitySegments);
    const std::size_t scan_byte_limit =
        std::min(max_scan_bytes, kMaxConfiguredEntityScanBytes);
    values.reserve(value_limit);
    std::size_t begin = 0;
    std::size_t inspected_segments = 0;
    const std::size_t scan_end = std::min(raw.size(), scan_byte_limit);
    while (begin <= raw.size() &&
           begin < scan_end &&
           inspected_segments < segment_limit &&
           values.size() < value_limit) {
        const auto separator_it = std::find(
            raw.begin() + begin,
            raw.begin() + scan_end,
            ';');
        if (separator_it == raw.begin() + scan_end &&
            scan_end < raw.size())
            break;
        const std::size_t separator =
            separator_it == raw.begin() + scan_end
                ? std::string::npos
                : static_cast<std::size_t>(
                      separator_it - raw.begin());
        const std::size_t end =
            separator == std::string::npos ? raw.size() : separator;
        const std::size_t length = end - begin;
        ++inspected_segments;
        if (length != 0 && length <= value_byte_limit) {
            const bool duplicate = std::any_of(
                values.begin(),
                values.end(),
                [&raw, begin, length](const std::string &value) {
                    return value.size() == length &&
                           raw.compare(begin, length, value) == 0;
                });
            if (!duplicate)
                values.emplace_back(raw, begin, length);
        }

        if (separator == std::string::npos)
            break;
        begin = separator + 1;
    }
    return values;
}

struct FilteredEntityState
{
    std::string entity_id;
    std::string friendly_name;
    std::string state;
};

enum class FilteredEntityParseStatus : std::uint8_t
{
    Success,
    InvalidFilter,
    InvalidResponse,
};

struct FilteredEntityBatch
{
    std::vector<FilteredEntityState> entities;
    FilteredEntityParseStatus status = FilteredEntityParseStatus::Success;
    bool truncated = false;

    explicit operator bool() const noexcept
    {
        return status == FilteredEntityParseStatus::Success;
    }
};

inline bool valid_entity_domains(const std::vector<std::string> &domains)
{
    if (domains.empty() || domains.size() > kMaxEntityQueryDomains)
        return false;

    for (std::size_t index = 0; index < domains.size(); ++index) {
        const std::string &domain = domains[index];
        if (domain.empty() || domain.size() > kMaxEntityDomainBytes ||
            domain.front() < 'a' || domain.front() > 'z' ||
            std::find(domains.begin(), domains.begin() + index, domain) !=
                domains.begin() + index)
            return false;
        if (!std::all_of(
                domain.begin() + 1,
                domain.end(),
                [](unsigned char value) {
                    return (value >= 'a' && value <= 'z') ||
                           (value >= '0' && value <= '9') ||
                           value == '_';
                }))
            return false;
    }
    return true;
}

inline FilteredEntityBatch parse_filtered_entity_states(
    const std::string &body,
    const std::vector<std::string> &domains,
    std::size_t max_results = kMaxEntityFetchResults)
{
    FilteredEntityBatch result;
    if (!valid_entity_domains(domains)) {
        result.status = FilteredEntityParseStatus::InvalidFilter;
        return result;
    }

    try {
        const nlohmann::json states =
            nlohmann::json::parse(body, nullptr, false);
        if (!states.is_array()) {
            result.status = FilteredEntityParseStatus::InvalidResponse;
            return result;
        }

        const std::size_t result_limit =
            std::min(max_results, kMaxEntityFetchResults);
        result.entities.reserve(
            std::min(result_limit, states.size()));
        for (const nlohmann::json &state_object : states) {
            if (!state_object.is_object())
                continue;

            const auto entity_id_it = state_object.find("entity_id");
            if (entity_id_it == state_object.end() ||
                !entity_id_it->is_string())
                continue;
            const std::string &entity_id =
                entity_id_it->get_ref<const std::string &>();
            if (entity_id.empty() || entity_id.size() > kMaxEntityIdBytes)
                continue;

            const bool requested = std::any_of(
                domains.begin(),
                domains.end(),
                [&entity_id](const std::string &domain) {
                    return entity_id.size() > domain.size() + 1 &&
                           entity_id.compare(0, domain.size(), domain) == 0 &&
                           entity_id[domain.size()] == '.';
                });
            if (!requested)
                continue;

            const auto state_it = state_object.find("state");
            if (state_it == state_object.end() || !state_it->is_string())
                continue;
            const std::string &state =
                state_it->get_ref<const std::string &>();
            if (state.size() > kMaxEntityStateBytes)
                continue;

            if (result.entities.size() >= result_limit) {
                result.truncated = true;
                break;
            }

            std::string friendly_name = entity_id;
            const auto attributes_it = state_object.find("attributes");
            if (attributes_it != state_object.end() &&
                attributes_it->is_object()) {
                const auto friendly_it = attributes_it->find("friendly_name");
                if (friendly_it != attributes_it->end() &&
                    friendly_it->is_string()) {
                    const std::string &candidate =
                        friendly_it->get_ref<const std::string &>();
                    if (!candidate.empty() &&
                        candidate.size() <= kMaxFriendlyNameBytes)
                        friendly_name = candidate;
                }
            }

            result.entities.push_back(
                {entity_id, std::move(friendly_name), state});
        }
    } catch (...) {
        result.entities.clear();
        result.truncated = false;
        result.status = FilteredEntityParseStatus::InvalidResponse;
    }
    return result;
}

struct LightAlertTransaction
{
    std::string              target_url;
    std::string              authorization;
    std::vector<std::string> light_entity_ids;
    int                      red = 0;
    int                      green = 0;
    int                      blue = 0;
    int                      flashes = 3;
    std::uint64_t            generation = 0;
};

using LightRequest = std::function<bool(
    const LightAlertTransaction &transaction,
    const std::string &path,
    const std::string &body,
    long timeout_seconds,
    bool allow_during_shutdown,
    const std::atomic_bool &cancel_requested)>;
using InterruptibleWait = std::function<bool(std::chrono::milliseconds)>;

inline constexpr long kNormalLightRequestTimeoutSeconds = 10;
inline constexpr long kShutdownRecoveryTimeoutSeconds   = 2;
inline constexpr std::size_t kMaxLightEntityIds = 32;

inline std::vector<std::string> bounded_light_entity_ids(
    const std::vector<std::string> &entity_ids)
{
    std::vector<std::string> result;
    result.reserve(std::min(entity_ids.size(), kMaxLightEntityIds));
    for (const std::string &entity_id : entity_ids) {
        if (entity_id.empty() ||
            std::find(result.begin(), result.end(), entity_id) != result.end())
            continue;
        result.push_back(entity_id);
        if (result.size() == kMaxLightEntityIds)
            break;
    }
    return result;
}

inline std::string light_alert_scene_slug(std::uint64_t generation)
{
    std::ostringstream out;
    out << "bambustudio_light_restore_" << std::hex << generation;
    return out.str();
}

// Runs snapshot -> flash -> restore as one dependency-ordered transaction.
// If shutdown interrupts a flash request, restoration is attempted
// immediately using the same immutable target, authorization, and generation.
inline void run_light_alert_transaction(
    const LightAlertTransaction &transaction,
    const LightRequest &request,
    const InterruptibleWait &wait,
    const std::atomic_bool &cancel_requested)
{
    if (!request || !wait || transaction.light_entity_ids.empty() ||
        cancel_requested.load())
        return;

    const std::vector<std::string> light_entity_ids =
        bounded_light_entity_ids(transaction.light_entity_ids);
    if (light_entity_ids.empty())
        return;

    const std::string scene_slug = light_alert_scene_slug(transaction.generation);
    const std::string scene_entity = "scene." + scene_slug;
    const std::string scene_body = nlohmann::json{
        {"scene_id", scene_slug},
        {"snapshot_entities", light_entity_ids},
    }.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    if (!request(
            transaction,
            "/api/services/scene/create",
            scene_body,
            kNormalLightRequestTimeoutSeconds,
            false,
            cancel_requested))
        return;

    const std::string restore_body =
        nlohmann::json{{"entity_id", scene_entity}}.dump(
            -1, ' ', false, nlohmann::json::error_handler_t::replace);
    bool flash_attempted = false;
    // Keep cancellation at one decision point after scene creation. A separate
    // earlier check left a narrow hand-off race where cancellation could flip
    // between two loads, skip the flash, and also skip scene cleanup.
    if (!cancel_requested.load()) {
        flash_attempted = true;
        const std::string body = nlohmann::json{
            {"entity_id", light_entity_ids},
            {"rgb_color", {transaction.red, transaction.green, transaction.blue}},
            {"flash", transaction.flashes > 1 ? "long" : "short"},
        }.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        request(
            transaction,
            "/api/services/light/turn_on",
            body,
            kNormalLightRequestTimeoutSeconds,
            false,
            cancel_requested);
    }
    if (!flash_attempted) {
        // Nothing has been flashed, so there is nothing to restore. Remove the
        // freshly created dynamic scene during shutdown instead of leaking one
        // recovery scene per cancellation in this hand-off window.
        request(
            transaction,
            "/api/services/scene/delete",
            restore_body,
            kShutdownRecoveryTimeoutSeconds,
            true,
            cancel_requested);
        return;
    }

    bool restored = false;
    if (!cancel_requested.load())
        wait(std::chrono::seconds(4));
    restored = request(
        transaction,
        "/api/services/scene/turn_on",
        restore_body,
        kShutdownRecoveryTimeoutSeconds,
        true,
        cancel_requested);

    if (!cancel_requested.load()) {
        wait(std::chrono::seconds(2));
        restored = request(
            transaction,
            "/api/services/scene/turn_on",
            restore_body,
            kShutdownRecoveryTimeoutSeconds,
            true,
            cancel_requested) || restored;
    }

    // Dynamic scenes are removed only after Home Assistant confirms at least
    // one restoration request. If every restore attempt fails, retain the
    // generation-specific scene as a manual recovery option.
    if (!restored)
        return;
    request(
        transaction,
        "/api/services/scene/delete",
        restore_body,
        kShutdownRecoveryTimeoutSeconds,
        true,
        cancel_requested);
}

// A single worker serializes real-light transactions. At most one latest
// pending transaction is kept per immutable HA target; replacement swaps the
// whole transaction and never mutates a queued target, token, or generation.
class LightAlertExecutor
{
public:
    LightAlertExecutor(LightRequest request, std::size_t max_pending_targets)
        : m_request(std::move(request))
        , m_max_pending_targets(max_pending_targets)
    {
        if (!m_request || max_pending_targets == 0)
            throw std::invalid_argument("A light executor needs a request function and capacity");
        m_worker = std::thread([this] { worker_loop(); });
    }

    ~LightAlertExecutor()
    {
        shutdown();
    }

    LightAlertExecutor(const LightAlertExecutor &) = delete;
    LightAlertExecutor &operator=(const LightAlertExecutor &) = delete;

    bool submit(LightAlertTransaction transaction) noexcept
    {
        try {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping)
                return false;

            const auto exact_duplicate = std::find_if(
                m_pending.begin(),
                m_pending.end(),
                [&transaction](const LightAlertTransaction &queued) {
                    return queued.target_url == transaction.target_url &&
                           queued.generation == transaction.generation;
                });
            if (exact_duplicate != m_pending.end())
                return true;

            const auto superseded = std::find_if(
                m_pending.begin(),
                m_pending.end(),
                [&transaction](const LightAlertTransaction &queued) {
                    return queued.target_url == transaction.target_url;
                });
            if (superseded != m_pending.end()) {
                if (transaction.generation > superseded->generation)
                    *superseded = std::move(transaction);
                m_condition.notify_all();
                return true;
            }

            if (m_pending.size() >= m_max_pending_targets)
                return false;
            m_pending.push_back(std::move(transaction));
        } catch (...) {
            return false;
        }
        m_condition.notify_one();
        return true;
    }

    void shutdown() noexcept
    {
        std::call_once(m_shutdown_once, [this] {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopping = true;
                m_cancel_requested.store(true);
                // A pending transaction has not created a scene or flashed a
                // light, so dropping it cannot leave a partial state.
                m_pending.clear();
            }
            m_condition.notify_all();
            if (m_worker.joinable())
                m_worker.join();
        });
    }

private:
    bool wait_interruptibly(std::chrono::milliseconds delay)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return !m_condition.wait_for(
            lock,
            delay,
            [this] { return m_cancel_requested.load(); });
    }

    void worker_loop()
    {
        while (true) {
            LightAlertTransaction transaction;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stopping || !m_pending.empty(); });
                if (m_stopping)
                    return;
                transaction = std::move(m_pending.front());
                m_pending.pop_front();
            }

            try {
                run_light_alert_transaction(
                    transaction,
                    m_request,
                    [this](std::chrono::milliseconds delay) {
                        return wait_interruptibly(delay);
                    },
                    m_cancel_requested);
            } catch (...) {
                // Keep the worker available if request setup, JSON building,
                // or a malformed local value throws unexpectedly.
            }
        }
    }

    LightRequest                        m_request;
    const std::size_t                  m_max_pending_targets;
    std::mutex                         m_mutex;
    std::condition_variable            m_condition;
    std::deque<LightAlertTransaction>  m_pending;
    std::atomic_bool                   m_cancel_requested{false};
    bool                               m_stopping = false;
    std::thread                        m_worker;
    std::once_flag                     m_shutdown_once;
};

}}}} // namespace Slic3r::GUI::HomeAssistant::Execution

#endif // slic3r_GUI_HomeAssistantTaskExecutor_hpp_
