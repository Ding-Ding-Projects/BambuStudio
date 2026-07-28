#include "HomeAssistantSharingService.hpp"

#include "nlohmann/json.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <iphlpapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#endif
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = asio::ip::tcp;
using udp       = asio::ip::udp;

namespace Slic3r { namespace GUI { namespace HomeAssistant {

namespace SharingNetworkPolicy {

bool mdns_sender_is_eligible_ipv4(
    std::uint32_t advertised_address,
    std::uint32_t sender_address,
    std::uint8_t on_link_prefix_length) noexcept
{
    if (on_link_prefix_length == 0 || on_link_prefix_length > 32)
        return false;

    const std::uint8_t first = static_cast<std::uint8_t>(sender_address >> 24);
    const std::uint8_t second =
        static_cast<std::uint8_t>((sender_address >> 16) & 0xff);
    const bool private_or_shared =
        first == 10 ||
        (first == 100 && second >= 64 && second <= 127) ||
        (first == 172 && second >= 16 && second <= 31) ||
        (first == 192 && second == 168);
    if (!private_or_shared)
        return false;

    const std::uint32_t mask =
        on_link_prefix_length == 32
            ? 0xffffffffu
            : 0xffffffffu << (32 - on_link_prefix_length);
    const std::uint32_t network = advertised_address & mask;
    if ((sender_address & mask) != network)
        return false;

    if (on_link_prefix_length <= 30) {
        const std::uint32_t broadcast = network | ~mask;
        if (sender_address == network || sender_address == broadcast)
            return false;
    }
    return true;
}

} // namespace SharingNetworkPolicy

namespace {

constexpr char        kHttpPath[]            = "/bambustudio/printers";
constexpr char        kServiceType[]         = "_bambu-slicer._tcp.local";
constexpr char        kMulticastAddress[]    = "224.0.0.251";
constexpr std::uint16_t kMdnsPort             = 5353;
constexpr std::size_t  kMaxHeaderBytes        = 8 * 1024;
constexpr std::size_t  kMaxTargetBytes        = 256;
constexpr std::size_t  kMaxResponseBytes      = SharingService::max_offer_response_bytes();
constexpr std::size_t  kMaxPrinterCount       = SharingService::max_printer_count();
constexpr std::size_t  kMaxPrinterFieldBytes  = 256;
constexpr std::size_t  kMaxConcurrentSessions = 16;
constexpr std::size_t  kMaxMdnsPacketBytes    = 1500;
constexpr std::size_t  kMaxDnsQuestions       = 16;
constexpr std::uint32_t kMdnsTtlSeconds        = 120;
constexpr std::size_t  kMaxPendingMdnsResponses =
    SharingService::max_pending_mdns_responses();
constexpr std::size_t  kOfferResponseBurst = SharingService::offer_response_burst();
constexpr std::size_t  kMdnsQueryBurst = SharingService::mdns_query_burst();
constexpr auto         kMdnsResponseInterval =
    std::chrono::milliseconds(SharingService::mdns_response_interval_ms());
constexpr auto         kMdnsQueryRefill =
    std::chrono::milliseconds(SharingService::mdns_query_refill_ms());
constexpr auto         kOfferResponseRefill =
    std::chrono::milliseconds(SharingService::offer_response_refill_ms());
constexpr auto         kRequestTimeout         = std::chrono::seconds(5);

struct ShareConfig
{
    std::string                         pairing_token;
    std::string                         display_name;
    std::string                         instance_name;
    std::string                         hostname;
    asio::ip::address_v4                address;
    std::uint8_t                        on_link_prefix_length = 0;
    std::uint16_t                       port = 0;
    SharingService::PrinterJsonSupplier supplier;
    std::optional<std::string>          cached_printer_payload;
    std::size_t                         offer_response_tokens = kOfferResponseBurst;
    std::chrono::steady_clock::time_point offer_response_last_refill =
        std::chrono::steady_clock::now();
    std::size_t                         mdns_query_tokens = kMdnsQueryBurst;
    std::chrono::steady_clock::time_point mdns_query_last_refill =
        std::chrono::steady_clock::now();
};

bool is_url_safe_token(const std::string &value)
{
    if (value.size() < 32 || value.size() > 128)
        return false;

    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~';
    });
}

bool is_safe_display_name(const std::string &value)
{
    if (value.empty() || value.size() > 128)
        return false;

    return std::none_of(value.begin(), value.end(), [](unsigned char c) {
        return c == 0 || c < 0x20 || c == 0x7f;
    });
}

std::string uuid_hex()
{
    std::string value = boost::uuids::to_string(boost::uuids::random_generator()());
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    return value;
}

std::string make_instance_label(const std::string &display_name, const std::string &suffix)
{
    std::string label;
    label.reserve(63);

    bool previous_dash = false;
    for (unsigned char c : display_name) {
        if (label.size() >= 48)
            break;

        const bool allowed = std::isalnum(c) != 0 || c == '-';
        const char out = allowed ? static_cast<char>(c) : '-';
        if (out == '-' && previous_dash)
            continue;
        label.push_back(out);
        previous_dash = out == '-';
    }

    while (!label.empty() && label.back() == '-')
        label.pop_back();
    if (label.empty())
        label = "Bambu-Studio";

    label += '-';
    label += suffix.substr(0, 8);
    return label;
}

bool valid_advertised_address(const asio::ip::address_v4 &address)
{
    if (address.is_unspecified() || address.is_loopback() || address.is_multicast())
        return false;

    // Match the integration's SSRF boundary exactly: advertise only RFC1918 or
    // shared-address-space interfaces. Public, link-local, and other special
    // addresses would either expose the endpoint beyond the intended LAN or be
    // rejected by Home Assistant after discovery.
    const auto bytes = address.to_bytes();
    return bytes[0] == 10 ||
           (bytes[0] == 100 && bytes[1] >= 64 && bytes[1] <= 127) ||
           (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) ||
           (bytes[0] == 192 && bytes[1] == 168);
}

std::uint32_t ipv4_address_value(const asio::ip::address_v4 &address)
{
    const auto bytes = address.to_bytes();
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

#ifndef _WIN32
std::optional<std::uint8_t> prefix_length_from_netmask(std::uint32_t mask)
{
    std::uint8_t prefix_length = 0;
    bool         saw_zero = false;
    for (int bit = 31; bit >= 0; --bit) {
        const bool set = (mask & (std::uint32_t{1} << bit)) != 0;
        if (set) {
            if (saw_zero)
                return std::nullopt;
            ++prefix_length;
        } else {
            saw_zero = true;
        }
    }
    return prefix_length;
}
#endif

std::optional<std::uint8_t>
interface_prefix_length(const asio::ip::address_v4 &address)
{
    const std::uint32_t wanted = ipv4_address_value(address);
#ifdef _WIN32
    ULONG buffer_size = 15 * 1024;
    std::vector<unsigned char> buffer(buffer_size);
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto *adapters =
            reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
        const ULONG result = GetAdaptersAddresses(
            AF_INET,
            GAA_FLAG_SKIP_ANYCAST |
                GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_DNS_SERVER,
            nullptr,
            adapters,
            &buffer_size);
        if (result == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(buffer_size);
            continue;
        }
        if (result != NO_ERROR)
            return std::nullopt;

        for (const IP_ADAPTER_ADDRESSES *adapter = adapters;
             adapter != nullptr;
             adapter = adapter->Next) {
            for (const IP_ADAPTER_UNICAST_ADDRESS *unicast =
                     adapter->FirstUnicastAddress;
                 unicast != nullptr;
                 unicast = unicast->Next) {
                if (unicast->Address.lpSockaddr == nullptr ||
                    unicast->Address.lpSockaddr->sa_family != AF_INET)
                    continue;
                const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(
                    unicast->Address.lpSockaddr);
                if (ntohl(ipv4->sin_addr.s_addr) == wanted &&
                    unicast->OnLinkPrefixLength <= 32)
                    return static_cast<std::uint8_t>(
                        unicast->OnLinkPrefixLength);
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
#else
    ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0)
        return std::nullopt;

    std::optional<std::uint8_t> result;
    for (const ifaddrs *interface = interfaces;
         interface != nullptr;
         interface = interface->ifa_next) {
        if (interface->ifa_addr == nullptr ||
            interface->ifa_netmask == nullptr ||
            interface->ifa_addr->sa_family != AF_INET ||
            interface->ifa_netmask->sa_family != AF_INET)
            continue;

        const auto *ipv4 =
            reinterpret_cast<const sockaddr_in *>(interface->ifa_addr);
        if (ntohl(ipv4->sin_addr.s_addr) != wanted)
            continue;

        const auto *netmask =
            reinterpret_cast<const sockaddr_in *>(interface->ifa_netmask);
        result = prefix_length_from_netmask(
            ntohl(netmask->sin_addr.s_addr));
        if (result)
            break;
    }
    freeifaddrs(interfaces);
    return result;
#endif
}

bool valid_mdns_sender(
    const udp::endpoint &sender,
    const ShareConfig &config)
{
    if (sender.port() == 0 || !sender.address().is_v4())
        return false;
    const asio::ip::address_v4 sender_address = sender.address().to_v4();
    return valid_advertised_address(sender_address) &&
           SharingNetworkPolicy::mdns_sender_is_eligible_ipv4(
               ipv4_address_value(config.address),
               ipv4_address_value(sender_address),
               config.on_link_prefix_length);
}

std::optional<asio::ip::address_v4> detect_lan_address()
{
    boost::system::error_code ec;
    asio::io_context          probe_io;
    udp::socket               probe(probe_io);
    probe.open(udp::v4(), ec);
    if (!ec) {
        // A multicast route may prefer a host-only Hyper-V/WSL adapter even
        // when the physical LAN owns the default route. UDP connect performs
        // no network transfer here; TEST-NET-1 only asks the kernel which
        // source address it would use for ordinary LAN/Internet traffic.
        probe.connect(
            udp::endpoint(asio::ip::make_address_v4("192.0.2.1"), 9),
            ec);
        if (!ec) {
            const auto endpoint = probe.local_endpoint(ec);
            if (!ec && endpoint.address().is_v4()) {
                const auto candidate = endpoint.address().to_v4();
                if (valid_advertised_address(candidate))
                    return candidate;
            }
        }
    }

    // Route probing is normally sufficient. Hostname resolution is a
    // conservative fallback for machines whose multicast route is not ready
    // until after an interface comes up.
    try {
        asio::io_context resolver_io;
        tcp::resolver    resolver(resolver_io);
        const auto       results = resolver.resolve(asio::ip::host_name(), "0");
        for (const auto &entry : results) {
            if (entry.endpoint().address().is_v4()) {
                const auto candidate = entry.endpoint().address().to_v4();
                if (valid_advertised_address(candidate))
                    return candidate;
            }
        }
    } catch (...) {
        // The caller receives a non-sensitive, deterministic error below.
    }

    return std::nullopt;
}

bool constant_time_equal(const std::string &lhs, const std::string &rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    unsigned char difference = 0;
    for (std::size_t index = 0; index < lhs.size(); ++index)
        difference |= static_cast<unsigned char>(lhs[index]) ^ static_cast<unsigned char>(rhs[index]);
    return difference == 0;
}

bool ascii_starts_with_case_insensitive(const std::string &value, const char *prefix)
{
    const std::size_t length = std::char_traits<char>::length(prefix);
    if (value.size() < length)
        return false;

    for (std::size_t index = 0; index < length; ++index) {
        const auto lhs = static_cast<unsigned char>(value[index]);
        const auto rhs = static_cast<unsigned char>(prefix[index]);
        if (std::tolower(lhs) != std::tolower(rhs))
            return false;
    }
    return true;
}

bool authorized(const std::string &authorization, const std::string &pairing_token)
{
    constexpr char scheme[] = "Bearer ";
    constexpr auto scheme_length = sizeof(scheme) - 1;
    if (!ascii_starts_with_case_insensitive(authorization, scheme))
        return false;
    if (authorization.size() != scheme_length + pairing_token.size())
        return false;

    return constant_time_equal(authorization.substr(scheme_length), pairing_token);
}

bool copy_printer_field(const nlohmann::json &source, const char *key, bool required, nlohmann::json &target)
{
    const auto found = source.find(key);
    if (found == source.end()) {
        return !required;
    }
    if (!found->is_string())
        return false;

    const auto &value = found->get_ref<const std::string &>();
    if ((required && value.empty()) || value.size() > kMaxPrinterFieldBytes)
        return false;

    target[key] = value;
    return true;
}

bool consume_offer_response_budget(ShareConfig &config)
{
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - config.offer_response_last_refill;
    if (elapsed >= kOfferResponseRefill) {
        const auto intervals = elapsed / kOfferResponseRefill;
        const auto capacity = kOfferResponseBurst - config.offer_response_tokens;
        const auto replenished = static_cast<std::size_t>(
            std::min<std::int64_t>(intervals, static_cast<std::int64_t>(capacity)));
        config.offer_response_tokens += replenished;
        if (config.offer_response_tokens == kOfferResponseBurst)
            config.offer_response_last_refill = now;
        else
            config.offer_response_last_refill += kOfferResponseRefill * intervals;
    }

    if (config.offer_response_tokens == 0)
        return false;
    --config.offer_response_tokens;
    return true;
}

bool consume_mdns_query_budget(
    ShareConfig &config,
    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now())
{
    const auto elapsed = now - config.mdns_query_last_refill;
    if (elapsed >= kMdnsQueryRefill) {
        const auto intervals = elapsed / kMdnsQueryRefill;
        const auto capacity = kMdnsQueryBurst - config.mdns_query_tokens;
        const auto replenished = static_cast<std::size_t>(
            std::min<std::int64_t>(
                intervals,
                static_cast<std::int64_t>(capacity)));
        config.mdns_query_tokens += replenished;
        if (config.mdns_query_tokens == kMdnsQueryBurst)
            config.mdns_query_last_refill = now;
        else
            config.mdns_query_last_refill +=
                kMdnsQueryRefill * intervals;
    }

    if (config.mdns_query_tokens == 0)
        return false;
    --config.mdns_query_tokens;
    return true;
}

const std::string *sanitized_printer_payload(ShareConfig &config)
{
    if (config.cached_printer_payload)
        return &*config.cached_printer_payload;

    try {
        const std::string supplied = config.supplier();
        if (supplied.empty() || supplied.size() > kMaxResponseBytes)
            return nullptr;

        const auto parsed = nlohmann::json::parse(supplied, nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object())
            return nullptr;

        const auto printers = parsed.find("printers");
        if (printers == parsed.end() || !printers->is_array())
            return nullptr;

        nlohmann::json output;
        output["printers"] = nlohmann::json::array();
        const std::size_t count = std::min(printers->size(), kMaxPrinterCount);
        for (std::size_t index = 0; index < count; ++index) {
            const auto &printer = (*printers)[index];
            if (!printer.is_object())
                return nullptr;

            nlohmann::json item;
            if (!copy_printer_field(printer, "serial", true, item) ||
                !copy_printer_field(printer, "host", true, item) ||
                !copy_printer_field(printer, "access_code", true, item) ||
                !copy_printer_field(printer, "name", false, item))
                return nullptr;
            output["printers"].push_back(std::move(item));
        }

        std::string result = output.dump();
        if (result.size() > kMaxResponseBytes)
            return nullptr;
        config.cached_printer_payload = std::move(result);
        return &*config.cached_printer_payload;
    } catch (...) {
        // Supplier exceptions and JSON details may contain credentials. Do not
        // include either in a log or an HTTP response.
        return nullptr;
    }
}

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
public:
    using Done = std::function<void(const std::shared_ptr<HttpSession> &)>;

    HttpSession(tcp::socket socket, std::shared_ptr<ShareConfig> config, Done done)
        : m_stream(std::move(socket))
        , m_buffer(kMaxHeaderBytes)
        , m_config(std::move(config))
        , m_done(std::move(done))
    {
        m_parser.header_limit(kMaxHeaderBytes);
        m_parser.body_limit(0);
    }

    void start()
    {
        m_stream.expires_after(kRequestTimeout);
        auto self = shared_from_this();
        http::async_read(m_stream, m_buffer, m_parser,
            [self](const boost::system::error_code &ec, std::size_t) { self->on_read(ec); });
    }

    void stop()
    {
        close();
    }

private:
    void on_read(const boost::system::error_code &ec)
    {
        if (ec) {
            if (ec == http::error::end_of_stream || ec == beast::error::timeout) {
                close();
                return;
            }
            send(http::status::bad_request, "Bad request.\n", "text/plain; charset=utf-8");
            return;
        }

        const auto &request = m_parser.get();
        if (request.method() != http::verb::get) {
            send(http::status::method_not_allowed, "Only GET is supported.\n", "text/plain; charset=utf-8", true);
            return;
        }

        if (request.target().size() > kMaxTargetBytes ||
            request.target() != beast::string_view(kHttpPath)) {
            send(http::status::not_found, "Not found.\n", "text/plain; charset=utf-8");
            return;
        }

        std::size_t authorization_count = 0;
        std::string authorization;
        for (const auto &field : request) {
            if (field.name() == http::field::authorization) {
                ++authorization_count;
                authorization.assign(field.value().data(), field.value().size());
            }
        }

        if (authorization_count != 1 || !authorized(authorization, m_config->pairing_token)) {
            send(http::status::unauthorized, "Authentication required.\n", "text/plain; charset=utf-8");
            return;
        }

        // The response budget is checked before touching either the supplier
        // or the cached offer, so a flood cannot repeatedly make the service
        // parse credentials or copy a 64 KiB response.
        if (!consume_offer_response_budget(*m_config)) {
            send(http::status::too_many_requests,
                 "Too many requests; retry later.\n",
                 "text/plain; charset=utf-8");
            return;
        }

        const auto payload = sanitized_printer_payload(*m_config);
        if (!payload) {
            BOOST_LOG_TRIVIAL(warning)
                << "[HomeAssistantSharingService] printer data was unavailable or outside its limits";
            send(http::status::service_unavailable,
                 "Printer data is temporarily unavailable.\n",
                 "text/plain; charset=utf-8");
            return;
        }

        send(http::status::ok, *payload, "application/json; charset=utf-8");
    }

    void send(http::status status, std::string body, const char *content_type, bool include_allow = false)
    {
        m_response = {};
        m_response.version(11);
        m_response.result(status);
        m_response.set(http::field::content_type, content_type);
        m_response.set(http::field::cache_control, "no-store");
        m_response.set(http::field::connection, "close");
        m_response.set("X-Content-Type-Options", "nosniff");
        if (include_allow)
            m_response.set(http::field::allow, "GET");
        if (status == http::status::unauthorized)
            m_response.set(http::field::www_authenticate, "Bearer");
        if (status == http::status::too_many_requests)
            m_response.set("Retry-After",
                           std::to_string(
                               (SharingService::offer_response_refill_ms() + 999) / 1000));
        m_response.body() = std::move(body);
        m_response.prepare_payload();
        m_response.keep_alive(false);

        m_stream.expires_after(kRequestTimeout);
        auto self = shared_from_this();
        http::async_write(m_stream, m_response,
            [self](const boost::system::error_code &, std::size_t) { self->close(); });
    }

    void close()
    {
        if (m_closed.exchange(true))
            return;

        boost::system::error_code ignored;
        m_stream.socket().cancel(ignored);
        m_stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
        m_stream.socket().close(ignored);
        if (m_done)
            m_done(shared_from_this());
    }

    beast::tcp_stream                  m_stream;
    beast::flat_buffer                 m_buffer;
    http::request_parser<http::empty_body> m_parser;
    http::response<http::string_body>  m_response;
    std::shared_ptr<ShareConfig>       m_config;
    Done                               m_done;
    std::atomic_bool                   m_closed{false};
};

void append_u16(std::vector<std::uint8_t> &packet, std::uint16_t value)
{
    packet.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    packet.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void append_u32(std::vector<std::uint8_t> &packet, std::uint32_t value)
{
    packet.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
    packet.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    packet.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    packet.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void append_dns_name(std::vector<std::uint8_t> &packet, const std::string &name)
{
    std::size_t begin = 0;
    while (begin < name.size()) {
        const std::size_t end = name.find('.', begin);
        const std::size_t length = (end == std::string::npos ? name.size() : end) - begin;
        if (length == 0 || length > 63)
            throw std::runtime_error("invalid DNS label");
        packet.push_back(static_cast<std::uint8_t>(length));
        packet.insert(packet.end(), name.begin() + begin, name.begin() + begin + length);
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    packet.push_back(0);
}

template<class AppendData>
void append_dns_record(std::vector<std::uint8_t> &packet,
                       const std::string &owner,
                       std::uint16_t type,
                       std::uint16_t record_class,
                       std::uint32_t ttl,
                       AppendData append_data)
{
    append_dns_name(packet, owner);
    append_u16(packet, type);
    append_u16(packet, record_class);
    append_u32(packet, ttl);

    const std::size_t length_offset = packet.size();
    append_u16(packet, 0);
    const std::size_t data_offset = packet.size();
    append_data(packet);
    const std::size_t data_length = packet.size() - data_offset;
    if (data_length > 0xffff)
        throw std::runtime_error("DNS record is too large");
    packet[length_offset]     = static_cast<std::uint8_t>((data_length >> 8) & 0xff);
    packet[length_offset + 1] = static_cast<std::uint8_t>(data_length & 0xff);
}

std::shared_ptr<std::vector<std::uint8_t>>
make_mdns_response(const ShareConfig &config, std::uint32_t ttl, std::uint16_t transaction_id)
{
    constexpr std::uint16_t kDnsTypeA   = 1;
    constexpr std::uint16_t kDnsTypePtr = 12;
    constexpr std::uint16_t kDnsTypeTxt = 16;
    constexpr std::uint16_t kDnsTypeSrv = 33;
    constexpr std::uint16_t kClassIn    = 1;
    constexpr std::uint16_t kCacheFlush = 0x8000;

    auto packet = std::make_shared<std::vector<std::uint8_t>>();
    packet->reserve(512);

    append_u16(*packet, transaction_id);
    append_u16(*packet, 0x8400); // response + authoritative answer
    append_u16(*packet, 0);      // questions
    append_u16(*packet, 4);      // answers
    append_u16(*packet, 0);      // authority records
    append_u16(*packet, 0);      // additional records

    append_dns_record(*packet, kServiceType, kDnsTypePtr, kClassIn, ttl,
        [&config](auto &bytes) { append_dns_name(bytes, config.instance_name); });

    append_dns_record(*packet, config.instance_name, kDnsTypeSrv, kClassIn | kCacheFlush, ttl,
        [&config](auto &bytes) {
            append_u16(bytes, 0); // priority
            append_u16(bytes, 0); // weight
            append_u16(bytes, config.port);
            append_dns_name(bytes, config.hostname);
        });

    append_dns_record(*packet, config.instance_name, kDnsTypeTxt, kClassIn | kCacheFlush, ttl,
        [&config](auto &bytes) {
            const std::array<std::string, 2> values{
                "pairing=" + config.pairing_token,
                "name=" + config.display_name
            };
            for (const auto &value : values) {
                if (value.size() > 255)
                    throw std::runtime_error("DNS TXT value is too large");
                bytes.push_back(static_cast<std::uint8_t>(value.size()));
                bytes.insert(bytes.end(), value.begin(), value.end());
            }
        });

    append_dns_record(*packet, config.hostname, kDnsTypeA, kClassIn | kCacheFlush, ttl,
        [&config](auto &bytes) {
            const auto address_bytes = config.address.to_bytes();
            bytes.insert(bytes.end(), address_bytes.begin(), address_bytes.end());
        });

    if (packet->size() > kMaxMdnsPacketBytes)
        throw std::runtime_error("mDNS response is too large");
    return packet;
}

std::uint16_t read_u16(const std::uint8_t *data)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8) |
        static_cast<std::uint16_t>(data[1]));
}

bool decode_dns_name(const std::uint8_t *data,
                     std::size_t size,
                     std::size_t &offset,
                     std::string &name)
{
    if (offset >= size)
        return false;

    std::size_t cursor = offset;
    std::size_t resume = offset;
    bool        jumped = false;
    std::array<std::size_t, 12> visited{};
    std::size_t visited_count = 0;
    std::size_t label_count = 0;
    name.clear();

    while (cursor < size && label_count <= 32) {
        const std::uint8_t length = data[cursor];
        if ((length & 0xc0) == 0xc0) {
            if (cursor + 1 >= size || visited_count == visited.size())
                return false;
            const std::size_t pointer =
                (static_cast<std::size_t>(length & 0x3f) << 8) | data[cursor + 1];
            if (pointer >= size)
                return false;
            if (std::find(visited.begin(), visited.begin() + visited_count, pointer) !=
                visited.begin() + visited_count)
                return false;
            visited[visited_count++] = pointer;
            if (!jumped)
                resume = cursor + 2;
            cursor = pointer;
            jumped = true;
            continue;
        }
        if ((length & 0xc0) != 0)
            return false;
        if (length == 0) {
            offset = jumped ? resume : cursor + 1;
            return !name.empty();
        }
        if (length > 63 || cursor + 1 + length > size)
            return false;
        if (!name.empty())
            name.push_back('.');
        if (name.size() + length > 253)
            return false;
        name.append(reinterpret_cast<const char *>(data + cursor + 1), length);
        cursor += 1 + length;
        ++label_count;
        if (!jumped)
            resume = cursor;
    }

    return false;
}

bool ascii_equal_case_insensitive(const std::string &lhs, const std::string &rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        const auto left  = static_cast<unsigned char>(lhs[index]);
        const auto right = static_cast<unsigned char>(rhs[index]);
        if (std::tolower(left) != std::tolower(right))
            return false;
    }
    return true;
}

struct ParsedMdnsQuery
{
    bool          relevant = false;
    bool          wants_unicast = false;
    std::uint16_t transaction_id = 0;
};

ParsedMdnsQuery parse_mdns_query(const std::uint8_t *data,
                                 std::size_t size,
                                 const ShareConfig &config)
{
    ParsedMdnsQuery result;
    if (size < 12 || size > kMaxMdnsPacketBytes)
        return result;

    result.transaction_id = read_u16(data);
    const std::uint16_t flags = read_u16(data + 2);
    const std::uint16_t question_count = read_u16(data + 4);
    if ((flags & 0x8000) != 0 || (flags & 0x7800) != 0 || (flags & 0x0200) != 0 ||
        question_count == 0 || question_count > kMaxDnsQuestions)
        return result;

    constexpr std::uint16_t kDnsTypeA   = 1;
    constexpr std::uint16_t kDnsTypePtr = 12;
    constexpr std::uint16_t kDnsTypeTxt = 16;
    constexpr std::uint16_t kDnsTypeSrv = 33;
    constexpr std::uint16_t kDnsTypeAny = 255;
    constexpr std::uint16_t kClassIn    = 1;
    constexpr std::uint16_t kClassAny   = 255;

    std::size_t offset = 12;
    for (std::size_t index = 0; index < question_count; ++index) {
        std::string name;
        if (!decode_dns_name(data, size, offset, name) || offset + 4 > size)
            return {};

        const std::uint16_t type = read_u16(data + offset);
        const std::uint16_t raw_class = read_u16(data + offset + 2);
        offset += 4;

        const std::uint16_t question_class = raw_class & 0x7fff;
        if (question_class != kClassIn && question_class != kClassAny)
            continue;
        result.wants_unicast = result.wants_unicast || (raw_class & 0x8000) != 0;

        const bool service_question =
            ascii_equal_case_insensitive(name, kServiceType) &&
            (type == kDnsTypePtr || type == kDnsTypeAny);
        const bool instance_question =
            ascii_equal_case_insensitive(name, config.instance_name) &&
            (type == kDnsTypeSrv || type == kDnsTypeTxt || type == kDnsTypeAny);
        const bool host_question =
            ascii_equal_case_insensitive(name, config.hostname) &&
            (type == kDnsTypeA || type == kDnsTypeAny);
        result.relevant = result.relevant || service_question || instance_question || host_question;
    }

    return result;
}

SharingService::StartResult start_failure(
    SharingStartErrorCode code,
    int native_error = 0)
{
    SharingService::StartResult result;
    result.error_code = code;
    result.native_error = native_error;
    return result;
}

} // namespace

class SharingService::Impl
{
public:
    Impl()
        : m_acceptor(m_io)
        , m_mdns_socket(m_io)
        , m_announcement_timer(m_io)
        , m_mdns_send_timer(m_io)
    {}

