#include "HomeAssistant.hpp"

#include "GUI_App.hpp"
#include "HomeAssistantTaskExecutor.hpp"
#include "HomeAssistantTransportPolicy.hpp"
#include "slic3r/Utils/Http.hpp"

#include "libslic3r/AppConfig.hpp"

#include "nlohmann/json.hpp"

#include <boost/log/trivial.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <wx/app.h>

namespace Slic3r { namespace GUI { namespace HomeAssistant {

namespace {

constexpr std::size_t kMaxServiceResponseBytes = 1024 * 1024;
constexpr std::size_t kMaxStatesResponseBytes  = 4 * 1024 * 1024;
constexpr std::size_t kServiceWorkerCount      = 4;
constexpr std::size_t kMaxQueuedServiceCalls   = 64;
constexpr std::size_t kMaxConfiguredEntities   = 32;
constexpr std::size_t kMaxPrinterHandoverCount = 32;
constexpr std::size_t kPrinterImportWorkerCount = 4;
constexpr std::size_t kMaxPendingLightTargets  = 4;

std::atomic_bool s_shutdown_requested{false};

using EntityFetchCompletion =
    Execution::OnceUiCompletion<EntityFetchResult>;

EntityFetchCompletion make_entity_fetch_completion(
    EntityFetchCallback done)
{
    return EntityFetchCompletion(
        [](std::function<void()> callback) {
            if (wxTheApp == nullptr)
                return false;
            try {
                wxTheApp->CallAfter(std::move(callback));
                return true;
            } catch (...) {
                return false;
            }
        },
        [done = std::move(done)](EntityFetchResult result) mutable {
            done(std::move(result));
        });
}

EntityFetchResult entity_fetch_error(
    EntityFetchErrorCode code,
    unsigned http_status = 0)
{
    EntityFetchResult result;
    result.error_code = code;
    if (http_status <= std::numeric_limits<std::uint16_t>::max())
        result.http_status = static_cast<std::uint16_t>(http_status);
    return result;
}

bool is_http_body_limit_error(const std::string &error)
{
    // Http currently reports its write-callback limit using this stable
    // internal prefix. It is used only to select a structured code; the raw
    // transport prose and response body never cross to the UI.
    static constexpr const char *prefix =
        "HTTP body data size exceeded limit";
    return error.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

struct ConnectionSnapshot
{
    std::string url;
    std::string token;
};

ConnectionSnapshot connection_snapshot()
{
    ConnectionSnapshot connection;
    connection.url   = wxGetApp().app_config->get("ha_url");
    connection.token = wxGetApp().app_config->get("ha_token");
    while (!connection.url.empty() && connection.url.back() == '/')
        connection.url.pop_back();
    return connection;
}

CredentialTransportSafety credential_transport_safety(const ConnectionSnapshot &connection)
{
    if (connection.token.empty())
        return CredentialTransportSafety::NotConfigured;
    return credential_transport_safety_for_url(connection.url);
}

std::vector<std::string> configured_list(const std::string &key)
{
    const std::string raw = wxGetApp().app_config->get(key);
    return Execution::bounded_semicolon_config_values(
        raw,
        kMaxConfiguredEntities);
}

struct PostResult
{
    bool     completed = false;
    unsigned status = 0;
};

bool post_printer_import_completion(
    std::function<void(int, std::vector<std::string>)> done,
    int processed,
    std::vector<std::string> errors) noexcept
{
    if (!done || s_shutdown_requested.load() || wxTheApp == nullptr)
        return false;
    try {
        wxTheApp->CallAfter(
            [done = std::move(done),
             processed,
             errors = std::move(errors)]() mutable {
                if (s_shutdown_requested.load())
                    return;
                try {
                    done(processed, std::move(errors));
                } catch (...) {
                    // Consumer code cannot escape into the wx dispatcher.
                }
            });
        return true;
    } catch (...) {
        // Scheduling can allocate, and wx may already be tearing down.
        return false;
    }
}

PostResult perform_http_post(
    const std::string &url,
    const std::string &authorization,
    const std::string &body,
    long timeout_seconds,
    const std::atomic_bool &cancel_requested,
    bool allow_during_shutdown,
    bool log_failure)
{
    PostResult result;
    if (cancel_requested.load() && !allow_during_shutdown)
        return result;

    try {
        auto http = Http::post(url);
        http.follow_redirects(false)
            .verbose(false)
            .header("Authorization", authorization)
            .header("Content-Type", "application/json")
            .set_post_body(body)
            .timeout_max(timeout_seconds)
            .size_limit(kMaxServiceResponseBytes)
            .on_progress(
                [&cancel_requested, allow_during_shutdown](Http::Progress, bool &cancel) {
                    cancel = !allow_during_shutdown && cancel_requested.load();
                })
            .on_complete([&result](std::string, unsigned status) {
                result.completed = true;
                result.status = status;
            })
            .on_error([&result](std::string, std::string, unsigned status) {
                result.status = status;
            })
            .perform_sync();
    } catch (...) {
        // Bodies, authorization values, and exception details are never
        // logged because all three may contain credentials.
    }
    if (log_failure && !result.completed &&
        !(cancel_requested.load() && !allow_during_shutdown)) {
        BOOST_LOG_TRIVIAL(info)
            << "HomeAssistant: request failed: "
            << (result.status ? "HTTP " + std::to_string(result.status)
                              : "transport error");
    }
    return result;
}

struct PrinterImportJob
{
    std::string                                        url;
    std::string                                        authorization;
    std::vector<PrinterHandover>                       printers;
    bool                                               truncated = false;
    std::function<void(int, std::vector<std::string>)> done;
};

class PrinterImportDispatcher
{
public:
    PrinterImportDispatcher()
    {
        m_worker = std::thread([this] {
            try {
                worker_loop();
            } catch (...) {
                // No allocation, JSON, callback, or HTTP setup failure may
                // escape an owned worker and terminate the process.
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopping = true;
                m_busy = false;
                m_job.reset();
            }
        });
    }

    ~PrinterImportDispatcher()
    {
        shutdown();
    }

    bool enqueue(PrinterImportJob &&job)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping || m_busy)
                return false;
            m_job = std::move(job);
            m_busy = true;
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
                m_job.reset();
            }
            m_condition.notify_all();
            if (m_worker.joinable())
                m_worker.join();
        });
    }

private:
    bool stopping() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stopping;
    }

    void worker_loop()
    {
        while (true) {
            PrinterImportJob job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stopping || m_job.has_value(); });
                if (m_stopping) {
                    m_job.reset();
                    m_busy = false;
                    return;
                }
                job = std::move(*m_job);
                m_job.reset();
            }

            int processed = 0;
            std::vector<std::string> errors;
            bool task_failed = false;
            try {
                if (job.truncated) {
                    errors.emplace_back(
                        "Additional printer requests were skipped because the 32-printer limit was reached");
                }

                // Match the companion integration's four-connection import
                // bound. A dead 32-printer batch now occupies at most eight
                // 30-second waves rather than serially tying up this worker
                // for as long as sixteen minutes.
                for (std::size_t offset = 0;
                     offset < job.printers.size();
                     offset += kPrinterImportWorkerCount) {
                    if (stopping())
                        break;

                    struct PendingImport
                    {
                        std::string             serial;
                        std::future<PostResult> future;
                    };
                    std::vector<PendingImport> pending;
                    const std::size_t end = std::min(
                        offset + kPrinterImportWorkerCount,
                        job.printers.size());
                    pending.reserve(end - offset);

                    for (std::size_t index = offset; index < end; ++index) {
                        const PrinterHandover printer = job.printers[index];
                        try {
                            pending.push_back(PendingImport{
                                printer.serial,
                                std::async(
                                    std::launch::async,
                                    [this, &job, printer] {
                                        PostResult result;
                                        try {
                                            nlohmann::json body = {
                                                {"serial", printer.serial},
                                                {"host", printer.host},
                                                {"access_code", printer.access_code},
                                            };
                                            if (!printer.name.empty())
                                                body["name"] = printer.name;

                                            result = perform_http_post(
                                                job.url,
                                                job.authorization,
                                                body.dump(
                                                    -1,
                                                    ' ',
                                                    false,
                                                    nlohmann::json::error_handler_t::replace),
                                                30,
                                                m_cancel_requested,
                                                false,
                                                false);
                                        } catch (...) {
                                            // The caller reports a generic,
                                            // credential-free per-printer error.
                                        }
                                        return result;
                                    })});
                        } catch (...) {
                            if (!m_cancel_requested.load())
                                errors.push_back(
                                    printer.serial + ": transport error");
                        }
                    }

                    for (PendingImport &item : pending) {
                        PostResult result;
                        try {
                            result = item.future.get();
                        } catch (...) {
                            // The per-printer generic result below is sufficient.
                        }
                        if (result.completed) {
                            ++processed;
                        } else if (!m_cancel_requested.load()) {
                            errors.push_back(
                                item.serial + ": " +
                                (result.status
                                     ? "HTTP " + std::to_string(result.status)
                                     : "transport error"));
                        }
                    }
                }
            } catch (...) {
                task_failed = true;
            }
            if (task_failed && !m_cancel_requested.load()) {
                try {
                    errors.emplace_back(
                        "The Home Assistant printer import worker could not finish the request");
                } catch (...) {
                    // An allocation failure may also prevent constructing an
                    // explanatory string; the callback still receives the
                    // honest processed count.
                }
            }

            bool deliver = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_busy = false;
                deliver = !m_stopping;
            }
            if (deliver)
                post_printer_import_completion(
                    std::move(job.done),
                    processed,
                    std::move(errors));
        }
    }

    mutable std::mutex             m_mutex;
    std::condition_variable        m_condition;
    std::optional<PrinterImportJob> m_job;
    bool                            m_busy = false;
    bool                            m_stopping = false;
    std::thread                     m_worker;
    std::atomic_bool                m_cancel_requested{false};
    std::once_flag                  m_shutdown_once;
};

