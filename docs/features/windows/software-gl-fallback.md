# Software OpenGL fallback (Mesa llvmpipe)

Bambu Studio MD3 requires OpenGL 2.0. On machines whose display driver cannot
provide it — most commonly virtual machines running on the "Microsoft Basic
Display Adapter" — earlier builds showed the "Unsupported OpenGL version"
error and exited, which users reported as "the app does not launch". Windows
installs now self-heal on such machines by switching to Mesa's llvmpipe
software rasterizer automatically.

## Behavior

- The installer ships two Mesa DLLs in the **inert** `mesa\` subfolder of the
  installation directory: `mesa\opengl32.dll` and `mesa\libgallium_wgl.dll`.
  While they stay in that subfolder they have no effect.
- At startup, when the created OpenGL context reports a version lower than
  2.0 (`OpenGLManager::init_gl`, `src/slic3r/GUI/OpenGLManager.cpp`), the app:
  1. checks that the `BBS_SOFTGL_RETRIED` environment variable is **not** set
     (relaunch-loop guard),
  2. checks that both DLLs exist in the `mesa\` subfolder beside
     `bambu-studio.exe`,
  3. checks that the executable's directory is writable,
  4. copies both DLLs **beside** `bambu-studio.exe` (`libgallium_wgl.dll`
     first, so a half-copied pair is never left behind), and
  5. relaunches itself with the identical command line plus the environment
     `BBS_SOFTGL_RETRIED=1`, `GALLIUM_DRIVER=llvmpipe`,
     `MESA_GL_VERSION_OVERRIDE=3.3`, `LIBGL_ALWAYS_SOFTWARE=1`, then exits.
- The relaunched process loads the Mesa `opengl32.dll` instead of the system
  one — Windows resolves a statically linked `opengl32.dll` from the
  executable's directory first — and the full UI renders in software.
- When any precondition fails, or the retried process still sees OpenGL below
  2.0, the familiar error dialog appears, extended with one sentence
  explaining that the software-rendering fallback failed or was unavailable.
- When the app runs on llvmpipe it logs
  `OpenGL renderer is Mesa llvmpipe (software rasterizer); the UI renders
  without GPU acceleration.` at info level. There is deliberately no UI nag.

## When it triggers

Only on Windows, only when the OpenGL version gate (< 2.0) fires, and only
once per launch chain: the relaunched process carries `BBS_SOFTGL_RETRIED=1`
in its environment, so it can never spawn another generation, and a
process-local flag limits the attempt to once per process even if GL
initialization is re-entered. Machines with a working GPU driver never reach
any of this code.

## How to undo

Delete the two copied DLLs beside the executable:

- `<install dir>\opengl32.dll`
- `<install dir>\libgallium_wgl.dll`

The next start uses the system OpenGL driver again (and would re-trigger the
fallback if the driver still lacks OpenGL 2.0). The `mesa\` subfolder itself
is harmless and is removed by the uninstaller, together with the two runtime
copies if the fallback ever fired (the generated uninstall manifest in
`packaging/windows/GenerateUninstallInclude.ps1` owns both).

## Security considerations

- The Mesa binaries come from the upstream
  [pal1000/mesa-dist-win](https://github.com/pal1000/mesa-dist-win) project,
  pinned in `.github/workflows/build_bambu.yml` to release **26.1.3**
  (`mesa3d-26.1.3-release-msvc.7z`), verified against a recorded SHA-256 for
  the archive **and** for each of the two extracted x64 DLLs. A hash mismatch
  fails the build, so a tampered or silently replaced upstream asset can
  never enter the payload.
  - Archive SHA-256:
    `6dd431f4620cea73970b13e3ffa94f721f2a3924306b8a4283c97648cdb6eb9c`
  - `x64\opengl32.dll` SHA-256:
    `12499866437a161d2b250d5105188ae00732dd74b4bebbcdf972e6145af00f9e`
  - `x64\libgallium_wgl.dll` SHA-256:
    `1895f8c19ede5efd0497f9dfab463b19bf4377e3af7c06c2d4d073e4680c5f69`
- The copy step never overwrites nothing-to-do-with-us files: it only creates
  `opengl32.dll` / `libgallium_wgl.dll` beside the executable, inside the
  per-user install directory.
- The payload SBOM (`New-WindowsCycloneDxSbom.ps1`) records the shipped
  `mesa/` files with their hashes like every other payload file.

## Failure modes

| Situation | Result |
| --- | --- |
| `mesa\` DLLs missing (e.g. portable unzip without the folder) | Error dialog with the "fallback unavailable" sentence. |
| Install directory not writable | Error dialog with the "fallback unavailable" sentence. |
| Relaunch itself fails (`CreateProcessW` error) | Environment restored; error dialog with the "fallback unavailable" sentence; the copied DLLs remain and take effect on the next manual start. |
| Retried process still reports OpenGL < 2.0 | Error dialog with the "already tried" sentence; no further relaunch. |

## Verification

- CI (`build_bambu.yml`, step "Stage Mesa software OpenGL fallback payload")
  downloads the pinned archive, verifies all three hashes, and stages the two
  x64 DLLs into `install-dir\mesa\` before SBOM generation, the portable zip,
  and NSIS packaging — so the installer test pass exercises install and
  uninstall of the `mesa\` payload automatically.
- Manually proven on a VM with the Microsoft Basic Display Adapter: placing
  the same two DLLs beside `bambu-studio.exe` with the environment above
  renders the full UI via llvmpipe.
