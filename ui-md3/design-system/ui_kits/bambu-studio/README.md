# Bambu Studio MD3 — UI kit

Interactive recreation of the desktop app, composed from the design-system primitives (mirrored in `Components.jsx` for the standalone runtime — keep in sync with `/components`).

## Editing

The `.jsx` files are the source. `index.html` is generated from them:

```bash
node ui-md3/scripts/assemble-ui-kit.mjs --write
```

Change a `.jsx`, re-run that, commit both. CI runs the same script with `--check`, so a page that disagrees with its sources fails the deploy instead of shipping a prototype nobody reviewed. This README used to point at an assembler that did not exist anywhere in the tree; the two were kept in step by hand.

The page is self-contained. React and ReactDOM are served from `vendor/` beside it, and the JSX is compiled at build time by [`jsx-transform.mjs`](../../../scripts/jsx-transform.mjs) — a small compiler for the subset used here, which throws on anything it does not understand rather than guessing. Until 2026-07-28 this page instead pulled React, ReactDOM and `@babel/standalone` from unpkg and compiled itself in the visitor's browser: three third-party requests, a 2.7 MB compiler, and a completely blank page whenever unpkg was unreachable. Do not reintroduce a CDN tag — the assembler refuses to build one, and `ui-md3/tests/offline-render.test.mjs` loads the composed page with the network cut and requires it to render.

One consequence worth knowing: the compiled blocks are classic scripts sharing a single global scope, so the same top-level `const` in two files is a `SyntaxError` that silently kills every script after it. Babel used to hide this by rewriting `const` to `var`. The screens alias theirs (`useDevState`, `usePrepState`, `useAppState`) for this reason, and the assembler fails the build if two files collide.

## Files

- `index.html` — generated; edit the `.jsx` and re-assemble.
- `App.jsx` — window shell: title bar, tab bar, appearance popover (theme / density / accent seed via accentVars()), snackbars, overlay routing.
- `Home.jsx` · `Prepare.jsx` — brand green scheme. Prepare wires Print plate → Send dialog, Add ink → Add-ink dialog.
- `Preview.jsx` — `data-scheme="preview"` (purple accents).
- `Device.jsx` — `data-scheme="device"` (teal accents).
- `Multi.jsx` · `Project.jsx` · `Calibration.jsx` · `Filament.jsx` · `Settings.jsx` — remaining workspaces (Settings drives the real theme/density/accent state).
- `Overlays.jsx` — version-history drawer (expandable commits + diff + restore) and the Export-inks / Send-to-print / Add-ink dialogs.
- `vendor/` — React 18.3.1 and ReactDOM 18.3.1 UMD production builds, MIT licensed, with their licence text.