class RuntimeState
{
public:
    ~RuntimeState()
    {
        shutdown();
    }

    std::shared_ptr<Execution::BoundedTaskExecutor> service_executor()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (s_shutdown_requested.load())
            return {};
        if (!m_service_executor) {
            m_service_executor = std::make_shared<Execution::BoundedTaskExecutor>(
                kServiceWorkerCount,
                kMaxQueuedServiceCalls);
        }
        return m_service_executor;
    }

    std::shared_ptr<Execution::LightAlertExecutor> light_executor()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (s_shutdown_requested.load())
            return {};
        if (!m_light_executor) {
            m_light_executor = std::make_shared<Execution::LightAlertExecutor>(
                [](const Execution::LightAlertTransaction &transaction,
                   const std::string &path,
                   const std::string &body,
                   long timeout_seconds,
                   bool allow_during_shutdown,
                   const std::atomic_bool &cancel_requested) {
                    return perform_http_post(
                               transaction.target_url + path,
                               transaction.authorization,
                               body,
                               timeout_seconds,
                               cancel_requested,
                               allow_during_shutdown,
                               true)
                        .completed;
                },
                kMaxPendingLightTargets);
        }
        return m_light_executor;
    }

    std::shared_ptr<Execution::BoundedTaskExecutor> query_executor()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (s_shutdown_requested.load())
            return {};
        if (!m_query_executor)
            m_query_executor =
                std::make_shared<Execution::BoundedTaskExecutor>(1, 1);
        return m_query_executor;
    }

    std::shared_ptr<PrinterImportDispatcher> printer_executor()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (s_shutdown_requested.load())
            return {};
        if (!m_printer_executor)
            m_printer_executor = std::make_shared<PrinterImportDispatcher>();
        return m_printer_executor;
    }

    std::uint64_t next_light_generation()
    {
        return m_light_generation.fetch_add(1);
    }

    void shutdown() noexcept
    {
        std::shared_ptr<Execution::BoundedTaskExecutor> services;
        std::shared_ptr<Execution::BoundedTaskExecutor> queries;
        std::shared_ptr<Execution::LightAlertExecutor> lights;
        std::shared_ptr<PrinterImportDispatcher> printers;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (s_shutdown_requested.exchange(true))
                return;
            services = std::move(m_service_executor);
            queries = std::move(m_query_executor);
            lights = std::move(m_light_executor);
            printers = std::move(m_printer_executor);
        }

        // Lights restore their own active generation before the general
        // service workers disappear. Pending whole transactions are inert.
        if (lights)
            lights->shutdown();
        if (services)
            services->shutdown();
        if (queries)
            queries->shutdown();
        if (printers)
            printers->shutdown();
    }

