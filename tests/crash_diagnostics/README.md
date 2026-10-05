# Native crash diagnostic regression

Run `powershell -ExecutionPolicy Bypass -File tests/crash_diagnostics/run.ps1` from a Windows host with Visual Studio C++ tools and the Windows SDK. The runner discovers the installed toolchain and builds the production `StackWalker.cpp` directly, without a PDB or the application's dependency superbuild.

The executable exports its calling function, walks a real x64 context with a custom symbol path, and requires frames to remain available when source lines are absent. It also requires a retained function symbol, full 16-digit frame addresses and module-relative output. This detects the former behavior that discarded every frame when symbol or line resolution failed.

During the October 2026 repair, the original implementation and matching header compiled successfully but the executable exited 1 because no frames survived. The repaired implementation returned 0 and retained five frames. This verifies diagnostic behavior; it does not identify the cause of any historical application crash.
