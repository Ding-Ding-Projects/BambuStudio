#!/usr/bin/env python3
"""press.py — press BambuStudio's buttons and menu items BY NAME.

Why this exists: driver.py can click a pixel, but pixel coordinates drift
every build and this app's topbar menus are custom-drawn (AutoHotkey's
MenuSelect reports "unsupported menu", and ControlClick on the menu labels
does not open them). This tool resolves a human label to the thing Windows
actually needs — a wx command id for menu items, a child hwnd for controls —
so callers say what they mean:

    press.py menus                       # list every topbar menu item + id
    press.py press "Version history"     # opens File > Version history...
    press.py controls                    # list clickable child controls
    press.py press "Slice plate"         # presses the button by its label

Everything runs THROUGH LOWLEVEL MCP ON THE HEADLESS DESKTOP. The outer CLI
never touches the app directly: it relays a worker copy of itself onto the
off-screen desktop with launch_on_headless_desktop, because hwnd-addressed
Win32 calls fail cross-desktop (IsWindow returns false from the default
desktop), and because any *separate* process spawned while a menu is open
kills that menu. One process does click + hook + read.

Menu ids are wxID_ANY allocations: they are stable within a build but shift
whenever menu construction changes, so they are always enumerated live and
never hardcoded.
"""

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

LLCU_VENV = Path(os.environ.get(
    "LLCU_VENV", r"C:\Users\Administrator\Documents\GitHub\lowlevel-computer-use-mcp\.venv"))
CHEAP = LLCU_VENV / "Scripts" / "lowlevel-computer-use-cheap.exe"
VENV_PY = LLCU_VENV / "Scripts" / "python.exe"
DESKTOP = os.environ.get("BS_DESKTOP", "bsrun")
STATE = Path(os.environ.get("TEMP", r"C:\Windows\Temp")) / "bs-run-driver"
CACHE = STATE / "press-menus.json"


def die(msg):
    print(json.dumps({"ok": False, "error": msg}))
    sys.exit(1)


def cheap(tool, **kw):
    argv = [str(CHEAP), tool]
    for k, v in kw.items():
        argv += [f"--{k}", str(v)]
    out = subprocess.run(argv, capture_output=True, text=True, timeout=120)
    try:
        return json.loads(out.stdout)
    except json.JSONDecodeError:
        die(f"{tool}: non-JSON output: {out.stdout[:300]} {out.stderr[:300]}")


def app_frame():
    """hwnd of the main frame, found live on the headless desktop."""
    r = cheap("list_headless_windows", name=DESKTOP)
    if not r.get("ok"):
        die(f"no headless desktop '{DESKTOP}' — run driver.py launch first ({r.get('error')})")
    frames = [w for w in r.get("windows", [])
              if w.get("class") == "wxWindowNR"
              and "BambuStudio" in (w.get("title") or "")
              and w.get("width", 0) > 800]
    if not frames:
        die("no BambuStudio main frame on the headless desktop — run driver.py launch")
    # Prefer a frame holding a document ("* Untitled - BambuStudio") when present.
    frames.sort(key=lambda w: (not (w.get("title") or "").startswith("*"), -w["width"]))
    return frames[0]["handle"]


def run_worker(mode, frame, arg=None, timeout=90):
    """Run this file's worker half ON the headless desktop and read its JSON."""
    STATE.mkdir(parents=True, exist_ok=True)
    res = STATE / f"press-{int(time.time()*1000)}.json"
    cmd = f'"{VENV_PY}" "{Path(__file__).resolve()}" --worker {mode} {frame} "{res}"'
    if arg is not None:
        cmd += f' "{arg}"'
    r = cheap("launch_on_headless_desktop", name=DESKTOP, command=cmd)
    if not r.get("ok"):
        die(f"could not launch the on-desktop worker: {r}")
    t0 = time.time()
    while time.time() - t0 < timeout:
        if res.exists() and res.stat().st_size > 0:
            time.sleep(0.3)
            try:
                data = json.loads(res.read_text(encoding="utf-8", errors="replace"))
            except json.JSONDecodeError as exc:
                die(f"worker wrote invalid JSON: {exc}")
            res.unlink(missing_ok=True)
            return data
        time.sleep(0.4)
    die(f"worker '{mode}' timed out after {timeout}s")


# --------------------------------------------------------------------------
# Worker half: runs ON the headless desktop, talks Win32 directly.
# --------------------------------------------------------------------------

