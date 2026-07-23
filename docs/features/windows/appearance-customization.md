# Appearance customization

Runtime, persisted appearance controls in Preferences ▸ Appearance: theme,
density, accent color (presets + free picker), full UI font customization, a
live MD3 token preview, and one-click reset to defaults. All controls apply
live where feasible.

## Behavior

| Control | AppConfig key | Applies via |
| --- | --- | --- |
| Theme (Light/Dark) | `dark_color_mode` | `apply_dark_mode` → full re-theme walk |
| Density (Comfortable/Compact) | `ui_density` | `MD3::Metrics::setDensity` + `refresh_md3_appearance` |
| Accent: 6 preset swatches | `ui_accent_seed` | `MD3::setAccentSeed` (derives 6 tonal roles for light+dark) |
| Accent: **Custom…** picker | `ui_accent_seed` | same pipeline — any `wxColourDialog` color is a valid seed |
| Font family (installed + bundled Roboto, CJK-safe fallback) | `ui_font_family` | `Label::rebuild_fonts` |
| Text size (Small/Default/Large) | `ui_font_scale` | `Label::rebuild_fonts` (scale multiplies the MD3 type ramp) |
| **Live MD3 preview panel** | — | repaints via `StateColor::semantic` + `Metrics::active()` on every refresh |
| **Reset appearance to defaults** | writes all of the above | re-syncs runtime state under a reentrancy guard |

- The custom picker keeps the preset swatch rings truthful: picking a color
  equal to a preset lights that swatch; anything else clears all rings.
- Reset restores density=comfortable, seed=`#146c2e` (Brand — clears the
  accent override), font family=default, scale=1.0. Theme is deliberately
  excluded (a light/dark flip mid-reset is disruptive). Programmatic
  re-selection runs under a guard because `MultiSwitchButton::SetSelection`
  emits its selection event; the guard keeps re-fired handlers from
  re-persisting mid-reset.

## Configuration

All keys live in the `app` section of AppConfig and are applied at startup in
two synchronized places: `GUI_App::on_init_inner` and
`PreferencesDialog::apply_persisted_md3_appearance` (invalid values normalize
to defaults).

## Failure modes

- **Invalid persisted seed/scale** → normalized to Brand seed / clamped scale
  at read time.
- **Density/radius live-relayout limitation**: windows built before a density
  change repaint but fully re-lay-out only after restart (documented in the
  Preferences source; consistent with the density note there).
- **User font lacking CJK glyphs in a CJK language mode** → automatic fallback
  to the locale's CJK face (no tofu).

## Security considerations

Font faces are validated against the enumerated system font table before use;
on Windows, fonts reaching `wxGraphicsContext` paths are strictly validated
(enumerator check, cached per face) because GDI+ heap-corrupts on unregistered
variable fonts — see the plain-GDI MaterialIcon rendering rationale.

## Verification

- Compiled clean after adversarial review; review findings (reset reentrancy,
  ineffective GC-font guard, custom-picker ring truthfulness) fixed.
- Appearance page and each control captured per button in the screenshot
  matrix under `docs/screenshots/appearance/`.
