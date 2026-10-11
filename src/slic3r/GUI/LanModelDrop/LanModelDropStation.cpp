// LAN model drop, station side: the worker thread, the GUI-thread state and the notifications.
// See LanModelDropStation.hpp and docs/features/application-integration/lan-model-drop.md.

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <iphlpapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif
#endif

#include "LanModelDropStation.hpp"
#include "LanModelDropFiles.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../NotificationManager.hpp"
#include "../Plater.hpp"
#include "../format.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"
#include "slic3r/Utils/Http.hpp"

#include <openssl/evp.h>

#include <boost/log/trivial.hpp>

#include <wx/app.h>
#include <wx/arrstr.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

namespace {

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------------------------------------
// Shared between the GUI thread and the worker (guarded by Shared::mutex)
// ---------------------------------------------------------------------------------------------

struct WorkerConfig
{
    std::uint64_t              generation = 0; // any settings change
    std::uint64_t              identity   = 0; // a change of address or key: start a fresh inbox
    std::uint64_t              session    = 0; // switched on again: start a fresh inbox
    bool                       enabled    = false;
    std::optional<BaseAddress> address;
    std::string                key; // secret: wiped when replaced, never logged
    std::filesystem::path      received_root;
    std::filesystem::path      pending_file; // the deletes the drop sites have not confirmed yet

    WorkerConfig() = default;
    WorkerConfig(const WorkerConfig &) = default;
    WorkerConfig &operator=(const WorkerConfig &) = default;
    ~WorkerConfig() { wipe(key); }
    bool usable() const { return address.has_value() && is_station_key(key); }
};

struct Shared
{
    std::mutex               mutex;
    std::condition_variable  wake;
    bool                     stop     = false;
    bool                     poll_now = false;
    bool                     test     = false;
    bool                     new_code = false;
    std::vector<PendingDelete> decided; // items the user opened or discarded: DELETE them at their site
    WorkerConfig             config;
};

// shutdown() joins the worker. Should the process end on a path that skips it, the thread object is
// detached at static destruction instead of being destroyed while joinable, which would call
// std::terminate. (By then ExitProcess has already ended the thread itself.)
struct WorkerThread
{
    std::thread thread;
    ~WorkerThread()
    {
        if (thread.joinable()) thread.detach();
    }
};

Shared                     g_shared;
WorkerThread               g_worker;
std::thread               &g_thread = g_worker.thread;
std::atomic<bool>          g_stopping{false};
std::atomic<std::uint64_t> g_identity{0};
std::atomic<std::uint64_t> g_session{0};
std::atomic<bool>          g_enabled{false};
// Every callback the worker queues for the GUI thread checks this first; shutdown() clears it, so
// nothing queued before the join reaches a window afterwards.
const std::shared_ptr<std::atomic<bool>> g_alive = std::make_shared<std::atomic<bool>>(true);

// ---------------------------------------------------------------------------------------------
// GUI-thread state
// ---------------------------------------------------------------------------------------------

struct Waiting
{
    std::string           id;
    std::filesystem::path file;
    std::string           file_name;
    std::string           sender;
    std::uint64_t         bytes = 0;
    std::string           text; // the notification text, distinct among waiting files
    std::string           base; // the drop site that listed it, which alone is asked to delete it
};

struct GuiState
{
    View                                  view;
    std::uint64_t                         generation = 0;
    std::vector<Waiting>                  waiting;
    std::map<int, std::function<void()>>  listeners;
    int                                   next_listener = 1;
    std::vector<std::string>              lan_candidates;
    bool                                  lan_read = false;
    std::set<std::string>                 decided; // opened or discarded this session
};

GuiState &gui()
{
    static GuiState state;
    return state;
}

std::filesystem::path data_root() { return std::filesystem::u8path(data_dir()); }
std::filesystem::path drop_root() { return data_root() / "lan-model-drop"; }
std::filesystem::path received_root() { return drop_root() / "received"; }
std::filesystem::path pending_file() { return drop_root() / "pending-deletes.txt"; }
std::filesystem::path key_file() { return station_key_file(data_root()); }

// A received file waits for the user while LanModelDropFiles.hpp's marker is in its folder. Folders
// that still carry it at the next start were never opened: they are removed, and the file arrives
// again from the container if it still holds it. An opened file keeps its folder (the project may
// point at it), and the station never writes into it or removes it again.

AppConfig *config() { return wxGetApp().app_config; }

bool gui_alive() { return g_alive->load() && !wxGetApp().is_closing() && wxGetApp().mainframe != nullptr; }

// Runs `fn` on the GUI thread, only while the main frame is alive and shutdown() has not run.
void post(std::function<void()> fn)
{
    if (wxTheApp == nullptr || !g_alive->load()) return;
    const std::shared_ptr<std::atomic<bool>> alive = g_alive;
    wxTheApp->CallAfter([alive, fn = std::move(fn)]() {
        if (!alive->load() || !gui_alive()) return;
        fn();
    });
}

void notify_listeners()
{
    gui().view.waiting = static_cast<int>(gui().waiting.size());
    // Copy first: a listener may remove itself or another one.
    const auto listeners = gui().listeners;
    for (const auto &entry : listeners)
        if (entry.second) entry.second();
}

void recompute_invite()
{
    View &v = gui().view;
    const auto address = parse_base_address(v.address);
    const std::string preferred = config() ? config()->get(kInviteIpv4ConfigKey) : std::string();
    v.invite = choose_invite_base(v.public_url, address.value_or(BaseAddress{}), gui().lan_candidates, preferred);
    v.link   = v.enabled && !v.drop_code.empty() ? invite_link(v.invite.base, v.drop_code) : std::string();
}

NotificationManager *notifications()
{
    Plater *plater = wxGetApp().plater();
    return plater ? plater->get_notification_manager() : nullptr;
}

// ---------------------------------------------------------------------------------------------
// This computer's IPv4 addresses
// ---------------------------------------------------------------------------------------------

std::vector<AdapterAddress> adapter_addresses()
{
    std::vector<AdapterAddress> out;
#ifdef _WIN32
    ULONG                      size = 16 * 1024;
    std::vector<unsigned char> buffer(size);
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto *adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
        const ULONG result = ::GetAdaptersAddresses(AF_INET,
                                                    GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER |
                                                        GAA_FLAG_INCLUDE_GATEWAYS,
                                                    nullptr, adapters, &size);
        if (result == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(size);
            continue;
        }
        if (result != NO_ERROR) return out;
        for (const IP_ADAPTER_ADDRESSES *adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
            std::string name;
            if (adapter->FriendlyName != nullptr) name = wxString(adapter->FriendlyName).ToUTF8().data();
            if (adapter->Description != nullptr) {
                name += " ";
                name += wxString(adapter->Description).ToUTF8().data();
            }
            for (const IP_ADAPTER_UNICAST_ADDRESS *unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                if (unicast->Address.lpSockaddr == nullptr || unicast->Address.lpSockaddr->sa_family != AF_INET) continue;
                const auto *ipv4  = reinterpret_cast<const sockaddr_in *>(unicast->Address.lpSockaddr);
                const auto *octet = reinterpret_cast<const unsigned char *>(&ipv4->sin_addr);
                AdapterAddress address;
                address.ipv4 = std::to_string(octet[0]) + "." + std::to_string(octet[1]) + "." + std::to_string(octet[2]) + "." +
                               std::to_string(octet[3]);
                address.adapter_name = name;
                address.up           = adapter->OperStatus == IfOperStatusUp;
                address.loopback     = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
                address.has_gateway  = adapter->FirstGatewayAddress != nullptr;
                out.push_back(std::move(address));
            }
        }
        return out;
    }
#endif
    return out;
}

