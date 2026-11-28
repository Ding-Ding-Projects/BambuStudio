# Bambu Studio continuation, 27 November 2026

Start from `main`. State recorded here covers `765e397ed59ab26bc391687797ecb090d020c35f`, pushed and read back before this note was committed. See the "Branch integration" section of HANDOFF.md.

Done: every local work branch is merged into `main`. 10 linked worktrees, 12 local branches and 67 merged remote branches were removed after a verified repository archive. Only `main`, `gap/lan-drop-site-w1`, `gap/lan-drop-app-w1`, `gap/lan-drop-integrate-w1` and `md3-dynamic-color-wip` remain on the remote. The last four are not merged and are retained.

Not done: the combined source has no native build and no desktop app release. `md3-v338` is still the latest release. The Node contract suite on `46c813548` gave 910 passed, 105 failed and 1 skipped of 1016; 22 failing entries do not fail on `6be454d35`. C++ contracts that need `cl.exe`, the translation catalogue check and all native runtime checks were not run.

Published on the way: `lan-setup-v1.0.1-46104015502-1`, the separate hosting setup companion, not marked latest and not exercised.

Next steps: repair the 22 entries, verify the combined native build as listed in ROADMAP.md, then run "Windows build and release" on `main` with `publish_release` selected. The pushes recorded here carry `[skip ci]`. Keep issue 61 open as the owner asked.
