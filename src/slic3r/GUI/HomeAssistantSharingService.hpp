#ifndef slic3r_GUI_HomeAssistantSharingService_hpp_
#define slic3r_GUI_HomeAssistantSharingService_hpp_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Slic3r { namespace GUI { namespace HomeAssistant {

namespace SharingNetworkPolicy {

// Pure IPv4 policy used by the mDNS receiver and deterministic tests. Address
// values use network-byte significance (for example, 192.168.1.10 is
// 0xc0a8010a). The sender must be in an allowed private/shared range, on the
// advertised interface's actual prefix, and not a network or broadcast
// address.
bool mdns_sender_is_eligible_ipv4(
    std::uint32_t advertised_address,
    std::uint32_t sender_address,
    std::uint8_t on_link_prefix_length) noexcept;

} // namespace SharingNetworkPolicy

enum class SharingStartErrorCode : std::uint8_t
{
    None,
    AlreadyRunning,
    PreviousWorkerActive,
    MissingSupplier,
    InvalidPairingToken,
    InvalidDisplayName,
    InvalidPort,
    InvalidAdvertisedAddress,
    NoLanAddress,
    HttpEndpointUnavailable,
    MdnsUnavailable,
    WorkerUnavailable,
};

// A short-lived, opt-in LAN handover endpoint for Home Assistant discovery.
//
// Construction does not open a socket. The caller must explicitly start one
// sharing window and must stop it when that window closes. Destruction also
// stops the service, so an abandoned UI flow cannot leave printer credentials
// available on the network.
class SharingService
{
public:
    using PrinterJsonSupplier = std::function<std::string()>;

    struct Options
    {
        // Must be newly generated for this sharing window. make_pairing_token()
        // returns a suitable high-entropy token. The value is advertised in mDNS
        // and required verbatim in the HTTP Authorization header.
        std::string pairing_token;

        // Human-readable TXT record value. It is never written to the log.
        std::string display_name = "Bambu Studio";

        // Empty means auto-detect the route's non-loopback IPv4 address.
        // Supplying an address is useful when the host has several LANs.
        std::string advertised_ipv4;

        // Zero asks the OS for an unused port. Explicit ports must be in the
        // unprivileged 1024-65535 range accepted by the Home Assistant peer.
        // The selected port is advertised through the DNS-SD SRV record.
        std::uint16_t port = 0;
    };

    struct StartResult
    {
        bool                  started = false;
        std::uint16_t         port = 0;
        std::string           advertised_ipv4;
        SharingStartErrorCode error_code = SharingStartErrorCode::None;

        // Optional platform error value for diagnostics and logs. It is not
        // localized UI copy and must never be shown to the user as prose.
        int native_error = 0;

        explicit operator bool() const noexcept { return started; }
    };

    SharingService();
    ~SharingService();

    SharingService(const SharingService &) = delete;
    SharingService &operator=(const SharingService &) = delete;
    SharingService(SharingService &&) = delete;
    SharingService &operator=(SharingService &&) = delete;

    // Generates a fresh URL-safe token with more than 240 random bits.
    static std::string make_pairing_token();
    static constexpr std::size_t max_printer_count() noexcept { return 32; }
    static constexpr std::size_t max_offer_response_bytes() noexcept { return 64 * 1024; }
    static constexpr std::size_t max_pending_mdns_responses() noexcept { return 8; }
    static constexpr std::uint32_t mdns_response_interval_ms() noexcept { return 50; }
    static constexpr std::size_t mdns_query_burst() noexcept { return 8; }
    static constexpr std::uint32_t mdns_query_refill_ms() noexcept { return 1000; }
    static constexpr std::size_t offer_response_burst() noexcept { return 4; }
    static constexpr std::uint32_t offer_response_refill_ms() noexcept { return 1000; }

    // Starts both the authenticated HTTP endpoint and the mDNS responder.
    // The supplier is invoked on the service's private worker thread only
    // after a request has authenticated. It must return:
    //   {"printers":[{"serial","host","access_code"[,"name"]}, ...]}
    // It must return promptly and must not call start() or stop() itself.
    StartResult start(Options options, PrinterJsonSupplier printer_json_supplier);

    // Idempotent. Sends a zero-TTL mDNS goodbye, closes active HTTP sessions,
    // cancels pending work, and joins the private worker thread.
    void stop() noexcept;

    bool is_running() const noexcept;
    std::uint16_t port() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}}} // namespace Slic3r::GUI::HomeAssistant

#endif // slic3r_GUI_HomeAssistantSharingService_hpp_
