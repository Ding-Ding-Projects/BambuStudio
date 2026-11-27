# Multi-plate Preview verification

The synthetic project in [six-cups.3mf](../../../tests/data/preview/six-cups.3mf) contains two hollow cups on each of three plates. Each pair uses Bambu PLA Basic (green, ID 1) and Bambu PLA Matte (orange, ID 2). The project selects the Bambu Lab H2D with two 0.4 mm nozzles, Textured PEI Plate and a 0.20 mm Standard process. It was generated with the released slicer's assembly-list CLI and reopened in the GUI. No physical printer was connected.

## Release evidence

The [provenance manifest](provenance.json) records the package, executable, source, project and image hashes. Each original PNG is the frame returned by the native OpenGL canvas. A separate isolated profile was used for each language. The first plate sliced to 60 layers. The plate thumbnails show both colors on all three plates.

| Mode | Canvas after slicing |
| --- | --- |
| English | [Narrow](en-light-comfortable.png) · [Wide](en-wide.png) |
| Hong Kong Cantonese | [Narrow](yue_HK-light-comfortable.png) |
| Bilingual | [Narrow](bilingual_en_yue_HK-light-comfortable.png) |

At wide English width, both nozzle assignments and both grouping links are fully visible. The notification stack sits to the left of the grouping panel. The layer chip clears the thumbnail strip, the Time Estimation card fills its panel, and the intended purple playback controls remain visible. At narrow width notifications cover part of the dock while visible. The narrow frames cannot establish grouping-card clearance. These project-open/update notices differ from the two specific notifications in the original report. The images also do not establish the Objects-list border, Plate Settings layout, split-button behavior or physical loading.

## Reproduce

Run the commands below from the source root in PowerShell. Set `$exe` to the installed executable and `$root` to a fresh, task-owned folder. The driver replaces its destination profile, so keep that path separate from any user profile.

```powershell
python scripts/md3/prepare-capture-datadirs.py "$root/source" --themes light --densities comfortable
foreach ($language in 'en','yue_HK','bilingual_en_yue_HK') {
    $tuple = "$language-light-comfortable"
    python scripts/md3/capture-preview.py --exe $exe --project tests/data/preview/six-cups.3mf `
        --source-datadir "$root/source/$tuple" --datadir "$root/profile-$tuple" `
        --tuple $tuple --out "$root/images-$tuple" --desktop "preview51-$language"
}
```

Inspect the active document, both filament IDs in Preview, and the language of the visible labels before recording a pass.

