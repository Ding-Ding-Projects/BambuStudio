# Bambu Studio continuation, 27 November 2026

Continue from the integration changes recorded in HANDOFF.md and ROADMAP.md. Preserve the work of each contributor and validate the combined source before publishing a release.

The published baseline is md3-v338 at 6be454d35815295ac6d8d6361fa2807e868a7b03. Issue 53 closed after the released automation executable and SBOM were checked. Keep issue 61 open as requested by the owner for a separate desktop confirmation. The hosted interactive installation passed, opened the workspace and setup wizard by itself and stayed alive for 241.63 seconds. Its evidence is on issue 61.

The first completed local native build is 0a12cd0945b782fb28d02cfc0cd1c02ed1ee094b. The second build was started at 350301d4c9c025cd6b0310bbf1e0c100cd6a90b9 and includes upstream release v02.09.02.62, three distinct slicing states, live extrusion snapshots, corrected melody generation and server-management changes. Later source changes require an additional build and direct evidence. See docs/integration/reliability-56-acceptance.md for the measured reliability results, and docs/features/windows/minecraft-servers.md for the edition and deployment coverage matrix.

Verify the selected plate in normal slicing, stale print setup and reslicing. Include sequential objects, two distinct models, cancellation, plate switching and reduced motion. Check the corrected melody in both computer preview and exported print data. Record any physical operation separately. Reproduce the corrected ink deletion and history restore on synthetic data, and verify backed-up data before any destructive cleanup.

After the current build and focused checks finish, record the exact source and evidence. Release and close only what meets the owner's acceptance criteria. The existing publication check is unresolved. No public release is claimed for this combined candidate.