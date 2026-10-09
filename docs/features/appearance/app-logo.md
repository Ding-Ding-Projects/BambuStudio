# App logo

Choose the mark the app shows for itself in **Preferences > Appearance > App logo**.
The choice changes presentation only. The installed program, its executable
icon, shortcuts, installer, update feed, application ids and data folder keep
the shipped identity.

## Where the logo appears

| Surface | Size | When it updates |
| --- | --- | --- |
| Title bar tile (left of the menus) | 26 px, scaled for the display | At once |
| Window and taskbar icon | 16, 20, 24, 32, 40, 48, 64, 128 and 256 px | At once |
| About dialog banner | 64 px tile over the shipped mark | Next time the dialog opens |
| Startup screen | 122 px | Next start |

Every size is rendered from the source, never resampled from another size. A
render is generated at no more than 256 pixels; a larger display surface
resamples that bounded render up. On the green About banner a transparent logo
sits on a white rounded tile so it stays visible.

## Choosing a source

The source list is a radio group: Tab reaches it, the arrow keys, Home and End
move the selection, and each row reports its radio role, name and checked state
to screen readers. The search field above it filters the rows; its `.*` toggle
and tune button give the shared regex builder. When nothing matches, the panel
says so instead of showing an empty list.

| Source | What it shows |
| --- | --- |
| Shipped mark (default) | The original artwork on every surface |
| Green printer, Blue layers, Amber nozzle | Original printer-themed marks drawn from local geometry |
| Custom image (unavailable) | Listed but disabled; see below |

Choosing a row applies it immediately and saves it. **Reset to shipped mark**
restores the shipped artwork everywhere and clears the saved setting.

## Presentation controls

These apply to the presets; with the shipped mark selected they are disabled and
the panel says why.

- **Image fit**: contain shows the whole mark; cover crops it to fill.
- **Horizontal and vertical focal point**: where the mark sits, or which part a
  cover crop keeps (0–100%).
- **Safe area inset**: 0–25% of empty margin on every side.
- **Transparent background**, or an opaque **background color** chosen with
  the shared color picker.

Live previews at 16, 24, 32 and 64 pixels show the same render the surfaces
use at those sizes. Small sizes use nearest-neighbor sampling, so fine detail can drop out; a
transparent mark can lose contrast on some backgrounds, which the panel points
out.

## What is stored

Only ten small numbers are saved, in the `app_logo` key of the app's own
settings file: the source, the preset, the fit, both focal points, the inset,
the transparency flag and the background color. No image, file name or path is
stored. An empty key means the shipped mark. A value that does not parse shows
the shipped mark, and the panel reports it as invalid without overwriting it
until you choose a logo or reset. Settings from the earlier one-source format
still load as their preset.

## States

| State | What the panel says |
| --- | --- |
| No custom logo | Shipped mark in use, no custom image loaded |
| Preset in use | Which preset is in use and that it is saved |
| Invalid saved value | The shipped mark is shown; the saved value is kept |
| Applied | Which source was applied and saved |
| Reset | The shipped mark is restored and the saved setting cleared |
| Save failed | The previous logo is kept |
| No search match | No logo source matches the search |

## Custom images

Custom image import is not available yet. The row and the **Import custom image**
button stay visible and disabled, and no image file is ever opened. The core
has a bounded decode primitive for exact uncompressed 24-bit BMP files
(4 MiB, 2048 pixels per side, 1,048,576 pixels), but it is not an isolation
boundary and is not connected to a file picker. Import, with its loading,
converted, replace and conversion-failure states, waits for an isolated decoder
process with operating-system CPU and memory limits and verified conversion.

## Not covered yet

- Scheduled settings, appearance export and import, local history snapshots,
  command palette teleport and per-element appearance editing do not include the
  logo yet. The Preferences settings search does find the App logo row.
- Secondary dialog window icons keep the shipped icon; no notification draws the
  app mark.
- Packaged rendering at every display size still needs a capture from a built
  app.

## Verification

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc -Itests -Itests/catch2 tests/app_logo/app_logo_tests_main.cpp -o "$env:TEMP/app-logo-model.exe"
& "$env:TEMP/app-logo-model.exe"
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests/app_logo/test_logo.cpp -o "$env:TEMP/app-logo-core.exe"
& "$env:TEMP/app-logo-core.exe"
node --test ui-md3/tests/app-logo-chrome.test.mjs
```

Both C++ targets are registered with CTest (`app_logo_tests`, `app_logo_core`).
The model test covers the shipped default, reset clearing the key, preset round
trips, invalid and legacy stored values, the picker catalogue, every chrome
target size and the render plans for high-DPI displays. The core test covers the
presets, fit, focal, inset, alpha compositing, settings rollback and the bounded
BMP primitive. The node test checks that the panel is compiled, hosted and wired
to every surface, that no identity path reads the key, and that every panel
string has a Cantonese entry.

Postman is not applicable: this local presentation feature exposes no HTTP API.
