# Status Hub C++ client

This directory contains the supplied C++17 client implementation. Transport,
protocol limits, retries, private-emission preflight and session cursor behavior
remain in that implementation rather than a second application HTTP client.

Consumer changes are deliberately limited:

* The default base URL is empty. An owner-configured origin is required.
* The optional standalone example is disabled by default.
* WinHTTP redirect following is disabled before request transmission.
* WinHTTP response reading has an elapsed deadline in addition to phase timeouts.

The copy does not include the private dictionary, emission validator, credentials
or machine configuration. At runtime the client uses `STATUS_HUB_NODE`,
`STATUS_HUB_PRIVATE_EMISSION_GUARD` and `STATUS_HUB_PRIVATE_EMISSION_PREFLIGHT`
to resolve the separately installed canonical preflight when their defaults are
not available. Writes fail closed when those required components are unavailable.
Never log the subprocess input or output: they carry private validation material.

The application's focused test entry point is `tests/native_status`. Optional
upstream example/contract-test targets are not part of this vendored subset and
must remain disabled. The native application links the `status_hub_client` target.
