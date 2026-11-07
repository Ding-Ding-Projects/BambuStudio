# Base64 files beside their originals

Open the local file converter from navigation or the command palette. Use
**Encode files beside originals** or **Decode files beside originals** to pick
up to 1,000 files, or drop them on the corresponding button. Then run the queue.
These targets use the same packaged-worker route and durable queue as other
conversions. They do not use the selected output-folder field.

Encoding `report.pdf` creates `report.pdf.base64.txt` beside the input. Decoding
that name restores `report.pdf`. Existing files are never overwritten: a clash
creates `report (1).pdf`, then the next free numbered name. Encoded collision
names such as `report.pdf.base64 (1).txt` retain the original-name relationship.
For other names, decoding uses an allowlisted data-URL MIME type, then bounded
byte-signature detection, then `.bin`. A name is a suggestion, never permission
to execute the decoded content.

The result table shows the actual written path. Select a completed result and
use **Show selected output in folder**. This opens the native folder view using
item identifiers, without building a shell command from the filename.

## Verification and boundaries

Encoding sources and decoded outputs are limited to 16 MiB. Same-folder decoding
accepts at most 22,370,649 encoded bytes: the padded representation of 16 MiB,
plus a header of at most 1,024 bytes and its comma. Other adapters retain their
existing input limits. Files are read into bounded memory; this is not
a streaming implementation. The current option is canonical RFC 4648 Base64
with padding and no whitespace or wrapping. A bounded `data:` header ending in
`;base64` is accepted when decoding. URL-safe variants, lenient input, selectable
text encodings, streaming pause/resume, preview and full toolkit export/history
remain separate unfinished work.

The broker compares a SHA-256 round trip through the worker, rechecks the source,
and publishes through an exclusive temporary file only after rereading and
hash-checking its bytes. Concurrent destination creation selects another numbered
name without replacing that file. Publication stops after 10,000 occupied names.
Cancellation before publication leaves no completed output; the existing atomic
write cannot be interrupted midway and may finish if cancellation arrives during
that short operation. No payload is logged or sent over the network. Queue
records retain source and destination paths and stable result codes locally.

`tests/local_converter/run_base64_files.ps1` compiles the actual converter core,
archive adapter and worker with the installed MSVC toolchain, then exercises
neutral files and the real queue with an in-process transform executor.
`-Mutant` removes the round-trip hash comparison and must fail.
`-RepresentationMutant` restores the incorrect 16 MiB encoded-input limit and
must fail the saved-file queue round trip. This does not
prove a packaged AppContainer launch or rendered picker/drop interaction.