// ---------------------------------------------------------------------------------------------
// Worker: HTTP exchanges
// ---------------------------------------------------------------------------------------------

enum class Method { Get, Post, Delete };

// Cancels a transfer when the application stops or, for polling work, when the inbox it belongs to
// is no longer the current one.
struct CancelWhen
{
    bool          polling  = true;
    std::uint64_t identity = 0;
    std::uint64_t session  = 0;
    bool operator()() const
    {
        if (g_stopping.load()) return true;
        return polling && (!g_enabled.load() || g_identity.load() != identity || g_session.load() != session);
    }
};

Http make_request(Method method, const std::string &url)
{
    switch (method) {
    case Method::Post: return Http::post(url);
    case Method::Delete: return Http::del(url);
    case Method::Get: break;
    }
    return Http::get(url);
}

Exchange exchange(const WorkerConfig &cfg, Method method, const std::string &url, std::size_t size_limit, long timeout_seconds,
                  const CancelWhen &cancel_when, std::string *headers = nullptr)
{
    Exchange result;
    if (url.empty() || !cfg.usable()) return result;
    Http http = make_request(method, url);
    std::string authorization = "Bearer " + cfg.key;
    // Only the fields this protocol defines go to the drop site: no cloud client identity headers.
    http.clear_headers()
        .header("Authorization", authorization)
        .header("Accept", "application/json")
        .timeout_connect(5)
        .timeout_max(timeout_seconds)
        .follow_redirects(false)
        .verbose(false)
        // Straight to the drop site: the key must not pass through a proxy from the environment.
        .no_proxy()
        .size_limit(size_limit)
        .on_progress([cancel_when](Http::Progress, bool &cancel) {
            if (cancel_when()) cancel = true;
        })
        .on_complete([&result](std::string body, unsigned status) {
            result.completed   = true;
            result.http_status = status;
            result.body        = std::move(body);
        })
        .on_error([&result](std::string body, std::string, unsigned status) {
            result.completed   = status != 0;
            result.http_status = status;
            result.body        = std::move(body);
        });
    wipe(authorization);
    if (method == Method::Post) http.header("Content-Type", "application/json").set_post_body(std::string("{}"));
    if (headers != nullptr) http.on_header_callback([headers](std::string all) { *headers = std::move(all); });
    http.perform_sync();
    return result;
}

