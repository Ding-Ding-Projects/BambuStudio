# Bambu Studio MD3 Design System

Material Design 3 (Material You) design system for the **native C++ (wxWidgets/OpenGL) rewrite of Bambu Studio**, regenerated from the design artifacts already living in the codebase. This is a desktop-application design system — not a website or web-app style guide. Every value here is meant to be transcribed into native code.

## Sources (all in the mounted `BambuStudio` folder)

- `src/slic3r/GUI/Widgets/MD3Tokens.hpp` — **the native source of truth**: `MD3::Role` enum, light/dark role hexes, `MD3::ColorScheme` (Brand / Preview / Device) contextual schemes, `MD3::Metrics` density structs and fixed bar heights. Token values in this system match it exactly.
- `ui-md3/design-source/Bambu Studio.dc.html` — the original Claude Design source (all 9 screens, dialogs, drawer, snackbars) that the MD3 concept was built from. Component specs here are lifted verbatim from its markup.
- `ui-md3/design-source/SearchField.dc.html` — the regex-capable search field component.
- `ui-md3/app/styles.css` — the shipped web token system (identical to MD3Tokens.hpp).
- `HANDOFF.md` — native integration state; names the brand-green / Preview-purple / Device-teal contextual schemes.
- Repo: https://github.com/Ding-Ding-Projects/BambuStudio (fork of the open-source Bambu Studio project). Independent concept; not affiliated with Bambu Lab.

## The product

Bambu Studio is a desktop 3D-printing slicer. One window, nine workspaces reached from a persistent tab bar: Home, Prepare (3D editor), Preview (G-code), Device (printer monitor), Multi-device, Project, Calibration, Filament manager, Settings. Plus an app-local Git-backed **version history** drawer, Export/Send-to-print/Add-filament dialogs, an appearance popover, and snackbars.

## Color architecture

Three layers:

1. **Neutral roles** (shared everywhere): surface, surface-dim/bright, five surface-container steps (lowest→highest), on-surface(-variant), outline(-variant), inverse, error, scrim, shadow.
2. **Accent roles** (six): primary, on-primary, primary-container, on-primary-container, secondary-container, on-secondary-container.
3. **Contextual schemes** swap ONLY the six accent roles per workspace (`MD3::ColorScheme`):
   - **Brand (green)** seed `#146c2e` — Prepare + all general UI. Light primary `#146c2e`, dark primary `#8bd89b`.
   - **Preview (purple)** seed `#7c5cff` — light `#7050e8`, dark `#ad98ff`.
   - **Device (teal)** seed `#14b8a6` — light `#0f766e`, dark `#5eead4`.

In CSS: `data-theme="light|dark"` picks the theme, `data-scheme="preview|device"` on a workspace root swaps accents. In C++: `MD3::resolve(role, dark, scheme)`.

**Accent seed algorithm** (Settings → accent color regenerates the ramp live; port of `accentVars()` in the design source): clamp seed HSL saturation to 32–92, then
- dark: primary L76 · on-primary L16 · primary-container L28 @ s×0.9 · on-primary-container L90 · secondary-container L26 @ s×0.35 · on-secondary-container L88
- light: primary L36 · on-primary white · primary-container L88 @ s×0.7 · on-primary-container L12 · secondary-container L90 @ s×0.45 · on-secondary-container L20

## CONTENT FUNDAMENTALS

- Sentence case everywhere ("Add filament", "Send print", "Restore this version"). Never Title Case; SCREAMING CAPS only in 11px section labels via CSS `text-transform:uppercase`.
- Terse, imperative button copy: verb first ("Slice plate", "Export 12"). Counts are appended to verbs, not parenthesized.
- Supporting copy is one short sentence, no marketing voice: "Pick a preset to add to this project."
- Numeric/technical values are ALWAYS Roboto Mono: temperatures, percentages, times ("1h 24m"), dimensions, commit hashes ("#a1f9c02"), coordinates.
- Units use middle dots as separators: "23.4 g · 7.85 m", "0.4 nozzle · Connected".
- Second person, but rarely; UI mostly labels things. Status text is fragmentary: "Layer 124 / 180 · 42 min remaining".
- No emoji in UI copy (the two 🌡/▤ glyphs in the camera HUD are icon stand-ins, not tone).
- Snackbar messages are past-tense confirmations: "Restored project to #a1f9c02".

## VISUAL FOUNDATIONS

