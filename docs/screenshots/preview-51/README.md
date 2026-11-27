# Multi-plate Preview verification

The synthetic project in [six-cups.3mf](../../../tests/data/preview/six-cups.3mf) contains two hollow cups on each of three plates. Each pair uses Bambu PLA Basic (green, ID 1) and Bambu PLA Matte (orange, ID 2). The project selects the Bambu Lab H2D with two 0.4 mm nozzles, Textured PEI Plate and a 0.20 mm Standard process. It was generated with the released slicer's assembly-list CLI and reopened in the GUI. No physical printer was connected.

## Release evidence

The [provenance manifest](provenance.json) records the package, executable, source, project and image hashes. The canvas images come from the native OpenGL renderer. The window images show the surrounding native controls; their blank GL region is a limitation of the window capture. A separate isolated profile was used for each language. The first plate sliced to 60 layers. The plate thumbnails show both colors on all three plates.

| Mode | Canvas after slicing |
| --- | --- |
| English | [Narrow](en-light-comfortable.png) Â· [Wide](en-wide.png) Â· [Window](en-wide-window.png) |
| Hong Kong Cantonese | [Narrow](yue_HK-light-comfortable.png) Â· [Wide](yue_HK-wide.png) Â· [Window](yue_HK-wide-window.png) |
| Bilingual | [Narrow](bilingual_en_yue_HK-light-comfortable.png) Â· [Wide](bilingual_en_yue_HK-wide.png) Â· [Window](bilingual_en_yue_HK-wide-window.png) · [Tall](bilingual_en_yue_HK-tall.png) |

In the wide English and Cantonese frames, both nozzle assignments and both grouping links are fully visible. The notification stack sits to the left of the grouping panel. The layer chip clears the thumbnail strip, the Time Estimation card fills its panel, and the intended purple playback controls remain visible. At narrow width notifications cover part of the dock while visible. The narrow frames cannot establish grouping-card clearance. These project-open/update notices differ from the two specific notifications in the original report. All three wide window images show the split-button arrows, the complete plate name on the Ink tab, and both assigned materials. Physical loading remains unverified. In the wide bilingual frame the panel has a scrollbar, and the lower grouping link falls below the visible area. At 1700 × 1200 the complete English and Cantonese Regroup ink link is visible beneath both nozzle assignments.

## Sidebar verification

The Objects tab and the separate Process → Objects → Plate Settings workflow were exercised in the released package. Each capture report identified the visible native settings page as `Slic3r::GUI::TabPrintPlate`. All three Objects views have a soft border without a native header row, and the selected plate name is fully readable in each settings view. English and bilingual show all four settings selectors. The Cantonese frame is shorter and shows the upper portion; the rows below its fold cannot be assessed from that image. None of these plate-selected frames verifies the separate Object Manipulation panel on an individual selected cup.

| Mode | Objects list | Plate Settings |
| --- | --- | --- |
| English | [Objects](en-objects.png) | [Settings](en-prepare-plate-settings.png) |
| Hong Kong Cantonese | [Objects](yue_HK-objects.png) | [Settings](yue_HK-prepare-plate-settings.png) |
| Bilingual | [Objects](bilingual_en_yue_HK-objects.png) | [Settings](bilingual_en_yue_HK-prepare-plate-settings.png) |

## Reproduce

Run the commands below from the source root in PowerShell. Set `$exe` to the installed executable and `$root` to a fresh, task-owned folder. The driver replaces its destination profile, so keep that path separate from any user profile.

```powershell
python scripts/md3/prepare-capture-datadirs.py "$root/source" --themes light --densities comfortable
foreach ($language in 'en','yue_HK','bilingual_en_yue_HK') {
    $tuple = "$language-light-comfortable"
    python scripts/md3/capture-preview.py --exe $exe --project tests/data/preview/six-cups.3mf --project-sidebar `
        --source-datadir "$root/source/$tuple" --datadir "$root/profile-$tuple" `
        --tuple $tuple --out "$root/images-$tuple" --desktop "preview51-$language"
}
```

Inspect the active document, both filament IDs in Preview, and the language of the visible labels before recording a pass.
