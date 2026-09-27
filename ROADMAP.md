# Roadmap

## Official-source reapplication

- [x] Start from official `v02.08.04.57` (`f977235e6d736c4c0b650520ac5a5b72cbfe9244`) and preserve the separate baseline diagnostic change.
- [x] Record selected fork source paths, source blob IDs, official blob IDs, and review states in `docs/reapplication/source-manifest.csv`.
- [x] Reapply native controls, workspace and history, print and device flows, Model Creator, localization, website source, and supported Windows packaging source in separate commits.
- [x] Review the project-tab opening adapter for rejected loads, snapshot failures, rollback save targets, and internal Recent Projects entries.
- [ ] Complete independent source review of the project-loading adapter and resolve any further findings.
- [ ] Build the exact official-source candidate and fix verified build failures without dropping requested controls or menus.
- [ ] Package and install the exact built candidate through the supported unsigned Squirrel.Windows route.
- [ ] Compare saved-3MF opening on the official baseline and candidate under isolated profiles, then investigate the installed-release crash using a genuine stack or trace.
- [ ] Exercise the focused project-tab rollback cases in `docs/reapplication/loader-adapters.md`, including named, Untitled, clean, and discarded dirty documents.
- [ ] Verify the requested native controls, print and device flows, Model Creator, workspace/history, localization, and website behavior in their built surfaces.
- [ ] Capture fresh, privacy-reviewed evidence tied to the candidate source and installed package.
- [ ] Update release documentation and publish only after the build, runtime, and distribution checks reach verified outcomes.

## Deliberate exclusions

- [ ] Keep the detachable live-camera widget as separate future work; it is not implemented in this reapplication.
- [ ] Revisit historical screenshots only as historical records. They do not verify the new candidate.
- [ ] Revisit the legacy NSIS source-build installer only if a separate requirement establishes a need for it. The selected route is Squirrel.Windows.
