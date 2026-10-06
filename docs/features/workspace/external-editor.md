# External editor

"Open in External Editor" opens the current project's folder in a user-chosen
code/text editor, mirroring GitHub Desktop's external-editor integration.

## Behavior

- **File ▸ Open in External Editor** (below Version history) opens the folder
  containing the current project's `.3mf`. Disabled while no project file
  exists.
- Editor detection (`src/slic3r/Utils/ExternalEditor.{hpp,cpp}`): probes
  Windows uninstall registry keys (HKCU + HKLM, incl. WOW6432Node) for
  VS Code, VS Code Insiders, VSCodium, Sublime Text, Notepad++, Cursor,
  Windsurf, and Zed (table transcribed from the desktop-material reference
  implementation), then falls back to scanning `PATH` for the editors' CLI
  shims (`.cmd` shims resolve to their real executables). macOS probes
  `/Applications` bundles + PATH; Linux probes well-known paths + PATH.
  Detection runs once per process and is cached.
- Launch (`open_in_external_editor` in `src/slic3r/Utils/Process.cpp`):
  detached async spawn (`wxExecute(argv, wxEXEC_ASYNC)` on Windows,
  `boost::process::spawn` on macOS), argv-based so paths with spaces are safe;
  the editor outlives the app.

## Configuration

Preferences ▸ General ▸ **External editor**:

- A dropdown of detected editors plus **Custom…** — persisted as AppConfig
  `external_editor` (editor name, or `custom`).
- **External editor path** row with Browse (executable picker) — persisted as
  `external_editor_path`; used when the dropdown is set to Custom.

An explicit Custom choice is honored strictly: if its path is empty the app
shows a warning toast pointing at Preferences rather than silently launching
an auto-detected editor.

## Failure modes

- **No editor found/configured** → warning corner toast ("No external editor is
  configured…"), no crash, no dialog.
- **Editor uninstalled after detection** → the spawn fails asynchronously;
  detection is cached per process, so newly (un)installed editors are picked up
  on the next app start (documented limitation).
- **Unsaved/never-saved project** → menu item disabled (no folder to open).

## Security considerations

- Only executables found under registry install locations, well-known
  installation paths, or the user's `PATH` are offered; the Custom path is
  user-picked through a file dialog and stored locally in AppConfig.
- The project folder path is passed as a single argv element (no shell string
  interpolation), preventing argument-injection via crafted project paths.
- Registry access is read-only.

## Verification

- Compiled clean into `libslic3r_gui` (0 errors) after adversarial review
  (review findings on silent Custom fallback fixed).
- UI captured in the screenshot matrix under `docs/screenshots/preferences/`
  (General tab rows) once built into the running exe.


## Dedicated export handoff

The shared export dialog uses `open_export_in_visual_studio_code(target_utf8)` rather
than the general preferred-editor fallback. It re-detects stable and Insiders installs,
checks per-user and machine locations, and resolves portable executables placed on
PATH. The target must exist. Arguments are passed as a vector with an option separator;
a directory opens as a new workspace root. No unrelated editor is substituted.

When Code is missing, the dialog shows an explicit Download VS Code action opening the
official download page. Installing Code then retrying does not require an application
restart. Missing output or launch errors are reported in the dialog. The user-selected
general editor preference is unchanged. Native GUI compile and interaction evidence for
this new action remains pending; console export tests do not verify this surface.