bool success(const Exchange &e) { return e.completed && e.http_status >= 200 && e.http_status < 300; }

std::string sha256_hex(const std::string &bytes)
{
    unsigned char digest[EVP_MAX_MD_SIZE] = {};
    unsigned int  length = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest, &length, EVP_sha256(), nullptr) != 1 || length != 32) return {};
    return to_hex(digest, length);
}

enum class Refusal { None, Size, Checksum, Type, Listing };

struct Download
{
    enum class Kind { Ok, Refused, Retry, Gone, Unauthorized, Exists } kind = Kind::Retry;
    Refusal               refusal = Refusal::None;
    std::filesystem::path file;
};

long download_timeout(std::uint64_t bytes)
{
    // At least 128 KiB/s, plus room to connect; at most an hour.
    const std::uint64_t seconds = 30 + bytes / (128 * 1024);
    return static_cast<long>(std::min<std::uint64_t>(seconds, 3600));
}

Download download(const WorkerConfig &cfg, const InboxItem &item, const CancelWhen &cancel_when)
{
    Download    result;
    std::string headers;
    const Exchange e = exchange(cfg, Method::Get, file_url(*cfg.address, item.id), static_cast<std::size_t>(item.bytes) + 1,
                                download_timeout(item.bytes), cancel_when, &headers);
    if (!e.completed) return result; // network failure or cancelled: try again later
    if (e.http_status == 404) {
        result.kind = Download::Kind::Gone;
        return result;
    }
    if (e.http_status == 401 || e.http_status == 403) {
        result.kind = Download::Kind::Unauthorized;
        return result;
    }
    if (!success(e)) return result;

    result.kind = Download::Kind::Refused;
    if (e.body.size() != item.bytes) {
        result.refusal = Refusal::Size;
        return result;
    }
    std::string announced = header_value(headers, "X-Content-SHA256");
    std::transform(announced.begin(), announced.end(), announced.begin(),
                   [](char c) { return (c >= 'A' && c <= 'F') ? static_cast<char>(c - 'A' + 'a') : c; });
    if (sha256_hex(e.body) != item.sha256 || (!announced.empty() && announced != item.sha256)) {
        result.refusal = Refusal::Checksum;
        return result;
    }
    const auto by_name = type_from_extension(item.file_name);
    if (!by_name || *by_name != item.type || !content_matches(item.type, e.body)) {
        result.refusal = Refusal::Type;
        return result;
    }
    // Saved under a name of the station's own first, and never into a folder that already exists.
    switch (write_received(cfg.received_root, item.id, item.file_name, item.type, e.body, result.file)) {
    case WriteResult::Written: result.kind = Download::Kind::Ok; break;
    case WriteResult::Exists: result.kind = Download::Kind::Exists; break;
    // A local disk problem is not the sender's fault: keep the file on the drop site.
    case WriteResult::Failed: result.kind = Download::Kind::Retry; break;
    }
    return result;
}

// ---------------------------------------------------------------------------------------------
// Worker: results for the GUI thread (declared here, defined with the GUI functions below)
// ---------------------------------------------------------------------------------------------

void on_link_state(std::uint64_t generation, LinkState state, std::optional<StationStatus> status);
void on_test_result(std::uint64_t generation, LinkState state, std::optional<StationStatus> status);
void on_code_result(std::uint64_t generation, std::optional<std::string> code, bool fixed, LinkState failure);
void on_received(InboxItem item, std::filesystem::path file, std::string base);
void on_refused(InboxItem item, Refusal refusal);
void on_given_up(InboxItem item);

// ---------------------------------------------------------------------------------------------
// Worker: the loop
// ---------------------------------------------------------------------------------------------

LinkState check_station(const WorkerConfig &cfg, const CancelWhen &cancel_when, StationStatus &status)
{
    const Exchange e = exchange(cfg, Method::Get, status_url(*cfg.address), 64 * 1024, 15, cancel_when);
    return classify_status(e, &status);
}

LinkState test_station(const WorkerConfig &cfg, StationStatus &status)
{
    const CancelWhen once{false, 0, 0};
    const Exchange   health = exchange(cfg, Method::Get, health_url(*cfg.address), 16 * 1024, 10, once);
    if (!health.completed) return LinkState::NotReachable;
    if (!success(health) || !parse_health(health.body)) return LinkState::ProtocolNotSupported;
    return check_station(cfg, once, status);
}

