#pragma once
// LAN model drop, station side: the pure part (docs/features/application-integration/lan-model-drop.md).
//
// Everything here is plain C++17 with no wxWidgets, no network and no file system, so
// tests/lan_model_drop/lan_model_drop_model_test.cpp can compile it with g++ alone. It parses and
// validates what the drop container answers (protocol version 1), decides which received files are
// acceptable, names them safely on disk, schedules polling, and classifies the connection status.

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

constexpr int           kProtocolVersion   = 1;
constexpr int           kPollSeconds       = 5;
constexpr int           kMaxBackoffSeconds = 60;
constexpr std::size_t   kMaxFileNameChars  = 200;
constexpr std::size_t   kMaxSenderChars    = 40;
constexpr std::size_t   kMaxInboxItems     = 1000;
// Hard station-side cap for one file, whatever the container advertises (its own default is 256 MiB
// per file and 2 GiB for the whole queue).
constexpr std::uint64_t kMaxItemBytes      = 2147483648ULL;
// A download that fails for a network reason is tried this many times before it is given up.
constexpr int           kMaxDownloadAttempts = 3;
constexpr const char   *kDefaultAddress    = "http://localhost:8833";

// ---------------------------------------------------------------------------------------------
// Accepted model types
// ---------------------------------------------------------------------------------------------

enum class FileType { ThreeMF, Stl, Step, Obj, Amf };

// Protocol names: "3mf", "stl", "step", "obj", "amf".
std::optional<FileType> type_from_name(std::string_view name);
const char             *type_name(FileType type);
// By extension, case-insensitive: .3mf .stl .step .stp .obj .amf.
std::optional<FileType> type_from_extension(std::string_view file_name);
// Content check that mirrors the container's sniffing rules (defence in depth: the bytes have already
// passed the container and the SHA-256 check).
bool content_matches(FileType type, std::string_view bytes);

// ---------------------------------------------------------------------------------------------
// Names and fields
// ---------------------------------------------------------------------------------------------

// Base name only; control characters and <>:"/\|?* removed; trimmed; trailing dots removed; a
// Windows device name (CON, PRN, AUX, NUL, COM1-9, LPT1-9) gets a leading underscore; at most 200
// characters with the extension kept. Empty when the name is not valid UTF-8 or does not keep an
// accepted extension with a non-empty stem. Never used as anything but the last path component.
std::string sanitize_file_name(std::string_view raw);
// Control and bidirectional-override characters removed, trimmed, at most 40 characters. Invalid
// UTF-8 sequences are dropped.
std::string sanitize_sender(std::string_view raw);

bool is_item_id(std::string_view id);        // exactly 32 lowercase hexadecimal digits
bool is_sha256_hex(std::string_view digest); // exactly 64 lowercase hexadecimal digits
bool is_utc_stamp(std::string_view stamp);   // exactly YYYY-MM-DDTHH:MM:SSZ, a real date and time
// The station key travels in an Authorization header: 16 to 512 visible ASCII characters, no spaces.
bool is_station_key(std::string_view key);
// The drop code people type: 1 to 64 visible ASCII characters.
bool is_drop_code(std::string_view code);
// Lowercase hexadecimal of raw digest bytes.
std::string to_hex(const unsigned char *bytes, std::size_t size);

// ---------------------------------------------------------------------------------------------
// Protocol messages
// ---------------------------------------------------------------------------------------------

struct InboxItem
{
    std::string   id;
    std::string   file_name; // sanitized
    std::string   sender;    // sanitized, may be empty
    std::uint64_t bytes = 0;
    std::string   sha256;    // lowercase hex
    std::string   received_at;
    FileType      type = FileType::Stl;
};

struct InboxListing
{
    bool                     ok = false;  // false: not a protocol-1 inbox answer at all
    std::vector<InboxItem>   items;       // valid entries, in the container's order (oldest first)
    std::vector<std::string> invalid_ids; // entries with a usable id whose other fields failed the checks
    std::size_t              unusable = 0; // entries without a usable id (cannot be addressed)
};
InboxListing parse_inbox(std::string_view body);

struct StationStatus
{
    std::string   station_name;
    std::string   drop_code;
    std::uint64_t queued       = 0;
    std::uint64_t queued_bytes = 0;
    std::uint64_t max_bytes    = 0;
    std::uint64_t ttl_hours    = 0;
};
// Only a protocol-1 answer with every field present and well formed.
std::optional<StationStatus> parse_status(std::string_view body);
// {"dropCode":"654321"}
std::optional<std::string> parse_drop_code(std::string_view body);
// {"ok":true,"service":"lan-model-drop","protocol":1}
bool parse_health(std::string_view body);

