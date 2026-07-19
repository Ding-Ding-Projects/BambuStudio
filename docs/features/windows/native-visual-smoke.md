# Native Windows visual smoke test

## Purpose

`scripts/ci/Test-WindowsNativeVisual.ps1` is a deterministic native-startup and capture gate for the
unsigned Windows candidate. A Windows-only, environment-gated wxWidgets surface is entered before
normal wizard, profile, or network startup. It is intended to detect a process that exits before
presenting the exact test window, a wrong scenario/language contract, missing Cantonese text, a
missing or implausibly small top-level window, duplicate scenarios, and blank, low-contrast, or
theme-inconsistent captures. It produces PNG and JSON evidence for these scenarios:

| Capture | Mode | Evidence theme |
|---|---|---|
| `light-en.png` | `en` | Light |
| `dark-yue_HK.png` | `yue_HK` | Dark |
| `light-bilingual.png` | `bilingual_en_yue_HK` | Light |

Each launch receives a fresh data directory below `RUNNER_TEMP` and a versioned scenario request in
the child-process environment. Both the script and application require CI, GitHub Actions, and a
GitHub-hosted runner; an unknown protocol/scenario is rejected without entering normal startup. The
script waits for the exact title and Win32 child-text contract, checks the required or forbidden CJK
presence, captures with `PrintWindow` (falling back to a screen copy), and validates dimensions,
file size, sampled color diversity, contrast, light/dark luminance separation, unique SHA-256 hashes,
and pairwise RGB difference. It then closes or terminates the process and removes only its unique
temporary data. Per-scenario metadata plus `summary.json` record the observed text and image metrics.
The workflow uploads available PNG/JSON files even when a later assertion fails, leaving triage
evidence.

## Execution boundary

The script refuses to run unless `-CiExecutionApproved` was passed and `CI=true`,
`GITHUB_ACTIONS=true`, and `RUNNER_ENVIRONMENT=github-hosted`. It also requires the executable to be
the built `bambu-studio.exe` beneath `GITHUB_WORKSPACE`, its sibling resources directory to exist,
and all evidence to stay beneath `RUNNER_TEMP`. This restricts automatic execution of the unsigned
binary to a disposable GitHub-hosted Windows runner. The installer behavior matrix uses the same
execution boundary.

No local unsigned application or installer execution was performed while preparing this work. A
separate local Windows Sandbox review still requires explicit action-time confirmation and should use
a sandbox-local data directory, no user credentials, and networking disabled when feasible.

## What the smoke test does not prove

The capture gate is not a golden-image pixel-diff suite, OCR check, accessibility audit, font-family
introspection test, or traversal of ordinary product screens. It proves that the compiled Windows
wxWidgets path can render the versioned MD3 evidence contract; it does not by itself prove that every
production screen, Roboto/Microsoft JhengHei UI selection, CJK glyph, bilingual disclosure, or
light/dark token rendered correctly. Those claims require inspection of the uploaded evidence and
broader manual or automated coverage.

## Verification status

The Windows workflow is configured to run the three scenarios after the native build and C++ tests,
then publish the evidence as `BambuStudio_Windows_native_visual_<version>`. A successful candidate run
and human review of its captures are still pending; no future run or release is represented here as
passed evidence.
