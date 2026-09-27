# Material color picker & color translator

**Surface:** Preferences ▸ Appearance ▸ accent "+" tile → `MD3ColorPickerDialog`
(`src/slic3r/GUI/Widgets/MD3ColorPicker.{hpp,cpp}`), replacing the native
`wxColourDialog`.

## Behavior

- A **continuous (infinite) picker** styled with the MD3 tokens: a
  saturation/value field for the current hue (every RGB colour reachable), a
  continuous hue strip, and a **Material tonal ladder** — eleven tones
  (5…95) of the current pick as one-click quick picks. The ladder re-derives
  for every hue, so there is a fresh Material ramp for any colour on the
  wheel.
- A live preview chip and a `#RRGGBB` hex field in two-way sync (typing a
  full hex jumps the picker; picking updates the field).
- **Color translator**: a Translations column shows the same colour in
  every notation (CSS name, HEX, HEX8, RGB, RGBA, HSL, HSLA, HSV, HWB, XYZ,
  Lab, LCH, OKLab, OKLCH, CMYK), each with a Copy button, alongside an
  opacity slider, an "Enter any format" field, a gamut warning and a WCAG
  contrast readout. See [Color picker translator](color-picker-translator.md)
  for the full contract and precision notes.
- OK feeds the pick into the same `MD3::setAccentSeed` pipeline as the six
  preset swatches; Cancel changes nothing.

## Verification

- Compiles into `libslic3r_gui`; opened headlessly from the accent "+" tile
  via the Lowlevel MCP driving recipe; hex round-trip and tonal-ladder picks
  update the preview and translator live.