void flush_deletes(const WorkerConfig &cfg, InboxTracker &tracker, const CancelWhen &cancel_when)
{
    // Only what this site listed: a delete owed to another address waits until that address is
    // configured again, since this site's 204 for an id it never held would settle nothing.
    const std::string &base = cfg.address->base;
    for (const std::string &id : tracker.pending_deletes(base)) {
        if (cancel_when()) return;
        const Exchange e = exchange(cfg, Method::Delete, file_url(*cfg.address, id), 16 * 1024, 15, cancel_when);
        // 204 also when the file is already gone; 404 means the same thing.
        if (success(e) || e.http_status == 404) tracker.delete_confirmed(id, base);
        else return; // try again on the next round
    }
}

void worker_main()
{
    InboxTracker  tracker;
    std::uint64_t tracked_identity = 0;
    std::uint64_t tracked_session  = 0;
    int           failures         = 0;
    Clock::time_point next_poll    = Clock::now();
    // The deletes the drop sites have not confirmed are kept on disk, so a decision outlives a restart
    // or an address change and the item never comes back as new.
    bool        pending_loaded = false;
    std::string pending_saved;
    const auto  keep_pending = [&tracker, &pending_saved](const WorkerConfig &cfg) {
        if (cfg.pending_file.empty()) return;
        const std::string now = format_pending_deletes(tracker.all_pending_deletes());
        if (now != pending_saved && save_pending_deletes(cfg.pending_file, tracker.all_pending_deletes())) pending_saved = now;
    };

    for (;;) {
        WorkerConfig               cfg;
        bool                       test = false, new_code = false;
        std::vector<PendingDelete> decided;
        {
            std::unique_lock<std::mutex> lock(g_shared.mutex);
            const auto ready = [] {
                return g_shared.stop || g_shared.poll_now || g_shared.test || g_shared.new_code || !g_shared.decided.empty();
            };
            if (g_shared.config.enabled && g_shared.config.usable())
                g_shared.wake.wait_until(lock, next_poll, ready);
            else
                g_shared.wake.wait(lock, ready);
            if (g_shared.stop) break;
            cfg      = g_shared.config;
            test     = std::exchange(g_shared.test, false);
            new_code = std::exchange(g_shared.new_code, false);
            decided.swap(g_shared.decided);
            if (std::exchange(g_shared.poll_now, false)) {
                next_poll = Clock::now();
                failures  = 0;
            }
        }

        // A new address, key or session starts a fresh inbox, keeping the deletes still owed, each to
        // the site that listed the item.
        if (cfg.identity != tracked_identity || cfg.session != tracked_session) {
            const std::vector<PendingDelete> pending = tracker.all_pending_deletes();
            tracker = InboxTracker{};
            for (const PendingDelete &p : pending) tracker.queue_delete(p.id, p.base);
            tracked_identity = cfg.identity;
            tracked_session  = cfg.session;
        }
        if (!pending_loaded && !cfg.pending_file.empty()) {
            pending_loaded                            = true;
            const std::vector<PendingDelete> restored = load_pending_deletes(cfg.pending_file);
            for (const PendingDelete &p : restored) tracker.queue_delete(p.id, p.base);
            pending_saved = format_pending_deletes(restored);
        }
        for (const PendingDelete &d : decided) tracker.queue_delete(d.id, d.base);
        keep_pending(cfg);
        if (!cfg.usable()) {
            // Never leave a requested test or new code without an answer.
            const LinkState missing = cfg.address ? LinkState::NeedsStationKey : LinkState::InvalidAddress;
            if (test) post([generation = cfg.generation, missing] { on_test_result(generation, missing, std::nullopt); });
            if (new_code) post([generation = cfg.generation, missing] { on_code_result(generation, std::nullopt, false, missing); });
            continue;
        }

        const CancelWhen polling{true, cfg.identity, cfg.session};

        if (test) {
            StationStatus status;
            const LinkState state = test_station(cfg, status);
            post([generation = cfg.generation, state, status] {
                on_test_result(generation, state, state == LinkState::Connected ? std::optional<StationStatus>(status) : std::nullopt);
            });
        }
        if (new_code) {
            const CancelWhen once{false, 0, 0};
            const Exchange   e = exchange(cfg, Method::Post, drop_code_url(*cfg.address), 16 * 1024, 15, once);
            std::optional<std::string> code = success(e) ? parse_drop_code(e.body) : std::nullopt;
            const bool      fixed   = e.completed && e.http_status == 409;
            const LinkState failure = code ? LinkState::Connected : (success(e) ? LinkState::ProtocolNotSupported : classify_failure(e));
            post([generation = cfg.generation, code, fixed, failure] { on_code_result(generation, code, fixed, failure); });
        }
        const std::string &base = cfg.address->base;
        if (!cfg.enabled) {
            if (!tracker.pending_deletes(base).empty()) {
                flush_deletes(cfg, tracker, CancelWhen{false, 0, 0});
                keep_pending(cfg);
            }
            continue;
        }

        flush_deletes(cfg, tracker, polling);
        keep_pending(cfg);
        if (Clock::now() < next_poll) continue;

        StationStatus status;
        LinkState     state = check_station(cfg, polling, status);
        InboxListing  listing;
        if (state == LinkState::Connected) {
            const Exchange inbox = exchange(cfg, Method::Get, inbox_url(*cfg.address), 8 * 1024 * 1024, 20, polling);
            if (!success(inbox)) state = classify_failure(inbox);
            else {
                listing = parse_inbox(inbox.body);
                if (!listing.ok) state = LinkState::ProtocolNotSupported;
            }
        }
        if (polling()) continue; // switched off or changed meanwhile: the next round starts afresh
        if (state != LinkState::Connected) {
            ++failures;
            post([generation = cfg.generation, state] { on_link_state(generation, state, std::nullopt); });
            next_poll = Clock::now() + std::chrono::seconds(next_poll_delay_seconds(failures));
            continue;
        }
        failures = 0;
        post([generation = cfg.generation, status] { on_link_state(generation, LinkState::Connected, status); });

        tracker.prune(listing.items);
        // An item already decided whose delete is owed to another address is listed here as well
        // (the same drop site under a second address): this site owes the delete too.
        for (const InboxItem &item : listing.items)
            if (tracker.delete_pending(item.id)) tracker.queue_delete(item.id, base);
        // Entries the container lists with an unusable name, size, digest, stamp or type are removed
        // without being downloaded.
        for (const std::string &id : listing.invalid_ids) {
            const bool known = tracker.taken(id) || tracker.delete_pending(id);
            if (tracker.delete_pending(id)) tracker.queue_delete(id, base);
            if (known) continue;
            tracker.queue_delete(id, base);
            InboxItem unnamed;
            unnamed.id = id;
            post([unnamed] { on_refused(unnamed, Refusal::Listing); });
        }
        for (const InboxItem &item : tracker.fresh(listing.items)) {
            if (polling()) break;
            tracker.take(item.id);
            // Never downloaded over a folder of its own: one that waits is shown already, and one that
            // was opened belongs to the user, so only the drop site's copy has to go.
            const ReceivedState local = received_state(cfg.received_root, item.id);
            if (local == ReceivedState::Waiting) continue;
            if (local == ReceivedState::Opened) {
                tracker.queue_delete(item.id, base);
                continue;
            }
            const Download d = download(cfg, item, polling);
            if (d.kind == Download::Kind::Ok) {
                BOOST_LOG_TRIVIAL(info) << "LAN model drop: received item " << item.id << " (" << item.bytes << " bytes)";
                post([item, file = d.file, base] { on_received(item, file, base); });
            } else if (d.kind == Download::Kind::Exists) {
                if (received_state(cfg.received_root, item.id) == ReceivedState::Opened) tracker.queue_delete(item.id, base);
            } else if (d.kind == Download::Kind::Refused) {
                BOOST_LOG_TRIVIAL(warning) << "LAN model drop: refused item " << item.id << " (check " << static_cast<int>(d.refusal) << ")";
                tracker.queue_delete(item.id, base);
                post([item, refusal = d.refusal] { on_refused(item, refusal); });
            } else if (d.kind == Download::Kind::Unauthorized) {
                tracker.retry_later(item.id);
                post([generation = cfg.generation] { on_link_state(generation, LinkState::WrongStationKey, std::nullopt); });
                break;
            } else if (d.kind == Download::Kind::Retry) {
                if (polling()) {
                    tracker.retry_later(item.id);
                    break;
                }
                if (!tracker.retry_later(item.id)) post([item] { on_given_up(item); });
            }
            // Gone: the sender's file expired or was removed meanwhile; prune forgets it.
        }
        flush_deletes(cfg, tracker, polling);
        keep_pending(cfg);
        next_poll = Clock::now() + std::chrono::seconds(kPollSeconds);
    }
}

