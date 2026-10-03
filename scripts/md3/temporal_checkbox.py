"""One installed checkbox observation, contained by the caller's 120-second Job."""
from __future__ import annotations

import asyncio
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time

import motion_temporal as temporal


def require(value):
    if not value:
        raise RuntimeError("Temporal checkbox observation unavailable")


def checkbox_valid(row, top, selected):
    return (isinstance(row, dict) and type(row.get("hwnd")) is int and row["hwnd"] > 0
            and row.get("top") == top and row.get("name") == "Case sensitive"
            and row.get("enabled") is True and row.get("offscreen") is False
            and row.get("focused") is True and type(row.get("toggle")) is int
            and row["toggle"] == int(selected)
            and isinstance(row.get("rect"), list) and len(row["rect"]) == 4
            and all(type(v) is int for v in row["rect"])
            and row["rect"][0] < row["rect"][2] and row["rect"][1] < row["rect"][3])


def transition_valid(before, after, top, original, remaining, no_match):
    return (checkbox_valid(before, top, False) and checkbox_valid(after, top, True)
            and before["hwnd"] == after["hwnd"] and before["rect"] == after["rect"]
            and original == ["Add Primitive"] and remaining == [] and no_match is True)


def source_manifest(source):
    from startup_diagnostics import VERIFIER_FILES
    root = Path(__file__).resolve().parents[2]
    names = set(VERIFIER_FILES) | {"scripts/md3/temporal_checkbox.py", "scripts/md3/motion_temporal.py",
        "scripts/md3/minimum_resize.py", "scripts/ci/HostedScaleProcess.cs",
        ".github/workflows/native-interface-runtime.yml"}
    files = {}
    for name in sorted(names):
        raw = temporal.plain(root / name).read_bytes().replace(b"\r\n", b"\n")
        committed = subprocess.run(["git", "show", source + ":" + name], cwd=root,
            capture_output=True, timeout=5, creationflags=subprocess.CREATE_NO_WINDOW)
        require(committed.returncode == 0 and raw == committed.stdout.replace(b"\r\n", b"\n"))
        files[name] = hashlib.sha256(raw).hexdigest()
    return {"source_commit": source, "hash_format": "sha256-lf-v1", "files": files}


def fixed_focus(binding, rectangle):
    """Read native ownership/focus/geometry only, without UIA inside the burst."""
    user = ctypes.WinDLL("user32", use_last_error=True)
    class Info(ctypes.Structure):
        _fields_ = [("size", wintypes.DWORD), ("flags", wintypes.DWORD),
            ("active", wintypes.HWND), ("focus", wintypes.HWND), ("capture", wintypes.HWND),
            ("menu", wintypes.HWND), ("move", wintypes.HWND), ("caret", wintypes.HWND), ("rect", wintypes.RECT)]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetGUIThreadInfo.argtypes = [wintypes.DWORD, ctypes.POINTER(Info)]
    user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.IsWindowEnabled.argtypes = [wintypes.HWND]
    target = binding["click"]["target_hwnd"]
    pid, rect, info = wintypes.DWORD(), wintypes.RECT(), Info()
    info.size = ctypes.sizeof(info)
    thread = user.GetWindowThreadProcessId(target, ctypes.byref(pid))
    require(pid.value == binding["pid"] and user.GetGUIThreadInfo(thread, ctypes.byref(info))
            and info.focus == target and user.IsWindowEnabled(target)
            and user.GetWindowRect(target, ctypes.byref(rect))
            and [rect.left, rect.top, rect.right, rect.bottom] == rectangle)


async def burst(request):
    require(set(request) == {"binding", "rectangle", "output", "exe", "package", "tools"})
    binding = request["binding"]
    temporal.validate_binding(binding)
    user = ctypes.WinDLL("user32", use_last_error=True)
    user.SetProcessDpiAwarenessContext.argtypes = [wintypes.HANDLE]
    user.SetProcessDpiAwarenessContext(wintypes.HANDLE(-4))
    original = temporal.observe
    def observe(*args, **kwargs):
        result = original(*args, **kwargs)
        fixed_focus(binding, request["rectangle"])
        return result
    temporal.observe = observe
    try:
        return await temporal.capture_transition(binding, output_root=Path(request["output"]),
            executable=Path(request["exe"]), package=Path(request["package"]), tool_root=Path(request["tools"]))
    finally:
        temporal.observe = original


