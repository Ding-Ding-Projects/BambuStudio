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
events. There is no rewrite, prune, network synchronization, or restore API. Metadata-only
export is described below; snapshot and label plaintext are never exported.

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
tests compiled and executed through MSVC 19.51 against libgit2 1.9.3 produced by
the repository's pinned dependency recipe: `PASS 57 identity history checks`.
That service result does not prove the native history panel or full packaged flow.

## Metadata, labels, and redacted export

`read_metadata(answer, offset, count)` uses the same independent credential and
page limits as `read`, but returns `IdentityHistoryMetadata` without decrypting
snapshot contents. Rows contain the revision, stable identity, allowlisted action,
commit timestamp in UTC seconds, recorded timezone offset in minutes, format
version, and pruning state. The timestamp is the commit's recorded timestamp,
not capture provenance or a trusted external clock. Existing `read` results expose
the same fields while retaining their move-only snapshot.

`append_label(answer, identity, label)` appends an encrypted `Labelled` event for
that stable identity. Labels must contain valid UTF-8, have 1 to 256 bytes, and
exclude ASCII and C1 control characters. `read_label(answer, revision)` traverses
at most 10,000 anchored events and returns the encrypted label's plaintext in a
move-only `Secret` only when the selected event is a label. It cannot return an
ordinary identity snapshot under this API. Labels are chronological events; they
do not overwrite commit messages or automatically replace earlier labels.

`export_redacted(answer, offset, count)` returns bounded JSON containing only the
metadata page. It intentionally excludes labels, snapshots, ciphertext, keys,
credential records, payload lengths, and payload-derived hashes. No filesystem
write or destination selection occurs in this function. `redacted_diff(before,
after)` reports only whether the rows have the same identity and whether the
allowlisted action changed. It is not a semantic diff of identity contents.

### Retention remains unavailable

All currently written events remain format version 1 and report `pruned=false`.
The metadata fields are explicit state, not a claim that retention exists. This
slice does not provision per-event keys, produce tombstones, remove keys, or accept
a synthetic authorization boolean. Version 2 retention needs a bounded encrypted
key store outside the bare repository, atomic replacement and interruption
recovery, native two-key authorization, and executed integration tests before it
can be enabled. Existing version 1 events remain readable with the original key.

Future active-key removal must not be described as forensic erasure: older vault
state, backups, filesystem journals, and external copies may retain recoverable
material. The append-only graph must remain intact and an explicit authorized
pruning event must record the application-level loss of decryption capability.
