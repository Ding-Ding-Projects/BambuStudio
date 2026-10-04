# Local history continuation handoff

This is an unfinished implementation checkpoint, not a completion or release claim.

Objective: provide persistent printer incidents and one native local history center over existing per-project and preference repositories, with configuration and draft history, comparison, filters, submitted searches, and guarded restoration.

Current branch: codex/bambu-local-history. Base commit: 0c967a557.

Implemented source: bounded PrinterHistory JSON lifecycle ledger and local ProjectHistoryManager transition snapshots; fresh telemetry hooks in DevHMS and DeviceManager; preference snapshots now use versioned allowlisted JSON while legacy snapshots are retained; persisted submitted searches retain the latest 100 unpinned records plus pinned records and regex modifiers; LocalConfigHistory provides recognized non-sensitive print-option snapshots; ProjectHistoryVersion exposes actual commit parent IDs; ProjectHistoryDialog has aggregate asynchronous loading, Timeline/Table/Git graph/Compare/Past searches, category/device/status/date filters, submitted search management, local export, preference restoration, and bounded model geometry summaries.

Verification: the standalone native printer-history test executable passed ten lifecycle scenarios. That executable disables snapshots and substitutes history-engine stubs, so it does not verify libgit2 snapshots. git diff --check passed. The complete native application build, query-store executable tests, aggregate UI interactions, restore behavior, accessibility, translations, and captures remain unverified. No printer commands or network requests were introduced by this lane.

Known incomplete work: no automatic preset change hook was added to Tab.cpp; the draft lane has not connected LocalConfigHistory record calls or its draft restore callback; preset and draft restoration are therefore unavailable in the center. The graph is a textual parent-edge representation rather than a rendered interactive graph. Comparison shows geometry/configuration summaries but still needs a reviewed per-key delta presentation. Saved filters use pinned submitted records rather than a separate named filter editor. Incident revision refresh, theme propagation to all added controls, human-readable date helper integration, and selection persistence across category/view changes require verification and refinement. Preferences restoration still needs appearance/language refresh and value-type validation against current preference controls. No design handoff or representative runtime evidence was generated for this unfinished state.

Next safe steps: build this exact source and fix compiler diagnostics before merging; run the checked-in printer and submitted-search tests with real history integration; connect draft and preset history under current identity/revision guards; complete restore and comparison transactions; review privacy for every snapshot/export; then drive the real native UI and retain source-bound captures. Preserve existing legacy history. Never replay printer commands from history.

Preservation: no merge or push was performed by this lane. The coordinating agent owns integration, remote proof, repository documentation, release decisions and cleanup. This checkpoint must remain explicitly unfinished until those missing checks and behavior are completed.
