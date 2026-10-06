# Export safety and portable formats

The shared export dialog serializes its supplied dataset, reports loss before writing,
and offers a direct **Open in VS Code** action after a successful export. The dialog
remains open so the completed output can be opened. Plain files and ZIP replacements
require SuperConfirmGate approval for the exact primary and companion paths. The
engine defaults to no overwrite, rejects links/directories, and does not transfer
approval to a different destination. The captured job is not re-read after approval. Closing it returns success when
an export was completed. General external-editor preferences remain separate.

## Confidentiality boundaries

Preferences use an exact section/key allowlist and validate each allowed value.
Unknown sections, credentials, paths, private customizations, identifiers, and
unreviewed values are excluded by default. Preset options similarly use a reviewed
numeric-printing-setting allowlist, excluding host credentials and free-form scripts.
The loss report states the omitted count without revealing omitted names or values.
Schema headers carry the exclusion notice. These are safe partial exports, not
complete settings or preset backups. Adding a key requires a sensitivity review and
value validation in `ExportEverything.cpp`.

## Formats

Existing JSON, JSONL, YAML, TOML, XML, CSV, TSV, Markdown and HTML remain available.
Additional formats carry a complete JSON snapshot without silently coercing integers:

| Format | Representation and import |
| --- | --- |
| SQL | SQLite-compatible `export_snapshot.document_utf8` BLOB containing UTF-8 JSON; decode bytes, then parse JSON |
| JavaScript / TypeScript | Exported `json` string; use a lossless JSON reader for large integers |
| Python | `snapshot = json.loads(...)`; Python preserves arbitrary-size integers |
| Go | `JSON` string constant; use `json.Decoder.UseNumber` when decoding |
| Rust | `JSON` string constant; choose appropriate integer handling in the JSON reader |
| JSON Schema | Draft 2020-12 `const` containing the complete snapshot; this validates this exact snapshot, not future datasets |
| Protobuf text | `json_utf8` bytes field and a companion `schema.proto`; parse with the provided `ExportSnapshot` schema, then decode JSON |

These source forms preserve data as JSON, rather than inventing a native typed model
for heterogeneous records. Protobuf does not claim a typed binary record export.
UTF-8, selected line endings, schema identifier and version are recorded when headers
are enabled. Existing NaN/Infinity losses remain explicitly reported for JSON-derived
formats. Disabling headers also omits provenance metadata, not dataset values.

## Encrypted 7z

On Windows, 7-Zip receives a bare `-p` creation switch and reads the password from a
bounded anonymous input pipe. `-sccUTF-8` selects UTF-8 input. The password never enters
arguments, environment variables, temporary files, diagnostic output or persisted
settings. Only the input pipe and NUL output handle are inherited through an explicit
handle list. A suspended child is assigned to a kill-on-close job before execution;
each operation has a five-minute deadline. The transfer buffer is wiped and input is
closed. Password controls are cleared after execution. This is process-input hygiene,
not a claim that arbitrary third-party executables or all GUI heap copies are secure.

Input must be a single line, at most 1024 UTF-8 bytes, without NUL. Encrypted headers
are required. A successful creation is followed by a private-input integrity test and
a no-password listing that must return code 2 (encrypted-open failure) or 255
(closed-input password prompt abort, verified with the installed 7-Zip). Any warning,
missing output or inconclusive verification leaves the export incomplete. Existing
archive targets are preserved; choose a new name. Partial output after failure may
remain for inspection and is never advertised as verified. Split sets open at `.001`.

The non-Windows encrypted transport is unavailable and fails before staging. ZIP is
unencrypted and rejects a supplied password rather than silently ignoring it.
The dialog owns one worker and polls its completion with a GUI-owned timer. The worker
captures an immutable job and shared result/control only, never a GUI pointer or queued
callback. Cancel reaches the owned 7-Zip job through a 50 ms process wait; ZIP compression
checks cancellation through its read callback. Dialog destruction requests cancellation
and joins. Serialization runs off the GUI thread; cancellation is observed between
serialization and writing, so cancellation of one large serialization waits for that
serialization call to return. GUI completion must still be verified in the native build.

Output is staged in a private task directory beside the destination. Confirmation captures
target existence, size, modification time and Windows volume/file identity. These are
rechecked before writing and publication. A newly appearing target is never overwritten:
publication uses atomic no-replace moves. Existing authorized output is moved into a
recovery backup, revalidated, and replaced only by a completed staged file. Multi-file
publication is a short transaction with recoverable backups, not one filesystem-wide
atomic operation. Cancellation is sealed before publication starts and the UI states that
boundary. A collision triggers recovery attempts without replacing another writer's new
file; unresolved backups and incomplete output remain in a reported recovery directory.
A crash during publication can also leave that recovery directory for manual recovery.
No claim of complete application export coverage is made by this engine.

The input route follows upstream 7-Zip `UserInputUtils.cpp`, `StdInStream.cpp` and
`UI/Console/Main.cpp`: password prompts read standard input; console charset also
configures standard input. Extraction must omit `-p`, because a bare `-p` on extraction
means an explicitly empty password instead of prompting.

## Verification

`tests/export_everything/export_everything_tests.cpp` exercises the wx-free engine,
including installed 7-Zip encrypted creation, integrity verification and header
rejection. `portable_roundtrip.cpp` plus `verify_portable_roundtrip.py` round-trip real
SQLite, Python and Node output and validate the remaining source literal payloads.
The standalone engine was compiled with MSVC C++17. GUI compilation and built-dialog
interaction are separate requirements, not established by these console tests.
The TypeScript, Go, Rust and Protobuf compilers were not run in this lane.

The current focused result is 382 assertions across 24 cases. The cancellation child
fixture is compiled from `tests/export_everything/archive_test_child.cpp`; set
`EXPORT_TEST_CHILD` to its executable path to run the actual child-termination test.
Without it, that test explicitly reports unavailable coverage. The actual child test
was enabled for the recorded result. A deliberate mutation
removing the configuration-section boundary failed the privacy regression (1 failure
in 7 assertions); the restored production source passed the complete focused suite.
