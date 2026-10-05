# Application logo presentation

The native logo panel provides three original printer-themed presets, contain and
cover fitting, numeric horizontal and vertical focal placement, a 0–25% safe-area
inset, transparency, and an arbitrary background color. It generates real RGBA
previews at 16, 24, 32 and 64 pixels. Geometry, crops and backgrounds affect the
rendered result, rather than being decorative controls. Sampling is nearest
neighbor, so small output can lose detail. Cover fitting crops the source.

## Integration

Add `src/slic3r/GUI/AppLogo/LogoPanel.cpp` to the GUI target. The core
`src/libslic3r/AppLogo/Logo.hpp` is header-only and needs C++17.

Create `Slic3r::GUI::AppLogoUI::LogoPanel(parent, private_file, changed, translate)`
inside a scrollable settings host. Supply an existing writable application-private
directory and a dedicated neutral settings filename. The optional translator takes
each English label and returns the current language's text. The change callback
runs only after atomic persistence succeeds. Consumers render via
`AppLogoUI::bitmap(settings, pixels)`, with an output bound of 256 pixels. The host
must load the same file and apply the initial state on startup; the constructor
does not invoke callbacks before the host has finished construction.

`AppLogoUI::load` rejects oversized, malformed and unsupported-version settings,
leaving its destination intact. Saving uses `wxTempFile` replacement. A failed
save restores the previous control values and retains the previous active mark.
Reset atomically writes the shipped default settings. Only ten bounded numeric
values are stored. The panel does not modify package identity, application IDs,
executables, installers, update feeds, or operating-system installed icons.

## Explicit capability limits

Custom import is visibly disabled. There is no packaged, independently verified
isolated decoder integration yet. No custom source file is selected or read by
the panel. PNG, JPEG, GIF, SVG, WebP, animation, compressed BMP, and icon-container
conversion are unavailable. The internal `decode_bmp` primitive supports only
exact uncompressed 24-bit BMP with a 40-byte header, bottom-up rows, and no trailing
content. It is not a sandbox and must not be connected to a user picker directly.
It is bounded to 4 MiB input, 2048 per dimension, and 1,048,576 decoded pixels;
its loops are bounded and it does not call an external codec or network API.
It has no OS CPU deadline or memory/process isolation. A future isolated worker
must supply those properties and independently validate output before enabling
custom import. No unsupported format is inferred from a filename.

The panel's controls have accessible names and translation hooks. Full Cantonese,
bilingual and funny-level catalogs, adjacent regex search, palette integration,
scheduled settings, per-element editing, privacy-reviewed runtime captures, and
all actual chrome consumers still require host integration and verification.
The controls use the existing `ComboBox`, `Button`, `CheckBox`, `SpinInput`,
`Label` and `MD3ColorPickerDialog` kit. Numeric edits apply on Enter or focus loss;
arrow edits apply immediately. History is not implemented: the post-save change
callback is not an atomic history transaction. The host must supply generalized
appearance history before claiming snapshot/undo support.
Transparent marks can lose contrast against some backgrounds; the panel discloses
this and provides an opaque background option. DPI/layout proof is pending.

## Verification

Run the dependency-free core test with a C++17 compiler:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests/app_logo/test_logo.cpp -o "$env:TEMP/app-logo-tests.exe"
& "$env:TEMP/app-logo-tests.exe"
```

Alternatively configure `tests/app_logo` as a standalone CMake project, build it,
and run CTest. The explicit `require` checks remain active in Release/NDEBUG builds.

The executable checks every preset and supported preview size, exact crop/focal
pixels, contain letterboxing, alpha compositing, safe-area rendering, settings
round-trip and corruption rollback, BMP byte signatures, dimensions, truncation,
input limits and decode rollback. These are core checks only. Native panel build,
atomic-save failure handling on the target filesystem, keyboard/screen-reader
behavior, persistence across restart and real packaged rendering remain unverified
until the parent GUI target is built and exercised.
