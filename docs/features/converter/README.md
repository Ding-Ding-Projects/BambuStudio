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
excessive nesting and invalid encoding. Tabular conversion retains every cell
as a string; ragged rows and non-string JSON cells are rejected. Image
conversion accepts only the explicitly described bitmap variants and compares
every RGB pixel after reopening. ZIP extraction accepts one unencrypted stored
or deflated regular entry, rejects absolute/traversal names and verifies CRC.
Creating a ZIP stores the input as `payload.bin` and verifies a byte-for-byte
extraction round trip. Multi-entry extraction and 7z are not implemented.

Each input is limited to 16 MiB. PDF merge applies that limit to aggregate source
bytes. Output is limited to 64 MiB. The PDF transport envelope is limited to
24 MiB. Images are limited to 4,194,304 pixels; structured input has depth and
item bounds. Conversion disclosures describe line ending, number formatting,
filename, timestamp, image-resolution and metadata changes before the user
starts processing.

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
preserves completed outputs. Restart always pauses. A record interrupted while
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

The panel provides category searches, PDF-option search, guided input/output
pickers, PDF page/rotation/title controls, explicit ordered multi-file merge,
folder discovery, pause/resume/cancel, paged history, page selection/inversion,
selected retry and visible-row export. PDF page indices are one-based.

## Verification and remaining integration

The standalone test project is `tests/local_converter` and can be configured
with `LOCAL_CONVERTER_QPDF_SDK` pointing to the verified SDK. It builds three
behavioral test executables and the real worker. The initial verified counts
are 182 core assertions, 31 typed PDF assertions and 63 framed PDF assertions.
The core count includes actual hash-verified AppContainer conversion. The PDF
request test accepts optional worker path and worker digest arguments to run
all seven operations through the real sandbox.

The service-host execution context used during initial verification could not
initialize system `USER32.dll` inside AppContainer (`1114`). The official qpdf
binary imports that component. Win32k system calls were confirmed enabled.
PDF stays disabled when the live capability probe encounters this condition.
An interactive hidden-desktop verification is pending; direct typed tests do
not establish packaged sandbox support.

The native panel has not yet been compiled against the full application's wx
dependency build or exercised in the packaged application. Full localization,
per-element appearance/locking menus, notification/history integration,
command-palette routing, complete bulk-action coverage and the documented
language/theme/display-scale capture matrix still require shared integration
and runtime evidence. No source-only result establishes those properties.
Material Designer creation/export tooling was unavailable in this implementation
session; the panel reuses the existing native controls instead of claiming a
design-tool handoff.

The converter has no HTTP API. A Postman collection is not applicable.