void ensure_worker()
{
    if (g_thread.joinable() || g_stopping.load()) return;
    g_thread = std::thread(worker_main);
}

void wake_worker(const std::function<void(Shared &)> &change)
{
    {
        std::lock_guard<std::mutex> lock(g_shared.mutex);
        change(g_shared);
    }
    g_shared.wake.notify_all();
}

// ---------------------------------------------------------------------------------------------
// GUI thread: results
// ---------------------------------------------------------------------------------------------

void apply_status(View &v, const StationStatus &status)
{
    v.station_name = status.station_name;
    v.drop_code    = status.drop_code;
    v.public_url   = status.public_url;
    v.queued       = status.queued;
}

// What a poll can change in the view.
bool same_poll_view(const View &a, const View &b)
{
    return a.state == b.state && a.station_name == b.station_name && a.drop_code == b.drop_code && a.public_url == b.public_url &&
           a.queued == b.queued && a.link == b.link && a.invite.source == b.invite.source && a.invite.base == b.invite.base &&
           a.invite.chosen == b.invite.chosen && a.invite.choices == b.invite.choices;
}

void on_link_state(std::uint64_t generation, LinkState state, std::optional<StationStatus> status)
{
    if (generation != gui().generation || !gui().view.enabled) return;
    View &v = gui().view;
    const View before = v;
    v.state = state;
    if (status) apply_status(v, *status);
    recompute_invite();
    // Every poll reports, every 5 seconds; the windows hear of it only when what they show changed,
    // so an unchanged poll sets no text and lays nothing out again.
    if (!same_poll_view(before, v)) notify_listeners();
}

