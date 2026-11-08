# Design handoff

## Guidance used

- Stageshow (`benjitaylor/skills`, folder `stageshow`) from a pinned copy at commit `299140fafd08df55a9c234e610c3f64af3f45531`. A fresh install was not possible in the environment this pass ran in, so the pinned copy was used.
- Read: `SKILL.md`, `references/craft.md`, `references/framing.md`, `references/rehearse.md`, `references/review.md` and `references/verify.md`.
- Gold example copied: none. These boards are real app screens built from the app's own Material Design 3 tokens ([boards/kit.css](boards/kit.css)), not marketing scenes. The restraint, hierarchy, icon, density and review rules applied. The cursor, stage framing, phone width and loop rules did not apply.
- Board titles in [boards/canvas.json](boards/canvas.json) are the public names.

## What each board needs natively

| Board | Native work |
| --- | --- |
| FeatureFinder | A wx-free catalogue of every feature with its place, shortcut and state; a Local tools page with search (plain text and regex), category filters, three sort orders and recently opened; Open runs the real menu action or opens Preferences on the right row; a feature that is not built yet shows Not built yet and has no Open button |
| InfoIcons | Preferences sections regrouped as drawn; for every row an info button, an explanation panel, the built-in default, who changed the value and when, live versus restart, the stored key, per-row reset, reset a section behind two-key confirmation, Preferences history with undo, export |
| Base64 | A panel over the existing codec: text, files written beside the originals with verification, data: URLs, sandboxed preview, local history |
| Extensions | Manifest validation, an out-of-process JSON host with time and size limits, permissions with consent, the manager page, built-in extensions and an author guide |
| Setup | A practical setup wizard with real actions, saved progress, skip for optional steps and keyboard access |
| ContextMenu | Context menus on every element with live controls and shortcuts from the same table that binds the keys |
| Liveline | A live chart widget for Device readings with every honest state and reduced motion |
| Drawesome, DrawesomeShapes | Drawing on captures and a blank board with seven pens, the eraser, the shape-changing toolbar, export and history |
| VirtualDisks | The Windows virtual disk manager over VirtDisk, the Storage module and DiskPart, with no Hyper-V requirement |
| Servers | The Minecraft server manager for four targets and the world generator |
| Main | The Local tools hub that links all of the above |

Every board's copy is the intended product copy in English; each surface also needs its Cantonese strings and documentation article when it is built.

## Review

See [REHEARSAL.md](REHEARSAL.md): 27 findings from a fresh-eye review of the frames, all fixed in the design. Nothing fixable was left open.
