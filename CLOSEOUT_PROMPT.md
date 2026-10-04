# Responsive model import implementation checkpoint

Objective: keep interactive model import responsive and optionally simplify meshes with at least 1,000,000 triangles before scene attachment. The current user requested preservation and closeout before the work was verified.

Working branch: `codex/bambu-import-simplify`. Baseline: `0c967a557`. Shared-service commits: `3d4a75298` and `83df3dd9f`. No implementation from this lane has been merged or pushed by this worker.

Implemented source, not yet compiled:

- Shared quadric simplification service used by the manual tool and new import job. Inclusive threshold 1,000,000, default maximum error 0.001, all five existing detail choices available.
- General > Model import stores the opt-out and detail level. Only absent keys migrate to enabled and the highest detail. Command-palette settings entries and reset keys were added.
- STL, OLTP, OBJ, STEP/STP, GLB/GLTF, FBX and AMF interactive drop/file-dialog routes queue a native ImportJob. Parsing, hull preparation and eligible simplification operate on detached models. STEP prompts are dispatched to the UI thread. Synchronous vector callers keep their result contract.
- The detached batch is dropped on cancellation before finalization. Original meshes/hulls are retained immutably for a recovery notification and command-palette action, with pointer-identity matching to avoid overwriting later mesh edits. Restoration has an undo snapshot.
- Mesh reduction skips textures, painting, cut objects and modifier relationships. Per-axis extent restoration preserves local bounding dimensions within float rounding, but can change angles, volume and deviation beyond the engine error setting. Collapsed dimensions are skipped rather than published.
- Saved 3MF parsing and hull computation use a detached future while the existing modal UI owner polls progress/cancellation. Configuration, plate changes and prompts remain on the UI thread. Saved-project automatic simplification is skipped with status copy.

Verified evidence: `git diff --check` passed. Six standalone Catch cases were authored for threshold/default settings, engine reduction, cancellation and extent handling. They have not run. No native compile, runtime interaction, screenshot, installer, release or CI verification occurred for this candidate.

Remaining work before claiming completion:

- Compile the exact candidate using the supported Windows toolchain and run `mesh_simplification_tests`, command-palette checks, focused import tests and the appropriate native regression targets.
- Prove cancellation during parsing, reduction, STEP prompts and finalization in the real built UI. Existing readers and hull computation have cooperative cancellation granularity rather than interruption inside every primitive.
- The existing UI scene/texture continuation is still synchronous and unbounded. Implement or prove bounded publication and rollback for cancellation or an exception after publication begins, especially multi-file textured imports and mixed archive batches.
- Review saved-project async lifetime, plate/preset cleanup on exceptions, unit conversion, assembly transforms, originals recovery after subsequent edits and undo behavior.
- Add complete localized catalogs and direct feature documentation for new copy. Runtime layout, all language modes, scale/theme evidence and model-size/interaction measurements remain pending.
- The parent owns integration, preservation pushes, remote proof, release work and task cleanup. Keep this unfinished lane isolated. Do not publish its output as verified.

Next safe action: compile the pinned candidate, repair compiler findings in a separate owned change, then complete the import publication/cancellation contract and its real runtime evidence. Preserve source files and unrelated work. No physical printer or local host power actions are authorized by this checkpoint.
