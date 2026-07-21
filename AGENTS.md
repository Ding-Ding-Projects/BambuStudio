# Build dependency policy

When a task requires configuring, building, testing, packaging, or launching Bambu Studio, agents must make a best-effort dependency bootstrap part of the task.

1. Inspect the required toolchain and project prerequisites first. Prefer the repository's documented build/bootstrap scripts, lockfiles, dependency superbuild, and supported package-manager paths.
2. If a normal build prerequisite is missing, install it automatically before retrying the build. Do not stop merely to ask the user to install ordinary tools, SDKs, libraries, or project dependencies.
3. Install only what is required and reuse existing build/dependency caches when valid. Do not downgrade or remove unrelated packages, change security policy, or persist credentials.
4. If installation requires unavailable credentials, a license acceptance the agent cannot make, administrator access that is denied, or a potentially disruptive system change, record the exact blocker and continue with every safe verification step available.
5. In the task handoff, report the detected prerequisites, every dependency installed, the build/test command retried, and the resulting status.

On Windows, check for the appropriate Visual Studio C++ toolset and Windows SDK, CMake, Git, and this repository's dependency build before treating a missing toolchain as a blocker.
