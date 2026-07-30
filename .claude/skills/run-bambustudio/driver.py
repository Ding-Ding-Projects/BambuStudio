#!/usr/bin/env python3
"""Headless driver for BambuStudio on this GPU-less Windows box.

Run with the repository-vendored Lowlevel MCP venv python:
  vendor\\lowlevel-computer-use-mcp\\.venv\\Scripts\\python.exe driver.py <cmd> ...

The app runs on an off-screen ("headless") Windows desktop created by the
lowlevel-computer-use cheap CLI, with Mesa llvmpipe software GL (the Mesa
DLLs must sit beside bambu-studio.exe; the build tree already has them).

Key constraint this driver encapsulates: hwnd-addressed tools (screenshot,
mouse_click, type_text, list_child_windows, AHK ControlClick) FAIL when run
from the default desktop against windows living on the headless desktop
(IsWindow fails cross-desktop). So every such call is relayed: the cheap CLI
(or AutoHotkey) is itself launched ON the headless desktop via
launch_on_headless_desktop, wrapped in `cmd /c ... 1>resultfile 2>&1`, and
the driver polls the result file.

Commands (all print JSON):
  launch [--fresh]           create desktop, launch empty app, wait for GL init
  open --model PATH          spawn a 2nd instance holding PATH (app must be running)
  windows                    list toplevel windows on the headless desktop
  ss --hwnd H --out FILE     PrintWindow screenshot of window H -> FILE
  click --hwnd H --x X --y Y [--button left|right] [--double]
  ahkclick --hwnd H --x X --y Y   AHK v2 ControlClick (custom wx buttons need this)
  type --hwnd H --text T
  children --hwnd H          list child windows/controls of H
  tool NAME [--key value]... run any cheap-CLI tool ON the headless desktop
  log [--lines N]            tail the newest studio log
  stop                       kill bambu-studio + close the headless desktop
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
EXE = REPO / "build" / "src" / "Release" / "bambu-studio.exe"
LLCU_VENV = Path(os.environ.get("LLCU_VENV", REPO / "vendor" / "lowlevel-computer-use-mcp" / ".venv"))
CHEAP = LLCU_VENV / "Scripts" / "lowlevel-computer-use-cheap.exe"
DESKTOP = os.environ.get("BS_DESKTOP", "bsrun")
STATE = Path(os.environ.get("TEMP", r"C:\Windows\Temp")) / "bs-run-driver"
LOGDIR = Path(os.environ["APPDATA"]) / "BambuStudioInternal" / "log"
GL_READY = "finished init opengl"


def die(msg):
    print(json.dumps({"ok": False, "error": msg}))
    sys.exit(1)


def cheap(tool, **kw):
    """Run a cheap-CLI tool locally (default desktop) and parse its JSON."""
    argv = [str(CHEAP), tool]
    for k, v in kw.items():
        argv += [f"--{k}", str(v)]
    out = subprocess.run(argv, capture_output=True, text=True, timeout=120)
    try:
        return json.loads(out.stdout)
    except json.JSONDecodeError:
        die(f"{tool}: non-JSON output: {out.stdout[:400]} / {out.stderr[:400]}")


def relay(cmdline, timeout=30):
    """Run a full command line ON the headless desktop, return its stdout.

    cmdline must use plain Windows paths (no spaces are fine when quoted).
    """
    STATE.mkdir(parents=True, exist_ok=True)
    res = STATE / f"relay-{int(time.time()*1000)}.txt"
    wrapped = f'cmd /c "{cmdline} 1>"{res}" 2>&1"'
    r = cheap("launch_on_headless_desktop", name=DESKTOP, command=wrapped)
    if not r.get("ok"):
        die(f"relay launch failed: {r}")
    t0 = time.time()
    while time.time() - t0 < timeout:
        if res.exists() and res.stat().st_size > 0:
            time.sleep(0.5)  # let the writer finish
            txt = res.read_text(errors="replace")
            res.unlink(missing_ok=True)
            return txt
        time.sleep(0.5)
    die(f"relay timed out after {timeout}s: {cmdline}")


def relay_tool(tool, timeout=30, **kw):
    """Run a cheap-CLI tool ON the headless desktop and parse its JSON."""
    args = " ".join(f'--{k} "{v}"' for k, v in kw.items())
    txt = relay(f'"{CHEAP}" {tool} {args}', timeout=timeout)
    try:
        return json.loads(txt)
    except json.JSONDecodeError:
        die(f"{tool} (relayed): non-JSON output: {txt[:400]}")


def newest_log():
    logs = sorted(LOGDIR.glob("studio_*.log*"), key=lambda p: p.stat().st_mtime)
    return logs[-1] if logs else None


def wait_gl_init(t0, timeout=240):
    """Wait until a studio log newer than t0 contains the GL-ready line."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        for p in LOGDIR.glob("studio_*.log*"):
            if p.stat().st_mtime >= t0 - 1:
                try:
                    if GL_READY in p.read_text(errors="replace"):
                        return p
                except OSError:
                    pass
        time.sleep(2)
    extra = ""
    for cap in ("bs-out.txt", "bs-out-model.txt"):
        p = STATE / cap
        if p.exists() and p.stat().st_mtime >= t0 - 1:
            extra += f" | {cap}: {p.read_text(errors='replace')[-300:].strip()}"
    die(f"no studio log contained '{GL_READY}' within {timeout}s "
        f"(app crashed at GL init? check `driver.py log`){extra} "
        f"| windows now: {list_windows()}")


