# Smart home: printer handover, TTS narrator, and alert lights

**Surface:** `File ▸ Smart home…` (`SmartHomeDialog`).

**Runtime modules:** `HomeAssistant`, `HomeAssistantTransportPolicy`,
`HomeAssistantSharingService`, and `TtsNarrator` under `src/slic3r/GUI/`.

> [!IMPORTANT]
> The printer-handover implementation and focused Windows Release targets are complete in the
> current working tree. The 30-case/267-assertion native suite and all five focused CTest entries
> pass; the full Release GUI build and English native 720×760/520×480 clipping review also pass.
> Native bilingual capture, live Home Assistant runtime paths, physical-printer success, hosted
> CI/release verdict, and remote-publication proof are recorded separately in
> [issue #16](https://github.com/Ding-Ding-Projects/BambuStudio/issues/16). This section documents
> the implemented and locally verified boundary; it does not claim those live or hosted results.

## Printer selection

Both handover paths collect the printers Bambu Studio can currently access from its local and
signed-in machine maps. A printer is included only when all of these values are available:

- a non-empty serial number;
- current LAN address;
- current LAN access code; and
- access rights in the active Bambu Studio session.

Duplicate serial numbers are removed. The local-discovery record wins when the same printer also
appears in the signed-in account map because it normally has the freshest LAN address. The payload
contains `serial`, `host`, `access_code`, and the optional display `name`.

The dialog says plainly that an access code is a credential before either action. Sharing is never
enabled merely by opening the dialog.

## Path B: add with the configured Home Assistant token

**Add my printers to Home Assistant** is an explicit, user-initiated transfer:

1. Enter the Home Assistant base URL and a long-lived access token.
2. Select **Add my printers to Home Assistant**.
3. Review the credential disclosure. The confirmation defaults to **No**.
4. If confirmed, Bambu Studio calls
   `POST /api/services/bambu_lab/add_printer` once per accessible printer with:

   ```json
   {
     "serial": "printer serial",
     "host": "printer LAN address",
     "access_code": "printer LAN access code",
     "name": "optional printer name"
   }
   ```

The credential confirmation uses the shared responsive message-dialog shell. Its optional
“don't show again” text lives in a separate wrapping footer row, and the action buttons stack when
the available work-area width cannot hold one row. The shared behavior has a passing static
clipping contract. English Smart Home review at 720×760 and the declared 520×480 minimum also found
and fixed a neighboring native defect: `make_responsive_action` had squeezed text buttons such as
**Close** and the media actions into 44-DIP widths instead of using 44 DIP as their minimum height.
Native bilingual confirmation capture remains pending.

The companion [`Ding-Ding-Projects/ha-bambulab`](https://github.com/Ding-Ding-Projects/ha-bambulab)
integration supplies that service. A cold Home Assistant installation needs `bambu_lab:` in
`configuration.yaml` so the service is registered before any printer has been configured.

The action is asynchronous. The button is disabled while requests are in flight, and progress,
complete success, partial success, or failure is reported with a corner notification (with the
dialog status as a fallback). The count says requests were **processed**, because an idempotent 2xx
response may mean the printer was already configured rather than newly created. A failure
identifies the printer serial and HTTP status but does not echo the response body, which may contain
credentials. One import owns a single worker, a second import is rejected until it finishes, and one
request processes at most 32 deduplicated printers. Within that single-flight import, requests run
in four-wide waves. A full batch of 32 unreachable printers therefore occupies at most eight
30-second timeout waves instead of 32 serial timeouts.

### Token transport policy

Every Home Assistant bearer-token request—not just printer handover—is rejected before opening a
network connection unless its base URL is:

- `https://…`; or
- `http://localhost…` or an explicit IPv4 loopback address.

Clear-text HTTP to a LAN hostname or LAN address, malformed URLs, unsupported schemes, and URLs
containing user information are rejected. This policy also covers entity listing, media controls,
TTS calls, and alert-light services. It prevents the Home Assistant bearer token and printer access
codes from being sent over clear-text remote HTTP. Credential-bearing requests also disable HTTP
redirect following, so a Home Assistant response cannot replay a bearer or printer payload to a
different or downgraded destination. Libcurl's verbose protocol trace is disabled for these
requests so Authorization headers do not enter stderr or captured diagnostics; an unhandled 3xx is
reported as an error. Responses are bounded to 1 MiB for service calls and 4 MiB for the entity
state list before parsing.

### Performance and shutdown bounds

Home Assistant work does not create an operating-system thread for every click:

- ordinary service calls use four owned workers and at most 64 waiting transactions;
- entity refresh uses one owned worker, while the dialog permits one active refresh and coalesces
  repeated Connect requests into one latest replay;
- entity requests accept at most four unique domains, parse at most 512 matching backend results,
  and render at most 256 matching rows through one Freeze/Set/Thaw update;
- volume changes are coalesced for 200 ms, so dragging the slider sends the latest value instead of
  one request per tick;
- speaker, light, and printer lists are deduplicated and capped at 32 before work is queued, and
  Path B runs printer requests in four-wide waves;
- persisted speaker/light lists inspect at most 256 semicolon-delimited segments and 64 KiB, reject
  values over 256 bytes, retain at most 32 unique active values, and do not silently rewrite
  hand-edited over-limit configuration; and
- app shutdown stops admission, cancels active curl transfers, suppresses late wx callbacks, drops
  only transactions that have not started, and joins every owned worker.

Alert-light work is one immutable, serialized transaction rather than unrelated queued calls:
snapshot, flash, restore, then a second idempotent restore. The generation-specific dynamic scene
is deleted only when at least one restore succeeds; if both fail, it remains available for manual
recovery. A target URL, authorization value, and generation never migrate to another transaction.
If shutdown arrives after scene creation but before the flash begins, there is nothing to restore,
so the unused scene is deleted immediately with the bounded shutdown timeout. A regression asserts
that exact create/delete sequence.
If shutdown interrupts a flash, recovery uses the same snapshot with two-second restore/delete
request bounds, limiting the recovery transport wait to about four seconds when Home Assistant is
unreachable.

## Path A: short-lived local discovery without a Home Assistant token

**Share for discovery for 5 minutes (no Home Assistant token)** creates a temporary capability on
the selected LAN interface:

- the toggle is **OFF by default** and its state is not persisted;
- enabling it snapshots the currently accessible printer data;
- a fresh URL-safe pairing token with more than 240 random bits is generated for that sharing
  window;
- Bambu Studio binds an authenticated HTTP endpoint to the RFC1918 or shared-address-space IPv4
  address on the ordinary default route and an operating-system-selected unprivileged port, rather
  than advertising a host-only WSL, Hyper-V, or container adapter that another LAN device cannot
  reach;
- it resolves the selected interface's real prefix length and answers mDNS only for usable sender
  addresses on that exact link, rejecting its network/broadcast addresses and public, loopback, or
  link-local senders;
- it advertises `_bambu-slicer._tcp.local.` through mDNS with `pairing` and `name` TXT records plus
  the matching PTR, SRV, and A records;
- Home Assistant fetches the printer payload from the exact
  `GET /bambustudio/printers` endpoint with
  `Authorization: Bearer <pairing-token>`; and
- Home Assistant still presents a discovery card and asks the user to confirm before importing.

The dialog automatically turns sharing off after five minutes. Turning the toggle off sooner,
closing the dialog, reaching that expiry, or destroying its service object stops HTTP serving,
closes active sessions, sends a zero-TTL mDNS goodbye, and discards the pairing token. Re-enabling
sharing starts a new five-minute window with a different token.

The endpoint is deliberately narrow. It allows only one exact method and path, requires exactly one
valid Authorization header, limits request headers and target length, times requests out after five
seconds, permits at most 16 concurrent sessions, caps the response at 64 KiB, caps the offer at 32
printers, and caps each field at 256 bytes. Responses use `Cache-Control: no-store`,
`X-Content-Type-Options: nosniff`, and `Connection: close`. Malformed or oversized supplier data
gets a generic `503` response. Authorized fetches share a burst-four, one-request-per-second token
bucket and return `429` with `Retry-After: 1` when it is exhausted. The first valid sanitized
snapshot is cached for the sharing window, so repeated authorized requests do not repeatedly
serialize printer credentials. Neither the pairing token nor printer payload is logged.

mDNS query admission uses a global burst of eight with one token refilled per second before any
response allocation. Replies are endpoint/transaction-ID deduplicated, queued to at most eight
pending packets with one send active, and paced 50 ms apart. Stop and restart cancel the old timer
and discard the old queue.

The complete HTTP contract and Postman artifacts are in
[Home Assistant printer-discovery API](../api/home-assistant-printer-discovery.md).

## Security and privacy

Printer LAN access codes are credentials. Both paths transfer them only after an explicit action,
but they have different trust boundaries:

- **Path B** sends both the Home Assistant bearer token and printer data over HTTPS, except that
  clear-text HTTP is permitted for a Home Assistant process on this same computer through a
  localhost name or IPv4 loopback address.
- **Path A** is a five-minute maximum, clear-text LAN handover. The pairing token is advertised in
  mDNS so Home Assistant can discover it; any device able to observe that broadcast domain can also
  obtain the token and request the payload without going through Home Assistant's confirmation
  screen. Use this path only on a trusted LAN and turn it off immediately after Home Assistant
  imports the printers instead of waiting for expiry.

Do not paste pairing tokens, Home Assistant tokens, printer access codes, endpoint responses, or
exported Postman environments into issues, logs, screenshots, or source control. The ordinary
`BambuStudio.conf` export warning remains applicable because it contains the configured Home
Assistant URL and long-lived token.

## Existing Home Assistant controls

- **Entity browser:** the shared `SearchField` (including the regex builder) filters
  `media_player.*` and `light.*` entities by entity ID, friendly name, and current state. Long
  friendly names remain horizontally reachable in the native list instead of being silently
  clipped at the dialog edge. The backend returns at most 512 filtered entities, the list renders
  at most 256 matching rows, and a status line asks the user to refine the search when more matches
  exist.
- **Media controls:** previous, play/pause, next, and volume operate on the selected media player.
- **Announcement speakers:** **Use as announcement speaker** appends the selected player to
  `ha_speakers`; the narrator and filament scanner can speak there. The dialog explains and
  enforces a 32-speaker limit.
- **Alert lights:** any selected `light.*` entity can flash red on printer errors or pulse green on
  print completion. The dialog explains and enforces a 32-light limit.
- **Stuck-colour guard:** before flashing real room lights, Bambu Studio snapshots them to
  a generation-specific `scene.bambustudio_light_restore_<generation>` and restores that exact
  scene about four seconds later. The restore is fired twice and is idempotent. If both attempts
  fail, the generation-specific scene remains available for manual recovery. Cancellation before
  the flash deletes an unused snapshot rather than leaving a scene that cannot aid recovery.

## TTS narrator

- The narrator is **OFF by default** (`narrator_enabled`).
- It uses a serialized queue: one utterance at a time, and a newer line in the same category
  replaces its queued predecessor.
- A 20-second per-category cooldown limits chatter. Error lines are not suppressed and retain the
  actual failure and recovery information at every funny level.
- It narrates printer state changes and printer error codes from the selected machine.
- Output is the local Windows SAPI voice plus configured Home Assistant announcement speakers via
  `tts.speak`.

## Failure modes

| Condition | Behavior |
| --- | --- |
| No Home Assistant URL or token | Path B does not open a connection and focuses the URL field. |
| Remote `http://` Home Assistant URL | All bearer-token operations are rejected locally. |
| No printer with access rights, LAN address, and access code | Neither handover starts; a warning notification explains what is missing. |
| User cancels the Path B disclosure | No printer request is sent. |
| Some Path B requests fail | Successful and failed counts are reported; failed serials remain visible for retry. |
| All 32 Path B requests reach their 30-second timeout | Four requests run per wave, bounding the batch to at most eight timeout waves. |
| No usable LAN IPv4 address, port, or mDNS socket | Path A turns its toggle back off and reports the concrete startup error. |
| Missing, wrong, or duplicate Path A Authorization header | The endpoint returns `401` without requesting printer data. |
| Authorized Path A request budget is exhausted | The endpoint returns `429` with `Retry-After: 1` before asking the data supplier. |
| Wrong method or path | The endpoint returns `405` or `404`, respectively. |
| More than 32 accessible printers | Both paths process the first 32 and state plainly that additional printers were not included. |
| More than 32 configured speakers or lights | The dialog keeps the first 32, reports the limit, and requires clearing an entry before another can be added. |
| Service queue is full or an import is already active | New work is rejected instead of creating another thread or silently growing memory use. |
| Shutdown begins after an alert-light scene is created but before the flash | The unused scene is deleted with the shutdown recovery timeout; no restore is needed. |
| Invalid or over-limit printer offer | The endpoint returns a generic `503` without reflecting sensitive data. |
| Dialog closes while sharing | HTTP and mDNS stop; a zero-TTL goodbye removes the advertisement. |
| Five-minute sharing lifetime ends | The toggle turns off, sessions close, a zero-TTL goodbye is sent, and a notification says access codes are no longer offered. |

## Verification status

On 2026-07-28, the Windows SDK 10.0.26100.0 Release build completed both focused targets.
`home_assistant_tests` passed **30 test cases / 267 assertions**, and CTest passed all five
`home_assistant_*` entries (the binary plus four security/performance contracts). The Cantonese
catalog check passed 718 translations, the static Pages/i18n/clipping suite passed 21/21, and the
browser Pages matrix passed all 156 combinations of 13 physical widths, four zoom levels, and three
language modes. Template assembly is synchronized.

The full `BambuStudio_app_gui` Release build exited 0 after **3,387 seconds**, and its first
no-change rebuild exited 0 in **8.3 seconds**. Native headless review then exposed the 44-DIP
text-action width defect described above. The focused `SmartHomeDialog.cpp` rebuild and DLL link
exited 0 after **141 seconds**; its no-change rebuild exited 0 in **8.0 seconds**. The subsequent
nonvisual printer-import and alert-light cleanup changes compiled and linked in **214.808 seconds**,
and the final no-change rebuild exited 0 in **8.544 seconds**. The final `BambuStudio.dll` is
**151,299,584 bytes**, timestamped **2026-07-28 08:15:46 -04:00**, with SHA-256
`41BB1BFC754E3184C5908E2145A93E3640D3866E59380F32EEFF7A76F418E972`.

The genuine Lowlevel MCP headless before/after evidence is:

- [`720×760 before`](../../screenshots/smart-home/dialog-720x760-before-text-action-fix.png) and
  [`720×760 after`](../../screenshots/smart-home/dialog-720x760-after-text-action-fix.png);
- [`520×480 before`](../../screenshots/smart-home/dialog-520x480-before-text-action-fix.png) and
  [`520×480 after`](../../screenshots/smart-home/dialog-520x480-after-text-action-fix.png); and
- [`520×480 corrected media actions`](../../screenshots/smart-home/dialog-520x480-media-actions-after-fix.png).

The corrected **Close** action is readable in primary captures recaptured from that exact final
`41BB1B…` DLL at both reviewed English sizes. The corrected media-action close-up is from the
preceding `EBF646…` DLL; the only later source changes are the nonvisual import-scheduling and
alert-light cleanup fixes, so that close-up exercises the same layout implementation. Native
bilingual capture remains pending.

On this GPU-less Mesa llvmpipe headless host, cold first-run launch took **37.427 seconds** and a
subsequent launch took **32.339 seconds**. These figures characterize this verification
environment; they are not production performance benchmarks. The app remained responsive with no
hang at about 523 MB after 13.74 minutes and about 549 MB after 6.26 minutes; the latter observation
included three incidental Version History windows.

The production `home_assistant_sharing_probe` was then observed from a second LAN host. That host
received the real PTR/SRV/TXT/A advertisement, used the advertised pairing value without printing
it, completed one authenticated fetch whose bounded JSON shape matched the synthetic TEST-NET
printer, and received the zero-TTL goodbye after shutdown. This pass exposed and fixed a real
interface-selection defect: multicast routing had selected a host-only WSL adapter; selection now
uses the ordinary default-route LAN address. The probe proves cross-host transport only, not a
physical-printer import or Home Assistant confirmation card.

Before issue #16 can be closed, complete all of the following:

- capture the real bilingual Smart home dialog through the repository's headless native-app
  harness;
- verify Path B end-to-end against a real Home Assistant with the companion integration;
- verify Path A's confirmation card inside a real Home Assistant instance;
- verify a real physical-printer success path when hardware is available; and
- record exact commit, test, screenshot, and hosted CI/release evidence without substituting a
  simulated printer for a real-hardware success claim.
