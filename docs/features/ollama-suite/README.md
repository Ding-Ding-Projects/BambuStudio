# Local Ollama suite

This module adds a native, application-owned entry point for local Ollama model inspection and streamed chat. It is an implementation in progress, not a declaration that the entire local-model feature contract is complete.

The host calls `Slic3r::GUI::show_ollama_suite(parent, state_root)` from `OllamaSuite/OllamaSuiteDialog.hpp`. `state_root` is the application's fixed private data directory. The dialog creates its own `ollama-suite` child directory and never derives storage identity from a display name or project file.

## Implemented boundaries

- Fixed local API destination `http://127.0.0.1:11434`, no proxy discovery, ambient headers, credentials, redirects or arbitrary endpoints.
- Allowlisted version, installed/running models, show, pull, delete, copy, chat and generation request construction. Unsupported fields and oversized payloads fail validation.
- Incremental NDJSON framing with per-record, transfer, duration and cancellation bounds. An incomplete response never becomes successful merely because the connection ended.
- Model capabilities and context are read from `/api/show`. Missing metadata is unknown. Remote-model metadata blocks the local-chat path.
- Hardware estimates require measured evidence and explicit context-memory overhead. No capability, size or memory requirement is guessed from a name. Current platform detection supplies RAM and architecture. The suite does not assume the application-data drive is Ollama's configured model destination; model-destination, GPU/backend and context-overhead evidence remain unavailable.
- Official HTML catalog traversal follows allowlisted family/tag/page links and records a SHA-256 of every received page. Source traversal and authoritative completeness are separate facts. The current HTML adapter has no verified total-count contract and therefore does not declare an exhaustive catalog or replace a verified cache.
- Exact variant transfer sizes come from schema-2 manifests on the official registry, including each layer and config object. A manifest with no local weight layer is rejected. This metadata operation never downloads model blobs.
- Persistent pull items use separate atomic files and bounded pages. Interrupted items become interrupted rather than successful. Retry preserves the exact model tag. The batch engine writes a separate reviewed plan, supports one to four workers, rechecks registry identity and destination capacity before transfer, reconciles the installed manifest, and preserves per-item partial outcomes. The UI start action remains disabled until the local service's configured model destination can be verified.
- Local chat persistence strips image bytes, thinking and tool-call fields. Ordinary exports omit all arbitrary text and image bytes because arbitrary model/user text cannot be proven free of credentials or private paths.
- Registered launch profiles use typed arguments, verified executable selection, restricted environment fields, preflight, snapshots and rollback. The native Windows adapter uses semantic file/folder pickers, signed-or-reviewed executable identity, retained file and directory locks, suspended process creation inside an owned kill-on-close job, owned-listener health checks, and a durable typed rollback journal. No shell command or inherited environment is accepted. The native adapter allows only the llama.cpp server profile; the terminal-chat profile directs users to the suite's documented local HTTP chat. Unknown hardware evidence blocks launch. Process and picker behavior still need real packaged verification.

## Native surface

The dialog uses the existing Material Design 3 caption, shared browser-style `TabStrip`, `SearchField` with its anchored regex builder, `ListBox`, buttons, labels, multiline fields and scroll containers. Model list presentation is paged at 100 rows. Network work runs outside the UI thread. Closing the dialog cancels its owned request before destroying callback state.

The tabs are Models, Chat, Batch pulls, Launch profiles and Troubleshooting. Chat includes guided parameter presets and validated advanced numeric inputs, transient PNG/JPEG attachment selection after verified vision capability, local session browsing/search, rename, confirmation-protected deletion, and redacted export. Disabled controls state their precise unmet condition. No model is downloaded, installed or launched during development verification.

## Incomplete delivery requirements

The following remain required before this feature can be claimed complete:

- Authoritative exhaustive catalog totals and immutable catalog revision support, complete guided filters, and full tag capability metadata before installation.
- Verified GPU/backend detection, model-destination discovery and context-memory sizing.
- Full native batch review/start integration and proof of the runtime's configured model destination. Core concurrency, preflight and reconciliation require real local-service verification.
- History searches beyond the current bounded page, per-session system-prompt editing, retry/regenerate controls, additional documented parameters and complete localized copy.
- Packaged Ollama installation/start recovery, native process containment/readiness evidence and cancellation/crash-recovery evidence.
- Native compilation, packaged runtime interactions, accessibility, language/scale matrix and genuine screen-capture evidence.

## Verification

The standalone C++17 core tests cover request allowlisting, malformed/oversized/deep responses, streaming fragments, attachment capability checks, conservative fit outcomes, full pagination and missing-page negative regressions, exact registry sizes, durable interrupted pulls and redacted history. Launch-profile tests cover typed preflight and rollback. Batch tests cover preflight, bounded concurrency, cancellation, installed reconciliation, partial outcomes and consumed plans. Native helper tests cover argument round-tripping, snapshot validation and the supported-profile boundary. They do not establish native UI, runtime installation, actual child containment, real model execution or packaged behavior.

Build the focused tests with `cmake -S tests/ollama_suite -B <temporary-build-directory>`, then build and run CTest in that directory. The ordinary application build additionally compiles `OllamaCore.cpp`, `PullBatch.cpp`, `LaunchProfiles.cpp`, `NativeLaunchAdapter.cpp`, `OllamaClient.cpp` and `OllamaSuiteDialog.cpp`, using the existing curl and OpenSSL targets. The Windows native adapter links bcrypt, wintrust, ole32, shell32, uuid, ws2_32, winhttp, iphlpapi, user32 and version.

## Official sources

The implementation was checked against the [Ollama documentation index](https://docs.ollama.com/llms.txt), [local model listing](https://docs.ollama.com/api/tags), [chat endpoint](https://docs.ollama.com/api/chat), [pull endpoint](https://docs.ollama.com/api/pull), [official model catalog](https://ollama.com/library), its model-specific tag listings, and the [official registry manifest client](https://github.com/ollama/ollama/blob/main/server/images.go). The catalog adapter is deliberately version-sensitive and fails closed when its source structure or completeness evidence changes.

## API collection

No public HTTP API is exposed by this module. A Postman collection is not applicable; this module is a bounded client of Ollama's documented local API.
