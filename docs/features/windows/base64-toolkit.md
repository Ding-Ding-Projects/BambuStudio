# Base64 toolkit

**Surface:** File > Base64 toolkit, the command palette (`Ctrl+Shift+F`, type
"Base64"), or the Every feature page → `Base64ToolkitPanel`
(`src/slic3r/GUI/Base64Toolkit/Base64ToolkitPanel.{hpp,cpp}`), a page in the
Local tools tab. The decisions live in three modules without wxWidgets:
`Base64Codec` (the codec), `Base64Files` (whole files) and `Base64Workbench`
(text conversion, previews, undo, history and records).

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board Base64.

Everything runs on this computer. Nothing is uploaded, and nothing decoded is
ever run or opened by itself.

## Text

Type or paste on the left; the result appears on the right as you type.

| Option | Choices |
| --- | --- |
| Direction | Encode, Decode |
| Alphabet | Standard (RFC 4648 section 4), URL-safe (section 5) |
| Lines | One line, MIME 76 with CRLF, Custom width (4 to 1000 characters) |
| Padding | On, Off |
| Decoder | Strict, Lenient |
| Text as | UTF-8, UTF-16 LE, UTF-16 BE, Latin-1 |

- Both sides show their counts, for example "28 characters · 31 bytes".
- The bytes row shows the first 48 bytes in hex, in the chosen text encoding.
- **Verification:** an encoded result is decoded again and compared with the
  bytes ("Decodes back to the same 31 bytes"), with a short SHA-256 (hover for
  all of it). A decoded result says whether the input was exactly the canonical
  form for the chosen options.
- **When decoding stops**, a card shows the problem, the offset in characters,
  the text around it with the bad character highlighted, and what the problem
  means: invalid character, wrong padding, data after padding, or cut short.
  **Try lenient** switches the decoder; **Remove the bad character** deletes
  exactly that character. Nothing partial is ever shown as complete.
- Text that the chosen encoding cannot hold (for example an emoji as Latin-1)
  stops with its offset, and offers **Use UTF-8**.
- Bytes that are not text in the chosen encoding stay available: the hex row
  shows them and **Save bytes as a file** writes them exactly.
- **Swap sides** moves the result to the input and flips the direction.
  **Clear** empties the input.
- **Copy as data: URL**, **Export record (JSON)** and **Use in a data: URL**
  work from the current result.
- Paste, or open a text file. Text is limited to 16 MB; the result box shows
  the first 1 MB and Copy and Save use all of it.

## Files, beside the originals

**Encode files** turns `report.pdf` into `report.pdf.base64.txt` in the same
folder. **Decode files** turns `report.pdf.base64.txt` or `report.pdf.b64`
back into `report.pdf`. Pick files or drop them on either card.

- Files stream in 1 MB chunks, so there is no size limit. **Pause all**,
  **Resume all** and **Cancel all** act at the next chunk.
- Every file is checked before it counts as done. An encoded file is decoded
  again and its SHA-256 compared with the source. A decoded file is written to
  a temporary name, read back and hashed before it gets its real name.
- A clash gets a numbered name (`report (1).pdf`), and the final rename never
  replaces a file that appeared meanwhile, so nothing is ever overwritten. A
  failed or cancelled file leaves nothing behind.
- A decoded name without an extension gets one from the data: URL type, or
  from the bytes, or `.bin`.
- A UTF-8 byte order mark and a `data:...;base64,` header are accepted.
  Offsets in error messages count characters from the start of the file.
- Files use the Text tab's alphabet, lines and padding. Files are always
  decoded leniently: line breaks and missing padding are accepted, any other
  character stops that file and names its offset.
- The table shows the source, the name written, the size, progress with time
  left, and the state (Verified, Verified, renamed, or the problem).
  **Show in folder** (or double-click) selects the file in Explorer.

## data: URL

- **Build** from a file (up to 16 MB, picked or dropped on the page) or from
  the Text tab. The MIME type is
  detected from the bytes and can be changed; a charset can be added for text.
  Copy or save the whole URL.
- **Read** a pasted data: URL. It shows the type, the declared MIME type, the
  size, the parameters, whether it was Base64 or percent-encoded, and whether
  the bytes' signature matches the declared type.
- **Preview**: images are drawn from their pixels (with size and colour), text
  is shown as plain text in its charset, and Hex shows the first 4 KB. Text
  over 2 MB and images over 64 MB show a size notice instead. Audio and video
  are not played here; **Save bytes as a file** writes them for your own player.

## History

History keeps names, sizes and hashes of encoded and decoded files, of copied,
saved and exported text, and of built and read data: URLs, newest first. It is
stored in `base64-toolkit-history.jsonl` in the data folder, keeps the newest
500 rows and never leaves the computer.

- **Keep payloads** (off by default) also keeps the input text of text rows.
  Turning it off deletes every kept text from the history at once.
- Double-click a text row to open it on the Text tab: its options always, its
  text when it was kept.
- **Undo** and **Redo** step through the Text tab's last 50 states (text and
  options) in memory.
- **Export history** writes JSON; **Clear history** asks first.

## Stored values

| AppConfig key | Value |
| --- | --- |
| `base64_toolkit_options` | The Text tab's options as JSON |
| `base64_keep_payloads` | `1` or `0` |

## Checks

- `tests/base64_codec.test.mjs`: the codec; a negative build drops padding and
  must fail.
- `tests/base64_files.test.mjs`: round trips over two chunk boundaries, numbered
  names, cancelling, a bad character in the second chunk with its exact offset,
  data: URLs, byte order marks, empty and missing files, and names outside
  ASCII. A negative build drops the encoder's last group and must fail.
- `tests/base64_workbench.test.mjs`: the board's sample text and error, text
  encodings, canonical form, previews, sizes, undo, history with and without
  payloads, and the exported record. A negative build keeps a payload without
  consent and must fail.

## Not done yet

Pausing or cancelling one file alone (the buttons act on all), audio and video
preview, and opening a Text tab session in a second tab are designed but not
built.
