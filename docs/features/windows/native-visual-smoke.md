# Native Windows visual smoke test

## Purpose

`scripts/ci/Test-WindowsNativeVisual.ps1` is a small native-startup and capture gate for the unsigned
Windows candidate. It is intended to detect a process that exits before presenting a window, a
missing or implausibly small top-level window, and blank or nearly uniform captures. It produces PNG
evidence for these scenarios:

| Capture | Mode | Windows app theme |
|---|---|---|
| `light-en.png` | `en` | Light |
| `dark-yue_HK.png` | `yue_HK` | Dark |
| `light-bilingual.png` | `bilingual_en_yue_HK` | Light |

Each launch receives a fresh data directory below `RUNNER_TEMP`. The script sets only the temporary
runner account's language preference and Windows app-theme value, waits for the native top-level
window, captures it with `PrintWindow` (falling back to a screen copy), and checks dimensions, file
size, and sampled color diversity. It then closes or terminates the test process, removes its data and
language key, and restores the runner's prior theme value. The workflow uploads any PNGs even when a
later assertion fails, which leaves useful triage evidence.

## Execution boundary

The script refuses to run unless all three conditions are true: `-CiExecutionApproved` was passed,
`GITHUB_ACTIONS=true`, and `RUNNER_ENVIRONMENT=github-hosted`. This restricts automatic execution of
the unsigned binary to a disposable GitHub-hosted Windows runner. The same guard is used for the
installer behavior matrix.

No local unsigned application or installer execution was performed while preparing this work. A
separate local Windows Sandbox review still requires explicit action-time confirmation and should use
a sandbox-local data directory, no user credentials, and networking disabled when feasible.

## What the smoke test does not prove

The capture gate is not a pixel-diff regression suite, OCR check, accessibility audit, font-family
introspection test, or traversal of every native screen. A nonblank PNG does not by itself prove that
Roboto, Microsoft JhengHei UI, every CJK glyph, every bilingual disclosure, or every light/dark token
rendered correctly. Those claims require inspection of the uploaded evidence and broader manual or
automated coverage.

## Verification status

The Windows workflow is configured to run the three scenarios after the native build and C++ tests,
then publish the evidence as `BambuStudio_Windows_native_visual_<version>`. A successful candidate run
and human review of its captures are still pending; no future run or release is represented here as
passed evidence.
