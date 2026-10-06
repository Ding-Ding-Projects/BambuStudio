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

Each new format version 2 event uses a random 256-bit event key, writes one
AES-256-GCM encrypted blob, and creates an append-only commit whose
parent is the previous event. Authentication binds the ciphertext to the version,
action, identity ID, and random event ID. Commit metadata contains only those
redacted fields and a fixed local author. Deleted identities retain their historical
events. There is no history rewrite, network synchronization, or restore API. Authorized
retention removes selected active event keys without deleting historical commits.
Metadata-only export is described below; snapshot and label plaintext are never
exported.

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
the actual head and the vault anchor stop both reads and writes. For version 2,
the encrypted key store also has a native-vault HMAC anchor and an embedded
expected history head. Both must match before access. A process-local
mutex plus an exclusive application-data lock file serialize cooperative access.
Reference updates also compare the expected parent.

The native vault, key-store file, and libgit2 do not share an atomic transaction.
The key store is atomically replaced and anchored before publishing its expected
new history head. If any later update fails, subsequent operations stop. An
unfinished `.pending` file also stops access.
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
snapshot contents. `read_revision(answer, revision)` authenticates and walks at
most 10,000 reachable events, decrypting only the requested active snapshot and
rejecting pruned targets. Ordinary pages remain readable when unrelated rows are
pruned, with empty payloads for those tombstoned rows. Rows contain the revision, stable identity, allowlisted action,
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

### Version 2 retention and explicit capacity

New events use format version 2 and independent random event keys. The bounded
`identity-history-keys-v2.enc` file lives beside, never inside, the bare repository.
The existing native-vault master key encrypts it with AES-256-GCM; one additional
fixed native-vault record authenticates the ciphertext with HMAC-SHA-256. There is
no credential-per-event design. A normal initialized history with events uses
four fixed native-vault records: independent credential, master key, history-head
anchor, and key-store anchor.

The key store contains at most 512 active keys and 4,096 explicit tombstones. Its
ciphertext is limited to 196,608 bytes. Counts, exact body length, event IDs,
duplicates, disjoint active/pruned sets, and the expected history head are all
validated before use. Keys serialize directly into a single cleansing `Secret`
allocation. The file is written to an exclusively created pending file, flushed,
and atomically renamed. An interrupted pending file, missing record, changed
ciphertext, rolled-back file, or head mismatch blocks access. Restoring both an
old key-store file and its old anchor still fails when its embedded history head
differs from the current history. Rolling back all vault and history state is
outside the protection boundary.

`capacity(answer)` reports exact active and tombstone counts plus their limits.
A full store rejects new events. Every prune operation retains one new audit-event
key, so pruning one event exchanges one active key for one audit key; pruning two
or more can free active capacity. Audit events cannot themselves be selected for
pruning. Tombstones and audit events accumulate within explicit limits. Exhausted
capacity requires a separately designed migration, not silent eviction, credential
proliferation, or bypass of the limits.

`preview_prune(answer, revisions)` accepts 1 to 16 unique reachable event revisions,
walking no more than 10,000 commits. It rejects unknown, already-pruned, version 1,
and prune-audit events. Its opaque `HistoryPrunePreview` binds the exact selected
rows, current history head, and owning service instance. `rows()` exposes only
metadata for the native confirmation panel.

`prune(answer, preview, confirmation)` re-authenticates and revalidates that binding
before consuming the toolkit-free `GUI::SuperConfirm::State`. Both independent
keys must be on, the slider must have reached 100%, authorization must be granted,
and cancellation must be false, as required by `may_fire()`. There is no naked
boolean authorization parameter. Failed validation consumes nothing. Actual
execution resets the state and cancels it so it cannot be reused unchanged.
The caller must supply the state produced by the native confirmation interaction;
this is an in-process UI contract, not a capability boundary against hostile code
running inside the same process.

Pruning removes selected keys from the active encrypted store and records their
event IDs as tombstones. It appends a `Pruned` event with the selected revisions
inside an encrypted audit payload. The original commits and ciphertext remain
unchanged. `read` and `read_metadata` retain those rows with `pruned=true`; `read`
returns an empty `Secret` for a pruned payload. Label retrieval refuses a pruned
label. Missing keys without a tombstone are corruption, never assumed pruning.
The audit commit, updated store, and two anchors use the same fail-closed update
ordering as append. A failed operation can leave application-level key removal
or retained encrypted objects requiring explicit recovery; it must never be
reported as an atomic rollback or a successful completed prune.

Version 1 snapshots remain readable using their original master key, including
mixed v1/v2 history after the first new append. Selective v1 pruning is explicitly
rejected because removing that shared key would also break unrelated legacy
records and the v2 key store. No v1 history is rewritten or silently re-encrypted.

Active-key removal is **not forensic erasure**. Previously read plaintext, older
vault state, backups, filesystem journals, and external copies may retain
recoverable material. It only removes the current application's active decryption
key through the validated storage path. The native UI must preserve this wording
and must not claim permanent destruction of all copies.

### Retention verification

The synthetic fixture driver additionally covers native confirmation states,
one-shot consumption, stale/foreign previews, unreachable and duplicate selections,
v1 readability and pruning rejection, mixed versions, append-only audit retention,
persisted pruned rows, label decryption refusal after pruning, key-store rollback,
old-anchor replay, missing/corrupted stores, interrupted anchor writes and pending
files, fixed vault-record counts, and the exact 512-key bound with capacity reuse.
These tests use only generated temporary histories and memory-vault fixtures.
Record the executed command and its actual result separately; source presence
alone does not prove verification.

### Executed verification for the retention slice

The supported MSVC toolchain, OpenSSL C ABI, and locally built libgit2 1.9.3
compiled and executed the final synthetic driver successfully:

```text
PASS 118 identity history checks
```

The existing driver was invoked with `--msvc --compiler cl --history-only`, the
matching OpenSSL include/DLL arguments, and `--libgit2-prefix` pointing at the
supported dependency installation. No real native-vault records or user history
were used by these fixtures.

Three independently compiled temporary source copies deliberately removed one
invariant each: the native `may_fire()` check, preview owner/head binding, and the
version 1 pruning rejection. Each compiled successfully and failed behaviorally.
The unmodified source was then restored in the isolated copy and passed all 118
checks again. The checked-in driver exposes this reproducible pass with
`--history-negative` in place of `--history-only`, using the same dependency
arguments:

```text
RED native-confirmation: behavioral rejection verified
RED preview-binding: behavioral rejection verified
RED legacy-retention: behavioral rejection verified
GREEN restored: PASS 118 identity history checks
PASS 3 isolated retention mutations; original source unchanged
```

These results cover the service and synthetic storage contract. They do not claim
native dialog interaction, packaged execution, actual operating-system credential
persistence, filesystem crash injection, backup destruction, or forensic erasure.
