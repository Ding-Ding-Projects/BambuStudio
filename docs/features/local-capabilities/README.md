# Local browser capability boundary

**Status: transport implementation only, not enabled in the installed application.**
The companion's existing MCP service remains unchanged. No production code starts
`LocalCapabilityHost`; no production native adapters or native pairing surface are
registered yet. Passing boundary tests does not establish desktop/browser integration.

## Boundary and protocol

The separate opt-in host binds an ephemeral port on literal `127.0.0.1`. It accepts
the exact listener Host header and loopback peer, never hostname aliases, forwarded
headers, query strings, redirects or arbitrary network addresses. Request logging
providers are removed. Responses are `no-store` and have no credential-bearing URLs.

The native owner must show the exact HTTPS origin and explicit capability grants in
its own trusted confirmation surface. Only that native gesture may call
`BeginPairing`. There is no HTTP or MCP operation to approve pairing. The generated
random offer is single use and expires after sixty seconds. The user transfers it
from the native private surface to the exact browser origin. Never put offers in URL
parameters, telemetry, screenshots, exports, history, clipboard automation or logs.

The browser sends `POST /v1/pair`, `Content-Type: application/json`, with exactly
`version` (integer 1) and `nonce` (string). Successful redemption returns version 1,
`bearer` and `expires`. The bearer must remain in browser memory, never storage or
service-worker caches. It expires after ten minutes. Native revocation, native
lifetime termination or a new native-approved pairing invalidates the prior session.

`POST /v1/invoke` requires `Authorization: Bearer ...` and exactly `version` (1),
`sequence` (positive integer), and `capability` (an allowlisted string). Sequence
starts at 1 and advances only for an accepted authorized invocation. There are no
arguments, paths, command lines, secrets, remote endpoints, instance selectors or
generic operation forwarding. The client must not automatically replay an invocation
after a lost response. Re-pair explicitly after an uncertain sequence.

| Capability | Intended native adapter |
| --- | --- |
| `school.state` | Read actual shared enabled state and chosen display name |
| `school.manage` | Open the actual native mode editor and native authentication flow |
| `vault.manage` | Open the actual native credential manager |
| `converter.manage` | Open the actual native converter with native file pickers |
| `ollama.manage` | Open the actual native local-model manager |

Management actions navigate native UI only. They do not unlock, read credentials,
execute a conversion, load a model, or launch a process. Native confirmation and
existing action validation still apply. There is no browser passkey implementation,
custom authentication protocol, raw credential endpoint, or cross-origin vault API.
The host requires real registered adapters and rejects unregistered grants.

Bodies are bounded to 4096 bytes, JSON depth to 3, headers to 8192 bytes/16 entries,
connections to 4, admitted bodies/invocations to one, and authenticated invocations
to 30 per minute. Duplicate and unknown JSON properties, compression, absent origin,
wrong host, unknown capabilities, wrong schema versions and replay are rejected.
Reading/invocation receives a ten-second linked cancellation deadline. Revocation
cancels in-flight adapter work. A noncooperative adapter must never be registered:
the host does not abandon that task and permit overlapping native side effects.

## Native integration still required

The current process separation is unresolved. `LocalCapabilityHost` is managed code;
the installed UI is native C++. A reviewed current-user-only broker must convey a
native-approved pairing decision to the managed host and dispatch only the typed
operations above. It must bind the selected native process and its lifetime, marshal
onto the GUI thread, check cancellation and weak-window lifetime before dispatch,
and revoke on native close. Neither `DeviceWebBridge` nor generic MCP dispatch is
an approved substitute. Do not start this listener until that broker, genuine native
adapters, consent UI and independent review are complete.

The existing script/navigation bridge lacks a demonstrated reusable origin and
pre-parse byte boundary for these new operations. This is an incomplete boundary
assessment, not a claim of an exploitable route through the complete navigation chain.

## Browser permission and failures

Cross-origin CORS permission does not grant Local Network Access permission. Chrome
and Edge require browser-controlled consent for applicable public-to-loopback
requests. The browser frontend must initiate its connection from an explicit user
gesture, distinguish unavailable local service from denied browser permission where
the browser exposes that distinction, and retain an honest unavailable state. Never
disable browser protections, install permissive policy or use wildcard CORS.

Primary references checked 2026-10-05:

- [Chrome Local Network Access](https://developer.chrome.com/blog/local-network-access)
- [Microsoft Edge Local Network Access](https://learn.microsoft.com/en-us/deployedge/ms-edge-local-network-access)

Browser versions differ. Permission grants are independent of native pairing and
neither implies the other. Browser runtime acceptance, native runtime acceptance,
packaging, accessibility, localized consent and captures are still unverified.

## Verification

Run `dotnet run --project tests/local_capabilities/LocalCapabilities.Tests.csproj`.
The executable compiles the production boundary directly with no external test
packages. It exercises session expiry, exact origins, grant isolation, nonce reuse,
sequence replay, bounded rate/concurrency, revocation cancellation, genuine loopback
HTTP, Host rebinding, missing/foreign origin, oversized/duplicate/unknown requests,
schema versions and CORS preflight. Its counting adapter is explicitly synthetic;
it proves transport behavior only and returns no user state.
