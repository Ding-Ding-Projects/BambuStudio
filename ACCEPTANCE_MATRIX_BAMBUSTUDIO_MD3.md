# Acceptance matrix — BambuStudio MD3 gizmo completion

| Gate | Expected result | Artifact status |
|---|---|---|
| Asset inventory | Exactly 34 named replacement SVGs exist | Passed statically |
| XML safety | XML parses; no script, style, text, image, external reference, CSS variable, or filter | Passed statically |
| Token palette | Every hex color is a checked-in semantic role or exact axis data color | Passed statically |
| Legacy colors | No `#00AE42`, `#808080`, `#6B6B6B`, or `#F4F4F4` | Passed statically |
| Intrinsic geometry | Keyboard 30×22; fit camera 33×32; B/T 20×20; align/distribute 36×36 | Passed statically |
| Semantic uniqueness | No align/distribute asset is an accidental byte duplicate | Passed statically |
| Rasterization | All 34 SVGs render to non-empty 144 px PNGs with Inkscape | Passed in artifact workspace |
| Current source linkage | Every expected filename is still referenced by `GLGizmosManager.cpp` | Run in materialized checkout |
| Windows compile/link | Existing supported build completes | Pending native machine |
| Light theme | All controls readable, aligned, crisp, and correctly selected | Pending native machine |
| Dark theme | All controls readable, aligned, crisp, and correctly selected | Pending native machine |
| Comfortable/compact | No clipping, overlap, or hit-target regression | Pending native machine |
| 100/150/200% scaling | No blur, crop, one-pixel seams, or wrong texture size | Pending native machine |
| Fit camera | Plate remains legible over both very light and very dark scene content | Pending native machine |
| Keyboard hint | Idle/hover remain legible in both themes without stretching artifacts | Pending native machine |
| Bold/italic | Correct state, tooltip, click behavior, and resulting text style | Pending native machine |
| Align X | Min/center/max artwork and operation result are correct | Pending native machine |
| Align Y | Min/center/max artwork and operation result are correct | Pending native machine |
| Align Z | Min/center/max artwork and operation result are correct | Pending native machine |
| Distribute X/Y/Z | Artwork and operation result are correct for all three axes | Pending native machine |
| Custom accent | Neutral composites stay neutral; axis data colors stay X/Y/Z | Pending native machine |
| Theme switching | No stale cached light/dark texture after repeated switching | Pending native machine |
| Memory | Repeated open/close/theme cycles do not cause unbounded private-byte or GPU-texture growth | Pending native machine |
| Crash investigation | Reproduced with stack/log and regression test, or explicitly remains unreproduced | Pending native machine |
| “model has no data” | Reproduced and fixed with regression test, or explicitly remains unreproduced | Pending native machine |
| Parity closure | Row changed to `done` with exact native evidence | Pending native machine |
| Full source package | Verified worktree ZIP and SHA-256 produced | Pending native machine |
