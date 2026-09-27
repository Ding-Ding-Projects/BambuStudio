# AI printer watch (local models)

**Surface:** Preferences ▸ Other ▸ *AI printer watch*; runtime in
`src/slic3r/GUI/PrinterWatch.{hpp,cpp}`.

## Behavior

- While the app is running (the user need not be actively watching), a timer
  (default every 5 minutes, `printer_watch_interval`, min 1) captures a frame
  of the Device page's **live camera view** (PrintWindow of the media
  control; skipped silently when no stream is rendering — blank/uniform
  frames never produce a request).
- The frame (bounded to 768px, JPEG) goes to a **local** model over the
  Ollama HTTP API (`printer_watch_endpoint`, default
  `http://127.0.0.1:11434`, `/api/generate`, `stream:false`).
- The model is asked for a two-line verdict: `OK`/`PROBLEM` plus a short
  summary. `OK` becomes a quiet info toast ("Printer watch: …"); `PROBLEM`
  becomes a **persistent warning toast** describing what likely happened
  (spaghetti, detachment, blob…) and one concrete fix suggestion.

## Models

- `printer_watch_model` (default `qwen2.5vl`). Per the stated preference the
  supported family is **gpt-oss / Qwen / Gemma** — frames need a
  vision-capable tag (`qwen2.5vl`, `gemma3`); text-only tags such as
  `gpt-oss` cannot read images and are only useful for text-side
  summarization.

## Privacy & failure modes

- **Opt-in and OFF by default** (`printer_watch_enabled`). Frames never
  leave the machine; the only endpoint is localhost. One request in flight
  at a time; results are marshalled to the UI thread.
- Ollama not running / model missing → logged at info level, **no nagging
  toast** every interval. No live view rendering → the tick is a no-op.

## Verification

- Compiles into `libslic3r_gui`; the capture path (window blit, uniform-frame
  skip, JPEG/base64 bounding) and the disabled-by-default gating are
  verified headlessly. An end-to-end run needs a printer with a live stream
  plus a local Ollama with a vision model — recorded as pending hardware
  verification.
