# Local Ollama suite native design handoff

Target: the existing wxWidgets application and its registered Material Design 3 controls.

Material Designer creation/export tools were not exposed to this implementation session. The fallback uses the established target-owned widget kit, without introducing a different design framework. This document is a state inventory and implementation handoff, not a rendered reference or runtime screenshot.

| Section | Required states | Implementation |
| --- | --- | --- |
| Models | uninspected, unreachable, installed, running, metadata unavailable, catalog unverified, catalog offline; catalog fully traversed, certified, stale, latest refresh failed beside the last verified catalog, saved catalog invalid; hardware unmeasured or measured; GPU backend verified, processor-only, changed or unverified; model folder confirmed, configured or unconfirmed; estimate context and cache precision pickers; store filters at any or a chosen value, variant picker disabled until a family is chosen, grouped with counted headings, sorted, heading or variant explanation | `OllamaSuiteDialog.cpp`, `OllamaClient.cpp`, `OllamaSuiteText.cpp` |
| Chat | model unavailable, ready, streaming, stopped, incomplete, complete, attached image, history, rename/delete/export | `OllamaSuiteDialog.cpp`, `OllamaCore.cpp` |
| Batch pulls | empty, queued, interrupted, preflight unavailable | `OllamaSuiteDialog.cpp`, `OllamaCore.cpp` |
| Launch profiles | predefined, semantic executable/model/folder registration, preflight blocked, reviewed, snapshot/rollback, cancelled | `OllamaSuiteDialog.cpp`, `LaunchProfiles.cpp`, `NativeLaunchAdapter.cpp` |
| Troubleshooting | absent-or-stopped ambiguity, unhealthy API, storage shortage, offline catalog, unknown fit | bundled native text |

The default client size is 960 by 760 DIP and the minimum is 640 by 560 DIP. Sections scroll independently. The shared tab strip owns ordering, grouping, discovery, persistence and dock direction. Buttons wrap in narrow layouts. Every selectable model uses the shared search matcher rather than a separate regex engine.

Required verification remains pending: English, Cantonese and bilingual modes; light and dark; 100%, 125%, 150% and 200%; default and minimum client area; keyboard and assistive navigation; production build identity; per-click and final-state captures; layout-probe receipts. No source preview or fixture is accepted as that evidence.
