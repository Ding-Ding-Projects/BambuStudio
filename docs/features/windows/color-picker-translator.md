# Color picker translator (infinite picker)

**Surface:** every `MD3ColorPickerDialog` — Preferences ▸ Appearance ▸ accent
**Custom…**, the preset editor's `ColourPicker` field (`Field.cpp`), and the
bulk filament colour action (`BulkFilamentDialog.cpp`).

**Code:** `src/slic3r/GUI/Widgets/MD3ColorPicker.{hpp,cpp}` (dialog) and
`src/slic3r/GUI/Widgets/ColorSpaces.hpp` (header-only maths, namespace
`MD3::Color`).

This article extends [Material color picker & color translator](md3-color-picker.md),
which describes the saturation/value field, hue strip and Material tonal
ladder. Everything below is additional.

## Behavior

- **Infinite picker.** The saturation/value field and hue strip reach every
  sRGB colour; there is no finite swatch grid anywhere in the dialog. The
  field and strip are keyboard operable: arrow keys nudge by 1 %, Shift
  by 10 %.
- **Opacity slider** (0–100 %) using the shared MD3 `Slider` (focusable,
  screen-reader named "Opacity"). The alpha is carried in
  `GetColour().Alpha()`; callers that only read `#RRGGBB` are unaffected.
  The preview chip shows the colour composited over the dialog surface.
- **Translations column.** The current colour written in every supported
  notation, each in a read-only field with its own Copy button (accessible
  names `Copy HEX`, `Copy OKLCH`, and so on). Rows, in order:

  | Row | Format | Notes |
  |---|---|---|
  | Name | CSS colour name | exact 8-bit match only; an em dash when none |
  | HEX / HEX8 | `#RRGGBB` / `#RRGGBBAA` | alpha in HEX8 |
  | RGB / RGBA | `rgb(r, g, b)` / `rgba(r, g, b, a)` | 0–255, alpha 0–1 |
  | HSL / HSLA | `hsl(h, s%, l%)` | degrees, percent |
  | HSV | `hsv(h, s%, v%)` | also parsed as `hsb()` |
  | HWB | `hwb(h w% b%)` | CSS Color 4 syntax |
  | XYZ | `xyz-d65(x y z)` | D65 white, Y(white)=1 |
  | Lab / LCH | `lab(L a b)` / `lch(L C h)` | CIELAB, L 0–100 |
  | OKLab / OKLCH | `oklab(L a b)` / `oklch(L C h)` | L 0–1 |
  | CMYK | `cmyk(c%, m%, y%, k%)` | **naive** formula, not a press profile |

- **Enter any format.** One field that parses any of the notations above
  (comma or space separated, optional `/ alpha`, `%`/`deg`/`turn` units,
  `none`) plus the 148 CSS colour names, and jumps the picker to it. The
  inline status line names the active colour space (`Parsed as OKLCH.`) or
  says why the text was not understood. Parsing is live on every keystroke;
  there is no Enter to press.
- **Gamut label and warning.** The `Active space … Gamut …` line tells which
  space the colour was last defined in and whether it lies inside sRGB. When
  a typed Lab/LCH/OKLab/OKLCH/XYZ value is outside sRGB the status line turns
  to the Error role and reads *"Outside the sRGB gamut: the picker shows the
  nearest sRGB color (clipped)"* — a non-blocking inline text, never a
  dialog. The clipped value is what the picker shows and what OK returns.
- **Contrast readout.** Two WCAG 2.x ratios against a foreground/background
  pair: the picked colour as a *background under the caller's text*, and as
  *text on the caller's surface*. Each ratio carries its grade (AAA / AA /
  AA large text only / fails WCAG). Alpha is composited over the surface
  before measuring, so a translucent pick reports what the eye would see.

## Configuration

- The contrast pair is a constructor parameter:

  ```cpp
  MD3ColorPickerDialog dlg(parent, initial);                      // OnSurface vs Surface (MD3 tokens)
  MD3ColorPickerDialog dlg(parent, initial, { text_colour, surface_colour });
  ```

  `MD3ColorPickerDialog::defaultContrastContext()` returns the default pair.
  Either half may be `wxNullColour` to skip that ratio.
- Nothing is persisted by the dialog itself; the caller stores the pick as
  before (`ui_accent_seed`, the preset option, the staged filament colour).
- The dialog has no settings of its own and nothing to opt out of; the three
  language modes and both funny-level sliders apply through `_L()` like every
  other surface.

## Precision

Documented in the header and asserted by `tests/color_spaces`: every
sRGB → space → sRGB round trip reproduces the 8-bit input exactly. In
double precision the residuals are ~5e-5 for XYZ/Lab/LCH (the published
7-digit sRGB matrices are not exact inverses), ~1e-5 for OKLab/OKLCH, and
~1e-9 for HSL/HSV/HWB/CMYK. Hue is reported as 0 at zero chroma.

## Failure modes

- **Unrecognised text** — status line (Error role): *Not a recognised
  color…* listing the accepted notations; the picker keeps its current
  colour. No modal, no beep.
- **Out-of-gamut input** — parsed, flagged, shown clipped with the warning
  above. The returned colour is always a valid sRGB triple.
- **Clipboard busy** — Copy silently does nothing when `wxTheClipboard`
  cannot be opened; a successful copy is confirmed in the status line.
- **Colour with no CSS name** — the Name row shows an em dash and its Copy
  button is disabled.
- **Oversized input** — the any-format field is capped at 128 characters,
  the longest legitimate notation being well under that.

## Security considerations

- All parsing and conversion is local, allocation-bounded (a fixed number of
  numeric tokens; `std::strtod` on short strings) and never touches the
  network, the file system or app settings.
- The parser accepts no code, no URLs and no escape sequences; anything that
  is not a recognised notation is rejected, not guessed.
- Copy writes only the displayed text to the system clipboard, on the user's
  explicit click.

## Verification

- `tests/color_spaces/color_spaces_tests.cpp` (Catch2 target
  `color_spaces_tests`, registered in `tests/CMakeLists.txt`): round trips
  for white, black, mid grey and the sRGB primaries through every space;
  landmark values (D65 white, L=100, pure-red hues); a P3-ish
  `oklch(0.85 0.3 145)` flagged clipped; named-colour lookup both ways;
  black-on-white contrast = 21; formatting of every row; parsing of every
  notation including `/ alpha`, percentages, `hsb()`, `device-cmyk()`, names
  and eight kinds of garbage. Compiled and run standalone with MSVC:
  **All tests passed (223 assertions in 8 test cases)**.
- `MD3ColorPicker.cpp` passes `cl /Zs` with the `libslic3r_gui` include
  directories and defines.
- Layout budget: two columns (picker left, fifteen translation rows right)
  keep the dialog inside 1000×600 DIP; the status and contrast lines reserve
  their wrapped height up front so the column never re-flows.
- Headless HuiShot of the built dialog is not part of this change (no full
  build was run); the visual smoke recipe in
  [Native visual smoke test](native-visual-smoke.md) covers it on the next
  build.

## Suggested articles

- [Material color picker & color translator](md3-color-picker.md) — the
  saturation/value field, hue strip and tonal ladder this article builds on.
- [Appearance customization](appearance-customization.md) — where the picker
  feeds the accent seed.
- [Keyboard, assistive, and responsive GUI accessibility](gui-accessibility.md)
  — the focus, naming and narrow-width rules the dialog follows.
- [Bulk filament actions](bulk-filament-actions.md) — the second caller of
  the picker.
