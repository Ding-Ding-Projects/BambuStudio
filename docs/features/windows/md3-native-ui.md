# Native Material Design 3 UI on Windows

## Behavior

The native wxWidgets application uses semantic Material Design 3 color tokens from
`src/slic3r/GUI/Widgets/MD3Tokens.hpp` and `StateColor.hpp`. `StateColor.cpp` maps those light tokens
to the dark palette. The rewrite covers application chrome, custom widgets, Home, Filament Manager,
and the React-based Device page while retaining data-bearing colors such as filament swatches and
printer status indicators.

Roboto Regular, Medium, and Bold are installed as application resources and registered privately on
Windows. They do not modify the user's system font collection.

## Configuration

The existing Bambu Studio appearance setting controls light and dark mode. Theme changes update the
global `StateColor` mode before semantic colors are resolved, then repaint the native widget tree.
Application preferences continue to use Bambu Studio's normal per-user configuration directory.

## Failure modes

- Missing semantic dark-map entries fall back to their light token rather than terminating the app.
- Missing Roboto files fall back to the existing HarmonyOS/system font path.
- Device and cloud webviews may show offline content when networking is unavailable; native chrome
  and local slicing remain independently testable.
- The published Windows binaries are currently unsigned. Windows may show an unknown-publisher
  warning; users should verify the release checksum before running the installer.

## Security considerations

The installer is per-user and writes application files only below
`%LOCALAPPDATA%\Programs\Bambu Studio MD3`. Its uninstaller refuses recursive deletion unless it is
running from that exact fixed directory. User profiles and projects are outside the install directory
and are not removed by uninstall.

## Verification

- Windows build job for commit `e0f6b2f92`: successful native compile, install, package, and artifact
  upload in GitHub Actions run `29665576610`.
- Portable artifact: 6,743 files; `bambu-studio.exe`, `BambuStudio.dll`, resources, and all three
  Roboto TTF files present.
- The cross-translation-unit `wxColour` comparator regression was fixed by keeping palette lookup
  inside `StateColor.cpp`; Windows CI passed after the fix.
- A final interactive light/dark visual smoke test is still awaiting explicit permission to execute
  the newly built unsigned binary in Windows Sandbox.
