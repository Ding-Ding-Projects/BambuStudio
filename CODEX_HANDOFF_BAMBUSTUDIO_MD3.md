# Codex handoff — finish BambuStudio's Material 3 UI contract

## Mission

Finish the checked-in Material 3 design contract without rebuilding working UI or changing product behavior. The repository has already completed the broad native migration. At pinned commit `8f2ba7047e385e2d28c8a64d5cd9d1b8689f507f`, the live parity register has one implementation row still marked partial: `gizmo-rail-svg-icons`.

This overlay implements the conservative closure path already allowed by the register: keep semantic composite controls as **MD3-tokenized vector artwork** instead of collapsing them into generic Material Symbols or adding a second GL bridge API.

## Read first

1. `HANDOFF.md`
2. `ui-md3/design-system/readme.md`
3. `ui-md3/design-system/tokens/colors.css`
4. `ui-md3/design-system/components/navigation/GizmoRail.jsx`
5. `docs/features/design-system/md3-parity-register.md`
6. `docs/features/design-system/gizmo-rail-svg-icons-completion.md`
7. `ACCEPTANCE_MATRIX_BAMBUSTUDIO_MD3.md`

Do not infer that the old register introduction is current. Trust the row statuses and the re-audit warning attached to `gizmo-rail-svg-icons`.

## What this drop-in changes

It replaces the 34 legacy files still loaded directly by `GLGizmosManager.cpp`:

- 2 keyboard-hint states: `toolbar_tooltip{,_hover}.svg`
- 4 plated fit-camera states: `fit_camera{,_hover,_dark,_dark_hover}.svg`
- 4 formatting controls: `text_B`, `text_T`, and their dark variants
- 18 axis-preserving alignment tiles: X/Y/Z × min/center/max × light/dark
- 6 axis-preserving distribution tiles: X/Y/Z × light/dark

The filenames and intrinsic dimensions remain compatible with the current eager and lazy resource loads. No C++ source is changed by this overlay.

## Resolved design decisions

### Composite artwork is intentional

The main gizmo rail already uses Material Symbols through the glyph-to-GL-texture bridge. These remaining controls are different:

- Fit camera needs an opaque plate because it appears over arbitrary light and dark 3D scene content.
- Alignment/distribution operations must retain X/Y/Z and positional meaning. One generic alignment glyph cannot express the full matrix.
- The keyboard hint is 30×22 and rendered at a non-square ratio; a normal icon glyph would be visibly stretched.
- Bold/italic remain deterministic local paths, avoiding a new runtime font dependency in this small completion change.

### Token use

Neutral colors are copied from the design contract's semantic roles. Viewport axis colors are the exact checked-in functional data colors:

```text
X #EA4335
Y #34A853
Z #4C8BF5
```

Those axis colors are data, not general UI accent colors, and must not be replaced by the user accent seed.

### Theme-independent keyboard mark

There are no separate dark keyboard resource keys. Both keyboard files therefore use inverse/dark container roles so the mark remains legible over either theme rather than freezing a light-only surface.

## Apply

For the standalone kit:

```powershell
py tools\md3_completion\apply_overlay.py C:\path\to\BambuStudio
```

For the repository-relative drop-in ZIP, extract over the repository root, then run:

```powershell
py scripts\validate_md3_gizmo_assets.py --root .
```

Do not force-apply to an unrelated history. The script accepts the pinned commit or a descendant. Review first if `GLGizmosManager.cpp` changed its filenames, key set, or dimensions.

## Native verification — required before saying “done”

Use the repository's Windows toolchain and `.claude/skills/run-bambustudio/`. Capture the exact evidence named in the acceptance matrix:

1. Prepare workspace, comfortable density, light theme.
2. Prepare workspace, comfortable density, dark theme.
3. Compact density in light and dark.
4. Windows scaling at 100%, 150%, and 200%.
5. Fit-camera button over a very light model and a very dark model.
6. Keyboard hint idle and hover.
7. Bold and italic controls.
8. Every X/Y/Z align min/center/max and distribute operation.
9. Custom accent seed, confirming only neutral chrome and functional axis colors remain fixed.
10. Keyboard navigation, tooltip text, click targets, and unchanged operation results.

Only after all runtime rows pass should you change `gizmo-rail-svg-icons` from `partial` to `done` and replace its warning with the captured build/run evidence.

## Focused bug hunt

Do not claim the following are fixed unless reproduced and verified:

### Reported crash

`HANDOFF.md` says the crash remains unreproduced. Follow its crash section before editing. Collect:

```text
%APPDATA%\BambuStudioInternal\log\crash_*.log
%APPDATA%\BambuStudioInternal\log\studio_*.log*
```

Record the model, exact clicks, workspace, printer connection state, theme, density, and whether the failure happens with this asset-only commit reverted. Fix only the proven fault.

### “model has no data” tab failure

Follow the repository handoff reproduction sequence. Test a known-good local model through open, Prepare, slice, Preview, project-tab switch, close/reopen, and save/reload. Preserve the exact log and model-state boundary. Do not mask the message or weaken validation.

### Visual/resource regressions

Repeatedly open and close the object-manipulation alignment menu, switch themes, and use fit-camera. Check for stale light/dark textures, soft 32 px fallback rendering, clipped tiles, wrong axis, duplicate artwork, or unbounded memory growth. The light distribute-Z tile must use the same 64 px eager path as the other eleven tiles.

## Minimal commit sequence

1. `Replace remaining gizmo composites with MD3-token artwork`
2. `Record native gizmo asset evidence and close parity row`
3. Separate bug-fix commits only for reproduced defects

Do not mix speculative crash fixes, slicing changes, or broad icon refactors into the visual completion commit.

## Release gate

A releasable result requires:

- static validator green;
- Windows compile/link green;
- native light/dark and DPI matrix green;
- unchanged transform/alignment/distribution behavior;
- no new crash or memory-growth signal;
- parity register updated with concrete evidence;
- complete source ZIP generated from the verified worktree, excluding build outputs and `.git`.

## Honest status at handoff

The repository-relative replacement files are implemented and statically validated. Native Windows build/runtime evidence, parity-register closure, and the two previously reported runtime investigations remain for the machine that can run BambuStudio.
