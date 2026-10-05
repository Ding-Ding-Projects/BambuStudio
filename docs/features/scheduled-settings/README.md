# Scheduled settings

Scheduled settings keep temporary effective values separate from saved preferences. The native settings panel edits rules through a typed registry and the same validation used for external responses.

## Time and precedence

Times are minute-resolution local wall-clock values in the selected Windows timezone. `system` tracks the current operating-system timezone. Native date/time pickers accept optional first and last **start dates**. Monday is bit 0 of the weekday mask; `127` means every day. Start times are inclusive and end times exclusive. Equal times mean the entire local day. An overnight interval belongs to its starting weekday and date, including its portion after midnight. Invalid dates, missing times, empty weekday selections, and inverted date bounds are rejected.

Daylight-saving gaps never match because those local minutes do not occur. Repeated local minutes match on both occurrences. Evaluation is stateless with respect to clock movement, so clock corrections and resume do not require replay. Unknown timezone IDs are reported by the native time resolver, never guessed.

Higher integer priority wins per setting. With equal priority the later array entry wins. A rule can override only its selected setting fields. Disabled or nonmatching rules contribute nothing. An empty schedule restores all base values. A schedule must not be used to overwrite permanent AppConfig preferences.

## Schema and storage

The version-1 document has exactly `version`, `timezone`, and `rules`. Maximum encoded size is 64 KiB, maximum nesting is 12, and at most 128 rules are accepted. Duplicate object keys, unknown fields, unknown versions, unsafe keys, wrong types, numeric overflow, unknown setting names, and out-of-range values reject the entire document. Failed imports leave the prior valid schedule intact. Version 1 is the initial format; unsupported future versions require an explicit migration rather than an automatic reset.

Each rule has a stable `id`, local `label`, `enabled`, `priority` (0 to 10000), `window`, typed `values`, and `source`. Date bounds are null or `[year, month, day]`. Window fields are `first`, `last`, `start`, `end`, and `weekdays`. Source fields are `kind` (0 local, 1 API, 2 Home Assistant), `url`, `entity`, `consent`, `privateNetwork`, `loopbackDevelopment`, and `refreshSeconds` (30 to 86400).

The owner supplies one transactional `ServiceHooks::save` implementation that writes the complete local document and records a local-history action. The service changes its active schedule only after that callback succeeds. Base values enter separately through `set_base`; effective changes leave through `ServiceHooks::apply`. The owner's School-mode presentation filter runs after scheduled overrides and never modifies base settings or the schedule. Private vocabulary content and credentials must never enter the registry or schedule document.

## External sources

The API contract is `{"version":1,"values":{"theme":"dark"}}`. Every key must exist in the shared typed registry **and** be selected in the rule. Every scheduleable setting uses this same contract. Partial, malformed, oversized, expired, unauthorized, or unavailable responses cannot become permanent preferences. Failed sources fall back to local base values or another matching rule. A valid response expires after one refresh interval.

Home Assistant uses a base HTTPS URL plus a `binary_sensor.name` or `input_boolean.name` entity. Its state response must identify that exact entity and contain `on` or `off`. `on` activates the local values in that rule; `off` removes its contribution. Missing credentials are unauthorized. Credentials are stored under the stable rule ID in Windows Credential Manager and excluded from schedules and history. The editor provides explicit store and clear controls; it never displays a stored credential.

Each rule requires explicit endpoint consent. The transport sends GET requests and uploads no preferences. It uses its own curl handle, inherits no proxy or application account headers, verifies TLS, rejects redirects, and bounds responses to 64 KiB, connection time to five seconds and total time to ten seconds. Builds lacking asynchronous DNS return unavailable instead of promising a false deadline. Every actual socket address is checked before connection, preventing DNS rebinding from switching a validated name to a forbidden address. Loopback HTTP is permitted only with its separate development consent. Private IPv4 networks require separate consent and are allowed only for Home Assistant. Link-local metadata addresses, multicast, reserved addresses, embedded IPv4 IPv6 forms and credentials in URLs are rejected. IPv6 is conservatively restricted to global unicast; private IPv6 Home Assistant endpoints are currently unsupported.

One worker processes requests serially. Activation refreshes immediately, normal refresh intervals are bounded, and failures retry no faster than the configured interval. Edits, explicit refreshes, and activation-set changes cancel the old generation. Completed old generations cannot overwrite newer settings. The worker never touches GUI controls; the UI thread drains results and applies effects on its next tick. Teardown cancels the worker and waits for the bounded transport to finish.

## Native integration

Compile `Schedule.cpp`, `Service.cpp`, and `Native.cpp` in the core target, linking curl and the existing Windows credential/socket libraries. Compile `GUI/ScheduledSettings/Panel.cpp` in the native GUI target. Embed `ScheduledSettingsUI::Panel(parent, service, translate, visible)` in the existing settings shell. The translation callback accepts English and Cantonese source strings; the visibility callback removes presentation settings suppressed by School mode. Own a `ScheduledSettingsUI::Driver` after the service and destroy it before the service. It evaluates local time every second, queues network work, and applies completed results without blocking HTTP on the UI thread.

Register every appearance setting actually consumed by the application, using the same names, types, ranges and choices as its live adapter. Call `set_base` on startup and after each deliberate user preference edit. Supply a nonpersistent application adapter for effective values; calling a setter that writes AppConfig would destroy recovery semantics. The native panel supplies searchable rules, bulk enable/disable/remove, native date/time controls, weekday selection, registry-driven typed values, source consent, timezone search and credential-vault controls. Each search field has the shared anchored regex builder. Tab navigation uses the existing Material component kit.

## Verification and remaining integration

`tests/scheduled_settings/run.ps1` builds and runs the standalone C++17 assertions using MSVC. `-Negative` compiles a disposable mutation that permits stale responses and requires the regression to detect it. No remote endpoint is contacted by these tests.

The implementation lane has not itself established packaged UI interaction, accessibility, narrow-layout, language-mode, live-appearance-consumer, or installer evidence. Those require shell registration, the owner's persistence/history and effective-value callbacks, a full native build, and genuine runtime captures. The existence of a panel or a passing core test is not evidence that those integration steps are finished.
