# Proposed hosting protocol, version 1

Design only. The existing `host.ps1` implements Mode/PairingPipe and terminal pairing results. It does not implement this progress/configuration protocol. Preserve existing CLI callers when adding this separate interface.

## Common envelope and validation

Requests contain `schema: 1`, `requestId` (UUID), `method` (fixed enum below) and `params` (method-specific object). Reject unknown versions, methods and fields, malformed identifiers and oversized messages before any mutation. Responses contain `schema`, `requestId`, `ok`, and exactly one of typed `result` or a fixed safe `error.code`. Never include arbitrary exception text or confidential values. Bound each frame to 8192 UTF-8 bytes and reject duplicate keys, trailing data and invalid UTF-8.

Transport is a narrow renderer-to-main IPC allowlist plus a separately authenticated owner-only local pipe to the backend. Verify sender frame identity and operation ownership. No renderer-supplied executable, command string, script path, unrestricted URL or environment map is accepted. Production transport details require independent security review.

| Method | Parameters | Successful result |
| --- | --- | --- |
| inspect | optional `operationId` belonging to the current main-process session | `snapshotId`, `ownership` (existing-owned/fresh/conflict), prerequisite enum states, safe configuration summary and capabilities; operation-aware inspection additionally returns the same operation's reconciliation result |
| configure | `snapshotId`, station name, validated integer port, storage enum existing/new-volume/new-empty-folder, optional folder selection handle, credential mode preserve/generate/custom; custom bytes use protected input only | opaque `configurationId`, safe review summary, expiry time; never credential bytes |
| start | `configurationId`, `snapshotId` | `operationId`, accepted phase; rejects stale snapshots or an existing unresolved operation |
| progress | `operationId`, `afterSequence` | bounded ordered events and latest terminal result if present; read-only and repeatable |
| cancel | `operationId` | `accepted: true`, current phase; acceptance is not terminal cancellation |

`snapshotId`, `configurationId` and `operationId` are opaque main-process-generated UUIDs bound to the current application session and exact owned service. They grant no authentication by themselves. Configure expires after 10 minutes and is invalidated by changed settings, successful start consumption or session exit. Start is idempotent for the same request identifier; duplicate requests return the original operation, never start another worker. Different Start requests are rejected while an operation is unresolved.

Operation-aware inspect performs only the fixed owned-service observation. It cannot declare terminal reconciliation while its owned worker is still active. After worker exit, verified service identity, health and data preservation may transition that same operation from unresolved to a reconciled terminal result. Retain the operation identifier and append a terminal event; do not create another operation. If identity or retained-data evidence is unavailable, remain unresolved. Inspect never accepts an executable or arbitrary command.

Events contain `schema`, `operationId`, increasing `sequence`, `phase` (inspect/acquire/validate/pull/start/local-health/lan-health/reconcile), and `state` (active/cancelling/terminal). Optional progress is a validated completed/total pair with declared units, only when measured. Terminal result contains `outcome` (succeeded/failed/cancelled), `reconciled: true`, `serviceState` (running/stopped/absent), `dataPreserved: true`, safe error code if needed, and validated URLs plus verification scope. Unknown service state prevents a terminal reconciled result and permits only inspect/reconcile recovery. Never fabricate `dataPreserved` when inspection cannot establish it.

## Confidential configuration lifetime

For fresh custom credentials, the masked renderer input transfers once through the approved typed channel to main-process memory. Clear the renderer field and references immediately after accepted transfer, before Review. Review displays only custom/generated/preserved mode. JavaScript cannot guarantee erasure of immutable string copies; minimize copies, prefer bounded byte buffers in the protected backend and do not claim cryptographic memory erasure from clearing a field.

Main retains the value only in owned memory during the pending Review lifetime. It never serializes it into drafts, logs, telemetry, environment files, argv, URLs or ordinary progress. Returning to Configure invalidates and clears the pending value and requires re-entry. Timeout, cancellation, terminal failure, successful protected transfer and window/session exit release it and wipe mutable owned buffers. Resume never restores it. Backend handoff uses the authenticated owner-only pipe, and persistence uses protected storage, not plaintext configuration. Preserve mode never reads a credential into the renderer.

Review can retain nonconfidential configuration while its credential expires; Start then reports a fixed credential-required state and returns to Configure. A renderer crash does not orphan active work: main retains operation ownership and reconciles before allowing another Start. A main-process restart performs inspection and explicit recovery, never treats an old operation identifier as current process authority.
