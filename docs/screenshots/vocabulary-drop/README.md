# Vocabulary drop captures

Captured on 5 November 2026 (UTC) from the Release build of commit `c5510e983`, built and installed
with the one-click build. `BambuStudio.dll` SHA-256:
`4e94f5959abdea819d566186c15e2aecd257e4f1c4e949deef1c39ebd30d4924`.

The application ran on a hidden desktop in bilingual (English and Cantonese) mode with an isolated
data directory, using a copy of the install with the bundled Mesa software OpenGL DLLs beside the
executable. Each window was captured with PrintWindow; the celebration and the notice are their own
windows, captured separately from the main window.

OLE drag and drop needs the input desktop, so the drop was delivered with the layout probe's
`drop-file <path>` command, which hands the path to the main window's drop target. Everything after
the drop target (validation, saving, applying, sound, overlay and announcement) ran as it does for a
real drop; dragging a file from Explorer was not exercised.

The vocabulary files are made-up samples, not a real personal vocabulary:

```json
{"schemaVersion":1,"entries":{"private vocabulary":"Studio words","Prepare":"Get ready","Preview":"Look first"}}
```

The refused file is the same JSON cut off in the middle of an entry.

| File | State |
| --- | --- |
| `celebration.png` | The celebration about a second after the sample file was dropped. The headline is the sample's entry for `private vocabulary`; the subtitle shows both languages. |
| `refused.png` | The notice after the cut-off file was dropped: both languages, wrapped, naming the broken rule. The wording on screen did not change. |
| `applied-after-restart.png` | The main window after the application was closed and started again: the rail still reads "Get ready" and "Look first" from the sample file. |

Not covered here: the drop from Explorer, the sound itself (played but not recorded), the screen
reader announcement, reduced motion, light and dark themes other than the light theme shown, and the
125, 150 and 200 percent display scales.