private:
    std::mutex                                         m_mutex;
    std::shared_ptr<Execution::BoundedTaskExecutor>    m_service_executor;
    std::shared_ptr<Execution::BoundedTaskExecutor>    m_query_executor;
    std::shared_ptr<Execution::LightAlertExecutor>     m_light_executor;
    std::shared_ptr<PrinterImportDispatcher>           m_printer_executor;
    std::atomic<std::uint64_t>                         m_light_generation{
        static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count())};
};

RuntimeState &runtime_state()
{
    static RuntimeState state;
    return state;
}

bool post_service_async(const std::string &path, const std::string &body)
{
    if (s_shutdown_requested.load())
        return false;
    const ConnectionSnapshot connection = connection_snapshot();
    if (credential_transport_safety(connection) != CredentialTransportSafety::Safe)
        return false;

    std::shared_ptr<Execution::BoundedTaskExecutor> executor;
    try {
        executor = runtime_state().service_executor();
    } catch (...) {
        BOOST_LOG_TRIVIAL(info)
            << "HomeAssistant: service executor could not start";
        return false;
    }
    if (!executor)
        return false;
    const std::string url = connection.url + path;
    const std::string authorization = "Bearer " + connection.token;
    bool accepted = false;
    try {
        accepted = executor->submit(
            [url, authorization, body](const std::atomic_bool &cancel_requested) {
                perform_http_post(
                    url,
                    authorization,
                    body,
                    10,
                    cancel_requested,
                    false,
                    true);
            });
    } catch (...) {
        // std::function construction may allocate before submit() begins.
    }
    if (!accepted && !s_shutdown_requested.load()) {
        BOOST_LOG_TRIVIAL(info)
            << "HomeAssistant: service transaction dropped because the queue is full";
    }
    return accepted;
}

} // namespace

