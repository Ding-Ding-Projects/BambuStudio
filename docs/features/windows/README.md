# Windows features

- [Windows-only platform policy](windows-only-platform.md)
- [Native Material Design 3 UI](md3-native-ui.md)
- [Keyboard, assistive, and responsive GUI accessibility](gui-accessibility.md)
- [English, Hong Kong Cantonese, and bilingual modes](language-modes.md)
- [Funny levels and dialog emojis](funny-levels-and-dialog-emojis.md)
- [Ink terminology (filament → ink, AMS → Ink Dispenser)](ink-terminology.md)
- [Appearance customization](appearance-customization.md)
- [Renamable app name (display label only)](renamable-app-name.md)
- [Per-element appearance editor (right-click ▸ Edit appearance..., Ctrl+Shift+E)](appearance-editor.md)
- [Regex builder](regex-builder.md)
- [Material context menus (search, shortcuts, submenus)](material-context-menus.md)
- [Command palette (Ctrl+Shift+F)](command-palette.md)
- [In-app changelog viewer (Help ▸ What's new)](changelog-viewer.md)
- [Prepare sidebar search (settings + filament slots)](sidebar-search.md)
- [Material color picker & color translator](md3-color-picker.md)
- [Color picker translator (infinite picker, every notation, gamut, contrast)](color-picker-translator.md)
- [Bulk filament actions](bulk-filament-actions.md)
- [Stop-print safety interlock](stop-print-interlock.md)
- [Destructive-action super confirmation (two keys + full slide)](super-confirmation.md)
- [Print simulation playback (feedrate-true)](print-simulation.md)
- [AI printer watch (local models)](ai-printer-watch.md)
- [AI filament scanner (QR phone upload → AMS slot)](ai-filament-scanner.md)
- [Smart home: printer handover, TTS narrator, and alert lights](smart-home.md)
- [Release splash art (fresh dim sum per release)](release-splash-art.md)
- [Dim sum startup surprise (one launch in ten)](dim-sum-surprise.md)
- [Native visual smoke test](native-visual-smoke.md)
- [Cloud web-page failure recovery](cloud-web-recovery.md)
- [Software OpenGL fallback (Mesa llvmpipe)](software-gl-fallback.md)
- [App updates from this fork's releases](app-updates.md)

Windows is the active release target for this fork. macOS and Linux source support remains upstream,
but those platforms are not part of the fork's release acceptance gate.

The native application now exposes one tightly scoped HTTP contract only while the user explicitly
enables Home Assistant printer discovery. Its endpoint, security boundary, and Postman collections
are documented under [HTTP/API features](../api/README.md). It is a short-lived credential handover,
not a general remote-control API.

The `DeviceWeb` sub-project (`src/slic3r/GUI/DeviceWeb/`) remains an in-app webview front-end bundled
with the application, not a served HTTP API; this repository publishes no separate API contract for
it.
