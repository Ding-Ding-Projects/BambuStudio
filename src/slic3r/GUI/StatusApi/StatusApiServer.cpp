// Local, read-only printer status API: the loopback listener and the GUI-thread snapshot.
// See StatusApiServer.hpp, StatusApiCore.hpp and docs/features/device/status-api.md.

#include "StatusApiServer.hpp"
#include "StatusApiCore.hpp"

#include "../DeviceManager.hpp"
#include "../DeviceCore/DevManager.h"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "libslic3r/AppConfig.hpp"

#include <openssl/rand.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/log/trivial.hpp>

#include <wx/timer.h>

#include <atomic>
#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Slic3r { namespace GUI { namespace StatusApi {

namespace {

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = boost::beast::http;
using tcp       = asio::ip::tcp;

// What the server thread reads. Written on the GUI thread only, under the mutex.
struct Shared
{
    std::mutex                   mutex;
    Settings                     settings;
    std::vector<PrinterSnapshot> printers;
};

Shared &shared()
{
    static Shared s;
    return s;
}

// One accepted connection: read one request with a header limit and no body, answer, close.
class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(tcp::socket socket) : m_socket(std::move(socket)), m_timer(m_socket.get_executor()) {}

    void start()
    {
        m_parser.header_limit(static_cast<std::uint32_t>(kMaxRequestHeader));
        m_parser.body_limit(std::uint64_t(0));
        m_timer.expires_after(std::chrono::seconds(kReadTimeoutSeconds));
        auto self = shared_from_this();
        m_timer.async_wait([self](const beast::error_code &ec) {
            if (!ec) {
                beast::error_code ignored;
                self->m_socket.close(ignored);
            }
        });
        http::async_read(m_socket, m_buffer, m_parser, [self](const beast::error_code &ec, std::size_t) {
            self->m_timer.cancel();
            if (ec) {
                // A malformed, oversized or truncated request gets a plain 400 when it can still be answered.
                if (ec != http::error::end_of_stream && ec != asio::error::operation_aborted) self->reply_bad_request();
                return;
            }
            self->answer();
        });
    }

private:
    void answer()
    {
        const auto &req = m_parser.get();
        Request     request;
        request.method        = std::string(req.method_string());
        request.target        = std::string(req.target());
        request.authorization = std::string(req[http::field::authorization]);
        request.host          = std::string(req[http::field::host]);
        beast::error_code ec;
        const auto        peer = m_socket.remote_endpoint(ec);
        request.peer_is_loopback = !ec && peer.address().is_loopback();

        Settings                     settings;
        std::vector<PrinterSnapshot> printers;
        {
            std::lock_guard<std::mutex> lock(shared().mutex);
            settings = shared().settings;
            printers = shared().printers;
        }
        send(handle_request(settings, printers, request));
    }

    void reply_bad_request()
    {
        Response r;
        r.status = 400;
        r.body   = error_json("bad_request", "The request could not be read.");
        send(r);
    }

    void send(const Response &r)
    {
        m_response = std::make_shared<http::response<http::string_body>>(static_cast<http::status>(r.status), 11);
        m_response->set(http::field::server, "BambuStudio-StatusApi");
        m_response->set(http::field::content_type, "application/json; charset=utf-8");
        m_response->set(http::field::cache_control, "no-store");
        m_response->set(http::field::x_content_type_options, "nosniff");
        if (!r.allow.empty()) m_response->set(http::field::allow, r.allow);
        if (r.www_authenticate) m_response->set(http::field::www_authenticate, "Bearer realm=\"bambu-studio-status\"");
        m_response->body() = r.body;
        m_response->keep_alive(false);
        m_response->prepare_payload();
        auto self = shared_from_this();
        http::async_write(m_socket, *m_response, [self](const beast::error_code &, std::size_t) {
            beast::error_code ignored;
            self->m_socket.shutdown(tcp::socket::shutdown_both, ignored);
            self->m_socket.close(ignored);
        });
    }

    tcp::socket                                        m_socket;
    asio::steady_timer                                 m_timer;
    beast::flat_buffer                                 m_buffer;
    http::request_parser<http::empty_body>             m_parser;
    std::shared_ptr<http::response<http::string_body>> m_response;
};

// The listener: one io thread, bound to the IPv4 loopback address only.
class Listener
{
public:
    Listener() : m_acceptor(m_io) {}
    ~Listener() { stop(); }

    bool start(int port)
    {
        stop();
        beast::error_code   ec;
        const tcp::endpoint endpoint(asio::ip::make_address_v4(kBindAddress), static_cast<unsigned short>(port));
        m_acceptor.open(endpoint.protocol(), ec);
        if (ec) return false;
        // The port is not shared: another instance, or another program, on the port makes the bind fail.
        m_acceptor.bind(endpoint, ec);
        if (ec) { m_acceptor.close(ec); return false; }
        m_acceptor.listen(16, ec);
        if (ec) { m_acceptor.close(ec); return false; }
        m_running = true;
        accept();
        m_thread = std::thread([this] { m_io.run(); });
        return true;
    }

