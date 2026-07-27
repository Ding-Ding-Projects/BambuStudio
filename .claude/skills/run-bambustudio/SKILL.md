---
name: run-bambustudio
description: Run, drive, and screenshot the BambuStudio desktop app headlessly on this GPU-less Windows box. Use when asked to run/start/launch the app, click its UI, load or slice a model, or capture screenshots to verify a change in the real app. Wraps the lowlevel-computer-use cheap CLI + Mesa llvmpipe via driver.py.
---

# Run BambuStudio (headless, GPU-less Windows)

BambuStudio here is a wxWidgets/OpenGL desktop app; this VM has no GPU and no
usable interactive desktop for it. It runs on an **off-screen Windows desktop**
with **Mesa llvmpipe** software GL, driven entirely through
`.claude/skills/run-bambustudio/driver.py` — a wrapper around the
lowlevel-computer-use "cheap" CLI. All paths below are relative to the repo
root. Everything in this file was executed and verified on 2026-07-26.

## Prerequisites (already satisfied on this box — check, don't reinstall)

- Built app at `build/src/Release/bambu-studio.exe` **with Mesa DLLs beside it**
  (`opengl32.dll`, `libgallium_wgl.dll`). Without them the app exits at the
  OpenGL < 2.0 gate.
- Lowlevel MCP venv: `C:\Users\Administrator\Documents\GitHub\lowlevel-computer-use-mcp\.venv`
  (override with env `LLCU_VENV`). Its python runs the driver; its
  `Scripts\lowlevel-computer-use-cheap.exe` is the tool CLI.
- AutoHotkey v2 at `C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe` (for `ahkclick`).

```bash
PY="C:/Users/Administrator/Documents/GitHub/lowlevel-computer-use-mcp/.venv/Scripts/python.exe"
DRV="C:/Users/Administrator/Documents/GitHub/BambuStudio/.claude/skills/run-bambustudio/driver.py"
```

## Run (agent path) — the only path

```bash
"$PY" "$DRV" launch
```

Creates headless desktop `bsrun`, launches the app with the Mesa env, waits for
`finished init opengl` in a fresh studio log (GL init under llvmpipe takes
1–3 min; timeout 240 s), then prints all windows. The main frame is the
`wxWindowNR` window titled `Untitled - BambuStudio` (1200x800). A `Setup
Wizard` `#32770` may float above it — it does NOT block hwnd-targeted driving;
ignore it.

```bash
"$PY" "$DRV" windows                      # re-list windows (note: field is "handle", not "hwnd")
"$PY" "$DRV" ss --hwnd <H> --out shot.png # PrintWindow screenshot -> file
"$PY" "$DRV" open --model .claude/skills/run-bambustudio/cube.stl
```

`open` spawns a **second instance** holding the model (a cold launch with a
model argument crashes at GL init — the driver refuses it). The new frame is
titled `* Untitled - BambuStudio`; screenshot it and the llvmpipe-rendered 3D
viewport shows the build plate with the model, plus a toast with the model
stats.

Interact (all relayed to run *on* the headless desktop — see Gotchas):

```bash
"$PY" "$DRV" ahkclick --hwnd <H> --x 975 --y 760   # AHK ControlClick, CLIENT coords — use this for buttons
"$PY" "$DRV" click --hwnd <H> --x 210 --y 119      # raw click, DESKTOP-SCREEN coords (frame sits ~(136,95))
"$PY" "$DRV" type --hwnd <H> --text "hello"
"$PY" "$DRV" ahk --code 'PostMessage 0x0111, 6089, 0,, "ahk_id <H>"'  # arbitrary AHK v2 on the desktop
"$PY" "$DRV" children --hwnd <H>                   # enumerate child controls (~1700 for the main frame)
"$PY" "$DRV" tool <cheap-tool-name> --key value    # any other cheap-CLI tool, relayed on-desktop
"$PY" "$DRV" log --lines 40                        # tail newest studio log (AppData/Roaming/BambuStudioInternal/log)
"$PY" "$DRV" stop                                  # kill app + close desktop
```

**Verified end-to-end flow** (slice a cube): `launch` → `open --model cube.stl`
→ `ahkclick` the "Slice plate" button at client `x975 y760` of the 1200x800
model frame → wait ~25 s → `ss` shows the Preview tab with "Sliced · 100
layers", the gcode legend, and time estimation. Custom wx buttons often ignore
raw `click`; `ahkclick` is what actually works.

