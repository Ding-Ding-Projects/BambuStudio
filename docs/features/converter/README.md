# Local file converter

The converter consists of a native destination panel, a durable local queue, a
broker and a separate offline worker. It preserves source files and never
replaces an existing destination. The output folder is selected explicitly.

## Available adapters and limits

The catalog always shows Documents/PDF, Images, Audio, Video, Archives,
Structured Data/Spreadsheets, Code/Text and Binary Encodings. Each category has
an independent local search field and the shared anchored regex builder.
Unavailable formats remain visible with an exact reason.

Implemented conversions are hexadecimal and RFC 4648 Base64 encode/decode,
UTF-8 LF/CRLF normalization, JSON formatting, rectangular CSV/TSV string-table
conversion to/from JSON, 24-bit uncompressed BMP/P6 PPM conversion, single-entry
ZIP creation/extraction, and the [PDF operations](pdf.md).

Input detection uses bytes, not extensions. JSON rejects duplicate keys,
excessive nesting and invalid encoding. Formatting preserves the original
number literal and string escape bytes, including precision beyond binary
floating point. Tabular conversion retains every cell
as a string; ragged rows and non-string JSON cells are rejected. Image
conversion accepts only the explicitly described bitmap variants and compares
every RGB pixel after reopening. ZIP extraction accepts one unencrypted stored
or deflated regular entry, rejects absolute/traversal names and verifies CRC.
Creating a ZIP stores the input as `payload.bin` and verifies a byte-for-byte
extraction round trip. Multi-entry extraction and 7z are not implemented.

Each input is limited to 16 MiB. PDF merge applies that limit to aggregate source
bytes. Output is limited to 64 MiB. The PDF transport envelope is limited to
24 MiB. Images are limited to 4,194,304 pixels; structured input has depth and
item bounds.

## Change disclosures and acknowledgement

Each adapter's disclosure states exactly what it changes or leaves out: line
endings, whitespace, quoting, filenames, timestamps, image resolution, color
profile, metadata or PDF structure. An adapter that is lossy or changes metadata
or encoding admits files only after an explicit acknowledgement. Those are the
UTF-8 line ending conversions, JSON formatting, CSV/TSV and JSON table
conversion, the bitmap conversions, ZIP creation and extraction, and every PDF
tool. Hexadecimal and Base64 encoding and decoding preserve every byte and need
no acknowledgement.

Selecting such an adapter shows its disclosure, a line asking for confirmation
and a check box: *I reviewed what this conversion changes or leaves out, and I
want to convert with these changes*. Until it is ticked, **Add source file** and
**Add source folder** are refused before any picker opens, and keyboard focus
moves to the check box. Unticking withdraws the acceptance.

The acceptance is bound to the exact disclosure by a token
(`acknowledgement_token()`: 16 hexadecimal digits over the adapter id, the
disclosure text and its change flags). It lasts for the current session and is
saved in every admitted queue record. The queue refuses an admission without the
matching token (`disclosure_not_acknowledged`, and no record is written) and
skips a saved record whose token no longer matches the current disclosure
(`disclosure_changed_since_acknowledgement`), so resuming after an update that
changed a disclosure never converts on earlier consent.

## Bundled proof and process boundary

`LocalConverterPanel` accepts a `PackageProof` containing the installed
directory, exact adjacent `BambuStudio_converter_worker.exe` and the expected
SHA-256 from the package build receipt. Shared integration reads
`converter-worker.sha256`, containing 64 lowercase hexadecimal digits with an
optional LF. Packaging must regenerate it from the staged worker, not copy an
old build's digest.

The panel first lists all adapters as unavailable. An asynchronous capability
probe verifies the worker bytes, launches the worker and confirms its sandbox
before enabling core adapters. PDF additionally requires every compiled qpdf
12.4.2 runtime pin and a successful engine initialization inside that same
sandbox. A matching manifest alone never enables PDF.

The broker pins the worker file against replacement, launches it with zero
AppContainer capabilities, an explicit inherited-handle list and an isolated
environment containing only required operating-system directory locations.
There is no PATH lookup, shell invocation, proxy inheritance or network
capability. Source paths and options travel only in the private broker; the
worker receives bounded byte streams. A job object limits it to one process,
256 MiB and 30 seconds. Cancellation terminates the owned job. The worker itself
verifies `TokenIsAppContainer` before reading requests.