def write_wrapper(model=None):
    """One `set` per line; the exe runs in the FOREGROUND so the wrapper cmd
    stays alive and keeps the headless desktop object alive (a CreateDesktop
    desktop is destroyed when its last process exits). NB: write bytes —
    Path.write_text would rewrite \\n into \\r\\n and a stray \\r in a `set`
    line poisons the value (Mesa env silently disabled)."""
    STATE.mkdir(parents=True, exist_ok=True)
    name = "bs-launch-model.cmd" if model else "bs-launch.cmd"
    w = STATE / name
    out = STATE / ("bs-out-model.txt" if model else "bs-out.txt")
    lines = [
        "@echo off",
        "set GALLIUM_DRIVER=llvmpipe",
        "set MESA_GL_VERSION_OVERRIDE=3.3",
        "set LIBGL_ALWAYS_SOFTWARE=1",
        f'"{EXE}"' + (f' "{model}"' if model else "") + f' 1>"{out}" 2>&1',
    ]
    w.write_bytes(("\r\n".join(lines) + "\r\n").encode("ascii"))
    return w


def list_windows():
    r = cheap("list_headless_windows", name=DESKTOP)
    if not r.get("ok"):
        if "OpenDesktopW" in str(r.get("error", "")):
            return []  # desktop not created yet
        die(f"list_headless_windows: {r}")
    return r.get("windows", [])


def app_windows():
    return [w for w in list_windows() if "BambuStudio" in (w.get("title") or "")]


def cmd_launch(args):
    if not EXE.exists():
        die(f"{EXE} not built — see SKILL.md Build section")
    if not (EXE.parent / "opengl32.dll").exists():
        die(f"Mesa opengl32.dll missing beside {EXE} — GL gate will kill the app")
    if not args.fresh and app_windows():
        print(json.dumps({"ok": True, "already_running": True,
                          "windows": list_windows()}, indent=1))
        return
    cheap("create_headless_desktop", name=DESKTOP)
    t0 = time.time()
    wrapper = write_wrapper()
    r = cheap("launch_on_headless_desktop", name=DESKTOP, command=f'cmd /c "{wrapper}"')
    if not r.get("ok"):
        die(f"launch failed: {r}")
    log = wait_gl_init(t0)
    time.sleep(3)  # let the frame finish first paint
    print(json.dumps({"ok": True, "log": str(log), "windows": list_windows()}, indent=1))


def cmd_open(args):
    model = Path(args.model).resolve()
    if not model.exists():
        die(f"model not found: {model}")
    if not app_windows():
        die("app not running — run `driver.py launch` first (a cold launch "
            "with a model on the command line crashes at GL init)")
    before = {w["handle"] for w in list_windows()}
    t0 = time.time()
    wrapper = write_wrapper(model)
    cheap("launch_on_headless_desktop", name=DESKTOP, command=f'cmd /c "{wrapper}"')

    # A second instance does not always write its GL-ready line to the log selected
    # by wait_gl_init(). The document frame is the authoritative readiness signal.
    deadline = time.time() + 240
    log = None
    new = []
    while time.time() < deadline:
        current = list_windows()
        new = [w for w in current if w["handle"] not in before]
        if any(w.get("class") == "wxWindowNR" and
               "BambuStudio" in (w.get("title") or "") and
               w.get("width", 0) > 800 for w in new):
            break
        for p in LOGDIR.glob("studio_*.log*"):
            if p.stat().st_mtime >= t0 - 1:
                try:
                    if GL_READY in p.read_text(errors="replace"):
                        log = p
                        break
                except OSError:
                    pass
        time.sleep(2)
    else:
        die(f"no document frame or studio log contained '{GL_READY}' within 240s "
            f"| windows now: {list_windows()}")

    time.sleep(5)  # model load + first paint
    current = list_windows()
    new = [w for w in current if w["handle"] not in before]
    print(json.dumps({"ok": True, "log": str(log) if log else None,
                      "readiness": "document_frame" if log is None else "studio_log",
                      "new_windows": new, "all_windows": current}, indent=1))


def cmd_windows(_):
    print(json.dumps({"ok": True, "windows": list_windows()}, indent=1))


def cmd_ss(args):
    out = Path(args.out).resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = STATE / f"cap-{int(time.time()*1000)}.png"
    STATE.mkdir(parents=True, exist_ok=True)
    r = relay_tool("screenshot", hwnd=args.hwnd, output_path=str(tmp), timeout=45)
    if not r.get("ok"):
        die(f"screenshot: {r}")
    shutil.move(str(tmp), str(out))
    r["path"] = str(out)
    print(json.dumps(r, indent=1))