void on_test_result(std::uint64_t generation, LinkState state, std::optional<StationStatus> status)
{
    if (generation != gui().generation) return;
    View &v      = gui().view;
    v.busy_test  = false;
    v.tested     = true;
    v.test_state = state;
    if (status) apply_status(v, *status);
    if (v.enabled && (state == LinkState::Connected || is_error_state(state))) v.state = state;
    recompute_invite();
    notify_listeners();
}

void on_code_result(std::uint64_t generation, std::optional<std::string> code, bool fixed, LinkState failure)
{
    if (generation != gui().generation) return;
    View &v       = gui().view;
    v.busy_code   = false;
    v.fixed_code  = fixed;
    v.code_failed = !code && !fixed;
    if (code) v.drop_code = *code;
    if (!code && !fixed && v.enabled && is_error_state(failure)) v.state = failure;
    recompute_invite();
    notify_listeners();
}

std::string notification_text(const Waiting &w)
{
    const std::string size = format_size(w.bytes);
    return w.sender.empty() ? format(_u8L("Someone sent %1% (%2%)"), w.file_name, size)
                            : format(_u8L("%1% sent %2% (%3%)"), w.sender, w.file_name, size);
}

Waiting *find_waiting(const std::string &id)
{
    auto &list = gui().waiting;
    const auto it = std::find_if(list.begin(), list.end(), [&id](const Waiting &w) { return w.id == id; });
    return it == list.end() ? nullptr : &*it;
}

// The user opened or discarded the item: the site that listed it is asked to delete its copy, and
// the item is never offered again in this session.
void decide(const std::string &id, const std::string &base)
{
    gui().decided.insert(id);
    ensure_worker();
    wake_worker([&id, &base](Shared &s) { s.decided.push_back({id, base}); });
}

void open_item(const std::string &id);
void discard_item(const std::string &id);

void push_waiting_notification(const Waiting &w)
{
    NotificationManager *manager = notifications();
    if (manager == nullptr) return;
    const std::string id = w.id;
    manager->push_lan_model_drop_notification(
        w.text,
        _u8L("Open"),
        [id](wxEvtHandler *) {
            // The link is clicked while the canvas renders: load on the next turn of the event loop.
            wxGetApp().CallAfter([id] { open_item(id); });
            return true;
        },
        _u8L("Discard"),
        [id](wxEvtHandler *) {
            wxGetApp().CallAfter([id] { discard_item(id); });
            return true;
        });
}

void on_received(InboxItem item, std::filesystem::path file, std::string base)
{
    if (gui().decided.count(item.id) != 0) {
        // Decided while this copy was on its way (a poll that began before the decision): it goes,
        // and the delete is owed to this site as well.
        remove_waiting(received_root(), item.id);
        decide(item.id, base);
        return;
    }
    if (!gui().view.enabled) {
        // Switched off while the file was on its way: it stays on the drop site for next time.
        remove_waiting(received_root(), item.id);
        return;
    }
    if (find_waiting(item.id) != nullptr) return;
    Waiting w;
    w.id        = item.id;
    w.file      = std::move(file);
    w.file_name = item.file_name;
    w.sender    = item.sender;
    w.bytes     = item.bytes;
    w.base      = std::move(base);
    std::vector<std::string> texts;
    for (const Waiting &other : gui().waiting) texts.push_back(other.text);
    w.text = distinct_text(notification_text(w), texts);
    gui().waiting.push_back(w);
    push_waiting_notification(w);
    notify_listeners();
}