The official qpdf DLLs reside under `tools/pdf`. Only those shipped runtime
files need read/execute permission for the converter AppContainer. Never grant
access to source documents, queue records or unrelated directories. The loader
checks compiled release hashes, retains read-only handles to prevent
replacement and uses restricted DLL search. It does not run `qpdf.exe`.

## Durable queue and output publication

The queue lives under the application's private local data directory. It owns
an exclusive process lock and writes one bounded JSON record per job. There is
no application-level total-file limit. Pages contain at most 100 records and
folder discovery admits one file at a time. One conversion runs at a time,
providing backpressure without collecting the complete queue in memory.

Pause waits for the current file, resume processes pending records and cancel
preserves completed outputs. Cancellation advances one durable generation
number, so cancelling a million pending records does not rewrite a million
files on the UI thread. Result paging interprets the saved generation, and
retry records its explicit new generation. Rejected admissions receive their
own stable per-file result. Restart always pauses. A record interrupted while
running becomes `Recovery required`; it is never silently treated as completed
or automatically rewritten. Explicit retry requires the destination to be
absent and the admitted source identity to remain valid. Additional PDF merge
sources also have durable size/time identities. The broker compares source
bytes again before publication.

Available destination space is checked before admission and execution. Output
is written to an exclusive sibling temporary file, flushed, read back and
compared, then published with a no-replace atomic operation. Split PDF output
is one validated ZIP, so it also has one atomic publication boundary. A failed
write removes its own temporary output. Existing destinations are skipped,
including a destination created concurrently after preflight.

Result CSV export contains visible page rows, filename only, state and stable
result codes. Ordinary diagnostics contain neither document content nor source
paths. Private queue files necessarily retain the user-selected paths to resume
work; they must not be synchronized, uploaded or included in ordinary logs.

## Native integration

Include `slic3r/GUI/LocalConverter/LocalConverterPanel.hpp` and create the panel
inside the normal tab router. Provide the installed package receipt and a
private queue directory. Compile `Converter.cpp`, `ArchiveAdapter.cpp`,
`Worker.cpp`, `PdfAdapter.cpp`, `PdfRequest.cpp` and `PdfPackage.cpp`; link the
existing miniz target plus `bcrypt`, `userenv` and `advapi32` on Windows. Define
`LOCAL_CONVERTER_WITH_PDF=1` only when the verified qpdf SDK headers are present.
The adapter resolves C API symbols dynamically, so no qpdf import library is
required. Compile `WorkerMain.cpp` into the separate worker with the same core.
The application build requires `LOCAL_CONVERTER_QPDF_SDK`. `build.bat`,
`build-installer.bat`, `OneClickBuildInstaller.cmd`, `build_win.bat` and the hosted
Windows build workflow stage the verified SDK before configure; all but
`build_win.bat` also place the runtime in the payload's `tools/pdf` (see
[Bundled PDF engine](pdf-engine.md)).

The panel provides category searches, PDF-option search, guided input/output
pickers, PDF page/rotation/title controls, explicit ordered multi-file merge,
folder discovery, pause/resume/cancel, paged history, page selection/inversion,
selected retry and visible-row export. PDF page indices are one-based.

## Panel components

Every control on the panel is a registered kit primitive; no stock wx control
is constructed. The source contract is pinned by
`ui-md3/tests/local-converter-panel.test.mjs`.

| Part | Kit primitive |
| --- | --- |
| Eight categories | `TabStrip` (surface `local_converter_categories`, docked on top, no close) over a `wxSimplebook` page per category |
| Category, PDF-setting and queue searches | `SearchField` with its anchored regex builder |
| Adapter catalogues and the queue/result history | `MD3DataViewListCtrl` with `md3_style_data_view` (the queue allows multiple selection) |
| Page order, PDF title and output folder | `TextInput` |
| Title, guidance, details, empty states, page and status lines | `Label` (title in the kit headline face) |
| Actions, rotation choice and paging | `Button` with an explicit Material variant; the chosen rotation is the filled button |
| Change acknowledgement | `LabeledCheckBox` (the kit `CheckBox` glyph with a `Label`), named for assistive technology |
| Scrolling body | `MD3ScrolledWindow` on the Surface role |