def cmd_click(args):
    kw = dict(hwnd=args.hwnd, x=args.x, y=args.y, button=args.button)
    if args.double:
        kw["double"] = "true"
    print(json.dumps(relay_tool("mouse_click", **kw), indent=1))


def cmd_ahkclick(args):
    """AHK v2 ControlClick, run ON the headless desktop (see run_ahk_on_desktop
    for the AHK gotchas this route works around). Param order matters:
    4 = button name, 5 = click count (a number), 6 = options."""
    txt = run_ahk_on_desktop(
        f'ControlClick "x{args.x} y{args.y}", "ahk_id {args.hwnd}",, "Left", 1, "NA"')
    print(json.dumps({"ok": txt == "done", "raw": txt}, indent=1))


def run_ahk_on_desktop(body, timeout=20):
    """Run an AHK v2 snippet ON the headless desktop. The snippet may use the
    variable `res` (result file path) and MUST NOT ExitApp itself — a
    FileAppend of the outcome plus ExitApp is appended automatically."""
    status = cheap("ahk_status")
    ahk = status.get("path")
    if not status.get("installed") or not ahk:
        die("AutoHotkey v2 is unavailable; ahk/ahkclick require it. "
            "Use press.py or another same-desktop Lowlevel MCP route instead.")
    STATE.mkdir(parents=True, exist_ok=True)
    res = STATE / f"ahk-res-{int(time.time()*1000)}.txt"
    script = STATE / "snippet.ahk"
    script.write_text(
        f'res := "{res}"\n'
        'try {\n'
        + "\n".join("    " + ln for ln in body.splitlines()) + "\n"
        '    FileAppend "done", res\n'
        '} catch as e {\n'
        '    FileAppend "ERR: " e.Message, res\n'
        '}\n'
        'ExitApp\n')
    r = cheap("launch_on_headless_desktop", name=DESKTOP, command=f'"{ahk}" "{script}"')
    if not r.get("ok"):
        die(f"ahk launch failed: {r}")
    txt = ""
    t0 = time.time()
    while time.time() - t0 < timeout:
        if res.exists() and res.stat().st_size > 0:
            time.sleep(0.3)
            txt = res.read_text(errors="replace")
            break
        time.sleep(0.5)
    res.unlink(missing_ok=True)
    cheap("kill_process", name="AutoHotkey64.exe", force="true")
    return txt.strip() or "no result (AHK hung?)"


def cmd_ahk(args):
    print(json.dumps({"ok": True, "raw": run_ahk_on_desktop(args.code)}, indent=1))


def cmd_type(args):
    print(json.dumps(relay_tool("type_text", hwnd=args.hwnd, text=args.text), indent=1))


def cmd_children(args):
    print(json.dumps(relay_tool("list_child_windows", hwnd=args.hwnd, timeout=45), indent=1))


def cmd_tool(args):
    kw = {}
    it = iter(args.rest)
    for k in it:
        if not k.startswith("--"):
            die(f"expected --key value pairs, got {k}")
        kw[k[2:]] = next(it, "")
    print(json.dumps(relay_tool(args.name, **kw), indent=1))


def cmd_log(args):
    p = newest_log()
    if not p:
        die(f"no studio logs in {LOGDIR}")
    lines = p.read_text(errors="replace").splitlines()
    print(f"=== {p} (last {args.lines} of {len(lines)} lines)")
    print("\n".join(lines[-args.lines:]))


def cmd_stop(_):
    r1 = cheap("kill_process", name="bambu-studio.exe", force="true")
    r2 = cheap("close_headless_desktop", name=DESKTOP)
    print(json.dumps({"ok": True, "kill": r1, "close": r2}, indent=1))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("launch"); s.add_argument("--fresh", action="store_true")
    s = sub.add_parser("open"); s.add_argument("--model", required=True)
    sub.add_parser("windows")
    s = sub.add_parser("ss"); s.add_argument("--hwnd", type=int, required=True); s.add_argument("--out", required=True)
    s = sub.add_parser("click")
    s.add_argument("--hwnd", type=int, required=True); s.add_argument("--x", type=int, required=True)
    s.add_argument("--y", type=int, required=True); s.add_argument("--button", default="left")
    s.add_argument("--double", action="store_true")
    s = sub.add_parser("ahkclick")
    s.add_argument("--hwnd", type=int, required=True); s.add_argument("--x", type=int, required=True)
    s.add_argument("--y", type=int, required=True)
    s = sub.add_parser("ahk"); s.add_argument("--code", required=True)
    s = sub.add_parser("type"); s.add_argument("--hwnd", type=int, required=True); s.add_argument("--text", required=True)
    s = sub.add_parser("children"); s.add_argument("--hwnd", type=int, required=True)
    s = sub.add_parser("tool"); s.add_argument("name"); s.add_argument("rest", nargs="*")
    s = sub.add_parser("log"); s.add_argument("--lines", type=int, default=40)
    sub.add_parser("stop")
    args = p.parse_args()
    if not CHEAP.exists():
        die(f"cheap CLI not found at {CHEAP} — set LLCU_VENV to the "
            "lowlevel-computer-use-mcp .venv")
    globals()[f"cmd_{args.cmd}"](args)


if __name__ == "__main__":
    main()