bool configured()
{
    if (s_shutdown_requested.load())
        return false;
    const ConnectionSnapshot connection = connection_snapshot();
    return !connection.url.empty() && !connection.token.empty();
}

CredentialTransportSafety credential_transport_safety()
{
    if (s_shutdown_requested.load())
        return CredentialTransportSafety::NotConfigured;
    return credential_transport_safety(connection_snapshot());
}

void list_entities(EntityQuery query, EntityFetchCallback done)
{
    if (!done)
        return;

    const EntityFetchCompletion completion =
        make_entity_fetch_completion(std::move(done));
    if (s_shutdown_requested.load()) {
        completion.dispatch(
            entity_fetch_error(EntityFetchErrorCode::ShuttingDown));
        return;
    }
    if (!Execution::valid_entity_domains(query.domains)) {
        completion.dispatch(
            entity_fetch_error(EntityFetchErrorCode::InvalidFilter));
        return;
    }

    const ConnectionSnapshot connection = connection_snapshot();
    const CredentialTransportSafety transport = credential_transport_safety(connection);
    if (transport != CredentialTransportSafety::Safe) {
        completion.dispatch(entity_fetch_error(
            transport == CredentialTransportSafety::NotConfigured
                ? EntityFetchErrorCode::NotConfigured
                : EntityFetchErrorCode::InsecureTransport));
        return;
    }

    const std::string url  = connection.url + "/api/states";
    const std::string auth = "Bearer " + connection.token;
    std::shared_ptr<Execution::BoundedTaskExecutor> executor;
    try {
        executor = runtime_state().query_executor();
    } catch (...) {
        completion.dispatch(
            entity_fetch_error(EntityFetchErrorCode::WorkerUnavailable));
        return;
    }
    if (!executor) {
        completion.dispatch(entity_fetch_error(
            s_shutdown_requested.load()
                ? EntityFetchErrorCode::ShuttingDown
                : EntityFetchErrorCode::WorkerUnavailable));
        return;
    }

    Execution::BoundedTaskExecutor::SubmitStatus submit_status =
        Execution::BoundedTaskExecutor::SubmitStatus::AllocationFailure;
    try {
        submit_status = executor->submit_with_discard(
            [url,
             auth,
             domains = std::move(query.domains),
             completion](const std::atomic_bool &cancel_requested) mutable {
                EntityFetchResult result =
                    entity_fetch_error(EntityFetchErrorCode::TransportError);
                bool terminal_callback_ran = false;
                bool response_too_large = false;
                try {
                    auto http = Http::get(url);
                    http.follow_redirects(false)
                        .verbose(false)
                        .header("Authorization", auth)
                        .header("Accept", "application/json")
                        .header("Accept-Encoding", "identity")
                        .timeout_max(10)
                        .size_limit(kMaxStatesResponseBytes)
                        .on_progress(
                            [&cancel_requested,
                             &response_too_large](Http::Progress progress, bool &cancel) {
                            response_too_large =
                                response_too_large ||
                                progress.dltotal > kMaxStatesResponseBytes ||
                                progress.dlnow > kMaxStatesResponseBytes;
                            cancel = cancel_requested.load();
                        })
                        .on_complete(
                            [&result,
                             &domains,
                             &terminal_callback_ran](std::string body, unsigned) {
                            terminal_callback_ran = true;
                            Execution::FilteredEntityBatch filtered =
                                Execution::parse_filtered_entity_states(
                                    body,
                                    domains,
                                    Execution::kMaxEntityFetchResults);
                            if (!filtered) {
                                result = entity_fetch_error(
                                    filtered.status ==
                                            Execution::FilteredEntityParseStatus::InvalidFilter
                                        ? EntityFetchErrorCode::InvalidFilter
                                        : EntityFetchErrorCode::InvalidResponse);
                                return;
                            }

                            result = EntityFetchResult{};
                            result.truncated = filtered.truncated;
                            result.entities.reserve(filtered.entities.size());
                            for (Execution::FilteredEntityState &state :
                                 filtered.entities) {
                                result.entities.push_back(
                                    {std::move(state.entity_id),
                                     std::move(state.friendly_name),
                                     std::move(state.state)});
                            }
                        })
                        .on_error(
                            [&result,
                             &terminal_callback_ran,
                             &response_too_large](
                                std::string,
                                std::string error,
                                unsigned status) {
                            terminal_callback_ran = true;
                            if (status != 0) {
                                result = entity_fetch_error(
                                    EntityFetchErrorCode::HttpStatus,
                                    status);
                            } else if (
                                response_too_large ||
                                is_http_body_limit_error(error)) {
                                result = entity_fetch_error(
                                    EntityFetchErrorCode::ResponseTooLarge);
                            } else {
                                result = entity_fetch_error(
                                    EntityFetchErrorCode::TransportError);
                            }
                        })
                        .perform_sync();
                } catch (...) {
                    result = entity_fetch_error(
                        EntityFetchErrorCode::RequestSetupFailed);
                    terminal_callback_ran = true;
                }

                if (cancel_requested.load() ||
                    s_shutdown_requested.load()) {
                    result = entity_fetch_error(
                        EntityFetchErrorCode::ShuttingDown);
                } else if (!terminal_callback_ran) {
                    result = entity_fetch_error(
                        EntityFetchErrorCode::TransportError);
                }
                completion.dispatch(std::move(result));
            },
            [completion]() {
                completion.dispatch(
                    entity_fetch_error(EntityFetchErrorCode::ShuttingDown));
            });
    } catch (...) {
        completion.dispatch(
            entity_fetch_error(EntityFetchErrorCode::WorkerUnavailable));
        return;
    }

    switch (submit_status) {
    case Execution::BoundedTaskExecutor::SubmitStatus::Accepted:
        break;
    case Execution::BoundedTaskExecutor::SubmitStatus::QueueFull:
        completion.dispatch(
            entity_fetch_error(EntityFetchErrorCode::QueueFull));
        break;
    case Execution::BoundedTaskExecutor::SubmitStatus::Stopping:
        completion.dispatch(
            entity_fetch_error(EntityFetchErrorCode::ShuttingDown));
        break;
    case Execution::BoundedTaskExecutor::SubmitStatus::InvalidTask:
    case Execution::BoundedTaskExecutor::SubmitStatus::AllocationFailure:
        completion.dispatch(
            entity_fetch_error(EntityFetchErrorCode::WorkerUnavailable));
        break;
    }
}