    void stop()
    {
        if (!m_running.exchange(false)) return;
        beast::error_code ec;
        m_acceptor.close(ec);
        m_io.stop();
        if (m_thread.joinable()) m_thread.join();
        m_io.restart();
    }

private:
    void accept()
    {
        m_acceptor.async_accept([this](const beast::error_code &ec, tcp::socket socket) {
            if (!ec && m_running) {
                beast::error_code peer_ec;
                const auto        peer = socket.remote_endpoint(peer_ec);
                // Defence in depth: the bind is loopback only, and a peer that is not loopback is dropped here.
                if (!peer_ec && peer.address().is_loopback()) std::make_shared<Session>(std::move(socket))->start();
            }
            if (m_running) accept();
        });
    }

    asio::io_context  m_io{1};
    tcp::acceptor     m_acceptor;
    std::thread       m_thread;
    std::atomic<bool> m_running{false};
};

Listener &listener()
{
    static Listener l;
    return l;
}

State g_state = State::Off;
int   g_port  = kDefaultPort;

// ---------------------------------------------------------------------------------
// GUI-thread snapshot
// ---------------------------------------------------------------------------------

std::vector<PrinterSnapshot> take_snapshot()
{
    std::vector<PrinterSnapshot> result;
    DeviceManager               *manager = wxGetApp().getDeviceManager();
    if (manager == nullptr) return result;
    for (const auto &entry : manager->get_my_machine_list()) {
        MachineObject *obj = entry.second;
        if (obj == nullptr) continue;
        PrinterSnapshot s;
        s.dev_id         = obj->get_dev_id();
        s.name           = obj->get_dev_name();
        s.model          = obj->get_show_printer_type();
        s.connected      = obj->is_connected();
        s.printing       = obj->is_in_printing();
        s.door_supported = obj->support_door_open_check();
        s.door_bit       = obj->door_open_bit;
        result.push_back(std::move(s));
    }
    return result;
}

class SnapshotTimer : public wxTimer
{
public:
    void Notify() override { refresh(); }

    static void refresh()
    {
        std::vector<PrinterSnapshot> next = take_snapshot();
        std::lock_guard<std::mutex>  lock(shared().mutex);
        carry_updated_at(shared().printers, next, iso8601_utc(std::time(nullptr)));
        shared().printers = std::move(next);
    }
};

SnapshotTimer &timer()
{
    static SnapshotTimer t;
    return t;
}

Settings read_settings()
{
    Settings   s;
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr) return s;
    s.enabled = cfg->get(kEnabledConfigKey) == "true";
    s.port    = parse_port(cfg->get(kPortConfigKey));
    s.token   = cfg->get(kTokenConfigKey);
    return s;
}

std::string make_token()
{
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) return {};
    return hex_encode(bytes, sizeof(bytes));
}

} // namespace

std::string regenerate_token()
{
    const std::string token = make_token();
    if (token.empty()) return {};
    if (AppConfig *cfg = wxGetApp().app_config) {
        cfg->set(kTokenConfigKey, token);
        cfg->save();
    }
    apply_settings();
    return token;
}

std::string ensure_token()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr) return {};
    std::string token = cfg->get(kTokenConfigKey);
    if (token_usable(token)) return token;
    return regenerate_token();
}

void apply_settings()
{
    Settings settings = read_settings();
    // Switching the API on for the first time makes its token.
    if (settings.enabled && !token_usable(settings.token)) {
        settings.token = make_token();
        if (!settings.token.empty()) {
            if (AppConfig *cfg = wxGetApp().app_config) {
                cfg->set(kTokenConfigKey, settings.token);
                cfg->save();
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(shared().mutex);
        shared().settings = settings;
    }

    const bool was_listening = g_state == State::Listening;
    if (!settings.enabled) {
        listener().stop();
        timer().Stop();
        {
            std::lock_guard<std::mutex> lock(shared().mutex);
            shared().printers.clear();
        }
        g_state = State::Off;
        g_port  = settings.port;
        return;
    }
    if (!token_usable(settings.token)) {
        listener().stop();
        timer().Stop();
        g_state = State::NoToken;
        return;
    }
    // A token change needs no restart (it is read per request); a port change does.
    if (was_listening && g_port == settings.port) return;

    g_port = settings.port;
    if (listener().start(settings.port)) {
        g_state = State::Listening;
        SnapshotTimer::refresh();
        timer().Start(1000);
        BOOST_LOG_TRIVIAL(info) << "[StatusApi] listening on " << kBindAddress << ":" << settings.port;
    } else {
        timer().Stop();
        g_state = State::PortInUse;
        BOOST_LOG_TRIVIAL(warning) << "[StatusApi] could not listen on " << kBindAddress << ":" << settings.port;
    }
}

void start_after_startup() { apply_settings(); }

void shutdown()
{
    timer().Stop();
    listener().stop();
    g_state = State::Off;
}

State current_state() { return g_state; }
int   current_port() { return g_port; }

wxString state_text(State state)
{
    switch (state) {
    case State::Listening: return _L("Listening on this computer only");
    case State::NoToken: return _L("No token is available");
    case State::PortInUse: return _L("The port is already in use");
    default: return _L("Off");
    }
}

}}} // namespace Slic3r::GUI::StatusApi
