# BambuStudio feature documentation

This directory documents the `Ding-Ding-Projects/BambuStudio` fork's maintained features and release
surfaces. Upstream Bambu Studio material remains in [`../doc/`](../doc/).

## Categories

- [Design system](features/design-system/README.md) — vendored Material Design 3 token layer, the
  ground-up color/type/metric migration, and the parity audit.
- [GitHub Pages site](features/pages/README.md) — the published tabbed site: navigation, language
  modes and funny levels, the shared regex builder, the changelog viewer, and the 444-case layout
  gate that guards its deploy.
- [Windows](features/windows/README.md) — native MD3 UI, language modes, and visual verification.
- [Prepare](features/prepare/README.md) — Prepare-workspace features, including the dockable sidebar.
- [Model preview](features/model-preview/README.md) — the MakerWorld OpenGL preview shown before
  import.
- [G-code preview](features/gcode-preview/README.md) — the sliced Preview page: toolpath viewport,
  the color-scheme legend, and the layer/transport controls.
- [HTTP/API](features/api/README.md) — short-lived, user-enabled network contracts, including the
  authenticated Home Assistant printer-discovery handover and its Postman collections.
- [Releases](features/releases/README.md) — installer behavior, CI gates, SBOMs, attestations, and
  immutable-release policy.

The current delivery state and external dependencies are tracked in [`../HANDOFF.md`](../HANDOFF.md).
Planned work is tracked in [`../ROADMAP.md`](../ROADMAP.md).