    ~Impl()
    {
        stop();
    }

    StartResult start(Options options, PrinterJsonSupplier supplier)
    {
        std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mutex);
        if (m_stopping)
            return start_failure(SharingStartErrorCode::PreviousWorkerActive);
        if (m_running.load())
            return start_failure(SharingStartErrorCode::AlreadyRunning);
        if (m_worker.joinable())
            return start_failure(SharingStartErrorCode::PreviousWorkerActive);
        if (!supplier)
            return start_failure(SharingStartErrorCode::MissingSupplier);
        if (!is_url_safe_token(options.pairing_token))
            return start_failure(SharingStartErrorCode::InvalidPairingToken);
        if (!is_safe_display_name(options.display_name))
            return start_failure(SharingStartErrorCode::InvalidDisplayName);
        if (options.port != 0 && options.port < 1024)
            return start_failure(SharingStartErrorCode::InvalidPort);

        StartResult success;
        try {
        const bool explicit_address = !options.advertised_ipv4.empty();
        std::optional<asio::ip::address_v4> address;
        if (explicit_address) {
            boost::system::error_code ec;
            const auto parsed = asio::ip::make_address(options.advertised_ipv4, ec);
            if (!ec && parsed.is_v4() && valid_advertised_address(parsed.to_v4()))
                address = parsed.to_v4();
            else
                return start_failure(
                    SharingStartErrorCode::InvalidAdvertisedAddress);
        } else {
            address = detect_lan_address();
            if (!address)
                return start_failure(SharingStartErrorCode::NoLanAddress);
        }
        const std::optional<std::uint8_t> on_link_prefix =
            interface_prefix_length(*address);
        if (!on_link_prefix || *on_link_prefix == 0) {
            return start_failure(
                explicit_address
                    ? SharingStartErrorCode::InvalidAdvertisedAddress
                    : SharingStartErrorCode::NoLanAddress);
        }

        const std::string suffix = uuid_hex().substr(0, 8);
        auto config = std::make_shared<ShareConfig>();
        config->pairing_token = std::move(options.pairing_token);
        config->display_name  = std::move(options.display_name);
        config->instance_name = make_instance_label(config->display_name, suffix) + "." + kServiceType;
        config->hostname      = "bambu-studio-" + suffix + ".local";
        config->address       = *address;
        config->on_link_prefix_length = *on_link_prefix;
        config->supplier      = std::move(supplier);

        m_io.restart();
        m_mdns_send_queue.clear();
        m_mdns_send_active = false;
        m_active_mdns_response.reset();
        m_next_mdns_send = {};
        m_guard = std::make_unique<WorkGuard>(m_io.get_executor());

        boost::system::error_code ec;
        const tcp::endpoint http_endpoint(config->address, options.port);
        m_acceptor.open(http_endpoint.protocol(), ec);
        if (ec)
            return fail_setup(
                SharingStartErrorCode::HttpEndpointUnavailable,
                ec);
        m_acceptor.bind(http_endpoint, ec);
        if (ec)
            return fail_setup(
                SharingStartErrorCode::HttpEndpointUnavailable,
                ec);
        m_acceptor.listen(static_cast<int>(kMaxConcurrentSessions), ec);
        if (ec)
            return fail_setup(
                SharingStartErrorCode::HttpEndpointUnavailable,
                ec);

        config->port = m_acceptor.local_endpoint(ec).port();
        if (ec)
            return fail_setup(
                SharingStartErrorCode::HttpEndpointUnavailable,
                ec);

        m_mdns_socket.open(udp::v4(), ec);
        if (ec)
            return fail_setup(SharingStartErrorCode::MdnsUnavailable, ec);
        m_mdns_socket.set_option(asio::socket_base::reuse_address(true), ec);
        if (ec)
            return fail_setup(SharingStartErrorCode::MdnsUnavailable, ec);
        m_mdns_socket.bind(udp::endpoint(udp::v4(), kMdnsPort), ec);
        if (ec)
            return fail_setup(SharingStartErrorCode::MdnsUnavailable, ec);

        const auto multicast_address = asio::ip::make_address_v4(kMulticastAddress);
        m_mdns_socket.set_option(asio::ip::multicast::join_group(multicast_address, config->address), ec);
        if (ec)
            return fail_setup(SharingStartErrorCode::MdnsUnavailable, ec);
        m_mdns_socket.set_option(asio::ip::multicast::outbound_interface(config->address), ec);
        if (ec)
            return fail_setup(SharingStartErrorCode::MdnsUnavailable, ec);
        m_mdns_socket.set_option(asio::ip::multicast::hops(255), ec);
        if (ec)
            return fail_setup(SharingStartErrorCode::MdnsUnavailable, ec);
        m_mdns_socket.set_option(asio::ip::multicast::enable_loopback(true), ec);
        if (ec)
            return fail_setup(SharingStartErrorCode::MdnsUnavailable, ec);

        success.started = true;
        success.port = config->port;
        success.advertised_ipv4 = config->address.to_string();
        success.error_code = SharingStartErrorCode::None;
        success.native_error = 0;

        m_config = std::move(config);
        m_port.store(m_config->port);
        m_running.store(true);
        do_accept();
        do_mdns_receive();
        announce_and_schedule(0);

        m_worker_active.store(true);
        m_worker = std::thread([this] {
            try {
                m_io.run();
            } catch (...) {
                try {
                    BOOST_LOG_TRIVIAL(error)
                        << "[HomeAssistantSharingService] the sharing worker stopped unexpectedly";
                } catch (...) {
                }
                m_running.store(false);
            }
            m_worker_active.store(false);
        });
        } catch (const boost::system::system_error &error) {
            return rollback_startup_failure(error.code().value());
        } catch (const std::system_error &error) {
            return rollback_startup_failure(error.code().value());
        } catch (...) {
            return rollback_startup_failure();
        }

