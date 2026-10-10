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
// Content check with the container's own sniffing rules (lan-model-drop/server/sniff.mjs), so a model
// the container accepted is never refused here (defence in depth: the bytes have already passed the
// container and the SHA-256 check). Markers are looked for in the first 256 KiB, after an optional
// UTF-8 byte order mark and leading white space; ASCII STL and OBJ must be text.
bool content_matches(FileType type, std::string_view bytes);

// ---------------------------------------------------------------------------------------------
// Names and fields
// ---------------------------------------------------------------------------------------------

// Base name only; control characters and <>:"/\|?* removed; trimmed; trailing dots removed; at most
// 200 characters with the extension kept; then a name Windows would take for a device gets a leading
// underscore (see is_windows_reserved_name: "COM3 .x.stl" becomes "_COM3 .x.stl"). Empty when the
// name is not valid UTF-8 or does not keep an accepted extension with a non-empty stem. Applying it
// again changes nothing. Never used as anything but the last path component.
std::string sanitize_file_name(std::string_view raw);
// True when Windows would not treat `name` as an ordinary file in a folder: empty, "." or "..", not
// valid UTF-8, a trailing dot or space, a control character or one of <>:"/\|?*, or a device name.
// Windows opens a device whenever the part before the first dot, trailing spaces removed, is CON,
// PRN, AUX, NUL, CONIN$, CONOUT$, COM0-9 or LPT0-9 in any case, or COM or LPT followed by a
// superscript one, two or three (U+00B9, U+00B2, U+00B3), whatever follows: "COM3 .x.stl.part" is
// the serial port COM3.
bool is_windows_reserved_name(std::string_view name);
// A name the station may save a received file under as it is: sanitize_file_name leaves it unchanged
// and it is not reserved.
bool is_clean_file_name(std::string_view name);
// The name a received file is saved under when its own cannot be used: "model.<type>".
std::string fallback_file_name(FileType type);
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
// The value of the last `name:` field in raw HTTP response header lines, matched without regard to
// case and trimmed; empty when absent. Used for X-Content-SHA256 on a file download.
std::string header_value(std::string_view headers, std::string_view name);

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
    double        ttl_hours    = 0; // the container accepts fractions of an hour
    // DROP_PUBLIC_URL as the container reports it, normalized (see normalize_public_url); empty when
    // the container has none, reports null, or reports a value that is not an acceptable link base.
    std::string   public_url;
};
// Only a protocol-1 answer with every required field present and well formed. `publicUrl` is
// optional so a container from before the invite link still connects.
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
    bool        loopback = false; // localhost, 127.0.0.0/8, [::1], or the unspecified 0.0.0.0 and [::]
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
// The private IPv4 addresses (10/8, 172.16/12, 192.168/16) of up adapters that other devices on
// the LAN could open, best first: an adapter with a gateway, then a physical one. Never loopback,
// link-local, multicast or public addresses; each address once.
std::vector<std::string> lan_ipv4_candidates(const std::vector<AdapterAddress> &addresses);
// The first of lan_ipv4_candidates, or empty when there is none.
std::string choose_lan_ipv4(const std::vector<AdapterAddress> &addresses);

// ---------------------------------------------------------------------------------------------
// Invite link
// ---------------------------------------------------------------------------------------------

// An http or https URL with a host, an optional port and an optional path, without a user name,
// query or fragment, returned without a trailing slash. Empty when the text is not such a URL.
std::string normalize_public_url(std::string_view url);

enum class InviteSource {
    None,        // no usable base: the address is this computer and it has no private LAN IPv4
    PublicUrl,   // the container's DROP_PUBLIC_URL
    Configured,  // the configured address, which is not a loopback address
    LanAddress   // http(s)://<private LAN IPv4>:<port> of this computer
};

struct InviteBase
{
    InviteSource             source = InviteSource::None;
    std::string              base;    // no trailing slash; empty with InviteSource::None
    std::vector<std::string> choices; // with LanAddress: every candidate, for the picker
    std::string              chosen;  // with LanAddress: the candidate in `base`
};
// The base of the invite link, in the brief's order of preference: the container's public URL, then
// the configured address when it is not a loopback or unspecified address, then this computer's
// private LAN IPv4 with the configured scheme and port. `preferred_ipv4` (the user's pick) wins
// when it is still one of `lan_candidates`; otherwise the first candidate is used.
InviteBase choose_invite_base(const std::string &public_url, const BaseAddress &configured,
                              const std::vector<std::string> &lan_candidates, const std::string &preferred_ipv4);
// <base>/#code=<code>, the code percent-encoded like encodeURIComponent. The code travels in the
// fragment, so it reaches neither server logs nor a Referer header. Empty when either part is unusable.
std::string invite_link(const std::string &base, const std::string &drop_code);

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