// ---------------------------------------------------------------------------------------------
// Connection status
// ---------------------------------------------------------------------------------------------

enum class LinkState {
    Off,                  // the switch is off
    Checking,             // on, no answer yet
    Connected,
    NotReachable,
    WrongStationKey,
    ProtocolNotSupported,
    NeedsStationKey,      // on, but no station key is stored
    InvalidAddress,       // on, but the address cannot be used
    KeyStorageUnavailable // the protected key store cannot be read on this system
};

// What one HTTP exchange produced. `completed` is false when no HTTP answer arrived at all.
struct Exchange
{
    bool        completed   = false;
    unsigned    http_status = 0;
    std::string body;
};
// Classifies a GET /api/station/status exchange; fills `status` when Connected.
LinkState classify_status(const Exchange &exchange, StationStatus *status = nullptr);
// Classifies a failed station API call (inbox, file, delete, new code).
LinkState classify_failure(const Exchange &exchange);
bool      is_error_state(LinkState state);

// Delay before the next poll: 5 s while healthy, doubling after each consecutive failure up to 60 s.
int next_poll_delay_seconds(int consecutive_failures);

// ---------------------------------------------------------------------------------------------
// Addresses
// ---------------------------------------------------------------------------------------------

struct BaseAddress
{
    std::string scheme; // "http" or "https"
    std::string host;   // lowercase; IPv6 keeps its brackets
    int         port = 0;
    bool        explicit_port = false;
    bool        loopback = false;
    std::string base;   // scheme://host[:port], no trailing slash
};
// Accepts "http://host[:port]", "https://host[:port]" or "host[:port]" (http), with an optional
// trailing slash. No user name, path, query or fragment.
std::optional<BaseAddress> parse_base_address(std::string_view input);

std::string health_url(const BaseAddress &address);
std::string status_url(const BaseAddress &address);
std::string inbox_url(const BaseAddress &address);
std::string file_url(const BaseAddress &address, const std::string &id);
std::string drop_code_url(const BaseAddress &address);

bool is_ipv4(std::string_view text);
bool is_private_ipv4(std::string_view text);

// One IPv4 address of a network adapter, as the platform reports it.
struct AdapterAddress
{
    std::string ipv4;
    std::string adapter_name; // friendly name or description, used only to recognise virtual adapters
    bool        up          = false;
    bool        loopback    = false;
    bool        has_gateway = false;
};
bool        looks_virtual_adapter(std::string_view adapter_name);
// The address other devices on the LAN most likely reach this computer at: an up, non-loopback,
// non-link-local IPv4, preferring an adapter with a gateway, a private range and a physical adapter.
// Empty when there is none.
std::string choose_lan_ipv4(const std::vector<AdapterAddress> &addresses);
// The address to share with senders: the configured address, or http://<LAN IPv4>:<port> when the
// configured host is this computer's loopback. Empty when a loopback address has no LAN address.
std::string share_address(const BaseAddress &address, const std::string &lan_ipv4);

// ---------------------------------------------------------------------------------------------
// Presentation helpers
// ---------------------------------------------------------------------------------------------

// "512 B", "1.5 KB", "12.3 MB", "1.25 GB" (binary multiples).
std::string format_size(std::uint64_t bytes);
// `text`, or `text` followed by " (2)", " (3)" ... when it equals one already in `existing`, so two
// identical arrivals keep separate notifications.
std::string distinct_text(const std::string &text, const std::vector<std::string> &existing);

// ---------------------------------------------------------------------------------------------
// Which listed items still need work
// ---------------------------------------------------------------------------------------------

class InboxTracker
{
public:
    // Items of a fresh listing that were never taken, in listing order. Taking them is separate so a
    // stopped worker never marks an item it did not start.
    std::vector<InboxItem> fresh(const std::vector<InboxItem> &listed) const;
    void take(const std::string &id);
    // A download that failed for a network reason: returns true when the item may be tried again on
    // a later poll, false once kMaxDownloadAttempts were used (the caller then gives it up).
    bool retry_later(const std::string &id);
    // The item must be removed from the container (opened, discarded or refused).
    void queue_delete(const std::string &id);
    std::vector<std::string> pending_deletes() const;
    void delete_confirmed(const std::string &id);
    // Forget taken ids the container no longer lists, unless a delete is still pending.
    void prune(const std::vector<InboxItem> &listed);
    bool taken(const std::string &id) const { return m_taken.count(id) != 0; }

private:
    std::set<std::string>      m_taken;
    std::set<std::string>      m_deletes;
    std::map<std::string, int> m_attempts;
};

}}} // namespace Slic3r::GUI::LanModelDrop
