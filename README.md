![image](https://user-images.githubusercontent.com/106916061/179006347-497d24c0-9bd6-45b7-8c49-d5cc8ecfe5d7.png)
# BambuStudio
Bambu Studio is a cutting-edge, feature-rich slicing software.  
It contains project-based workflows, systematically optimized slicing algorithms, and an easy-to-use graphic interface, bringing users an incredibly smooth printing experience.

This fork's maintained delivery target is native Windows UI modernization informed by Material Design 3. The
[latest Windows installer](https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download/BambuStudioMD3-Setup.exe)
is a per-user install and does not require administrator elevation. It is currently unsigned; verify
the accompanying
[SHA-256 file](https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download/BambuStudioMD3-Setup.exe.sha256)
before running it. Upstream cross-platform releases remain available from
[Bambu Lab](https://github.com/bambulab/BambuStudio/releases/), but macOS and Linux are outside this
fork's release acceptance gate.

The Windows UI provides three canonical fork modes: English (`en`), playful Hong Kong Cantonese
preview (`yue_HK`), and compact English + Cantonese preview (`bilingual_en_yue_HK`). Existing Bambu Studio
locales remain available. Missing Cantonese copy falls back to English; native bilingual presentation
is opt-in on migrated surfaces, and the Cantonese catalog remains a curated preview pending broader
human review. See the
[language-mode documentation](docs/features/windows/language-modes.md) for coverage and fallback
details.

## Native UI modernization (Material Design 3)

The production wxWidgets application now consumes the vendored Material Design 3 design system
end-to-end. The canonical in-repo design source is [`ui-md3/design-system/`](ui-md3/design-system/);
its token values match `src/slic3r/GUI/Widgets/MD3Tokens.hpp` exactly, and the native code resolves
colors through semantic tokens instead of hardcoded hexes. A ground-up migration converted theme
colors and fonts across essentially the whole GUI tree (roughly 120 files over six waves), backed by
Roboto and Roboto Mono shipped as application resources. Contextual schemes are resolved per
workspace: brand green for Prepare and general UI, Preview purple for the G-code preview, and Device
teal for the printer surfaces. Functional data colors (filament swatches, G-code feature colors, 3D
paint palettes) are deliberately preserved.

An element-by-element parity audit reports the color, token, and typography layer as complete. The
remaining deltas are structural component anatomy — the camera-HUD overlay system, the Material
Symbols icon-font infrastructure, and some pill-geometry variants — documented as future work in the
[roadmap](ROADMAP.md), not mis-colorings. This is a completed token layer, not yet a faithful
component-by-component Material Design 3 rewrite.

Two native features were added in the same effort:

- A native OpenGL model preview for the MakerWorld "Download and Open" flow shows an interactive
  orbit/zoom/fit view before import, with **Open in Prepare** and **Close** actions and a
  failure-safe fallback to the normal import. See
  [MakerWorld OpenGL preview](docs/features/model-preview/makerworld-opengl-preview.md).
- A dockable Prepare sidebar can be docked left, right, top, or bottom (default left) and re-docks
  live from a Preferences control. See
  [Dockable Prepare sidebar](docs/features/prepare/dockable-sidebar.md).

Full documentation of the token layer, migration, failure modes, and audit result is in
[Vendored Material Design 3 design system](docs/features/design-system/md3-design-system.md).

### Installed-app captures

These are full-display compositor captures of the locally installed native executable, not
`ui-md3` reference images. They were reviewed on 2026-07-20 and predate the full token sweep;
fresh captures of the fully migrated tree are still pending. They demonstrate native modernization
only, not full component-anatomy conformance.

| Home | Filament Manager |
| :---: | :---: |
| ![Native Home](docs/readme-assets/native-material-home-light-en.png) | ![Native Filament Manager](docs/readme-assets/native-material-filament-manager-light-en.png) |

| Device boundary | Project Version History |
| :---: | :---: |
| ![Native Device plug-in gate](docs/readme-assets/native-material-device-plugin-gate-light-en.png) | ![Native Version History](docs/readme-assets/native-material-project-history-light-en.png) |

### Interactive design reference

The images below are deterministic captures of the separate [`ui-md3`](ui-md3/) interactive design
reference, not screenshots of the native application. Select an image to open the same reference
screen, theme, density, accent, and language state. The installed-app captures are the separate
gallery above.

**Prepare · light theme · English**

[![Bambu Studio Material Design 3 Prepare design reference in the light theme and English](docs/readme-assets/material-prepare-light-en.png)](https://ding-ding-projects.github.io/BambuStudio/app/?view=prepare&theme=light&density=comfortable&accent=%2322c55e&lang=en)

| Preview · dark theme · Hong Kong Cantonese | Device · compact dark theme · English + Cantonese |
| :---: | :---: |
| [![Bambu Studio Material Design 3 Preview design reference in the dark theme and Hong Kong Cantonese](docs/readme-assets/material-preview-dark-yue-hk.png)](https://ding-ding-projects.github.io/BambuStudio/app/?view=preview&theme=dark&density=comfortable&accent=%237c5cff&lang=yue_HK) | [![Bambu Studio Material Design 3 Device design reference in the compact dark theme with English and Cantonese](docs/readme-assets/material-device-dark-bilingual.png)](https://ding-ding-projects.github.io/BambuStudio/app/?view=device&theme=dark&density=compact&accent=%2314b8a6&lang=bilingual_en_yue_HK) |

## Project history

The native app includes app-local, Git-backed version history for `.3mf` projects. Each retained
revision is a complete project snapshot in an isolated bare repository below Bambu Studio's data
directory, never a `.git` directory beside the user's project. Automatic edits and completed manual
saves are queued in order; identical snapshots are suppressed. **File → Version history** lists and
restores revisions without directly overwriting the saved project, and **Save As** forks the existing
history to the new project identity when it is safe to do so.

History is local to this device: it is not pushed to the source-code repository, synced to another
computer, or a replacement for backups. There is not yet a retention or pruning policy, so storage
can grow with project size and edit count. A restore changes the open session; the project file is
only replaced when the user explicitly saves it.

The Windows pipeline builds and tests the native application, exercises installer upgrade
and recovery behavior on a disposable GitHub-hosted runner, produces a per-file CycloneDX 1.6 SBOM,
and creates GitHub provenance and SBOM attestations for the installer. It validates all three assets
in a draft before publication and refuses to publish unless repository immutable releases are
enabled. The current Material and project-history changes still require a successful final workflow
run before they become release evidence. GitHub attestations and checksums are not Authenticode
signatures; configuring a trusted
Windows signing identity remains external work. Local focused validation on commit
`3b00dc6aa` passed `language_mode_tests`, `project_history_tests`, and
`deterministic_bbs_3mf_tests` (3/3). This focused gate is not the full aggregate suite: the
upstream aggregate `libslic3r_tests` has current API-drift compile failures and `libnest2d_tests`
has known baseline runtime failures, so both are explicitly waived from this branch gate pending
upstream repair. On the migrated tree the hosted `Build BambuStudio` job succeeds (runs
[`29848731027`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/29848731027) and
[`29862992010`](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/29862992010)), but a
fully green run that also publishes the immutable release has not yet completed. See
[`HANDOFF.md`](HANDOFF.md) for the authoritative, current CI state.

Bambu Studio is based on [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community.

See this fork's [wiki](https://github.com/Ding-Ding-Projects/BambuStudio/wiki),
[feature documentation](docs/README.md), [roadmap](ROADMAP.md), and [handoff](HANDOFF.md) for the
MD3 rewrite, verification status, and Windows release details. The original documentation remains in
[`doc/`](doc/).

# What are Bambu Studio's main features?
Key features are:

- Basic slicing features & GCode viewer
- Multiple plates management
- Remote control & monitoring
- Auto-arrange objects
- Auto-orient objects
- Hybrid/Tree/Normal support types, Customized support
- multi-material printing and rich painting tools
- Upstream multi-platform source support; this fork accepts native Windows releases only
- Global/Object/Part level slicing parameters

Other major features are:

- Advanced cooling logic controlling fan speed and dynamic print speed
- Auto brim according to mechanical analysis
- Support arc path(G2/G3)
- Support STEP format
- Assembly & explosion view
- Flushing transition-filament into infill/object during filament change

# How to compile on Windows

Use the upstream
[Windows compile guide](https://github.com/bambulab/BambuStudio/wiki/Windows-Compile-Guide) for a
developer build. The fork's release configuration is encoded in
[`.github/workflows/build_bambu.yml`](.github/workflows/build_bambu.yml) and is orchestrated by
[`.github/workflows/build_all.yml`](.github/workflows/build_all.yml). The release workflow enables
native C++ tests and packages the installed payload with NSIS; see the
[Windows CI and supply-chain documentation](docs/features/releases/windows-release-supply-chain.md)
before treating a local build as equivalent to a published artifact.

# Report issue
You can add an issue to the [github tracker](https://github.com/bambulab/BambuStudio/issues) if **it isn't already present.**

# License
Bambu Studio is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.

The bambu networking plugin is based on non-free libraries. It is optional to the Bambu Studio and provides extended networking functionalities for users.
By default, after installing Bambu Studio without the networking plugin, you can initiate printing through the SD card after slicing is completed.

