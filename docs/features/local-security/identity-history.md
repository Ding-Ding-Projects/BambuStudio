# Encrypted local identity history

`IdentityHistory` owns a bare libgit2 repository at the stable application-data
location `identity-history-v1.git`. The host supplies its private application-data
directory, never the active project directory, current directory, portable export,
or source checkout. The location must already exist and cannot contain symlink or
Windows reparse-point ancestors. The repository must be bare and have no remotes.

## API and storage

Explicit `initialize` enrolls an independent history credential, creates a random
256-bit encryption key in the operating-system vault, and initializes the bare
repository. Existing or partially initialized state is never overwritten. Native
vault unavailability has no plaintext fallback.

`append` is a trusted internal mutation sink. It accepts a generated 32-character
hexadecimal identity ID, an allowlisted action, and a move-only snapshot. Rename
operations retain the same identity ID. Labels are never accepted as identifiers.
The caller serializes the identity metadata and optional authenticator seed needed
for restoration into a move-only `Secret`. Seed-bearing plaintext stays in memory
and is encrypted before any object is written. PINs, passwords, current or next
codes, and recovery answers must never enter snapshots. Live credentials remain
in their dedicated native-vault records. This service does not itself manage live
identities or their serialization.

Each event writes one AES-256-GCM encrypted blob and an append-only commit whose
parent is the previous event. Authentication binds the ciphertext to the version,
action, identity ID, and random event ID. Commit metadata contains only those
redacted fields and a fixed local author. Deleted identities retain their historical
events. There is no rewrite, prune, export, network synchronization, or restore API.

Every `read` independently verifies the history credential. Results contain at
most 16 snapshots, each at most 1 MiB, and offset cannot exceed 10,000. Reads walk
only the anchored history chain; callers cannot select arbitrary object IDs or
filesystem paths. Commit, tree, and blob headers are size-checked before materialization. The
service rejects unknown action values, malformed IDs, unexpected tree entries,
non-blob entries, merge commits, and authentication failures. Failed credential
attempts use the shared escalating attempt budget for the lifetime of the service.
Keep one service alive per application session; reconstructing the service resets
that in-memory throttle and is not a cross-process brute-force defense.

`replace_credential` verifies the old independent history credential before
replacement. It retains the snapshot key, so historical ciphertext stays readable.
Its credential-change audit event must be appended by the calling coordinator.
Credential replacement and the audit append are not one atomic operation.

## Failure and threat boundaries

The vault stores the expected latest commit ID separately from the repository.
Missing keys, credentials, anchors, missing repositories, or a mismatch between
the actual head and the vault anchor stop both reads and writes. A process-local
mutex plus an exclusive application-data lock file serialize cooperative access.
Reference updates also compare the expected parent.

The native vault and libgit2 do not share an atomic transaction. If repository
publication succeeds but the anchor write fails, subsequent operations stop.
This state requires explicit support recovery after independently validating the
retained objects and vault records. Automatic acceptance of a newer head, automatic
re-keying, or deletion of the repository would erase the evidence and is forbidden.
A failed initialization likewise remains explicit partial state.

This protects stored snapshots from ordinary filesystem disclosure and detects
repository rollback relative to the intact native-vault anchor. It does not defend
against an attacker who controls the same logged-in operating-system account,
can read native credentials, can modify the running process, or can roll back both
storage systems together. The host must use an application-data directory with
appropriate owner-only access controls. No success is claimed for UI integration,
restore flows, live identity reconciliation, or packaged execution by this module.

## Build and verification

The local-security CMake subdirectory defines `libslic3r_identity_history` when
the project's existing `libgit2::libgit2package` target is available, linking the
core cryptographic service and libgit2. The local-security test subdirectory
registers `identity_history_tests` as a separate executable and CTest test; its
`main` does not collide with the core driver. A standalone configuration without
libgit2 prints an explicit unavailable message and does not claim history coverage.

The behavioral driver covers encrypted round trips, append retention, stable IDs,
redacted raw object metadata, snapshot confidentiality, independent credentials,
replacement, read bounds, invalid input, missing and incorrect keys, unavailable
vaults, head-anchor mismatch, interrupted anchor writes, and attempt throttling.
It uses a memory-only fake vault and an isolated temporary bare repository. These
source tests are not evidence of successful execution until a real toolchain run
records its exit status and check count.
