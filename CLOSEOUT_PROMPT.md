# Bambu Studio continuation, 27 November 2026

Start from `main`. See the "Contract repairs" and "Branch integration" sections of HANDOFF.md.

Done: every local work branch is merged into `main`, and the contract failures new to the integrated source are repaired. In the Visual Studio developer environment the Node suite gives 976 passed, 42 failed and 1 skipped of 1019; none of the 42 passes on the released `6be454d35`. The seven changed or previously uncompiled C++ files compile.

Not done: no link, packaged build, runtime check or desktop app release of the combined source. `md3-v338` is still the latest release. The 42 older failures and the translation catalogue check have not been examined or run.

Remote branches kept because they are not merged: `gap/lan-drop-site-w1`, `gap/lan-drop-app-w1`, `gap/lan-drop-integrate-w1`, `md3-dynamic-color-wip`. The separate companion release `lan-setup-v1.0.1-46104015502-1` was published and has not been exercised.

Next steps: verify the combined native build as listed in ROADMAP.md, then run "Windows build and release" on `main` with `publish_release` selected. Before pushing any branch, check the push filters of every workflow. The pushes recorded here carry `[skip ci]`. Keep issue 61 open as the owner asked.
