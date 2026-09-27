# Native Material Design 3 UI on Windows

## Behavior

The native wxWidgets application uses semantic Material Design 3 color tokens from
`src/slic3r/GUI/Widgets/MD3Tokens.hpp` and `StateColor.hpp`. `StateColor.cpp` maps those light tokens
to the dark palette. The rewrite covers application chrome, custom widgets, Home, Filament Manager,
and the React-based Device page while retaining data-bearing colors such as filament swatches and
printer status indicators.

Roboto Regular, Medium, and Bold are installed as application resources and registered privately on
Windows. They do not modify the user's system font collection. Cantonese and Traditional CJK modes
prefer Microsoft JhengHei UI because Roboto does not contain those glyphs.

## Configuration

The existing Bambu Studio appearance setting controls light and dark mode. Theme changes update the
global `StateColor` mode before semantic colors are resolved, then repaint the native widget tree.
Application preferences continue to use Bambu Studio's normal per-user configuration directory.

## Failure modes

- Missing semantic dark-map entries fall back to their light token rather than terminating the app.
- Missing or unavailable preferred fonts fall back through the existing HarmonyOS/system font path.
- Device and cloud webviews may show offline content when networking is unavailable; native chrome
  and local slicing remain independently testable.
- The published Windows binaries are currently unsigned. Windows may show an unknown-publisher
  warning; users should verify the release checksum before running the installer.

## Security considerations

The installer is per-user and writes application files only below
`%LOCALAPPDATA%\Programs\Bambu Studio MD3`. Its uninstaller first validates the fixed install path,
then deletes only payload-owned files listed at build time and removes directories only when empty.
Unknown paths are preserved. Live destination junctions/symbolic links fail closed, and locked files
keep the uninstall registration available for retry. User profiles and projects are outside the
install directory and are not removed by uninstall.

## Verification

- The last fully published baseline before the current candidate work is commit `1f1ecb960`, GitHub
  Actions run `29671557311`, release `md3-windows-v02.08.01.55-r6.1`.
- That baseline compiled, installed, packaged, and published the native Windows payload. It contained
  `bambu-studio.exe`, `BambuStudio.dll`, resources, and all three Roboto TTF files.
- The cross-translation-unit `wxColour` comparator regression remains fixed by keeping palette lookup
  inside `StateColor.cpp`; the unused public `GetDarkMap()` accessor has now been removed.
- The candidate workflow adds a guarded native light/dark/language capture gate, but its first
  successful evidence run and human review are still pending. No local unsigned binary was executed.

Language behavior and its still-partial native coverage are documented in
[English, Hong Kong Cantonese, and bilingual modes](language-modes.md). The capture boundary and its
limitations are documented in [Native Windows visual smoke test](native-visual-smoke.md).