## Rebuild after editing a GUI source file (~min, verified)

The app logic lives in `build/src/Release/BambuStudio.dll`; `bambu-studio.exe`
is a thin launcher. Incremental rebuild + relink (write it as a .cmd file and
run that — quoting MSBuild args through git-bash mangles them):

```bash
cat > /c/Users/ADMINI~1/AppData/Local/Temp/msb.cmd <<'EOF'
@echo off
cd /d C:\Users\Administrator\Documents\GitHub\BambuStudio
"C:\Program\MSBuild\Current\Bin\MSBuild.exe" build\src\BambuStudio_app_gui.vcxproj /p:Configuration=Release /p:Platform=x64 /m
EOF
cmd //c "C:\Users\ADMINI~1\AppData\Local\Temp\msb.cmd"
```

(Write the .cmd with a quoted heredoc, NOT `printf` — bash printf eats the
backslashes in the format string, `\U` in `C:\Users` becomes "missing unicode
digit". The app must not be running when the link step fires or LNK1104 on the
locked `BambuStudio.dll` — `driver.py stop` first.)

Afterwards verify `BambuStudio.dll`'s mtime advanced — the exe may stay stale
at exit 0 (that's fine, the DLL is what matters). Full/cold builds and deps are
a different beast (VS at the literal path `C:\Program`, `PS_WINSDK=10.0.26100.0`
pin, `build_win.bat -s app-dirty -d deps\build\out_deps -c Release -v 17 -r none`)
— see `HANDOFF.md` and the project memory; not re-verified in this session.

## Run (human path)

None on this box — no GPU, and the RDP session cannot even switch to the
headless desktop (`SwitchDesktop` is denied). The agent path is the only path.

## Gotchas (all hit for real)

- **Headless-desktop lifetime:** a `CreateDesktop` desktop dies with its last
  process. The driver's launch wrapper therefore runs the exe in the
  *foreground* of its `cmd` (no `start`), so the desktop stays alive exactly as
  long as the app. If `windows` suddenly reports nothing / `OpenDesktopW ...
  GetLastError=2`, the app exited and took the desktop with it — relaunch.
- **Cross-desktop hwnd access fails** (`IsWindow` returns false from the
  default desktop). Every hwnd-addressed tool must itself run on the headless
  desktop. The driver relays via `launch_on_headless_desktop` + result-file
  polling; don't call the cheap CLI's `screenshot --hwnd` etc. directly.
- **CRLF poisoning:** generating the launch `.cmd` with Python `write_text`
  turns `\r\n` into `\r\r\n`; the stray `\r` lands in `set` values and silently
  disables Mesa → app dies at the GL gate with no log. The driver writes bytes.
  Same family: inline `set VAR=v && ...` poisons values with trailing spaces.
- **AutoHotkey on this box does not exit when the script ends** — every script
  needs `ExitApp` or it hangs forever. And `/ErrorStdOut` covers *load* errors
  only: a runtime error (e.g. `ControlClick` arg mistakes — param 5 is
  ClickCount, a number) raises an **invisible dialog** on the headless desktop.
  The driver's generated script try/catches into a result file and stragglers
  are force-killed.
- **Coordinate systems differ:** `ahkclick` = client coords of the target
  window; `click` (cheap `mouse_click --hwnd`) = headless-desktop *screen*
  coords (the 1200x800 frame sits at about (136,95)). For UI buttons use
  `ahkclick`; raw `click` at a "client" position will land ~(136,95) off.
- **WebView2 panes never render** in PrintWindow captures — Home/Project
  pages and the Setup Wizard body come out blank. Everything native (topbar,
  sidebar, the GL viewport, ImGui overlays, toasts, sliced preview) renders
  fine. To eyeball a webview page, open its source under `resources/web/` in a
  browser instead.
- **`list_headless_windows` returns `handle`**, not `hwnd`, per window.
- **cmd won't resolve executables from the current directory** on this machine
  (NoDefaultCurrentDirectoryInExePath) — always invoke .cmd/.exe by absolute
  path, and prefer writing a .cmd file over escaping quotes through
  bash→cmd (a `\"...\"` MSBuild invocation arrives mangled and fails with
  "not recognized").

## Troubleshooting (errors actually hit)

| Symptom | Fix |
|---|---|
| `launch` times out, `bs-out.txt` empty, no new studio log | Wrapper env poisoned (CRLF) or Mesa DLLs missing beside the exe — the app died pre-log at the GL gate. Check `%TEMP%\bs-run-driver\bs-launch.cmd` line endings; check `opengl32.dll` exists. |
| `OpenDesktopW('bsrun') ... GetLastError=2` | Desktop is gone because the app exited. `launch` again. |
| `ahkclick` → `no result (AHK hung?)` | Runtime error dialog invisible on the headless desktop (bad hwnd/coords). Driver already kills leftover `AutoHotkey64.exe`; fix the args and retry. |
| `ahkclick` → `ERR: Parameter #5 of ControlClick requires a Number` | You edited the generated script's arg order; param 4 = button name, 5 = click count (number), 6 = options. |
| App started but a cold `launch` with a model crashed 0xC0000005 | Never pass a model on first launch; `launch` empty, then `open --model`. |
| Slice click does nothing | Raw `click` doesn't press custom wx buttons; use `ahkclick` (client coords). |

## Press buttons and menu items BY NAME — `press.py`

`driver.py` clicks a pixel. `press.py` presses a **label**, which is what you
usually want: pixel coordinates drift every build, and this app's topbar menus
are owner-drawn (AutoHotkey `MenuSelect` answers "unsupported menu", and
`ControlClick` on the menu labels does nothing).

```bash
"$PY" "$DRV_DIR/press.py" menus                    # every menu item + its live command id
"$PY" "$DRV_DIR/press.py" press "Version history"  # opens File ▸ Version history…
"$PY" "$DRV_DIR/press.py" controls --filter ink    # labelled child controls
"$PY" "$DRV_DIR/press.py" press "Slice plate"      # falls back to a child-control click
"$PY" "$DRV_DIR/press.py" id 888                   # raw WM_COMMAND escape hatch
```

Verified: `press "Version history"` opens the dialog with no coordinates
anywhere. Menu ids are `wxID_ANY` allocations — they shift between builds
(`Version history…` was 849 in one build and 888 in the next), so **never
hardcode one**; `press.py` enumerates live and caches per frame hwnd
(`--refresh` re-scans).

How it works, and the two things that make it work at all:

- Menu ids can only be learned by opening each menu for real, catching
  `EVENT_SYSTEM_MENUPOPUPSTART`, and asking the popup for its `HMENU` via
  `MN_GETHMENU`. Pressing then needs no menu at all — it posts `WM_COMMAND`
  straight to the frame.
- **The frame must be parked at (-183, -6) during discovery.** At its normal
  position the owner-drawn strip accepts the posted clicks and opens nothing.
  This constant is empirical, is restored afterwards, and is the difference
  between "menus enumerate" and "menus never open".
- Everything runs on the headless desktop: `press.py` relays a worker copy of
  itself through `launch_on_headless_desktop`, because hwnd calls fail
  cross-desktop and because a separate process spawned while a menu is open
  kills that menu.

Gotcha worth knowing if you extend it: **ctypes swallows exceptions raised
inside an `EnumChildWindows` callback** — they reach stderr, which nobody sees
on an off-screen desktop, and the enumeration silently returns an empty list.
Collect handles in the callback and read their properties outside it.

## Files

- `press.py` — press buttons/menu items by name (above). Start here for UI work.
- `driver.py` — the harness (this directory). Env overrides: `LLCU_VENV`, `BS_DESKTOP`.
- `cube.stl` — 684-byte binary cube (20 mm), the standard test model for `open`/slice.
- `popovercap.py` — one-process click+catch+paint+capture for TRANSIENT popovers
  (e.g. the SearchField regex-builder popover): any separate process spawned on the
  desktop while a popover is open focus-kills it, so the click, the WinEvent catch,
  the repaint, and the PrintWindow must happen in a single on-desktop process. Copy
  it to a writable dir first (it writes .bmp/.txt beside itself), then run it
  on-desktop via `driver.py tool` semantics:
  `launch_on_headless_desktop --command "<venv-python> <copy>/popovercap.py <panel-hwnd> <local-x> <local-y> <outname>"`.
