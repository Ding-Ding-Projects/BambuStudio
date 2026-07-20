![image](https://user-images.githubusercontent.com/106916061/179006347-497d24c0-9bd6-45b7-8c49-d5cc8ecfe5d7.png)
# BambuStudio
Bambu Studio is a cutting-edge, feature-rich slicing software.  
It contains project-based workflows, systematically optimized slicing algorithms, and an easy-to-use graphic interface, bringing users an incredibly smooth printing experience.

This fork's maintained delivery target is the native Material Design 3 Windows application. The
[latest Windows installer](https://github.com/codingmachineedge/BambuStudio/releases/latest/download/BambuStudioMD3-Setup.exe)
is a per-user install and does not require administrator elevation. It is currently unsigned; verify
the accompanying
[SHA-256 file](https://github.com/codingmachineedge/BambuStudio/releases/latest/download/BambuStudioMD3-Setup.exe.sha256)
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

## Material Design 3 preview

These deterministic captures come from the real [`ui-md3`](ui-md3/) app. Select an image to open
the same screen, theme, density, accent, and language state in the live interactive concept.

**Prepare · light theme · English**

[![Bambu Studio Material Design 3 Prepare screen in the light theme and English](docs/readme-assets/material-prepare-light-en.png)](https://codingmachineedge.github.io/BambuStudio/app/?view=prepare&theme=light&density=comfortable&accent=%2322c55e&lang=en)

| Preview · dark theme · Hong Kong Cantonese | Device · compact dark theme · English + Cantonese |
| :---: | :---: |
| [![Bambu Studio Material Design 3 Preview screen in the dark theme and Hong Kong Cantonese](docs/readme-assets/material-preview-dark-yue-hk.png)](https://codingmachineedge.github.io/BambuStudio/app/?view=preview&theme=dark&density=comfortable&accent=%237c5cff&lang=yue_HK) | [![Bambu Studio Material Design 3 Device screen in the compact dark theme with English and Cantonese](docs/readme-assets/material-device-dark-bilingual.png)](https://codingmachineedge.github.io/BambuStudio/app/?view=device&theme=dark&density=compact&accent=%2314b8a6&lang=bilingual_en_yue_HK) |

The candidate Windows pipeline builds and tests the native application, exercises installer upgrade
and recovery behavior on a disposable GitHub-hosted runner, produces a per-file CycloneDX 1.6 SBOM,
and creates GitHub provenance and SBOM attestations for the installer. It validates all three assets
in a draft before publication and refuses to publish unless repository immutable releases are
enabled. These new gates still require a successful candidate run before they become release
evidence. GitHub attestations and checksums are not Authenticode signatures; configuring a trusted
Windows signing identity remains external work.

Bambu Studio is based on [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community.

See this fork's [wiki](https://github.com/codingmachineedge/BambuStudio/wiki),
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

