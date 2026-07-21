# Vendored Material Design 3 design system

## Behavior

The native application draws its theme colors, typography, and layout metrics from a single vendored
Material Design 3 kit. The in-repo design source is [`ui-md3/design-system/`](../../../ui-md3/design-system/);
the native source of truth is `src/slic3r/GUI/Widgets/MD3Tokens.hpp`, whose token values match the
kit exactly. C++ code resolves colors through `StateColor::semantic(MD3::Role[, ColorScheme])`,
`ThemeColor`, and `MD3::resolve(role, dark, scheme)` rather than hardcoded hexes.

`MD3Tokens.hpp` was extended to full kit parity (commit `23688c23d`). It now provides:

- the complete role set, including `OnError`, `OnErrorContainer`, and `InversePrimary`, plus scrim
  and shadow tints;
- an elevation ladder `elev1`–`elev5` (offset-y and blur radius, colored by the theme shadow tint);
- `MD3::Viewport` axis and live colors;
- fixed panel, dialog, and content metrics and shape radii under `MD3::Metrics`;
- the full 11-step `MD3::Type` scale (`headline` through `micro`) with the `Roboto` and
  `Roboto Mono` family constants and the `Material Symbols Outlined` icon-font name; and
- `accentFromSeed()`, the seed-ramp port that regenerates the six accent roles from a seed color.

A ground-up migration then converted hardcoded theme colors and fonts across essentially the whole
GUI tree — roughly 120 files over six waves: the shared Widgets library and the ImGui theme; chrome
and status bars; Prepare/Plater; the preview renderer and timeline; gizmos and viewport overlays;
Device, StatusPanel, AMS, DeviceTab, and multi-machine surfaces; Settings, parameters, and Search;
the leaf dialogs including calibration; residual files; and the Project webview CSS (tokenized), with
the Home webview verified tokenized. Numeric and technical values use the new `Label::Mono_*` faces
backed by Roboto Mono.

Contextual schemes swap only the accent roles per workspace and are resolved by the active
workspace: brand green for Prepare and general UI, Preview purple for the G-code preview, and Device
teal for the printer surfaces.

Functional data colors are deliberately preserved and were not migrated: filament swatches, G-code
feature colors, and the 3D paint palettes keep their data-bearing meaning.

## Configuration

- The existing Bambu Studio appearance setting selects light or dark mode. A theme change updates the
  global `StateColor` mode before semantic colors are resolved, then repaints the native widget tree.
- The Settings accent color regenerates the accent ramp live through `accentFromSeed()`.
- Contextual scheme selection is driven by the active workspace, not by a user setting.
- Roboto (Regular, Medium, Bold) and Roboto Mono (Regular, Medium, Bold) ship under
  `resources/fonts` and are registered privately at startup by `Label::initSysFont`; they do not
  modify the user's system font collection.

## Failure modes

- A missing semantic dark-map entry falls back to its light token rather than terminating the app.
- Missing or unavailable preferred fonts fall back through the existing system font path; CJK locales
  use their bundled families because Roboto does not contain those glyphs.
- Functional data colors are intentionally exempt from the token layer; changing them would alter
  meaning, so they are left untouched.
- The `Material Symbols Outlined` icon family is named as a token but the icon-font loading
  infrastructure is not yet in place, so icon glyphs continue to use existing bitmap assets.

## Verification

- Local Release builds of the migrated tree succeed (dependencies and application; VS2022
  BuildTools, Windows SDK 10.0.26100).
- The hosted "Windows build and release" workflow build job (`Build BambuStudio`) succeeded on the
  migrated tree: run `29848731027` (head `7a027fa26`) and the later run `29862992010`
  (head `c700c91b0`, remote `master`). The publish job is a separate concern; see
  [`../../../HANDOFF.md`](../../../HANDOFF.md).
- An element-by-element parity audit against the design kit produced a matrix. The color, token, and
  typography layer is reported complete: the final scan found 21 residual theme literals, of which 18
  were addressed in this effort and 3 were retained intentionally over fixed bitmap assets. The
  remaining deltas are structural component anatomy, not mis-colorings: the camera-HUD overlay
  system, the Material Symbols icon-font infrastructure, and some pill-geometry variants. These are
  tracked as future work in [`../../../ROADMAP.md`](../../../ROADMAP.md).
- Fresh full-compositor captures of the fully token-migrated native surfaces are still pending; the
  installed-app captures currently in the README predate the full sweep.
