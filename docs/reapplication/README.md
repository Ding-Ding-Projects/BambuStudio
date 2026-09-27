# Official-source feature reapplication

This branch begins at official Bambu Studio `f977235e6d736c4c0b650520ac5a5b72cbfe9244` plus a separately owned baseline diagnostic change. The feature source is the previous fork commit `c5df6199e1a83b1c94be12e999c0b322fded8730`. `source-manifest.csv` lists each selected path, its category, its status in that comparison, and the exact source and official blob IDs. A review state of "ported" means the source was restored or adapted; it does not mean the result compiled or ran.

The source was reapplied in separate commits for native controls, Model Creator, workspace/history, device and print flows, resources and localization, website design, application hosts, and project loading. The project-loading host includes deliberate changes for rejected loads and restoration, described in `loader-adapters.md`.

The selected Windows packaging route uses `build.bat`, `build-installer.bat`, `OneClickBuildInstaller.cmd`, `scripts/windows/Invoke-OneClickBuild.ps1`, `scripts/windows/Invoke-SquirrelPackage.ps1`, `scripts/windows/Stage-ModelCreatorRenderers.ps1`, and the shared toolchain bootstrap helper. The prior NSIS source-build installer and its automated source-repair scripts were not carried into this route. Official non-Windows source files were retained. Historical captures were not recopied as evidence for this candidate.

A fresh dependency build, application build, Squirrel package, isolated install, saved-3MF opening comparison, feature interaction drive, and new capture set remain pending. The previous installed-release crash has no established causal stack from source inspection alone.