The category strip keeps the shared tab contract: keyboard selection along its
axis, overflow menu, reorder, pinning and grouping, its own tab search, and a
saved dock edge. Moving the strip to another edge re-places the category pages
beside it. A category with no matching adapter shows a visible empty state
instead of a placeholder row. Every search, table and field carries an
accessible name.

## Language modes and funny levels

All converter copy follows the English, Hong Kong Cantonese and bilingual
language modes. The panel's own labels, buttons, searches and column titles are
catalogue messages. The registry in `Converter.cpp` keeps fixed English for
category, adapter and source-kind names, change disclosures, validators,
unavailable reasons and queue states; that English is the catalogue source, and
the panel translates it when it is shown. A runtime diagnostic never joins that
text. It travels separately as a stable machine code in the adapter's `detail`.

Every stable result code has one plain sentence (`result_message()` in
`Converter.cpp`). A result cell, a detail line and the exported `message`
column show the translated sentence followed by the code, for example
`The destination already exists. It was not replaced. (code destination_exists)`.
Codes that end in a Windows error number, such as `isolated_worker_start_1114`,
share one sentence for their prefix and keep the number. The CSV export keeps
the stable `state` and `result` identifiers beside the translated `message`.

In bilingual mode the status, details, page summary and empty-state lines show
the English and then the Cantonese, and each table cell reads
`English · 廣東話`. Window labels, buttons and column titles get their second
language from the shared bilingual decorator.

The funny level changes only non-factual lines: the guidance line, the empty
queue and no-match states, the no-selection hint, the worker check progress and
completion lines, the converting line, the stop summary and the export
confirmation. Each has a serious, light and playful variant
(`LocalConverterCopy.hpp`; levels 1 and 2 are serious, 3 is light, 4 and 5 are
playful), every variant states the same facts and keeps the same counts, and
English and Cantonese each use the variant for their own level. Adapter names,
disclosures, reasons, states, result sentences, limits and safety statements are
factual and never change with the level.

## Verification and remaining integration

The standalone test project is `tests/local_converter` and can be configured
with `LOCAL_CONVERTER_QPDF_SDK` pointing to the verified SDK. It builds three
behavioral test executables and the real worker. The core test reports 331
assertions without a worker path and 334 with one, where the extra three are the
actual hash-verified AppContainer conversion; it also checks that every failure
it provokes has a result sentence, that unacknowledged or mismatched lossy
admissions are refused, and that a record with stale consent is skipped without
output. The PDF tests recorded 31 typed and 63 framed
assertions. The PDF request test accepts optional worker path and worker digest
arguments to run all seven operations through the real sandbox.

Deliberate source mutations verified that enabling an unproven adapter fails
the availability assertion and removing the page limit fails the bounded-page
assertion. Both mutations were restored and the restored implementation was
recompiled before the passing run. Concurrency, cancellation before publication
and a racing destination writer have behavioral coverage.

The service-host execution context used during initial verification could not
initialize system `USER32.dll` inside AppContainer (`1114`). The official qpdf
binary imports that component. Win32k system calls were confirmed enabled.
PDF stays disabled when the live capability probe encounters this condition.
An interactive hidden-desktop verification is pending; direct typed tests do
not establish packaged sandbox support.

Localization is pinned by `ui-md3/tests/local-converter-localization.test.mjs`
(every catalogue source has English and Cantonese entries, every result code has
a sentence, registry text is translated at display time and only non-factual
lines take a voice) and `tests/local_converter_copy` (the voice ladders keep
their facts and placeholders at every level). The acknowledgement is pinned by
`ui-md3/tests/local-converter-acknowledgement.test.mjs` (flags and disclosures,
queue refusal and skip, and the check box gate before any picker).

The native panel has not yet been compiled against the full application's wx
dependency build or exercised in the packaged application, so the Cantonese and
bilingual renderings, the funny-level variants and the acknowledgement check
box still need to be seen there.
Per-element appearance/locking menus, notification/history integration,
command-palette routing, complete bulk-action coverage and the documented
language/theme/display-scale capture matrix still require shared integration
and runtime evidence. No source-only result establishes those properties.
Material Designer creation/export tooling was unavailable in this implementation
session; the panel is assembled from the existing kit primitives listed above
instead of claiming a design-tool handoff.

The converter has no HTTP API. A Postman collection is not applicable.