void on_refused(InboxItem item, Refusal refusal)
{
    // Nothing of a refused file was saved, so there is nothing to remove here; a copy received
    // earlier under the same id stays with the user.
    NotificationManager *manager = notifications();
    if (manager == nullptr) return;
    std::string reason;
    switch (refusal) {
    case Refusal::Size: reason = _u8L("its size does not match what the drop site announced"); break;
    case Refusal::Checksum: reason = _u8L("its SHA-256 checksum does not match"); break;
    case Refusal::Type: reason = _u8L("it is not a 3MF, STL, STEP, OBJ or AMF model"); break;
    case Refusal::Listing:
    case Refusal::None: reason = _u8L("the drop site described it with details that are not valid"); break;
    }
    const std::string name = item.file_name.empty() ? _u8L("A file") : item.file_name;
    manager->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel,
                               format(_u8L("Bambu Studio refused %1% from the LAN drop site because %2%. It was removed from the drop site."),
                                      name, reason));
}

void on_given_up(InboxItem item)
{
    NotificationManager *manager = notifications();
    if (manager == nullptr) return;
    manager->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel,
                               format(_u8L("%1% could not be received from the LAN drop site. It stays there until it expires; "
                                           "switch LAN model drop off and on again to retry."),
                                      item.file_name));
}

void forget_waiting(const std::string &id)
{
    auto &list = gui().waiting;
    const auto it = std::find_if(list.begin(), list.end(), [&id](const Waiting &w) { return w.id == id; });
    if (it == list.end()) return;
    if (NotificationManager *manager = notifications()) manager->close_lan_model_drop_notification(it->text);
    list.erase(it);
}

void open_item(const std::string &id)
{
    // The link's CallAfter may run while the application closes: nothing is loaded then.
    if (!gui_alive()) return;
    Waiting *w = find_waiting(id);
    if (w == nullptr) return;
    const std::filesystem::path file = w->file;
    const std::string           base = w->base;
    forget_waiting(id);
    // From here on the folder is the user's: the station never writes into it or removes it again.
    mark_opened(file);
    decide(id, base);
    notify_listeners();
    std::error_code ec;
    MainFrame *frame  = wxGetApp().mainframe;
    Plater    *plater = wxGetApp().plater();
    if (frame == nullptr || plater == nullptr || !std::filesystem::is_regular_file(file, ec)) return;
    frame->Raise();
    if (wxGetApp().is_editor()) frame->select_tab(size_t(MainFrame::tp3DEditor));
    // The normal load path, as for a file dropped on the window: the unsaved-project rules apply.
    wxArrayString files;
    files.Add(wxString::FromUTF8(file.u8string()));
    plater->load_files(files);
    frame->update_title();
}

