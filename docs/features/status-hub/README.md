# Native Status Hub

The native Status Hub panel displays the current connection state and delivery
evidence. It is opened from the application's local tools workspace. Its search
field uses the shared plain-text search and anchored regular-expression builder.
All status copy goes through the application's language service.

## Configuration and privacy

There is no bundled service endpoint. The trusted launch environment supplies
`STATUS_HUB_URL` as an HTTPS origin and `AGENT_INGEST_TOKEN` as its enrollment
credential. Do not put credentials in a command line, project file or source.
Changes require an application restart. Only default HTTPS port 443 is accepted;
URL credentials, paths, queries, fragments and redirects are rejected. The panel
reveals configuration presence, never its value.

The shipped C++ client owns request composition, authentication, bounded response
parsing, rate limits, retries, session keys and reply cursors. Its canonical local
emission preflight is required before a write. The consuming application does not
copy a private dictionary or preflight implementation. When the canonical
preflight or enrollment is absent, delivery fails closed while application work
continues. The environment configuration for the client is documented in
`libs/status-hub-client/README.md`.

The service sends the fixed application title, running/waiting status and machine
label to the configured service. It does not send open model names or paths,
document contents, personal-vocabulary data, logs or response bodies. An explicit
development-host repository path enables the client's measured worktree inventory;
the installed application leaves that path empty and labels inventory as not
applicable. Incomplete inventory blocks an update rather than claiming completeness.

## Lifecycle and evidence

One worker owns one client and one session key for the lifetime of the application.
Startup and explicit checkpoints request an update; a quiet heartbeat runs every
60 seconds. Requests coalesce into one pending slot. The client itself enforces
its request ceiling. Each transport operation uses a 1,200 ms phase timeout with
a bounded response-read deadline and maximum 512 KiB response. A terminal waiting
update has a 1,200 ms client deadline. Shutdown wakes the worker and joins after
in-flight bounded operations and the terminal attempt finish. Synchronous platform
phases may each consume their own timeout, so the client timeout is not a promise
of total shutdown wall time. No detached worker retains a GUI pointer.

Retry preserves the same client and session key. It never invents successful
enrollment after HTTP 401. Missing configuration makes no transport request.
Success requires an actual 2xx update response. Local no-ops, coalescing, health
checks and queued requests do not count as acceptance. A later failure does not
erase the historical last-accepted timestamp. Raw server text is never rendered.

After an accepted update, the shipped client polls replies using its own cursor.
The panel reports receipt counts only. It does not apply replies to the
application or claim that a reply reached an assistant. Interactive workflow
question publication is not yet integrated with a product workflow.

## Verification

`tests/native_status` builds the real C++ core, WinHTTP transport and service.
Its runtime test covers safe origin configuration, absent configuration and
enrollment, idempotent startup/shutdown, offline retry, wakeable shutdown and zero
transport calls for unenrolled writes. It does not demonstrate live server
acceptance, GUI rendering, owner enrollment, reply execution or installer behavior.

```powershell
cmake -S tests/native_status -B build-native-status -G Ninja
cmake --build build-native-status
ctest --test-dir build-native-status --output-on-failure
```

Live delivery is unverified without an enrolled credential and a compatible
server. GUI compile and capture evidence must be recorded against the integrated
application build. There is no fabricated session or substituted screenshot.
