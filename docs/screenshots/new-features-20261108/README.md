# New features tour, 8 November 2026

Captured on 8 November 2026 from the Release build of commit `fcf033fee`, built and installed with
the one-click build. `BambuStudio.dll` SHA-256:
`54b8e3596b51bd35be5d9a719811c5299d3a8dcd1d2408907cd0c763b7a5180b`.

The application ran on a hidden desktop in bilingual (English and Cantonese) mode with an isolated
data directory and no printer, using a copy of the install with the bundled Mesa software OpenGL DLLs
beside the executable. Each page was opened through the layout probe's `invoke <menu label>` command
and captured with PrintWindow. Before this build, `a4d2cec0f` did not compile on Windows: the lock
hint parameter was named `near`, which the Windows headers define as an empty macro; `fcf033fee`
renames it. The build started without the crash `a4d2cec0f` fixes.

| File | Page | Notes |
| --- | --- | --- |
| `local-tools.png` | File > Local tools | Pinned tools, filter chips, search with regex builder. The last filter chip is cut at the right edge. |
| `extensions.png` | File > Extensions | Three built-in extensions waiting for consent. The detail tabs and the folder path at the bottom are cut; several labels are English only. |
| `drawing-board.png` | File > Drawing board | Seven pens, eraser, width, opacity, smoothing, versions and export. Several labels are English only. |
| `virtual-disks.png` | File > Virtual disks | All four Windows routes found, empty list. One action button shows only its icon, the "Format and type" header and the last button are cut, most labels are English only, and the search has no regex builder. |
| `minecraft-servers.png` | File > Minecraft servers | Java versions and memory found, empty list. The status line repeats the Java list in bilingual mode, the title is English only, and the empty list has no empty-state message. |
| `element-locks.png` | File > Element locks | "Nothing is locked" is correct, but the window opens 1880 px tall and its hint sentence is cut off. |
| `every-feature.png` | Help > Every feature | 51 features. The Cantonese line of "Personal wording file" is hidden under its wrapped title; "Sort" and the footer sentence are cut at the right edge. |
| `base64-toolkit.png` | File > Base64 toolkit | All options shown; the tab labels are English only. |
| `live-charts.png` | File > Live charts | Correct "no printer" states. The "Now" panel, the Clock toggle and a chart label are cut at the right edge. |
| `export-preferences.png` | File > Export > Export preferences | JSON, YAML, TOML, JSON Lines, XML and CSV. The header says the record has 0 top-level fields while each format omits 174 settings, which suggests the export carries no settings; this needs checking. The "Lost" text at the bottom is cut. |
| `setup-wizard.png` | Help > Set up Bambu Studio | Seven steps, saves as it goes, bilingual. The segmented choices (language, funny levels) are too short and cut their text at the bottom; "Look and motion" has no Cantonese line. |

Not captured: the Preferences window itself (it opened and its modal loop was idle, but PrintWindow
returned a black image for it and for the main window behind it), the before-send check window, live
charts with readings, attention modes and element locks on a locked element (these need a printer or
interaction the hidden-desktop route cannot provide), and other themes and display scales.
