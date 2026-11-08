# Local tools design, November 2026

Twelve design boards for the native Bambu Studio app: the Local tools hub, a page that lists every required feature, the Preferences remake and nine tools. They are a design specification. A board here does not mean the feature ships; the status column says what the native app has at the latest commit that touched this file.

Open a frame to see a board as designed. The boards themselves are in [boards](boards), with the shared tokens in [boards/kit.css](boards/kit.css) and the board layout in [boards/canvas.json](boards/canvas.json).

| Board | Frame | What it designs | Native status at this commit |
| --- | --- | --- | --- |
| [Main](boards/Main.dc.html) | [frame](frames/Main.webp) | Local tools hub with search, filters and recent activity | The Local tools tab exists; this hub page is not built |
| [FeatureFinder](boards/FeatureFinder.dc.html) | [frame](frames/FeatureFinder.webp) | Every feature: where each one lives, its shortcut, one click to open, and an honest Not built yet state | Built: Help > Every feature ([article](../../docs/features/windows/every-feature.md)) |
| [InfoIcons](boards/InfoIcons.dc.html) | [frame](frames/InfoIcons.webp) | Preferences remake: sections, info icons, an explanation panel, where each value comes from, reset, history | Partly built: provenance lines, info buttons, the explanation panel, Changed by you, history and switch reset ([article](../../docs/features/windows/preferences-provenance.md)); regrouped sections, section reset, related settings and the playground are not built yet |
| [Base64](boards/Base64.dc.html) | [frame](frames/Base64.webp) | Base64 toolkit: text, files beside the originals, data: URLs, preview, history | The codec is built and tested (`src/slic3r/GUI/Base64Toolkit/Base64Codec.*`); the panel is not built |
| [Extensions](boards/Extensions.dc.html) | [frame](frames/Extensions.webp) | Extension manager: hooks, permissions, consent, runs, limits, manifest | Not built yet |
| [Setup](boards/Setup.dc.html) | [frame](frames/Setup.webp) | Practical setup wizard with saved progress and real actions | Not built yet; the existing printer setup wizard is unchanged |
| [ContextMenu](boards/ContextMenu.dc.html) | [frame](frames/ContextMenu.webp) | Context menus with live controls and the shortcuts that really work | Not built yet |
| [Liveline](boards/Liveline.dc.html) | [frame](frames/Liveline.webp) | Live charts on the Device page with every honest state | Not built yet |
| [Drawesome](boards/Drawesome.dc.html) | [frame](frames/Drawesome.webp) | Drawing on captures: seven pens, eraser, palette, export, history | Not built yet |
| [DrawesomeShapes](boards/DrawesomeShapes.dc.html) | [frame](frames/DrawesomeShapes.webp) | The drawing toolbar's palette, sliders and minimised shapes | Not built yet |
| [VirtualDisks](boards/VirtualDisks.dc.html) | [frame](frames/VirtualDisks.webp) | Windows virtual disk manager: library, create, resize, compact, chains, checkpoints, quick pack | Not built yet |
| [Servers](boards/Servers.dc.html) | [frame](frames/Servers.webp) | Minecraft servers on this PC, in Docker and over SSH, plus the world generator | Not built yet |

How the frames were made and reviewed is in [REHEARSAL.md](REHEARSAL.md). The design handoff, including the guidance used and what each board needs natively, is in [HANDOFF.md](HANDOFF.md).

## Rendering the boards again

The boards use the app's own fonts from `resources/fonts`. From this folder:

```sh
python3 render/prep.py boards render/pages ../../resources/fonts
cp boards/kit.css render/pages/
node render/film.mjs "$PWD/render/pages" "$PWD/frames" boards/canvas.json Main FeatureFinder
```

`film.mjs` needs Playwright with a Chromium build. Each frame is the board at its full content height, 1440 px wide.
