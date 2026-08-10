# Copy-paste Codex prompt

Work in `Ding-Ding-Projects/BambuStudio` from commit `8f2ba7047e385e2d28c8a64d5cd9d1b8689f507f` or a reviewed descendant.

Read `HANDOFF.md`, `ui-md3/design-system/`, `docs/features/design-system/md3-parity-register.md`, `CODEX_HANDOFF_BAMBUSTUDIO_MD3.md`, and `ACCEPTANCE_MATRIX_BAMBUSTUDIO_MD3.md` before editing.

Apply the included repository-relative overlay. It replaces the 34 remaining legacy SVG composites used by `GLGizmosManager.cpp` with deterministic MD3-token artwork while preserving filenames, dimensions, axis meaning, and behavior. Do not replace working migrated UI, add another UI framework, or refactor slicing/model/device code.

Run `py scripts/validate_md3_gizmo_assets.py --root .`, then build with the repository's supported Windows toolchain and drive the real app through `.claude/skills/run-bambustudio/`. Verify light/dark, comfortable/compact, 100/150/200% scaling, custom accent, keyboard hint, fit camera over light/dark scene content, bold/italic, and every X/Y/Z align/distribute operation. Capture screenshots and logs. Mark `gizmo-rail-svg-icons` done only after all native checks pass.

Also execute the two bounded bug investigations already documented in `HANDOFF.md`: the unreproduced crash and the “model has no data” tab failure. Do not claim either fixed without a reproduction and regression test. Keep each proven fix in a separate commit.

Do not overengineer. Preserve public behavior and current resource keys. Finish with a full verified source ZIP, checksum, changed-file list, build/test evidence, and an explicit list of anything still unverified.
