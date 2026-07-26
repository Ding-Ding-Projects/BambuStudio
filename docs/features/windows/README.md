# Windows features

- [Native Material Design 3 UI](md3-native-ui.md)
- [English, Hong Kong Cantonese, and bilingual modes](language-modes.md)
- [Ink terminology (filament → ink, AMS → Ink Dispenser)](ink-terminology.md)
- [Appearance customization](appearance-customization.md)
- [Regex builder](regex-builder.md)
- [Command palette (Ctrl+F)](command-palette.md)
- [Prepare sidebar search (settings + filament slots)](sidebar-search.md)
- [Material color picker & color translator](md3-color-picker.md)
- [Bulk filament actions](bulk-filament-actions.md)
- [Stop-print safety interlock](stop-print-interlock.md)
- [Print simulation playback (feedrate-true)](print-simulation.md)
- [AI printer watch (local models)](ai-printer-watch.md)
- [AI filament scanner (QR phone upload → AMS slot)](ai-filament-scanner.md)
- [Smart home: TTS narrator, Home Assistant, alert lights](smart-home.md)
- [Release splash art (fresh dim sum per release)](release-splash-art.md)
- [Native visual smoke test](native-visual-smoke.md)
- [Software OpenGL fallback (Mesa llvmpipe)](software-gl-fallback.md)

Windows is the active release target for this fork. macOS and Linux source support remains upstream,
but those platforms are not part of the fork's release acceptance gate.

No Postman collection is applicable: the native application exposes no HTTP API. The `DeviceWeb`
sub-project (`src/slic3r/GUI/DeviceWeb/`) is an in-app webview front-end bundled with the
application, not a served HTTP API; this repository publishes no API contract for it, so no Postman
collection exists for it either.