def worker(mode, frame, out_path, arg=None):
    import ctypes
    import ctypes.wintypes as wt

    u = ctypes.windll.user32
    u.SendMessageTimeoutW.restype = ctypes.c_ssize_t
    msg = wt.MSG()
    log = []

    def pump(seconds):
        end = time.time() + seconds
        while time.time() < end:
            while u.PeekMessageW(ctypes.byref(msg), 0, 0, 0, 1):
                u.TranslateMessage(ctypes.byref(msg))
                u.DispatchMessageW(ctypes.byref(msg))
            time.sleep(0.003)

    def child_windows(parent):
        # Collect handles first, then read each window's properties OUTSIDE the
        # enumeration callback: ctypes swallows exceptions raised inside a
        # callback (they only reach stderr, which nobody sees on an off-screen
        # desktop), so a single bad read silently yields an empty list.
        handles = []

        def cb(hwnd, _):
            handles.append(hwnd)
            return True

        u.EnumChildWindows(parent, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb), 0)

        found = []
        for hwnd in handles:
            try:
                cls = ctypes.create_unicode_buffer(128)
                u.GetClassNameW(hwnd, cls, 128)
                txt = ctypes.create_unicode_buffer(256)
                u.GetWindowTextW(hwnd, txt, 256)
                rc = wt.RECT()
                u.GetWindowRect(hwnd, ctypes.byref(rc))
                pt = wt.POINT(rc.left, rc.top)
                u.ScreenToClient(parent, ctypes.byref(pt))
                found.append({
                    "handle": int(hwnd), "class": cls.value, "text": txt.value,
                    "x": pt.x, "y": pt.y,
                    "w": rc.right - rc.left, "h": rc.bottom - rc.top,
                    "visible": bool(u.IsWindowVisible(hwnd)),
                    "enabled": bool(u.IsWindowEnabled(hwnd)),
                })
            except Exception as exc:  # noqa: BLE001 - one bad handle must not blind the sweep
                log.append(f"child {hwnd}: {exc}")
        return found

    def topbar(parent):
        """The custom-drawn menu strip: widest visible child hugging the top."""
        cands = [c for c in child_windows(parent)
                 if c["visible"] and c["y"] <= 6 and c["h"] <= 60 and c["w"] > 400]
        if not cands:
            return None
        cands.sort(key=lambda c: -c["w"])
        return cands[0]

    def post_click(hwnd, x, y, move_cursor=True):
        """Click at CLIENT coords (x, y) of hwnd.

        The cursor is parked on the target first. Owner-drawn controls (this
        app's menu strip among them) hit-test with GetCursorPos rather than the
        message's lParam, and PostMessage does not move the real pointer — so
        without this the strip receives the click and decides nothing was hit.
        Harmless for controls that do read lParam.
        """
        if move_cursor:
            # Best-effort only: SetCursorPos is unreliable on an off-screen
            # desktop, so nothing may depend on it succeeding.
            pt = wt.POINT(x, y)
            u.ClientToScreen(hwnd, ctypes.byref(pt))
            u.SetCursorPos(pt.x, pt.y)
            pump(0.05)
        lp = ((y & 0xFFFF) << 16) | (x & 0xFFFF)
        u.PostMessageW(hwnd, 0x0200, 0, lp)   # WM_MOUSEMOVE
        u.PostMessageW(hwnd, 0x0201, 1, lp)   # WM_LBUTTONDOWN
        u.PostMessageW(hwnd, 0x0202, 0, lp)   # WM_LBUTTONUP

    def enum_menu(hmenu, depth=0):
        out = []
        count = u.GetMenuItemCount(hmenu)
        buf = ctypes.create_unicode_buffer(512)
        for i in range(count):
            got = u.GetMenuStringW(hmenu, i, buf, 512, 0x400)  # MF_BYPOSITION
            item = {
                "pos": i,
                "text": buf.value if got else "",
                "id": u.GetMenuItemID(hmenu, i),
            }
            sub = u.GetSubMenu(hmenu, i)
            if sub and depth < 3:
                item["sub"] = enum_menu(sub, depth + 1)
            out.append(item)
        return out

    # --- menu discovery -------------------------------------------------
    # The menu strip is owner-drawn, so the only way to learn its command ids
    # is to open each menu for real, catch EVENT_SYSTEM_MENUPOPUPSTART, ask the
    # popup for its HMENU (MN_GETHMENU), then close it again.
    def discover_menus():
        bar = topbar(frame)
        if bar is None:
            return {"ok": False, "error": "no topbar child found under the frame"}

        popup = {"hwnd": None}
        WinEventProc = ctypes.WINFUNCTYPE(None, wt.HANDLE, wt.DWORD, wt.HWND,
                                          wt.LONG, wt.LONG, wt.DWORD, wt.DWORD)

        def on_event(hook, event, hwnd, objid, childid, tid, when):
            if event == 0x0006 and hwnd:  # EVENT_SYSTEM_MENUPOPUPSTART
                popup["hwnd"] = hwnd

        proc = WinEventProc(on_event)
        hook = u.SetWinEventHook(0x0006, 0x0007, 0, proc, 0, 0, 0)

        # The frame is parked at this exact off-screen offset for the duration
        # of the sweep. Empirically the owner-drawn strip only opens a menu
        # when the frame sits here; clicking it at its normal position posts
        # the messages and opens nothing. Discovered by the earlier menucap
        # experiment and kept verbatim because it is the difference between
        # "menus enumerate" and "menus never open". The frame is restored below.
        orig = wt.RECT()
        u.GetWindowRect(frame, ctypes.byref(orig))
        SWP_NOSIZE_NOZORDER_NOACTIVATE = 0x0001 | 0x0004 | 0x0010
        u.SetWindowPos(frame, 0, -183, -6, 0, 0, SWP_NOSIZE_NOZORDER_NOACTIVATE)
        pump(0.3)

        menus, x = {}, 8
        # Sweep the strip instead of assuming label positions: tool x drifts a
        # few pixels per build, and the label set can change.
        while x < min(bar["w"], 520):
            popup["hwnd"] = None
            post_click(bar["handle"], x, max(4, bar["h"] // 2))
            pump(0.85)
            ph = popup["hwnd"]
            if ph and u.IsWindow(ph):
                res = ctypes.c_size_t()
                ok = u.SendMessageTimeoutW(ph, 0x01E1, 0, 0, 0x0002 | 0x0008, 300,
                                           ctypes.byref(res))  # MN_GETHMENU
                hmenu = res.value if ok else 0
                if hmenu and u.IsMenu(hmenu):
                    items = enum_menu(hmenu)
                    key = f"menu@{x}"
                    # Name the menu after its first real item so callers get a
                    # stable-ish label even though the strip is owner-drawn.
                    first = next((i["text"] for i in items if i["text"]), "")
                    if items and not any(m["items"] == items for m in menus.values()):
                        menus[key] = {"x": x, "first_item": first, "items": items}
                        log.append(f"x={x}: {len(items)} items, first={first!r}")
                # close it again
                u.PostMessageW(ph, 0x0100, 0x1B, 0)  # WM_KEYDOWN VK_ESCAPE
                pump(0.25)
            u.PostMessageW(bar["handle"], 0x0100, 0x1B, 0)
            u.PostMessageW(frame, 0x001F, 0, 0)      # WM_CANCELMODE
            pump(0.2)
            x += 12
        u.UnhookWinEvent(hook)
        u.SetWindowPos(frame, 0, orig.left, orig.top, 0, 0, SWP_NOSIZE_NOZORDER_NOACTIVATE)
        pump(0.2)
        return {"ok": True, "topbar": bar["handle"], "menus": menus, "log": log}

    if mode == "menus":
        result = discover_menus()
    elif mode == "controls":
        kids = child_windows(frame)
        result = {"ok": True, "frame": frame,
                  "controls": [c for c in kids if c["visible"] and c["w"] > 8 and c["h"] > 8]}
    elif mode == "probe":
        txt = ctypes.create_unicode_buffer(256)
        u.GetWindowTextW(frame, txt, 256)
        raw = []

        def cb2(hwnd, _):
            raw.append(hwnd)
            return True

        rc = u.EnumChildWindows(frame, ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(cb2), 0)
        err = ctypes.get_last_error() if hasattr(ctypes, "get_last_error") else -1
        result = {"ok": True, "frame": frame, "is_window": bool(u.IsWindow(frame)),
                  "title": txt.value, "enumchild_ret": rc, "raw_children": len(raw),
                  "last_error": err, "desktop_pid": os.getpid()}
    elif mode == "command":
        # WM_COMMAND straight to the frame: this is what a menu selection
        # actually sends, and it needs no coordinates at all.
        u.PostMessageW(frame, 0x0111, int(arg), 0)
        pump(0.6)
        result = {"ok": True, "posted_command": int(arg)}
    elif mode == "clickchild":
        hwnd_s, x_s, y_s = arg.split(",")
        post_click(int(hwnd_s), int(x_s), int(y_s))
        pump(0.4)
        result = {"ok": True, "clicked": int(hwnd_s), "x": int(x_s), "y": int(y_s)}
    else:
        result = {"ok": False, "error": f"unknown worker mode {mode!r}"}

    Path(out_path).write_text(json.dumps(result, ensure_ascii=False), encoding="utf-8")


# --------------------------------------------------------------------------
# Outer CLI
# --------------------------------------------------------------------------

def flatten(menus):
    """[(label, id, menu_x)] for every enabled, non-separator item."""
    out = []

    def walk(items, x, prefix=""):
        for it in items:
            label = (it.get("text") or "").split("\t")[0].replace("&", "").strip()
            if it.get("sub"):
                walk(it["sub"], x, prefix + label + " > ")
                continue
            if not label or it.get("id", 0) in (0, -1, 65534):
                continue
            out.append({"label": prefix + label, "id": it["id"], "menu_x": x})

    for m in menus.values():
        walk(m["items"], m["x"])
    return out


def load_menus(frame, refresh=False):
    if CACHE.exists() and not refresh:
        try:
            data = json.loads(CACHE.read_text(encoding="utf-8"))
            if data.get("frame") == frame and data.get("menus"):
                return data
        except (json.JSONDecodeError, OSError):
            pass
    res = run_worker("menus", frame, timeout=180)
    if not res.get("ok"):
        die(f"menu discovery failed: {res.get('error')}")
    res["frame"] = frame
    STATE.mkdir(parents=True, exist_ok=True)
    CACHE.write_text(json.dumps(res, ensure_ascii=False, indent=1), encoding="utf-8")
    return res


def cmd_menus(args):
    frame = args.frame or app_frame()
    data = load_menus(frame, refresh=args.refresh)
    items = flatten(data["menus"])
    print(json.dumps({"ok": True, "frame": frame, "count": len(items),
                      "items": items}, ensure_ascii=False, indent=1))


def cmd_controls(args):
    frame = args.frame or app_frame()
    res = run_worker("controls", frame)
    ctrls = res.get("controls", [])
    if args.filter:
        needle = args.filter.lower()
        ctrls = [c for c in ctrls if needle in (c["text"] or "").lower()
                 or needle in c["class"].lower()]
    labelled = [c for c in ctrls if (c["text"] or "").strip()
                and c["text"] not in ("panel", "control")]
    print(json.dumps({"ok": True, "frame": frame,
                      "labelled": labelled if not args.all else ctrls,
                      "total": len(ctrls)}, ensure_ascii=False, indent=1))


def cmd_press(args):
    frame = args.frame or app_frame()
    want = args.label.lower().strip()

    data = load_menus(frame, refresh=args.refresh)
    items = flatten(data["menus"])
    exact = [i for i in items if i["label"].lower() == want]
    partial = [i for i in items if want in i["label"].lower()]
    hit = (exact or partial)
    if hit:
        if len(hit) > 1 and not exact:
            print(json.dumps({"ok": False, "error": "ambiguous label",
                              "candidates": [h["label"] for h in hit[:8]]},
                             ensure_ascii=False, indent=1))
            sys.exit(2)
        target = hit[0]
        res = run_worker("command", frame, target["id"])
        print(json.dumps({"ok": res.get("ok", False), "pressed": target["label"],
                          "via": "WM_COMMAND", "id": target["id"]},
                         ensure_ascii=False, indent=1))
        return

    # Not a menu item: fall back to a labelled child control.
    ctrl_res = run_worker("controls", frame)
    ctrls = [c for c in ctrl_res.get("controls", [])
             if want in (c["text"] or "").lower() and c["enabled"]]
    if not ctrls:
        die(f"no menu item or control matching {args.label!r} "
            f"(try: press.py menus / press.py controls)")
    ctrls.sort(key=lambda c: len(c["text"]))
    c = ctrls[0]
    res = run_worker("clickchild", frame, f"{c['handle']},{c['w']//2},{c['h']//2}")
    print(json.dumps({"ok": res.get("ok", False), "pressed": c["text"],
                      "via": "child click", "handle": c["handle"]},
                     ensure_ascii=False, indent=1))


def cmd_id(args):
    frame = args.frame or app_frame()
    res = run_worker("command", frame, args.command_id)
    print(json.dumps(res, indent=1))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--worker":
        # --worker <mode> <frame> <outpath> [arg]
        worker(sys.argv[2], int(sys.argv[3]), sys.argv[4],
               sys.argv[5] if len(sys.argv) > 5 else None)
        return

    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--frame", type=int, help="main-frame hwnd (default: find it)")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("menus", help="list every topbar menu item and its command id")
    s.add_argument("--refresh", action="store_true", help="re-discover instead of using cache")
    s = sub.add_parser("controls", help="list clickable child controls")
    s.add_argument("--filter", help="substring match on text or class")
    s.add_argument("--all", action="store_true", help="include unlabelled containers")
    s = sub.add_parser("press", help="press a menu item or control BY LABEL")
    s.add_argument("label")
    s.add_argument("--refresh", action="store_true")
    s = sub.add_parser("id", help="post a raw WM_COMMAND id to the frame")
    s.add_argument("command_id", type=int)

    args = p.parse_args()
    if not CHEAP.exists():
        die(f"lowlevel cheap CLI not found at {CHEAP} (set LLCU_VENV)")
    globals()[f"cmd_{args.cmd}"](args)


if __name__ == "__main__":
    main()