        try {
            BOOST_LOG_TRIVIAL(info)
                << "[HomeAssistantSharingService] sharing started on port "
                << success.port;
        } catch (...) {
        }
        return success;
    }

    void stop() noexcept
    {
        bool        was_running = false;
        std::thread worker_to_join;
        {
            std::unique_lock<std::mutex> lifecycle_lock(m_lifecycle_mutex);
            if (m_stopping) {
                // If a supplier re-enters stop() while another thread owns the
                // join, waiting would deadlock that owner. Its stop already
                // covers this worker generation.
                if (m_stopping_worker_id == std::this_thread::get_id())
                    return;
                m_lifecycle_condition.wait(
                    lifecycle_lock,
                    [this] { return !m_stopping; });
                return;
            }

            was_running = m_running.exchange(false);
            if (!was_running && !m_worker.joinable()) {
                cleanup_after_stop();
                return;
            }

            if (m_worker.joinable() &&
                m_worker.get_id() == std::this_thread::get_id()) {
                // A supplier must not control the service, but avoid joining
                // this worker from itself. A later owner call performs the
                // serialized join and cleanup.
                try {
                    shutdown_on_io_thread(was_running);
                } catch (...) {
                }
                m_guard.reset();
                m_io.stop();
                return;
            }

            // Move the only join handle into this stop operation before
            // releasing the mutex. Other stop calls wait, and start rejects
            // while this generation is joining and being cleaned.
            m_stopping = true;
            if (m_worker.joinable()) {
                m_stopping_worker_id = m_worker.get_id();
                worker_to_join = std::move(m_worker);
            } else {
                m_stopping_worker_id = {};
            }
        }

        bool shutdown_ran = false;
        if (worker_to_join.joinable() &&
            m_worker_active.load() &&
            !m_io.stopped()) {
            try {
                auto completed = std::make_shared<std::promise<void>>();
                auto future = completed->get_future();
                asio::post(m_io, [this, completed, was_running] {
                    try {
                        shutdown_on_io_thread(was_running);
                    } catch (...) {
                    }
                    try {
                        completed->set_value();
                    } catch (...) {
                    }
                });
                // The worker normally services this immediately. If a handler
                // has just thrown and the worker is exiting, an unbounded wait
                // here would freeze dialog destruction.
                shutdown_ran =
                    future.wait_for(kRequestTimeout) ==
                    std::future_status::ready;
            } catch (...) {
                shutdown_ran = false;
            }
        }

        m_guard.reset();
        m_io.stop();
        if (worker_to_join.joinable())
            worker_to_join.join();

        {
            std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mutex);
            if (!shutdown_ran) {
                try {
                    shutdown_on_io_thread(was_running);
                } catch (...) {
                }
            }

            // Deliver operation_aborted handlers while the implementation and
            // its session registry still exist, then make the io_context
            // restartable.
            try {
                m_io.restart();
                while (m_io.poll_one() != 0) {
                }
                m_io.restart();
            } catch (...) {
                m_io.stop();
            }
            try {
                cleanup_after_stop();
            } catch (...) {
                m_config.reset();
                m_port.store(0);
            }
            m_stopping_worker_id = {};
            m_stopping = false;
        }
        m_lifecycle_condition.notify_all();

