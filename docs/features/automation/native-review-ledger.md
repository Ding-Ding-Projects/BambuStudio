# Private native interaction ledger

[香港粵語](native-review-ledger.yue_HK.md)

`scripts/md3/review-ledger.py` validates already recorded native observations. It
does not launch an application, send input, take screenshots, alter pixels, publish
files or grant runtime acceptance. This is **source-only preparation for future
authorized review**, not evidence that the review queue has been executed.

The current [local review driver](local-native-review.md) handles the initial shell
and closes its owned process. It does not collect interaction steps. Recording
further steps still needs separately reviewed, authorized native interaction support;
this ledger does not add that support or extend a session after teardown.

## Private invocation and retention

Use the existing Python environment with Pillow available. No dependency is installed
by this helper. Select a new task-owned directory directly under the OS temporary
root, with a `bambu-ledger-` prefix:

```powershell
$EvidenceRoot = Join-Path ([IO.Path]::GetTempPath()) ('bambu-ledger-' + [guid]::NewGuid())
& $Python scripts/md3/review-ledger.py --input $PrivateObservations --evidence-root $EvidenceRoot
```

`$PrivateObservations` identifies an existing private JSON file. The output directory
must not already exist; repository destinations are rejected. `ledger.json` retains
the supplied observations, original build/session receipt data, native findings and
the paths/hashes of original evidence files. Originals remain unchanged in their
owned review directory. **Keep those originals and the producer files alongside the
ledger**: this is a reference ledger, not a portable evidence archive. A later deleted
or replaced original makes revalidation fail. No profile directory is copied or read.

Standard output contains only a status, count and unverified acceptance/publication
verdicts. Invalid input or output produces a generic failure on stderr and exit 2,
without echoing private paths or semantic text. A nonempty consistent ledger exits 0;
an empty ledger is written with `incomplete` and exits 2. Exit 0 means consistency
checks passed, never that the interface passed review. Raw input, receipts, filenames,
probes, screenshots and semantic text remain private, including after privacy review.

## Input contract

Unknown fields in the observation schema are rejected. All referenced paths are
absolute regular files; symbolic links and reparse points are rejected. A file
reference has exactly `path` and lowercase `sha256`. JSON input is bounded to 16 MiB,
individual probes to 16 MiB, and PNGs to 64 MiB.

| Record | Required fields and meaning |
| --- | --- |
| Root | `schemaVersion: 1`, `kind: "local-native-interactions"`, `producer`, `sourceCommit`, `buildReceipt`, `session`, `steps` |
| `buildReceipt` | File reference to the existing version-1 `local-root-build` receipt |
| `session` | `id`, `review` file reference; ID is the actual `bambu-local-review-*` directory name containing `review.json` directly under the OS temporary root |
| Each step | `id`, one-based `sequence`, `status: "observed"`, `sourceCommit`, `buildReceiptSha256`, `sessionId`, `pid`, `action`, `pre`, `post` |
| `action` | `method` (`native-keyboard` or `native-pointer`), observed accessible `target`, `atUtc` |
| `pre`, `post` | `atUtc`, nonempty observed `semanticState`, positive integer `hwnd`, `tuple`, `probe` file reference, `capture`, `privacy` |
| `tuple` | `language`, boolean `dark`, `density`, measured `dpiScale`, `client` with integer `w`/`h`, observed `motion` (`normal` or `reduced`) |
| `capture` | `file` reference and `reply` with exactly `rendered_ok`, `mode`, `window_hwnd`, `path`, preserving the corresponding Lowlevel response values |
| `privacy` | `status: "reviewed-safe"`, nonempty `reviewer`, `reviewedAtUtc`; this is a human review assertion, not a filename-derived verdict |

Record semantic state before and after each actual input, not the intended state.
Literal placeholder states such as `planned`, `pending`, `unknown`, `unverified`,
`todo` and empty text are rejected. IDs are unique; sequences must be contiguous.
Pre-observation time precedes input time, which precedes post-observation time. UTC
timestamps end in `Z`, fall after build completion and cannot be in the future.
Privacy review cannot predate its observation. The next step's complete `pre` record
must equal the preceding `post`, including evidence references. A missing transition
is a gap, not something the helper fills in. Pre/post probe and PNG paths must differ,
so one retained file cannot masquerade as two separate observations.

## Reused contracts and limits

Build validation calls the existing `local-native-review.py` validator before and
after reading observations. That preserves the successful exact root entrypoint,
24-hour freshness, clean producer/source-tree identity, transcript ordering, payload
hash and automation-companion checks. A changed or active producer is not relabelled
as the observation's build. The initial-session receipt must have the matching build
receipt hash, current driver hash, observed PID/shell, received probe and verified
teardown, without a recorded failure. `shell.jsonl` is revalidated against that PID,
HWND and directory-derived session tag.

Every per-step probe uses that same native validator: complete terminal `end`, matching
PID/tag, visible target, positive measured client geometry and finite DPI. This version
deliberately supports only the existing **English / light / comfortable** profile.
Other languages, dark theme and compact density need the reviewed native tuple contract
before ledger support can expand. The recorded motion value is an observer assertion:
the native probe does not report effective motion. No minimum-size, DOM or renderer
interior measurement is inferred from an outer native rectangle.

Per-step captures must be existing nonuniform PNGs with matching native client size,
Lowlevel rendered/window response and HWND, and matching SHA-256. All observation files
must remain in the owned session directory. The helper checks their hashes again at
the end. It retains native findings and other visible owned windows; it does not turn
their presence or absence into a visual verdict.

These are local, unsigned observer records. Hashes bind supplied bytes, not the truth
of a person's assertion. A supplied Lowlevel response is not an authenticated receipt;
neither it nor nonuniform pixels proves genuine screen origin, semantic correctness,
privacy, session timing or actual input. Observers must inspect genuine capture bytes,
review sensitive content, retain raw provenance and use only authorized native input.
No synthesized image may be submitted as actual review evidence. Synthetic unit-test
images exist solely to exercise rejection logic. `runtimeAcceptance` and
`visualAcceptance` always remain `unverified`, and `publication` is always
`not_authorized`. Publication needs a separate privacy/provenance review and authority.

Focused verification: `python -B -m unittest discover -s scripts/md3/tests -p test_review_ledger.py -v`.
Those tests exercise private file consistency with synthetic fixtures and an injected
Git reader. They do not execute the application or prove production interaction.
