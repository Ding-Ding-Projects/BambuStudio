# Start here — BambuStudio Material 3 completion overlay

This archive is laid out relative to the BambuStudio repository root. It targets commit:

```text
8f2ba7047e385e2d28c8a64d5cd9d1b8689f507f
```

## Existing checkout: immediate drop-in

1. Make a clean branch or commit your current work.
2. Extract the drop-in ZIP over the repository root and allow the 34 files under `resources/images/` to be replaced.
3. Run:

```powershell
py scripts\validate_md3_gizmo_assets.py --root .
```

4. Build through the repository's existing Windows build path, then use `.claude/skills/run-bambustudio/` for the live light/dark checks in `ACCEPTANCE_MATRIX_BAMBUSTUDIO_MD3.md`.

The replacement does not change slicing logic, models, networking, printer control, commands, object names, or hotkeys. It keeps every existing resource filename and load size so it can be applied without a C++ migration.

## Clean checkout: materialize everything

From the kit directory:

```powershell
py tools\md3_completion\bootstrap.py C:\work\BambuStudio-md3-complete --recurse-submodules
```

or:

```powershell
.\tools\md3_completion\bootstrap.ps1 -Target C:\work\BambuStudio-md3-complete -RecurseSubmodules
```

After native verification, package the complete worktree:

```powershell
py tools\md3_completion\package_completed_codebase.py C:\work\BambuStudio-md3-complete C:\work\BambuStudio-md3-complete-source.zip
```

## Evidence boundary

All 34 SVGs were XML-validated and rasterized successfully in this artifact environment. The Windows-only wxWidgets/OpenGL application was not compiled or launched here. Keep `gizmo-rail-svg-icons` as non-done in the parity register until the native evidence matrix passes.