void call_service(const std::string &domain, const std::string &service, const std::string &json_body)
{
    post_service_async("/api/services/" + domain + "/" + service, json_body);
}

void media_command(const std::string &entity_id, const std::string &service)
{
    call_service("media_player", service,
                 nlohmann::json{{"entity_id", entity_id}}.dump());
}

void media_volume(const std::string &entity_id, double level_0_1)
{
    call_service("media_player", "volume_set",
                 nlohmann::json{{"entity_id", entity_id}, {"volume_level", level_0_1}}.dump());
}

void speak_on_speakers(const wxString &line)
{
    if (s_shutdown_requested.load())
        return;
    const auto speakers = configured_list("ha_speakers");
    if (speakers.empty() || !configured())
        return;
    for (const std::string &speaker : speakers) {
        // Modern HA: tts.speak against the default TTS engine entity.
        nlohmann::json body = {
            {"entity_id", wxGetApp().app_config->get("ha_tts_entity").empty()
                              ? std::string("tts.google_translate_en_com")
                              : wxGetApp().app_config->get("ha_tts_entity")},
            {"media_player_entity_id", speaker},
            {"message", std::string(line.ToUTF8())},
        };
        call_service("tts", "speak", body.dump());
    }
}

void add_printers(const std::vector<PrinterHandover> &printers,
                  std::function<void(int, std::vector<std::string>)> done)
{
    if (!done || s_shutdown_requested.load() || wxTheApp == nullptr)
        return;
    const ConnectionSnapshot connection = connection_snapshot();
    const CredentialTransportSafety transport = credential_transport_safety(connection);
    if (transport == CredentialTransportSafety::NotConfigured) {
        post_printer_import_completion(
            std::move(done),
            0,
            {std::string("Home Assistant is not configured")});
        return;
    }
    if (transport == CredentialTransportSafety::Insecure) {
        post_printer_import_completion(
            std::move(done),
            0,
            {std::string("Home Assistant credential transport is insecure")});
        return;
    }
    if (printers.empty()) {
        post_printer_import_completion(std::move(done), 0, {});
        return;
    }

    const std::string url  = connection.url + "/api/services/bambu_lab/add_printer";
    const std::string auth = "Bearer " + connection.token;

    PrinterImportJob job;
    job.url           = url;
    job.authorization = auth;
    const std::size_t count = std::min(printers.size(), kMaxPrinterHandoverCount);
    job.printers.assign(printers.begin(), printers.begin() + count);
    job.truncated = printers.size() > count;
    job.done      = done;

    bool accepted = false;
    bool dispatcher_unavailable = false;
    try {
        const auto executor = runtime_state().printer_executor();
        accepted = executor && executor->enqueue(std::move(job));
    } catch (...) {
        // A worker construction failure is reported through the same honest
        // user-visible completion channel as a request failure.
        dispatcher_unavailable = true;
    }
    if (!accepted) {
        post_printer_import_completion(
            std::move(done),
            0,
            {dispatcher_unavailable
                 ? std::string("The Home Assistant printer import worker could not start")
                 : std::string("A Home Assistant printer import is already in progress")});
    }
}