        if (was_running) {
            try {
                BOOST_LOG_TRIVIAL(info)
                    << "[HomeAssistantSharingService] sharing stopped";
            } catch (...) {
            }
        }
    }

    bool is_running() const noexcept
    {
        return m_running.load();
    }

    std::uint16_t port() const noexcept
    {
        return m_port.load();
    }

private:
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    struct PendingMdnsResponse
    {
        std::shared_ptr<std::vector<std::uint8_t>> packet;
        udp::endpoint                              destination;
        std::uint16_t                              transaction_id = 0;
    };

    struct MdnsResponseKey
    {
        udp::endpoint destination;
        std::uint16_t transaction_id = 0;
    };

    StartResult rollback_startup_failure(int native_error = 0) noexcept
    {
        m_worker_active.store(false);
        m_running.store(false);
        try {
            shutdown_on_io_thread(false);
        } catch (...) {
            boost::system::error_code ignored;
            m_announcement_timer.cancel(ignored);
            m_mdns_send_timer.cancel(ignored);
            m_acceptor.close(ignored);
            m_mdns_socket.close(ignored);
        }

        m_guard.reset();
        m_io.stop();
        try {
            m_io.restart();
            while (m_io.poll_one() != 0) {
            }
            m_io.restart();
        } catch (...) {
            m_io.stop();
        }
        try {
            cleanup_after_stop();
        } catch (...) {
            m_config.reset();
            m_port.store(0);
        }
        return start_failure(
            SharingStartErrorCode::WorkerUnavailable,
            native_error);
    }

    StartResult fail_setup(
        SharingStartErrorCode code,
        const boost::system::error_code &native_error)
    {
        const int native_value = native_error.value();
        BOOST_LOG_TRIVIAL(error)
            << "[HomeAssistantSharingService] setup failed; error_code="
            << static_cast<unsigned>(code)
            << " native_error=" << native_value;

        boost::system::error_code ignored;
        m_announcement_timer.cancel(ignored);
        m_mdns_send_timer.cancel(ignored);
        m_mdns_send_queue.clear();
        m_mdns_send_active = false;
        m_active_mdns_response.reset();
        m_next_mdns_send = {};
        m_acceptor.close(ignored);
        m_mdns_socket.close(ignored);
        m_guard.reset();
        m_config.reset();
        m_port.store(0);
        m_io.restart();
        while (m_io.poll_one() != 0) {
        }
        m_io.restart();
        return start_failure(code, native_value);
    }

    void cleanup_after_stop()
    {
        boost::system::error_code ignored;
        m_mdns_send_timer.cancel(ignored);
        m_mdns_send_queue.clear();
        m_mdns_send_active = false;
        m_active_mdns_response.reset();
        m_next_mdns_send = {};
        m_sessions.clear();
        m_config.reset();
        m_port.store(0);
    }

    void shutdown_on_io_thread(bool send_goodbye)
    {
        boost::system::error_code ignored;
        m_announcement_timer.cancel(ignored);
        m_mdns_send_timer.cancel(ignored);
        m_mdns_send_queue.clear();
        m_mdns_send_active = false;
        m_active_mdns_response.reset();
        m_next_mdns_send = {};

        if (send_goodbye && m_config && m_mdns_socket.is_open()) {
            try {
                const auto goodbye = make_mdns_response(*m_config, 0, 0);
                m_mdns_socket.send_to(
                    asio::buffer(*goodbye),
                    udp::endpoint(asio::ip::make_address_v4(kMulticastAddress), kMdnsPort),
                    0,
                    ignored);
            } catch (...) {
                // Shutdown is best effort and never reports credential data.
            }
        }

        m_acceptor.cancel(ignored);
        m_acceptor.close(ignored);
        m_mdns_socket.cancel(ignored);
        m_mdns_socket.close(ignored);

        const auto sessions = m_sessions;
        for (const auto &session : sessions)
            session->stop();
        m_sessions.clear();
    }

    void do_accept()
    {
        m_acceptor.async_accept([this](const boost::system::error_code &ec, tcp::socket socket) {
            if (!ec && m_running.load() && m_config) {
                if (m_sessions.size() >= kMaxConcurrentSessions) {
                    boost::system::error_code ignored;
                    socket.close(ignored);
                } else {
                    auto session = std::make_shared<HttpSession>(
                        std::move(socket),
                        m_config,
                        [this](const std::shared_ptr<HttpSession> &finished) {
                            m_sessions.erase(finished);
                        });
                    m_sessions.insert(session);
                    session->start();
                }
            }

            if (m_running.load() && m_acceptor.is_open())
                do_accept();
        });
    }

    bool mdns_response_pending(
        const udp::endpoint &destination,
        std::uint16_t transaction_id) const
    {
        if (m_active_mdns_response &&
            m_active_mdns_response->destination == destination &&
            m_active_mdns_response->transaction_id == transaction_id)
            return true;

        return std::any_of(
            m_mdns_send_queue.begin(),
            m_mdns_send_queue.end(),
            [&destination, transaction_id](
                const PendingMdnsResponse &pending) {
                return pending.destination == destination &&
                       pending.transaction_id == transaction_id;
            });
    }

    void do_mdns_receive()
    {
        m_mdns_socket.async_receive_from(
            asio::buffer(m_mdns_buffer),
            m_mdns_sender,
            [this](const boost::system::error_code &ec, std::size_t bytes_received) {
                if (!ec && m_running.load() && m_config) {
                    const auto query = parse_mdns_query(
                        m_mdns_buffer.data(), bytes_received, *m_config);
                    if (query.relevant &&
                        valid_mdns_sender(m_mdns_sender, *m_config)) {
                        const bool unicast =
                            query.wants_unicast || m_mdns_sender.port() != kMdnsPort;
                        const udp::endpoint destination = unicast
                            ? m_mdns_sender
                            : udp::endpoint(asio::ip::make_address_v4(kMulticastAddress), kMdnsPort);
                        const std::uint16_t transaction_id =
                            unicast ? query.transaction_id : 0;

                        // Admission precedes response construction so a query
                        // flood cannot force one response allocation per
                        // packet. Duplicates and a full queue do not consume
                        // the global query budget.
                        if (!mdns_response_pending(
                                destination,
                                transaction_id) &&
                            m_mdns_send_queue.size() <
                                kMaxPendingMdnsResponses &&
                            consume_mdns_query_budget(*m_config)) {
                            try {
                                const auto response = make_mdns_response(
                                    *m_config,
                                    kMdnsTtlSeconds,
                                    transaction_id);
                                enqueue_mdns_response(
                                    response,
                                    destination,
                                    transaction_id);
                            } catch (...) {
                                BOOST_LOG_TRIVIAL(warning)
                                    << "[HomeAssistantSharingService] an mDNS response could not be created";
                            }
                        }
                    }
                }

                if (m_running.load() && m_mdns_socket.is_open())
                    do_mdns_receive();
            });
    }

    void announce_and_schedule(unsigned startup_round)
    {
        if (!m_running.load() || !m_config || !m_mdns_socket.is_open())
            return;

        try {
            const auto announcement = make_mdns_response(*m_config, kMdnsTtlSeconds, 0);
            enqueue_mdns_response(
                announcement,
                udp::endpoint(asio::ip::make_address_v4(kMulticastAddress), kMdnsPort),
                0);
        } catch (...) {
            BOOST_LOG_TRIVIAL(warning)
                << "[HomeAssistantSharingService] an mDNS announcement could not be created";
        }

        const auto delay = startup_round < 2 ? std::chrono::seconds(1) : std::chrono::seconds(60);
        m_announcement_timer.expires_after(delay);
        m_announcement_timer.async_wait([this, startup_round](const boost::system::error_code &ec) {
            if (!ec && m_running.load())
                announce_and_schedule(std::min(startup_round + 1, 2u));
        });
    }

    void enqueue_mdns_response(
        std::shared_ptr<std::vector<std::uint8_t>> packet,
        const udp::endpoint &destination,
        std::uint16_t transaction_id)
    {
        if (!packet || !m_running.load() || !m_mdns_socket.is_open())
            return;

        // Configuration is immutable for one sharing window, so an already
        // queued or active response is equivalent. Ignore it instead of
        // replacing its allocation.
        if (mdns_response_pending(destination, transaction_id))
            return;

        if (m_mdns_send_queue.size() >= kMaxPendingMdnsResponses)
            return;

        m_mdns_send_queue.push_back(
            {std::move(packet), destination, transaction_id});
        pump_mdns_responses();
    }

    void pump_mdns_responses()
    {
        if (m_mdns_send_active || m_mdns_send_queue.empty() ||
            !m_running.load() || !m_mdns_socket.is_open())
            return;

        m_mdns_send_active = true;
        const auto now = std::chrono::steady_clock::now();
        m_mdns_send_timer.expires_at(std::max(now, m_next_mdns_send));
        m_mdns_send_timer.async_wait([this](const boost::system::error_code &timer_ec) {
            if (timer_ec || !m_running.load() || !m_mdns_socket.is_open() ||
                m_mdns_send_queue.empty()) {
                m_mdns_send_active = false;
                return;
            }

            PendingMdnsResponse pending = std::move(m_mdns_send_queue.front());
            m_mdns_send_queue.pop_front();
            m_active_mdns_response =
                MdnsResponseKey{
                    pending.destination,
                    pending.transaction_id};
            const auto packet = pending.packet;
            try {
                m_mdns_socket.async_send_to(
                    asio::buffer(*packet),
                    pending.destination,
                    [this, packet](const boost::system::error_code &, std::size_t) {
                        // Capturing the packet keeps the response buffer valid
                        // until the single in-flight send has completed.
                        m_next_mdns_send =
                            std::chrono::steady_clock::now() + kMdnsResponseInterval;
                        m_mdns_send_active = false;
                        m_active_mdns_response.reset();
                        pump_mdns_responses();
                    });
            } catch (...) {
                m_next_mdns_send =
                    std::chrono::steady_clock::now() + kMdnsResponseInterval;
                m_mdns_send_active = false;
                m_active_mdns_response.reset();
                pump_mdns_responses();
            }
        });
    }

    mutable std::mutex                      m_lifecycle_mutex;
    std::condition_variable                 m_lifecycle_condition;
    bool                                    m_stopping = false;
    std::thread::id                         m_stopping_worker_id;
    std::atomic_bool                        m_running{false};
    std::atomic_bool                        m_worker_active{false};
    std::atomic<std::uint16_t>              m_port{0};
    asio::io_context                        m_io{1};
    std::unique_ptr<WorkGuard>               m_guard;
    tcp::acceptor                           m_acceptor;
    udp::socket                             m_mdns_socket;
    asio::steady_timer                      m_announcement_timer;
    asio::steady_timer                      m_mdns_send_timer;
    std::thread                             m_worker;
    std::shared_ptr<ShareConfig>             m_config;
    std::set<std::shared_ptr<HttpSession>>   m_sessions;
    std::array<std::uint8_t, 2048>           m_mdns_buffer{};
    udp::endpoint                           m_mdns_sender;
    std::deque<PendingMdnsResponse>          m_mdns_send_queue;
    bool                                    m_mdns_send_active = false;
    std::optional<MdnsResponseKey>           m_active_mdns_response;
    std::chrono::steady_clock::time_point    m_next_mdns_send{};
};

SharingService::SharingService()
    : m_impl(std::make_unique<Impl>())
{}

SharingService::~SharingService() = default;

std::string SharingService::make_pairing_token()
{
    // Two independent random UUID v4 values provide about 244 random bits after
    // their fixed version/variant bits. Hyphens are removed so the value is
    // directly safe in HTTP and DNS-SD TXT data.
    return uuid_hex() + uuid_hex();
}

SharingService::StartResult
SharingService::start(Options options, PrinterJsonSupplier printer_json_supplier)
{
    return m_impl->start(std::move(options), std::move(printer_json_supplier));
}

void SharingService::stop() noexcept
{
    m_impl->stop();
}

bool SharingService::is_running() const noexcept
{
    return m_impl->is_running();
}

std::uint16_t SharingService::port() const noexcept
{
    return m_impl->port();
}

}}} // namespace Slic3r::GUI::HomeAssistant
