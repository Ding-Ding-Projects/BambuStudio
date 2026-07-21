# Bambu Studio MD3 — UI kit

Interactive recreation of the desktop app, composed from the design-system primitives (mirrored in `Components.jsx` for the standalone runtime — keep in sync with `/components`).

- `index.html` — assembled from the .jsx files below (inline scripts; the sandboxed preview can't XHR-load babel src). To edit: change a .jsx, then re-run the assembler (see project history) or re-inline manually.
- `App.jsx` — window shell: title bar, tab bar, appearance popover (theme / density / accent seed via accentVars()), snackbars, overlay routing.
- `Home.jsx` · `Prepare.jsx` — brand green scheme. Prepare wires Print plate → Send dialog, Add filament → Add-filament dialog.
- `Preview.jsx` — `data-scheme="preview"` (purple accents).
- `Device.jsx` — `data-scheme="device"` (teal accents).
- `Multi.jsx` · `Project.jsx` · `Calibration.jsx` · `Filament.jsx` · `Settings.jsx` — remaining workspaces (Settings drives the real theme/density/accent state).
- `Overlays.jsx` — version-history drawer (expandable commits + diff + restore) and the Export-filaments / Send-to-print / Add-filament dialogs.
