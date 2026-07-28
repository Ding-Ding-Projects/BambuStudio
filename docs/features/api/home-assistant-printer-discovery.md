# Home Assistant printer-discovery API

**Owner:** `HomeAssistantSharingService`
(`src/slic3r/GUI/HomeAssistantSharingService.{hpp,cpp}`).

**Caller:** the Home Assistant discovery flow in
[`Ding-Ding-Projects/ha-bambulab`](https://github.com/Ding-Ding-Projects/ha-bambulab).

> [!IMPORTANT]
> This contract is implemented in the current working tree. Its focused Windows Release targets,
> 30-case/267-assertion native suite, five focused CTest entries, and production cross-host
> discovery/fetch/goodbye probe are complete. The full Release GUI build and English native
> 720×760/520×480 Smart Home clipping review are also complete. Native bilingual capture, the real
> Home Assistant confirmation flow, physical-printer success, hosted CI/release verdict, and
> remote-publication proof are recorded separately in
> [issue #16](https://github.com/Ding-Ding-Projects/BambuStudio/issues/16); they are not implied by
> the local evidence below.

## Lifecycle and discovery

The endpoint does not exist at startup. It is created only when the user enables
**File ▸ Smart home… ▸ Share for discovery for 5 minutes (no Home Assistant token)** and at least
one accessible printer has a serial number, LAN address, and access code.

Bambu Studio selects the usable RFC1918 or shared-address-space IPv4 interface on the ordinary
default route and binds TCP to that address on an operating-system-selected unprivileged port.

The default-route check is deliberate: a host-only Hyper-V, WSL, or container adapter can own the
kernel's preferred multicast route while remaining unreachable from the physical LAN. Advertising
that virtual address would make local tests pass but prevent Home Assistant on another computer
from fetching the offer.

The service resolves the selected interface's real prefix length. It answers mDNS only for usable
private/shared sender addresses on that exact link and rejects network, broadcast, public,
loopback, and link-local sources before constructing a response.

It advertises:

| DNS-SD item | Value |
| --- | --- |
| Service type | `_bambu-slicer._tcp.local.` |
| PTR | A fresh per-window service instance |
| SRV | The generated `.local` host and selected TCP port |
| TXT `pairing` | Fresh URL-safe bearer capability for this sharing window |
| TXT `name` | Generic `Bambu Studio` (the Windows host name is not published) |
| A | The selected LAN IPv4 address |
| Normal TTL | 120 seconds |

The responder answers PTR, SRV, TXT, A, and ANY questions for its records. It also sends unsolicited
announcements while sharing remains active. Turning sharing off, closing the dialog, or reaching the
five-minute automatic expiry sends the same records with TTL zero, closes HTTP sessions, and
destroys the capability.

mDNS admission uses a global burst of eight queries with one token refilled per second before
response allocation. Replies are paced through one in-flight send, a queue of at most eight
responses, endpoint plus transaction-ID coalescing, and a 50 ms interval. A multicast query flood
therefore cannot create an unbounded chain of allocations or async sends. Stop and restart cancel
the pacing timer and discard the old queue.

## Request

```http
GET /bambustudio/printers HTTP/1.1
Host: <advertised-ip>:<advertised-port>
Authorization: Bearer <pairing TXT value>
```

Requirements:

- method is exactly `GET`;
- target is exactly `/bambustudio/printers`;
- exactly one `Authorization` header is present;
- its scheme and value are exactly `Bearer <pairing-token>`; and
- the request has no body.

The bearer comparison is constant-time. Authentication happens before Bambu Studio asks the
printer-data supplier for its payload, so an unauthenticated request cannot invoke that supplier or
receive printer data.

## Successful response

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Cache-Control: no-store
Connection: close
X-Content-Type-Options: nosniff
```

```json
{
  "printers": [
    {
      "serial": "01P00A000000000",
      "host": "192.0.2.44",
      "access_code": "replace-with-a-local-test-value",
      "name": "Optional display name"
    }
  ]
}
```

`serial`, `host`, and `access_code` are required non-empty strings. `name` is optional. Unknown
fields supplied internally are removed before the response.

## Limits and status codes

| Contract | Limit or result |
| --- | --- |
| Request header block | 8 KiB |
| Request target | 256 bytes |
| Request timeout | 5 seconds |
| Concurrent sessions | 16 |
| Authorized response rate | Burst 4, refill 1 request/second |
| Response body | 64 KiB |
| Printer count | First 32 entries |
| Each printer field | 256 bytes |
| Malformed HTTP | `400 Bad Request` |
| Missing, duplicate, or invalid Authorization | `401 Unauthorized` with `WWW-Authenticate: Bearer` |
| Authorized rate limit exhausted | `429 Too Many Requests` with `Retry-After: 1` |
| Wrong path | `404 Not Found` |
| Wrong method | `405 Method Not Allowed` with `Allow: GET` |
| Missing, malformed, or over-limit supplied printer data | `503 Service Unavailable` |

Every response carries `Cache-Control: no-store`, `Connection: close`, and
`X-Content-Type-Options: nosniff`. Error bodies are fixed text and never echo the request, pairing
token, supplier exception, or printer data.

The response budget is checked before the supplier runs. The first valid sanitized payload is
cached for the life of the sharing window, so subsequent authorized requests reuse the bounded
snapshot instead of repeatedly asking the UI-side supplier to serialize credentials. Invalid
supplier output is never cached.

## Security boundary

This is a capability-protected **clear-text LAN** endpoint, not HTTPS. The pairing token must be
discoverable by Home Assistant, so it is present in the mDNS TXT record and is visible to other
devices on the same broadcast domain. Anyone who observes it can fetch the printer payload without
Home Assistant's confirmation screen until the user disables sharing or the five-minute timer
expires. Printer access codes can control printers on the LAN.

Consequently:

- enable sharing only on a trusted LAN and only for the shortest practical time;
- confirm the intended Home Assistant discovery card, then turn sharing off instead of waiting for
  the five-minute expiry;
- do not forward the port through a router, proxy it to another network, or expose it to a WAN;
- do not save pairing tokens or response bodies in Postman collections, environments, logs,
  screenshots, issues, or source control; and
- treat an unexpected discovery request as a reason to stop sharing and investigate the LAN.

The service logs only generic lifecycle/parser failures. It deliberately never logs the pairing
token, request Authorization header, printer payload, access codes, or supplier exception details.

## Postman

Import either:

- the focused
  [category collection](postman/home-assistant-printer-discovery.postman_collection.json); or
- the [Bambu Studio master collection](../../postman/BambuStudio.postman_collection.json).

Keep the collection's TEST-NET defaults. In a local, disposable Postman environment, set
`share_host`, `share_port`, and `pairing_token` from the active mDNS record. The authorized request
checks the response status, no-store header, JSON shape, count limit, required string fields, and
per-field size limit without printing response data.

Postman is a contract-inspection aid only. The completed cross-host transport pass uses the
production C++ probe; final acceptance also requires a real Home Assistant discovery flow because a
manually supplied Postman token does not prove the confirmation card.

## Verification matrix

The Windows SDK 10.0.26100.0 Release target was built and run on 2026-07-28.
`home_assistant_tests` passed **30 test cases / 267 assertions**, and the focused CTest selection
passed **5/5** entries. Coverage includes:

- URL policy for HTTPS, loopback HTTP, clear-text LAN HTTP, malformed URLs, and unsupported schemes;
- bounded worker cancellation, once-only UI completion, exception containment, and shutdown
  callbacks;
- bounded entity-domain validation, 4 MiB state-body transport, 512 backend results, 256 rendered
  rows, and persisted-list scan budgets;
- generation-safe light snapshots, batching/capping, supersession, shutdown recovery, retention of
  the recovery scene when both restore attempts fail, and deletion when cancellation lands after
  scene creation but before any flash;
- four-wide Path B printer-import waves, bounding 32 unreachable printers to at most eight
  30-second timeout waves instead of serial waits;
- fresh URL-safe pairing tokens;
- stable structured sharing-start failures;
- rejection of loopback/public advertised addresses and of off-link, network, broadcast, public,
  and link-local mDNS senders;
- missing, wrong, and duplicate Authorization rejection before supplier invocation;
- authorized canonical payload and no-store headers;
- exact method and target behavior;
- malformed, oversized, and over-count payload handling;
- authenticated token-bucket refill, supplier caching, and invalid-supplier budget consumption;
- a 512-query mDNS pacing/coalescing flood plus globally bounded pre-allocation admission;
- default-route interface selection and real interface-prefix lookup rather than a host-only
  multicast-preferred adapter; and
- idempotent restart plus serialized concurrent stop/join cleanup.

The complete `BambuStudio_app_gui` Release target also exited 0 after 3,387 seconds, followed by an
8.3-second no-change rebuild. Native clipping review exposed and fixed a text-action sizing defect;
the focused `SmartHomeDialog.cpp` rebuild/link exited 0 in 141 seconds and its no-change build in
8.0 seconds. The later nonvisual import-scheduling and cancellation-cleanup changes compiled and
linked in 214.808 seconds, followed by an 8.544-second no-change build. The final 151,299,584-byte DLL
is timestamped `2026-07-28 08:15:46 -04:00`, SHA-256
`41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`. English 720×760 and
520×480 corrected captures were recaptured from that exact final DLL and are indexed under
[`docs/screenshots/smart-home`](../../screenshots/smart-home/); native bilingual capture remains
pending.

`home_assistant_sharing_probe` supplies a synthetic TEST-NET printer solely for cross-host
transport verification. A second LAN host observed PTR/SRV/TXT/A records, completed one
authenticated bounded fetch without printing the pairing value or payload, and observed the
zero-TTL goodbye. Its result must not be described as a successful real-printer import or a real
Home Assistant confirmation-card pass.
