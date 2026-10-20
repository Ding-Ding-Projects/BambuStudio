# Continuation: branch integration and review, 20 October 2026

Repository: `Ding-Ding-Projects/BambuStudio`. Branch: `main`.

The tested integration candidate is `b07eaf67478dbdcdf485de1010845c44a7b9751a`, based on `07e3b8d206f3e87be6743cca7756237ea9f49102`. Read `docs/integration/branch-integration-20261020.md` for exact source heads, merge resolutions and verification. Four merge commits integrate LAN receiving, motion controls, wording validation and startup diagnostics. Three older LAN branches are content-equivalent to the existing newer LAN squash and remain preserved on their original refs; they were not merged into main's ancestry. No branches were deleted.

Baseline comparison: Node 979 passed / 7 failed / 10 skipped; candidate 1,112 passed / the same 7 failed / 13 skipped. CMake source contracts: both 11 passed and the same one failed. Translation validation, documentation bundle and Express G-code guard checks pass. Focused native model and offline Python assertions also pass. The seven Node failures include four missing-wxWidgets compile prerequisites and three pre-existing source contracts. Offline browser and online catalogue suites were excluded from both runs.

Next steps:

1. Read the hosted build status for the exact new main revision. Do not reuse an older release or a different revision's result.
2. Build with the supported Windows entrypoints, then verify the merged GUI and live LAN container. Do not launch the opt-in visible-desktop startup tracer without selecting the intended host and route.
3. Preserve the current newer Melody, Express printing and updater behavior during any subsequent repair.
4. Keep unrun native, browser and printer checks explicitly pending. Source integration is not release or runtime acceptance.

This record is committed with the integration closeout. Its own commit and remote verification are reported separately in the task result.