def run(drive):
    import psutil
    from recapture import cheap
    from minimum_resize import job_member
    args, app = drive.args, drive.app
    require((args.language, args.theme, args.scale, args.viewport) == ("en", "light", 1.0, "1200x800"))
    job_member(args.temporal_job_name, os.getpid())
    job_member(args.temporal_job_name, app.pid)
    drive.temporal = {"status": "unverified", "disposal_required": True,
        "binding": source_manifest(args.source_commit)}
    install = json.loads(args.install_receipt.read_text(encoding="utf-8-sig"))
    require(install.get("asset_sha256", {}).get(args.temporal_package.name) == temporal.digest(args.temporal_package))
    drive.click("prepare", drive.one("Prepare"))
    canvases = [r for r in drive.probe if r.get("on_screen") and "GLCanvas" in r.get("class", "")]
    require(bool(canvases))
    canvas = max(canvases, key=lambda r: r["screen"]["w"] * r["screen"]["h"])["screen"]
    drive.record("open-context", "click", point=[canvas["x"] + canvas["w"] // 2,
        canvas["y"] + canvas["h"] // 2], button="right")
    root = drive.one("Search menu", kind=50004)["top"]
    drive.click("search-focus", drive.one("Search menu", kind=50004, top=root))
    drive.type("literal-query", "add primitive", root)
    original = [r["name"] for r in drive.menu_items(root)]
    require(original == ["Add Primitive"])
    drive.click("builder-open", drive.one("Regex builder", top=root))
    pattern = drive.one("Regex pattern", kind=50004)
    top = pattern["top"]
    regex = [r for r in drive.candidates("Regex mode", top=top) if type(r.get("toggle")) is int]
    require(len(regex) == 1 and regex[0]["toggle"] == 0)
    # Two forward traversal steps normally pass Copy pattern and Regex mode.
    # Bound the actual traversal, retaining every key and its observed result.
    before = None
    for attempt in range(5):
        candidates = [r for r in drive.candidates("Case sensitive", top=top) if r.get("hwnd", 0) > 0
                      and type(r.get("toggle")) is int]
        require(len(candidates) == 1)
        if checkbox_valid(candidates[0], top, False):
            before = candidates[0]
            break
        require(attempt < 4)
        drive.key("focus-case-" + str(attempt + 1), ["tab"], top)
    require(before is not None and drive.one("Regex pattern", kind=50004, top=top).get("value") == "add primitive")
    user = ctypes.WinDLL("user32", use_last_error=True)
    user.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    origin, client = wintypes.POINT(), wintypes.RECT()
    require(user.ClientToScreen(top, ctypes.byref(origin)) and user.GetClientRect(top, ctypes.byref(client)))
    left, upper, right, bottom = before["rect"]
    roi = [left - origin.x, upper - origin.y, right - origin.x, bottom - origin.y]
    binding = {"schema": 1, "run_id": os.environ["GITHUB_RUN_ID"], "run_attempt": os.environ["GITHUB_RUN_ATTEMPT"],
        "source_commit": args.source_commit, "verifier_commit": args.source_commit,
        "exe_sha256": temporal.digest(args.exe), "package_sha256": temporal.digest(args.temporal_package),
        "pid": app.pid, "process_started": psutil.Process(app.pid).create_time(), "hwnd": top,
        "desktop": app.desktop, "job_name": args.temporal_job_name, "language": "en", "theme": "light",
        "dpi": 96, "client_size": [client.right, client.bottom], "roi": roi, "nonce": secrets.token_hex(16),
        "click": {"x": (roi[0] + roi[2]) // 2, "y": (roi[1] + roi[3]) // 2,
                  "button": "left", "target_hwnd": before["hwnd"]}}
    temporal.validate_binding(binding)
    request_path, output = drive.scratch / "temporal-request.json", drive.scratch / "temporal-burst"
    request = {"binding": binding, "rectangle": before["rect"], "output": str(output),
               "exe": str(args.exe), "package": str(args.temporal_package), "tools": str(args.temporal_tools)}
    request_path.write_text(json.dumps(request), encoding="utf-8")
    command = subprocess.list2cmdline([sys.executable, str(Path(__file__).resolve()), "--burst", str(request_path)])
    launched = cheap("launch_on_headless_desktop", name=app.desktop, command=command)
    worker = psutil.Process(launched["pid"])
    require(worker.cmdline()[1:] == [str(Path(__file__).resolve()), "--burst", str(request_path)])
    job_member(args.temporal_job_name, worker.pid)
    # Dedicated wait, never the unrelated 22/25-second native-worker path.
    worker.wait(timeout=35)
    receipt_path = temporal.plain(output / "temporal.json")
    require(receipt_path.stat().st_size <= 262144)
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    finished = json.loads((output / "temporal-finished.json").read_text(encoding="utf-8"))
    require(receipt["binding"] == binding and receipt.get("server_exit_verified") is True
            and receipt.get("disposal_required") is False and (output / "temporal-finished.json").is_file()
            and not (output / "temporal-pending.json").exists()
            and finished == {"nonce": binding["nonce"], "verifier_commit": binding["verifier_commit"]}
            and len(receipt["frames"]) == 4 and [r["index"] for r in receipt["frames"]] == [0, 1, 2, 3])
    drive.temporal.update({"receipt": receipt, "disposal_required": False, "status": receipt["status"]})
    retained = []
    for frame in receipt["frames"]:
        source = temporal.plain(output / frame["path"])
        require(source.parent == output and source.name == f"frame-{frame['index']}.png"
                and temporal.digest(source) == frame["sha256"] and len(drive.images) < 30)
        name = f"{len(drive.images):03d}-temporal-{frame['index']}.png"
        shutil.copyfile(source, args.output / name)
        retained.append(name)
        drive.images.append({"file": name, "sha256": frame["sha256"], "bytes": frame["bytes"],
            "observation_interval": frame["interval"], "dimensions": binding["client_size"],
            "capture_method": receipt["capture_semantics"]})
    drive.rows.append({"operation": "temporal-case-toggle", "input": "click", "status": receipt["status"],
        "captures": retained, "observation_bounds_only": True, "native_layered_alpha_verified": False})
    drive.worker()
    after = [r for r in drive.candidates("Case sensitive", top=top) if r.get("hwnd") == before["hwnd"]]
    require(len(after) == 1 and transition_valid(before, after[0], top, original,
        [r["name"] for r in drive.menu_items(root)], bool(drive.candidates("No matches.", top=root)))
        and drive.one("Regex pattern", kind=50004, top=top).get("value") == "add primitive")
    drive.temporal["semantic_transition_verified"] = True
    drive.key("builder-dismiss", ["esc"], top)
    drive.key("query-clear", ["esc"], root)
    drive.key("menu-dismiss", ["esc"], root)
    require(receipt["status"] == "observed" and len(receipt["frames"]) == 4
            and temporal.intermediate_indices(receipt["click_interval"], receipt["frames"]))


if __name__ == "__main__":
    require(len(sys.argv) == 3 and sys.argv[1] == "--burst")
    path = temporal.plain(sys.argv[2])
    require(path.stat().st_size <= 65536)
    # Exceptions stay in inherited private streams. The parent accepts only the
    # fixed receipt after this process exits and the containing Job is empty.
    try:
        result = asyncio.run(burst(json.loads(path.read_text(encoding="utf-8"))))
        raise SystemExit(0 if not result["disposal_required"] else 1)
    except Exception:
        raise SystemExit(1)
