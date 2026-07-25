# AI filament scanner

**Surface:** `File ▸ AI filament scanner…` → `FilamentScanDialog`
(`src/slic3r/GUI/FilamentScanner.{hpp,cpp}`, QR encoding vendored from
Project Nayuki's MIT `qrcodegen` under `GUI/third_party/`).

## Flow

1. The dialog starts a **LAN-only upload server** (boost::beast, ephemeral
   port) and shows a **QR code** plus the plain URL. The URL carries a
   random per-session token — requests without it get 404, uploads are
   capped at 12 MB, and the server dies with the dialog.
2. The phone opens the self-contained upload page (bilingual EN/Cantonese,
   no external assets, camera capture input) and posts the photo.
3. The desktop asks the **local Ollama vision model** (same
   `printer_watch_model`/`printer_watch_endpoint` settings; default
   `qwen2.5vl`) for strict JSON: `{type, brand, color_hex, confidence}`.
4. **AMS auto-assignment:** the first empty AMS tray is configured with the
   identified type + colour (`command_ams_filament_settings`, temperature
   range from a per-material table); without a printer the result falls
   back to "external spool" guidance.
5. **Auto print settings:** the best matching filament preset is selected
   for extruder 1 — brand-mapped onto the vendor families that actually
   ship in `resources/profiles/BBL.json` (Bambu, SUNLU, PolyLite,
   PolyTerra, Overture, eSUN, Fiberon) with `Generic <TYPE>` as the
   fallback — and the sidebar presets refresh.
6. **Announcement:** a HUGE flashing overlay ("AMS A SLOT 2" + filament +
   colour swatch; click/Esc/12 s dismisses; flash skipped under reduced
   motion), an optional **TTS line** (checkbox in the dialog, persisted,
   also routed to Home Assistant speakers when configured). AMS hardware
   exposes no tray-LED blink API, so the flash is on-screen — recorded as
   a deviation, not pretended.

## Privacy & failure modes

- The photo never leaves the machine except phone→desktop over the LAN;
  identification is localhost Ollama. Server errors, missing models and
  malformed model replies all surface as plain status text; nothing blocks.

## Verification

- Compiles into `libslic3r_gui`; QR renders from the vendored encoder;
  server start/stop and the token-gated 404 path are exercised headlessly.
  End-to-end (real phone photo → AMS slot) needs hardware — recorded as a
  pending hardware pass alongside the printer watch.
