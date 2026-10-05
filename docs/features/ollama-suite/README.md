# Local Ollama suite

This module adds a native, application-owned entry point for local Ollama model inspection and streamed chat. It is an implementation in progress, not a declaration that the entire local-model feature contract is complete.

The host calls `Slic3r::GUI::show_ollama_suite(parent, state_root)` from `OllamaSuite/OllamaSuiteDialog.hpp`. `state_root` is the application's fixed private data directory. The dialog creates its own `ollama-suite` child directory and never derives storage identity from a display name or project file.

## Implemented boundaries

- Fixed local API destination `http://127.0.0.1:11434`, no proxy discovery, ambient headers, credentials, redirects or arbitrary endpoints.
- Allowlisted version, installed/running models, show, pull, delete, copy, chat and generation request construction. Unsupported fields and oversized payloads fail validation.
- Incremental NDJSON framing with per-record, transfer, duration and cancellation bounds. An incomplete response never becomes successful merely because the connection ended.
- Model capabilities and context are read from `/api/show`. Missing metadata is unknown. Remote-model metadata blocks the local-chat path.
- Hardware estimates require measured evidence and explicit context-memory overhead. No capability, size or memory requirement is guessed from a name. Current platform detection supplies RAM, architecture and destination space, while GPU/backend/context overhead remain unverified.
- Official HTML catalog traversal follows allowlisted family/tag/page links and records a SHA-256 of every received page. Source traversal and authoritative completeness are separate facts. The current HTML adapter has no verified total-count contract and therefore does not declare an exhaustive catalog or replace a verified cache.
- Persistent pull items use separate atomic files and bounded pages. Interrupted items become interrupted rather than successful. Retry preserves the exact model tag.
- Local chat persistence strips image bytes, thinking and tool-call fields. Ordinary exports omit all arbitrary text and image bytes because arbitrary model/user text cannot be proven free of credentials or private paths.
- Registered launch profiles use typed arguments, verified executable selection, restricted environment fields, preflight, snapshots and rollback. No shell command is accepted. Runtime execution remains unavailable until the privileged native adapter is connected and proven.

## Native surface

The dialog uses the existing Material Design 3 caption, shared browser-style `TabStrip`, `SearchField` with its anchored regex builder, `ListBox`, buttons, labels, multiline fields and scroll containers. Model list presentation is paged at 100 rows. Network work runs outside the UI thread. Closing the dialog cancels its owned request before destroying callback state.

The tabs are Models, Chat, Batch pulls, Launch profiles and Troubleshooting. Disabled controls state their precise unmet condition. No model is downloaded, installed or launched during development verification.

## Incomplete delivery requirements

The following remain required before this feature can be claimed complete:

- Authoritative exhaustive catalog totals and immutable revision support, exact variant blob metadata and complete guided filters.
- Verified GPU/backend detection, model-destination discovery and context-memory sizing.
- Full batch review, storage preflight, bounded parallel execution and progress reconciliation.
- Attachment picker, full parameter editor, session-history management and complete localized copy.
- Packaged installation/start recovery, native executable registration, process containment/readiness evidence and durable profile restore UI.
- Native compilation, packaged runtime interactions, accessibility, language/scale matrix and genuine screen-capture evidence.

## Verification

The standalone C++17 core tests cover request allowlisting, malformed/oversized/deep responses, streaming fragments, attachment capability checks, conservative fit outcomes, full pagination and missing-page negative regressions, durable interrupted pulls and redacted history. Launch-profile tests separately cover typed preflight and rollback. They do not establish native UI, runtime installation, real model execution or packaged behavior.

Build the focused tests with `cmake -S tests/ollama_suite -B <temporary-build-directory>`, then build and run CTest in that directory. The ordinary application build additionally compiles `OllamaCore.cpp`, `LaunchProfiles.cpp`, `OllamaClient.cpp` and `OllamaSuiteDialog.cpp`, using the existing curl and OpenSSL targets.

## Official sources

The implementation was checked against the [Ollama documentation index](https://docs.ollama.com/llms.txt), [local model listing](https://docs.ollama.com/api/tags), [chat endpoint](https://docs.ollama.com/api/chat), [pull endpoint](https://docs.ollama.com/api/pull), [official model catalog](https://ollama.com/library), and its model-specific tag listings. The catalog adapter is deliberately version-sensitive and fails closed when its source structure or completeness evidence changes.

## API collection

No public HTTP API is exposed by this module. A Postman collection is not applicable; this module is a bounded client of Ollama's documented local API.
