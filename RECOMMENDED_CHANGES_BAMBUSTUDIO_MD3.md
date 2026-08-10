# Recommended changes — file by file

## Apply now

### `resources/images/toolbar_tooltip.svg`
### `resources/images/toolbar_tooltip_hover.svg`

Replace the frozen brand-green keyboard drawing with a theme-independent inverse/container keyboard plate. Preserve 30×22 geometry because callers intentionally render it at a wide ratio.

### `resources/images/fit_camera*.svg`

Replace the white/gray Bambu composites with MD3 surface-container plates, outline-variant borders, and neutral fit/cube geometry. Preserve the plate; a bare glyph is not legible over arbitrary 3D scene content.

### `resources/images/text_B*.svg`
### `resources/images/text_T*.svg`

Use local vector paths in OnSurfaceVariant. Keep 20×20 intrinsic size and avoid external font, `<text>`, or icon-font loading inside SVG.

### `resources/images/align_*.svg`
### `resources/images/distribute_*.svg`

Use 36×36 rounded MD3 tiles. Keep operation and axis meaning distinct through geometry plus vector X/Y/Z marks in the exact design-contract data colors. Dark files use dark container and foreground roles; axis data colors remain unchanged.

### `scripts/validate_md3_gizmo_assets.py`

Keep this validator in the repository. It checks the exact 34-file set, dimensions, XML safety, approved token palette, axis colors, unique semantic artwork, source references, and optional Inkscape rasterization.

### `docs/features/design-system/gizmo-rail-svg-icons-completion.md`

Keep the implementation decision and evidence boundary next to the parity register.

## Do after native verification

Update only the `gizmo-rail-svg-icons` parity row and its warning. Record the tested commit, Windows build result, screenshot paths, light/dark schemes, density modes, scaling factors, and all operation results. Do not close the row from static source review alone.

## Do not change in this completion commit

- `GLGizmosManager.cpp` resource keys or control behavior
- slicing, model parsing, project tabs, device networking, printer control
- functional viewport axis colors
- already migrated Material Symbol rail items
- the four documented product deviations elsewhere in the parity register
