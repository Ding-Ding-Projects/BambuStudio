# Local tools design, November 2026

Twelve design boards for the native Bambu Studio app: the Local tools hub, a page that lists every required feature, the Preferences remake and nine tools. They are a design specification. A board here does not mean the feature ships; the status column says what the native app has at the latest commit that touched this file.

Open a frame to see a board as designed. The boards themselves are in [boards](boards), with the shared tokens in [boards/kit.css](boards/kit.css) and the board layout in [boards/canvas.json](boards/canvas.json).

| Board | Frame | What it designs | Native status at this commit |
| --- | --- | --- | --- |
| [Main](boards/Main.dc.html) | [frame](frames/Main.webp) | Local tools hub with search, filters and recent activity | The Local tools tab exists; this hub page is not built |
| [FeatureFinder](boards/FeatureFinder.dc.html) | [frame](frames/FeatureFinder.webp) | Every feature: where each one lives, its shortcut, one click to open, and an honest Not built yet state | Built: Help > Every feature ([article](../../docs/features/windows/every-feature.md)); every feature on it is built now, so the Not built yet chip is gone |
| [InfoIcons](boards/InfoIcons.dc.html) | [frame](frames/InfoIcons.webp) | Preferences remake: sections, info icons, an explanation panel, where each value comes from, reset, history | Partly built: provenance lines, info buttons, the explanation panel, Changed by you, history and switch reset ([article](../../docs/features/windows/preferences-provenance.md)); regrouped sections, section reset, related settings and the playground are not built yet |
| [Base64](boards/Base64.dc.html) | [frame](frames/Base64.webp) | Base64 toolkit: text, files beside the originals, data: URLs, preview, history | Built: File > Base64 toolkit ([article](../../docs/features/windows/base64-toolkit.md)); pausing or cancelling one file alone and audio or video preview are not built yet |
| [Extensions](boards/Extensions.dc.html) | [frame](frames/Extensions.webp) | Extension manager: hooks, permissions, consent, runs, limits, manifest | Built: File > Extensions, with three built-in extensions ([article](../../docs/features/windows/extensions.md)); the model.imported, print.before_send and print.finished hooks and per-extension settings are not built yet |
| [Setup](boards/Setup.dc.html) | [frame](frames/Setup.webp) | Practical setup wizard with saved progress and real actions | Built: Help > Set up Bambu Studio, also after the printer guide on a new installation ([article](../../docs/features/windows/practical-setup.md)); following the Windows theme and undoing an added printer are not built yet |
| [ContextMenu](boards/ContextMenu.dc.html) | [frame](frames/ContextMenu.webp) | Context menus with live controls and the shortcuts that really work | Built: the menu on every setting (live value, back to the preset or system value, copy the key, compare, Explain this with F1), the view rows on empty space in Prepare and Copies on an object ([article](../../docs/features/windows/menu-live-controls.md)); snap to grid, the per-object view and opacity rows and live rows in the object list, Preview and Preferences menus are not built yet |
| [Liveline](boards/Liveline.dc.html) | [frame](frames/Liveline.webp) | Live charts on the Device page with every honest state | Built: Device page > Live charts and File > Live charts ([article](../../docs/features/device/live-charts.md)); AMS humidity and fan charts, the momentum arrow, particles and the 12-hour clock are not built yet |
| [Attention](boards/Attention.dc.html) | [frame](frames/Attention.webp) | Attention modes: Focus, Low stimulation, Time awareness, One thing at a time, Momentum | Built: Preferences > Attention modes and the strip under the title bar ([article](../../docs/features/windows/attention-modes.md)); Focus outside Prepare and a quieter colour scheme are not built yet |
| [Drawesome](boards/Drawesome.dc.html) | [frame](frames/Drawesome.webp) | Drawing on captures: seven pens, eraser, palette, export, history | Built: File > Drawing board ([article](../../docs/features/windows/drawing-board.md)); the floating toolbar with its dock positions and minimised disc, pen pressure, the eyedropper, undo across a restart and the export catalogue are not built yet |
| [DrawesomeShapes](boards/DrawesomeShapes.dc.html) | [frame](frames/DrawesomeShapes.webp) | The drawing toolbar's palette, sliders and minimised shapes | Partly built: the palette and the width, opacity and smoothing inputs sit in rows above the board; the toolbar's changing shapes and the minimised disc are not built yet |
| [VirtualDisks](boards/VirtualDisks.dc.html) | [frame](frames/VirtualDisks.webp) | Windows virtual disk manager: library, create, resize, compact, chains, checkpoints, quick pack | Built: File > Virtual disks ([article](../../docs/features/windows/virtual-disks.md)), checked on real disks on a hosted Windows machine; shrinking, attaching at every sign-in, folder mounts, Retrim, Pretrimmed and Prezeroed, VHD Set checkpoints, native boot, Dev Drive filters, metadata, mirrors and change tracking are not built yet |
| [Servers](boards/Servers.dc.html) | [frame](frames/Servers.webp) | Minecraft servers on this PC, in Docker and over SSH, plus the world generator | Built: File > Minecraft servers ([article](../../docs/features/windows/minecraft-servers.md)) for Java servers on this PC, with every tab but Plugins and packs, and the world generator in batches; Docker, SSH, Bedrock, installing other flavours, the plugin catalogue, the command tree check, the memory chart, ticks per second, the vault password and the generator's data pack settings are not built yet |

How the frames were made and reviewed is in [REHEARSAL.md](REHEARSAL.md). The design handoff, including the guidance used and what each board needs natively, is in [HANDOFF.md](HANDOFF.md).

## Rendering the boards again

The boards use the app's own fonts from `resources/fonts`. From this folder:

```sh
python3 render/prep.py boards render/pages ../../resources/fonts
cp boards/kit.css render/pages/
node render/film.mjs "$PWD/render/pages" "$PWD/frames" boards/canvas.json Main FeatureFinder
```

`film.mjs` needs Playwright with a Chromium build. Each frame is the board at its full content height, 1440 px wide.