- **Tone**: calm, tinted neutrals (violet-tinted grays), one saturated accent per workspace. Light surfaces sit around L97; dark around L11.
- **Type**: Roboto 300–700 + Roboto Mono for data. Base 14px (13px compact). Scale in `tokens/typography.css`; 11px/600/uppercase/+.6px section labels are the strongest recurring signature.
- **Icons**: Material Symbols Outlined ligature font, 15–24px inline (36px hero). Active/emphasized states switch the FILL variation axis to 1.
- **Shape**: full pills for standalone buttons/chips (radius = height/2); 28px dialogs and hero cards; 20px home cards; `--radius` (16/12) content cards; 14px icon tiles; 12px rail buttons + snackbars; 10px `--radius-s` inputs/selects; 8px tiny controls.
- **Elevation**: soft single shadows tinted by `--md-shadow` (see `--md-elev-1..5`); dark theme deepens shadow alpha to .5. Surfaces layer by container step, not by shadow — shadow is reserved for floating chrome (toolbars, dialogs, drawer, snackbar, popover).
- **Borders**: 1px `--md-outline-variant` on resting cards/tables; hover promotes the border to `--md-primary` (signature hover). 1px `--md-outline` on outlined buttons/selects; 2px `--md-primary` on the selected plate chip; dashed `--md-outline` for "add" affordances.
- **Hover states**: transparent → `--md-sc-high`/`--md-sc-highest` wash on ghost controls; `filter:brightness(1.06–1.08)` on filled controls; border-color promotion on cards. Press states: none beyond hover (desktop app).
- **Animation**: 150–220ms ease. `mdfade` (fade + 10px rise + .99 scale) for dialogs/popovers/drawer, `scrimin` for scrims, `pulse` for live dots, 2s infinite. No bounces, no springs.
- **Layout**: fixed chrome — 46px title bar, 52px tab bar, 66px Prepare action bar, 58px Preview timeline, `--rail` (60/50) gizmo rail, `--sidebar` (344/312) right sidebar, 230px settings nav, 410px history drawer. Content pages center at max-width 1080–1300px. Grids use 16px gaps (`--gap` inside panels).
- **Density**: two modes (comfortable/compact) scale gap/pad/row/font/rail/sidebar/radius together — see `tokens/density.css`.
- **Backgrounds**: flat token surfaces. Exceptions: viewport radial gradient (surface-bright → surface-dim), home hero linear gradient (primary-container → secondary-container), repeating-stripe placeholders for thumbnails/camera.
- **Transparency/blur**: none — desktop app, opaque surfaces only (camera HUD chips use rgba(0,0,0,.5) over video).

## ICONOGRAPHY

Single icon system: **Material Symbols Outlined** variable ligature font (Google Fonts), rendered via `<span data-icon>icon_name</span>` (`[data-icon]` rule in `tokens/typography.css`). Weight 400; `font-variation-settings:'FILL' 1` for active/emphasized glyphs (filled play_arrow, active tab, print CTA). No SVG icon set, no PNG icons, no emoji. Viewport axis letters X/Y/Z are colored text (`--md-axis-*`). The app logo is the `deployed_code` glyph on a primary tile — there is **no bespoke brand mark** in the sources; render the wordmark "Bambu Studio" in Roboto 500 where identity is needed.

## C++ handoff

`guidelines/cpp-mapping.html` renders the full mapping table. Pattern: CSS `--md-sc-high` ↔ `MD3::Role::SurfaceContainerHigh` ↔ `MD3::resolve(Role::SurfaceContainerHigh, dark, scheme)`. Density: `MD3::Metrics::comfortable{12,16,40,14,60,344,16,10}` / `compact{7,10,32,13,50,312,12,8}` = {gap, padding, row_height, font_size, rail_width, sidebar_width, radius, small_radius}. Fixed heights: top_bar 46, navigation_bar 52, prepare_actions 66, preview_timeline 58.

## Index

- `styles.css` → `tokens/` colors · schemes · typography · density · base
- `guidelines/` — foundation specimen cards (colors, schemes, seed algorithm, type, icons, density, elevation, C++ mapping)
- `components/actions` — Button, IconButton
- `components/selection` — Chip, SegmentedControl, Switch, Slider, Checkbox
- `components/fields` — SearchField, SelectField, ValueField
- `components/containment` — Card, SectionHeader, Badge, ProgressBar, Dialog, Snackbar
- `components/navigation` — TitleBar, TabBar, GizmoRail, NavItem
- `ui_kits/bambu-studio/` — interactive recreation of all 9 workspaces + Export/Send/Add-filament dialogs + version-history drawer (theme, density, accent-seed and contextual-scheme switching)
- `SKILL.md` — agent-skill entry point

## Intentional additions

None. Every component and value traces to the design source or MD3Tokens.hpp.
