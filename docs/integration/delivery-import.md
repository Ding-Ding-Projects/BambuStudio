# Detached import and simplification delivery

## Source changes

- Automatic simplification now publishes the quadric engine result directly. It no longer independently rescales each axis to force the original bounds. The configured quadric error setting governs that engine; it is not a guarantee of unchanged dimensions, volume, angles, topology or maximum physical deviation. The import notification explicitly warns that dimensions and volume may change.
- The original immutable mesh and convex hull remain available through the existing restore action. Original input files are never rewritten. Painting, textures, cut objects, non-model-part siblings, embossing and connector metadata continue to exclude automatic simplification.
- Detached import publication now uses a cancelable progress dialog. Scene progress callbacks return the actual cancellation result, including geometry-only imports and merged multipart batches. The existing per-object publication callback creates a cancellation opportunity after each object placement.
- Prepared-load parsing exceptions propagate to the detached completion boundary rather than continuing with a partial batch. Canceled or exceptional publication restores the pre-import undo snapshot, covering the model, plates, selection and project settings, and restores the import path and thumbnail flag. A new snapshot from the restored state removes the partial publication from redo without clearing earlier history. Recovery meshes are registered only after publication succeeds.
- The temporary multipart model uses exclusive ownership so cancellation and exception exits release it.

## Remaining limitations

Publication still runs on the UI thread. Cancellation is cooperative at existing progress boundaries; one object's scene upload, texture application, source reader stage or convex-hull calculation can still take an unbounded amount of time. This change does not claim a strict frame-time or wall-clock bound. Archive parsing continues to use its existing modal owner and is outside the detached mesh path. Exceptions during rollback itself can still prevent full recovery, especially resource exhaustion. Native execution is required before making a transactional acceptance claim.

The existing `retain_mesh_extents` helper is retained for compatibility with previously authored source tests, but automatic import no longer calls it. It must not be used to claim simplifier error compliance.

## Delivery evidence

Tests, lint, static-analysis suites, reviews, audits, runtime interaction and screenshots were intentionally not run under the requested speed mode. This document describes implemented source, not verified runtime behavior. Compilation, packaged delivery and default-branch integration are owned by the coordinating task.
