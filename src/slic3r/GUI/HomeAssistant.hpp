#ifndef slic3r_GUI_HomeAssistant_hpp_
#define slic3r_GUI_HomeAssistant_hpp_

#include "HomeAssistantTransportPolicy.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <wx/string.h>

namespace Slic3r { namespace GUI { namespace HomeAssistant {

// Minimal Home Assistant REST client. Connection comes from the config
// (`ha_url`, e.g. https://homeassistant.local:8123, and `ha_token`, a
// long-lived access token entered in the Smart home dialog and stored in
// BambuStudio.conf — the config-export secrets warning covers it). Calls are
// asynchronous (an owned, bounded worker dispatcher + curl via Slic3r::Http).
// Background conveniences remain log-only; user-initiated printer handover
// reports its result.

struct Entity
{
    std::string entity_id;   // e.g. media_player.kitchen, light.desk
    std::string friendly_name;
    std::string state;
};

enum class EntityFetchErrorCode : std::uint8_t
{
    None,
    ShuttingDown,
    NotConfigured,
    InsecureTransport,
    InvalidFilter,
    WorkerUnavailable,
    QueueFull,
    RequestSetupFailed,
    TransportError,
    HttpStatus,
    ResponseTooLarge,
    InvalidResponse,
};

struct EntityQuery
{
    // One to four lowercase Home Assistant domains, without the trailing dot.
    // An empty list is rejected so an accidental request cannot transfer every
    // state in a large Home Assistant installation to the UI.
    std::vector<std::string> domains;
};

struct EntityFetchResult
{
    std::vector<Entity>  entities;
    EntityFetchErrorCode error_code = EntityFetchErrorCode::None;
    std::uint16_t        http_status = 0;
    bool                 truncated = false;

    explicit operator bool() const noexcept
    {
        return error_code == EntityFetchErrorCode::None;
    }
};

using EntityFetchCallback = std::function<void(EntityFetchResult)>;

bool configured();

// Credentials may travel over HTTPS, or over HTTP only when Home Assistant is
// on this machine (localhost / an IPv4 loopback address). This is intentionally
// checked inside every token-bearing request, not only by the dialog, so future
// callers cannot accidentally send the HA token or printer access codes over
// clear-text LAN HTTP.
CredentialTransportSafety credential_transport_safety();

// GET /api/states, filtering and bounding the requested domains on the worker
// before the result crosses to the UI. A non-empty callback is dispatched
// exactly once through the UI event queue while that queue is available.
void list_entities(EntityQuery query, EntityFetchCallback done);

// POST /api/services/<domain>/<service> with a JSON body.
void call_service(const std::string &domain, const std::string &service,
                  const std::string &json_body);

// Media player conveniences (rich controls in the Smart home dialog).
void media_command(const std::string &entity_id, const std::string &service); // media_play_pause, media_next_track, ...
void media_volume(const std::string &entity_id, double level_0_1);

// Speak `line` on every configured announcement speaker (`ha_speakers`,
// semicolon-separated media_player entity ids) via tts.speak / tts.google_
// translate_say fallback. No-op when unconfigured.
void speak_on_speakers(const wxString &line);

// A printer this app knows about, in the shape the Home Assistant `bambu_lab`
// integration needs to add it in LAN mode.
struct PrinterHandover
{
    std::string serial;
    std::string host;        // LAN address of the printer
    std::string access_code; // LAN access code — a credential; never log it
    std::string name;        // optional friendly name
};

// Hand `printers` to Home Assistant's bambu_lab integration, one
// `bambu_lab.add_printer` service call each, so the user never retypes a serial
// or an access code. Requires the fork that provides that service:
// https://github.com/Ding-Ding-Projects/ha-bambulab
//
// Unlike the fire-and-forget calls above this reports back, because it is a
// user-initiated action: silently doing nothing is indistinguishable from
// success and worse than an honest failure. A non-empty `done` fires on the UI
// thread while its event queue remains available, with the number accepted by
// Home Assistant and a per-printer error list (empty on full success).
// Scheduling and consumer exceptions are contained. A successful idempotent
// request may refer to a printer that was already configured.
// At most 32 printers are processed by one backend-owned import worker; a
// second batch is rejected until the active batch finishes.
//
// NOTE: this path needs a long-lived Home Assistant token. The token-free
// alternative is discovery — see SlicerShare, where Home Assistant finds this
// app instead of this app authenticating into Home Assistant.
void add_printers(const std::vector<PrinterHandover> &printers,
                  std::function<void(int /*processed*/, std::vector<std::string> /*errors*/)> done);

// Light actions ("Philips Hue and friends" — any HA light entity).
// Flash the configured lights (`ha_lights`) in a colour; used by the
// narrator hooks (error -> red flash, finish -> green pulse) when the
// corresponding toggles are on.
void flash_lights(int r, int g, int b, int flashes = 3);

// Stops accepting work, cancels active HTTP transfers through their progress
// callbacks, restores any active light-alert transaction, and joins owned
// workers. Entity queries that were accepted still attempt exactly one typed
// `ShuttingDown` completion through the UI queue while it remains available;
// late printer-import/convenience callbacks are suppressed during teardown.
// Call before wx/AppConfig teardown.
void shutdown() noexcept;

} } } // namespace Slic3r::GUI::HomeAssistant

#endif // slic3r_GUI_HomeAssistant_hpp_