void flash_lights(int r, int g, int b, int flashes)
{
    if (s_shutdown_requested.load())
        return;
    const auto lights = configured_list("ha_lights");
    const ConnectionSnapshot connection = connection_snapshot();
    if (lights.empty() ||
        credential_transport_safety(connection) != CredentialTransportSafety::Safe)
        return;

    Execution::LightAlertTransaction transaction;
    transaction.target_url       = connection.url;
    transaction.authorization    = "Bearer " + connection.token;
    transaction.light_entity_ids = lights;
    transaction.red              = r;
    transaction.green            = g;
    transaction.blue             = b;
    transaction.flashes          = flashes;
    transaction.generation       = runtime_state().next_light_generation();

    std::shared_ptr<Execution::LightAlertExecutor> executor;
    try {
        executor = runtime_state().light_executor();
    } catch (...) {
        BOOST_LOG_TRIVIAL(info)
            << "HomeAssistant: light alert executor could not start";
        return;
    }
    if (!executor || !executor->submit(std::move(transaction))) {
        BOOST_LOG_TRIVIAL(info)
            << "HomeAssistant: light alert transaction dropped because its queue is full";
    }
}

void shutdown() noexcept
{
    runtime_state().shutdown();
}

} } } // namespace Slic3r::GUI::HomeAssistant
