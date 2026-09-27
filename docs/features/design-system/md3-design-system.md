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
  were addressed in this effort and 3 were retained intentionally over fixed bitmap assets (anchored
  and justified in "Retained theme literals" below). The remaining deltas are structural component
  anatomy, not mis-colorings: the camera-HUD overlay system, the Material Symbols icon-font
  infrastructure, and some pill-geometry variants. These are tracked as future work in
  [`../../../ROADMAP.md`](../../../ROADMAP.md).
- Pill geometry in the shared Widgets library is complete: every control the kit calls a pill derives
  its corner radius from height / 2 at paint or layout time. `Button::applyMD3Style()` sets
  `SetCornerRadius(FromDIP(height) / 2.0)` and re-runs on `Rescale()`, so the radius survives
  DPI/density changes; `SwitchButton` draws its track and thumb with `size.y / 2` every paint. The
  rule is now named additively as `MD3::Metrics::pill_radius(height)`. Segmented controls
  (`SwitchBoard`, `MultiSwitchButton`) are intentionally not pills — the kit draws them with a fixed
  track radius. The remaining pill-geometry variants are feature-level controls with no dedicated
  widget class (filter/choice chips, the search-field pill, the settings nav-item pill) and belong to
  the chrome/settings surfaces.

### Retained theme literals

Each site below keeps a raw color literal on purpose because a fixed bitmap asset bakes the color a
theme role would otherwise fight. Every one is theme-independent by design, so tokenizing it to a
role that flips between the light and dark schemes would invert its own contrast and break the
element in one mode. The three retained-over-bitmap literals from the audit are:

- **Assembly-tree delete badge** — `src/slic3r/GUI/Overview/AssemblyStepsUtilsImgui.cpp:4646-4647`.
  `badge_bg = IM_COL32(77, 77, 77, 255)` backs a light cross drawn by tinting the fixed
  `cross_dark.svg` asset (`m_tree_icon_cross_dark`, loaded at line 3901), which bakes a light
  `#E0E0E0` glyph so it reads on the always-dark badge (Figma 4098:10802/10803). `badge_cross =
  IM_COL32(255, 255, 255, 255)` is that tint. The badge is a theme-independent overlay: a neutral
  role would flip the backing light in dark mode and hide the light cross, so both literals stay
  bound to the asset. (Sibling close-X at line 5295-5300 can tint the same texture from
  `OnSurface`/`Outline` only because its background is a real themed surface, not a fixed badge.)
- **Helio rating header banner** — `src/slic3r/GUI/HelioReleaseNote.cpp:3168-3169`.
  `header_bg = wxColour(16, 16, 16)` (with its companion `header_text = wxColour("#FEFEFF")`) wraps
  the fixed `helio_icon` brand bitmap and, per the in-code comment, "always stays dark for Helio
  branding" in both themes. No inverse role stays light in both modes, so the near-black banner and
  white text are kept to match the partner brand asset rather than tokenized to a flipping surface
  role.

Two further literals are retained for reasons that are not bitmap-asset cases; they were not part of
the audit's "3 over fixed bitmap assets" count and are tracked as token-parity follow-ups:

- **Preview-timeline current-step marker** — `AssemblyStepsUtilsImgui.cpp:823` (white knob
  `IM_COL32(255, 255, 255, 255)`) and `:835` (near-black numeral `IM_COL32(0x32, 0x3A, 0x3D, 255)`).
  These are a coupled fixed marker taken from the Figma spec (white pill + dark digits); tokenizing
  either half alone would invert the other's contrast, so the pair is kept theme-independent.
- **"Unsaved view" amber dot** — `AssemblyStepsUtilsImgui.cpp:4606`, `IM_COL32(0xFF, 0xA0, 0x00,
  255)`. `MD3Tokens.hpp` has no amber/warning role for this status color to resolve to (a
  token-parity gap), so the literal is kept until such a role exists.
- Fresh full-compositor captures of the fully token-migrated native surfaces are still pending; the
  installed-app captures currently in the README predate the full sweep.