void discard_item(const std::string &id)
{
    if (!gui_alive() || find_waiting(id) == nullptr) return;
    const std::string base = find_waiting(id)->base;
    forget_waiting(id);
    remove_waiting(received_root(), id);
    decide(id, base);
    notify_listeners();
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Public, GUI thread
// ---------------------------------------------------------------------------------------------

const View &view() { return gui().view; }

int add_listener(std::function<void()> listener)
{
    const int id = gui().next_listener++;
    gui().listeners[id] = std::move(listener);
    return id;
}

void remove_listener(int id) { gui().listeners.erase(id); }

void refresh_lan_addresses()
{
    gui().lan_candidates = lan_ipv4_candidates(adapter_addresses());
    gui().lan_read       = true;
    recompute_invite();
    notify_listeners();
}

void choose_invite_ipv4(const std::string &ipv4)
{
    if (config() == nullptr || !is_private_ipv4(ipv4)) return;
    config()->set(kInviteIpv4ConfigKey, ipv4);
    config()->save();
    recompute_invite();
    notify_listeners();
}

void apply_settings()
{
    AppConfig *cfg = config();
    if (cfg == nullptr || g_stopping.load()) return;
    const bool  enabled      = cfg->get(kEnabledConfigKey) == "true";
    std::string address_text = cfg->get(kAddressConfigKey);
    if (address_text.empty()) address_text = kDefaultAddress;
    const auto address = parse_base_address(address_text);

    std::string          key;
    const KeyStoreResult stored = load_station_key(key_file(), key);

    View &v           = gui().view;
    const bool was_on = v.enabled;
    v.enabled         = enabled;
    v.address         = address ? address->base : address_text;
    v.address_valid   = address.has_value();
    v.has_key         = stored == KeyStoreResult::Ok;
    v.tested          = false;
    v.busy_test       = false;
    v.busy_code       = false;
    if (!enabled) v.state = LinkState::Off;
    else if (!address) v.state = LinkState::InvalidAddress;
    else if (stored == KeyStoreResult::Unavailable) v.state = LinkState::KeyStorageUnavailable;
    else if (stored != KeyStoreResult::Ok) v.state = LinkState::NeedsStationKey;
    else v.state = LinkState::Checking;

    std::uint64_t generation = 0;
    bool          identity_changed = false;
    wake_worker([&](Shared &s) {
        WorkerConfig &c = s.config;
        identity_changed = c.address.has_value() != address.has_value() || (address && c.address->base != address->base) || c.key != key;
        if (identity_changed) ++c.identity;
        if (enabled && !was_on) ++c.session;
        ++c.generation;
        c.enabled = enabled;
        c.address = address;
        wipe(c.key);
        c.key           = key;
        c.received_root = received_root();
        c.pending_file  = pending_file();
        s.poll_now      = true;
        generation      = c.generation;
        g_identity.store(c.identity);
        g_session.store(c.session);
    });
    wipe(key);
    g_enabled.store(enabled);
    gui().generation = generation;
    if (identity_changed || !enabled) {
        v.station_name.clear();
        v.drop_code.clear();
        v.public_url.clear();
        v.queued     = 0;
        v.fixed_code = false;
    }
    if (enabled && !gui().lan_read) {
        gui().lan_candidates = lan_ipv4_candidates(adapter_addresses());
        gui().lan_read       = true;
    }
    if (enabled) ensure_worker();
    recompute_invite();
    notify_listeners();
}

void start_after_startup()
{
    clean_unopened(received_root());
    apply_settings();
}

void shutdown()
{
    g_alive->store(false);
    g_stopping.store(true);
    {
        std::lock_guard<std::mutex> lock(g_shared.mutex);
        g_shared.stop = true;
        wipe(g_shared.config.key);
    }
    g_shared.wake.notify_all();
    if (g_thread.joinable()) g_thread.join();
    gui().listeners.clear();
}

void after_gui_rebuild()
{
    wxGetApp().CallAfter([] {
        if (!gui_alive()) return;
        for (const Waiting &w : gui().waiting) push_waiting_notification(w);
        notify_listeners();
    });
}

void test_connection()
{
    if (g_stopping.load()) return;
    View &v = gui().view;
    v.tested = false;
    std::string    key;
    const KeyStoreResult stored = load_station_key(key_file(), key);
    wipe(key);
    if (!v.address_valid) {
        v.tested     = true;
        v.test_state = LinkState::InvalidAddress;
    } else if (stored == KeyStoreResult::Unavailable) {
        v.tested     = true;
        v.test_state = LinkState::KeyStorageUnavailable;
    } else if (stored != KeyStoreResult::Ok) {
        v.tested     = true;
        v.test_state = LinkState::NeedsStationKey;
    } else {
        v.busy_test = true;
        ensure_worker();
        wake_worker([](Shared &s) { s.test = true; });
    }
    notify_listeners();
}

void request_new_code()
{
    View &v = gui().view;
    if (g_stopping.load() || v.busy_code || !v.has_key || !v.address_valid) return;
    v.busy_code   = true;
    v.code_failed = false;
    ensure_worker();
    wake_worker([](Shared &s) { s.new_code = true; });
    notify_listeners();
}

void show_waiting_again()
{
    for (const Waiting &w : gui().waiting) push_waiting_notification(w);
}

KeyStoreResult store_station_key(const std::string &key)
{
    const KeyStoreResult result = key.empty() ? forget_station_key(key_file()) : save_station_key(key_file(), key);
    if (result == KeyStoreResult::Ok || result == KeyStoreResult::Missing) apply_settings();
    return result;
}

std::string stored_station_key()
{
    std::string key;
    load_station_key(key_file(), key);
    return key;
}

void open_preferences_section() { wxGetApp().open_preferences(kEnabledConfigKey); }

wxString state_text(LinkState state)
{
    switch (state) {
    case LinkState::Off: return _L("Off");
    case LinkState::Checking: return _L("Connecting");
    case LinkState::Connected: return _L("Connected");
    case LinkState::NotReachable: return _L("Not reachable");
    case LinkState::WrongStationKey: return _L("Wrong station key");
    case LinkState::ProtocolNotSupported: return _L("Protocol not supported");
    case LinkState::NeedsStationKey: return _L("Station key needed");
    case LinkState::InvalidAddress: return _L("Address not valid");
    case LinkState::KeyStorageUnavailable: return _L("Protected key storage unavailable");
    }
    return _L("Not reachable");
}

wxString status_text(const View &v)
{
    wxString text = format_wxstr(_L("Status: %1%"), state_text(v.state));
    if (v.state == LinkState::Connected && !v.station_name.empty()) text += " - " + wxString::FromUTF8(v.station_name);
    if (v.waiting > 0) text += " - " + format_wxstr(_L("%1% received files waiting"), v.waiting);
    return text;
}

wxString invite_source_text(const View &v)
{
    switch (v.invite.source) {
    case InviteSource::PublicUrl: return _L("The link uses the public address the drop site announces.");
    case InviteSource::Configured: return _L("The link uses the drop site address from the LAN model drop settings.");
    case InviteSource::LanAddress: return _L("The link uses this computer's address on your local network.");
    case InviteSource::None: break;
    }
    return _L("This computer has no private local network address to put in a link.");
}

}}} // namespace Slic3r::GUI::LanModelDrop
