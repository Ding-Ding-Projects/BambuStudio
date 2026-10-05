"""Restricted four-frame observation through two prepared, supported MCP sessions.

Not a standalone driver. The caller owns the product, desktop, named Job and
encryption. Request intervals bound observation, never pixel acquisition time.
No stdout, screenshots of other windows, desktop switching or UIA traversal.
"""
from __future__ import annotations

import asyncio
from contextlib import AsyncExitStack, asynccontextmanager
from datetime import datetime, timedelta, timezone
import hashlib
from importlib.metadata import distribution, version
import json
import math
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import time

TOOL_COMMIT = "e6e42f2066d539256d6480401d7cef867f2b8dfe"
OFFSETS_MS = (None, 25, 60, 180)
MAX_PNG = 16 * 1024 * 1024


def require(value):
    if not value:
        raise RuntimeError("Temporal observation is unavailable")


def plain(path):
    path = Path(path).absolute()
    for item in (path, *path.parents):
        try:
            attributes = item.lstat()
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(attributes.st_mode) and
                not (getattr(attributes, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT))
    return path.resolve()


def digest(path):
    with open(path, "rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def stamp():
    return {"monotonic_ns": time.monotonic_ns(),
            "utc": datetime.now(timezone.utc).isoformat(timespec="microseconds")}


def interval_valid(interval):
    if not isinstance(interval, dict) or set(interval) != {"start", "end"}:
        return False
    for name in ("start", "end"):
        point = interval[name]
        if not isinstance(point, dict) or set(point) != {"monotonic_ns", "utc"}:
            return False
        if type(point["monotonic_ns"]) is not int or point["monotonic_ns"] < 0:
            return False
        try:
            if not isinstance(point["utc"], str) or not point["utc"].endswith("+00:00"):
                return False
            datetime.fromisoformat(point["utc"])
        except ValueError:
            return False
    return interval["start"]["monotonic_ns"] <= interval["end"]["monotonic_ns"]


def intermediate_indices(click, frames):
    """A transport interval must fit entirely after acknowledgement and <100 ms.

    Pixel differences are necessary, not proof of animation or correct semantics.
    The UI can consume a posted click after the transport acknowledgement.
    """
    if not interval_valid(click) or not isinstance(frames, list) or len(frames) != 4:
        return []
    if any(not isinstance(row, dict) or not interval_valid(row.get("interval")) for row in frames):
        return []
    hashes = [row.get("roi_sha256") for row in frames]
    if any(not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value) for value in hashes):
        return []
    begin, acknowledged = click["start"]["monotonic_ns"], click["end"]["monotonic_ns"]
    if frames[0]["interval"]["end"]["monotonic_ns"] > begin or hashes[0] == hashes[-1]:
        return []
    if frames[-1]["interval"]["start"]["monotonic_ns"] < begin + 180_000_000:
        return []
    if any(frames[i]["interval"]["end"]["monotonic_ns"] >
           frames[i + 1]["interval"]["start"]["monotonic_ns"] for i in range(3)):
        return []
    return [i for i in (1, 2) if
            frames[i]["interval"]["start"]["monotonic_ns"] >= acknowledged and
            frames[i]["interval"]["end"]["monotonic_ns"] < begin + 100_000_000 and
            hashes[i] not in (hashes[0], hashes[-1])]


def validate_binding(binding):
    required = {"schema", "run_id", "run_attempt", "source_commit", "verifier_commit",
                "exe_sha256", "package_sha256", "pid", "process_started", "hwnd",
                "desktop", "job_name", "language", "theme", "dpi", "client_size", "roi",
                "click", "nonce"}
    require(isinstance(binding, dict) and set(binding) == required and
            type(binding["schema"]) is int and binding["schema"] == 1)
    for key in ("source_commit", "verifier_commit"):
        require(isinstance(binding[key], str) and re.fullmatch(r"[0-9a-f]{40}", binding[key]))
    for key in ("exe_sha256", "package_sha256"):
        require(isinstance(binding[key], str) and re.fullmatch(r"[0-9a-f]{64}", binding[key]))
    require(all(isinstance(binding[key], str) for key in ("nonce", "job_name", "desktop")))
    require(re.fullmatch(r"[0-9a-f]{32}", binding["nonce"]) and
            re.fullmatch(r"Local\\BambuNativeScale-[0-9a-f]{64}", binding["job_name"]))
    require(re.fullmatch(r"[A-Za-z0-9_-]{1,100}", binding["desktop"]) and
            binding["desktop"].lower() not in ("default", "winlogon", "disconnect"))
    for key in ("pid", "hwnd", "dpi"):
        require(type(binding[key]) is int and binding[key] > 0)
    require(binding["dpi"] in (96, 120, 144, 192))
    require(type(binding["process_started"]) in (int, float) and
            math.isfinite(binding["process_started"]) and binding["process_started"] > 0)
    require(binding["language"] in ("en", "yue_HK", "bilingual_en_yue_HK") and
            binding["theme"] in ("light", "dark"))
    size, roi, click = binding["client_size"], binding["roi"], binding["click"]
    require(type(size) is list and len(size) == 2 and all(type(v) is int and 1 <= v <= 8192 for v in size)
            and size[0] * size[1] <= 16_777_216)
    require(type(roi) is list and len(roi) == 4 and all(type(v) is int for v in roi) and
            0 <= roi[0] < roi[2] <= size[0] and 0 <= roi[1] < roi[3] <= size[1])
    require(type(click) is dict and set(click) == {"x", "y", "button", "target_hwnd"} and
            type(click["target_hwnd"]) is int and click["target_hwnd"] > 0 and
            type(click["x"]) is int and type(click["y"]) is int and
            0 <= click["x"] < size[0] and 0 <= click["y"] < size[1] and
            click["button"] in ("left", "right"))
    require(all(isinstance(binding[key], str) and re.fullmatch(r"[0-9]+", binding[key])
                for key in ("run_id", "run_attempt")))


def observe(binding, executable, *, check_input=False):
    """Read-only native identity/geometry, no capture or input implementation."""
    import ctypes
    from ctypes import wintypes
    import psutil
    from minimum_resize import desktop_api, desktop_name, job_member
    process = psutil.Process(binding["pid"])
    require(process.create_time() == binding["process_started"] and
            plain(process.exe()) == executable)
    job_member(binding["job_name"], process.pid)
    user = ctypes.WinDLL("user32", use_last_error=True)
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetWindowThreadProcessId.restype = wintypes.DWORD
    user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.GetDpiForWindow.argtypes = [wintypes.HWND]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.IsChild.argtypes = [wintypes.HWND, wintypes.HWND]
    user.ChildWindowFromPointEx.argtypes = [wintypes.HWND, wintypes.POINT, wintypes.UINT]
    user.ChildWindowFromPointEx.restype = wintypes.HWND
    user.MapWindowPoints.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.POINTER(wintypes.POINT), wintypes.UINT]
    user.GetThreadDpiAwarenessContext.restype = wintypes.HANDLE
    user.AreDpiAwarenessContextsEqual.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    owner = wintypes.DWORD()
    thread = user.GetWindowThreadProcessId(binding["hwnd"], ctypes.byref(owner))
    desktops = desktop_api()
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    require(desktop_name(desktops, desktops.GetThreadDesktop(kernel.GetCurrentThreadId())) == binding["desktop"] and
            user.AreDpiAwarenessContextsEqual(user.GetThreadDpiAwarenessContext(), wintypes.HANDLE(-4)))
    require(thread and owner.value == binding["pid"] and
            desktop_name(desktops, desktops.GetThreadDesktop(thread)) == binding["desktop"] and
            user.IsWindowVisible(binding["hwnd"]))
    client, outer = wintypes.RECT(), wintypes.RECT()
    require(user.GetClientRect(binding["hwnd"], ctypes.byref(client)) and
            user.GetWindowRect(binding["hwnd"], ctypes.byref(outer)))
    require([client.right - client.left, client.bottom - client.top] == binding["client_size"] and
            user.GetDpiForWindow(binding["hwnd"]) == binding["dpi"])
    if check_input:
        # The caller supplies the observed child from its real control preflight.
        # Ownership is checked again immediately before posting and at acknowledgement.
        target = binding["click"]["target_hwnd"]
        thread = user.GetWindowThreadProcessId(target, ctypes.byref(owner))
        require(thread and owner.value == binding["pid"] and
                (target == binding["hwnd"] or user.IsChild(binding["hwnd"], target)) and
                desktop_name(desktops, desktops.GetThreadDesktop(thread)) == binding["desktop"] and
                user.IsWindowVisible(target))
        # Match the pinned tool's read-only descendant selection before any input.
        current = binding["hwnd"]
        point = wintypes.POINT(binding["click"]["x"], binding["click"]["y"])
        for _ in range(32):
            child = user.ChildWindowFromPointEx(current, point, 1 | 4)
            if not child or child == current:
                break
            user.MapWindowPoints(current, child, ctypes.byref(point), 1)
            current = child
        require(current == target)
    return [outer.left, outer.top, outer.right, outer.bottom]


def pinned_tools(tool_root):
    """Verify the caller's existing bootstrap, never install or modify tools."""
    root = plain(tool_root)
    result = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=5,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    require(result.returncode == 0 and result.stdout.strip() == TOOL_COMMIT.encode("ascii"))
    installed = distribution("lowlevel-computer-use-mcp")
    hashes = {}
    for name in ("server.py", "winio.py", "process.py", "processes.py"):
        relative = "lowlevel_computer_use_mcp/" + name
        source, live = plain(root / "src" / relative), plain(installed.locate_file(relative))
        # Compare both trees to immutable blobs, not only to each other.
        result = subprocess.run(["git", "-C", str(root), "show", TOOL_COMMIT + ":src/" + relative],
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=5,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        require(result.returncode == 0 and len(result.stdout) <= 2_097_152)
        require(source.stat().st_size <= 2_097_152 and live.stat().st_size <= 2_097_152)
        normalize = lambda data: data.replace(b"\r\n", b"\n")
        require(normalize(source.read_bytes()) == normalize(result.stdout) == normalize(live.read_bytes()))
        hashes[relative] = hashlib.sha256(normalize(result.stdout)).hexdigest()
    return hashes


@asynccontextmanager
async def joined_input(operation):
    """Never close the input session while its local request task is unjoined.

    Cancellation is not evidence that posted input was undone. Any exceptional
    path remains uncertain; the enclosing named Job supplies the hard deadline.
    """
    task = asyncio.create_task(operation)
    try:
        yield task
        await task
    finally:
        if not task.done():
            task.cancel()
        await asyncio.wait_for(asyncio.gather(task, return_exceptions=True), 3)


def verifier_identity(expected):
    root = Path(__file__).resolve().parents[2]
    result = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=5,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    require(result.returncode == 0 and result.stdout.strip() == expected.encode("ascii"))
    relative = "scripts/md3/motion_temporal.py"
    result = subprocess.run(["git", "-C", str(root), "show", expected + ":" + relative],
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=5,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    require(result.returncode == 0 and len(result.stdout) <= 131072)
    normalize = lambda data: data.replace(b"\r\n", b"\n")
    live = plain(__file__)
    require(live.stat().st_size <= 131072 and normalize(live.read_bytes()) == normalize(result.stdout))
    return hashlib.sha256(normalize(result.stdout)).hexdigest()


async def capture_transition(binding, *, output_root, executable, package, tool_root):
    """Observe one already-authorized background click, four frames maximum.

    Must run inside the caller's named kill-on-close Job, with an outer deadline
    no greater than 120 seconds. Caller must prove whole-Job exit before encrypting
    or accepting evidence. This function never owns product/desktop teardown.
    """
    import psutil
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from minimum_resize import job_member
    from PIL import Image
    require(os.environ.get("GITHUB_ACTIONS") == "true" and
            os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and
            os.environ.get("RUNNER_OS") == "Windows")
    validate_binding(binding)
    require(binding["run_id"] == os.environ.get("GITHUB_RUN_ID") and
            binding["run_attempt"] == os.environ.get("GITHUB_RUN_ATTEMPT") and
            binding["verifier_commit"] == os.environ.get("GITHUB_SHA"))
    root = plain(output_root)
    root.relative_to(plain(os.environ["RUNNER_TEMP"]))
    root.mkdir(exist_ok=False)
    executable, package = plain(executable), plain(package)
    require(digest(executable) == binding["exe_sha256"] and digest(package) == binding["package_sha256"])
    tool_hashes = pinned_tools(tool_root)
    verifier_hash = verifier_identity(binding["verifier_commit"])
    job_member(binding["job_name"], os.getpid())
    original = observe(binding, executable)
    sdk = version("mcp")
    require(re.fullmatch(r"1\.(\d+)\.(\d+)", sdk) and int(sdk.split(".")[1]) >= 2)
    receipt = {"schema": 1, "binding": binding, "status": "unverified", "frames": [],
               "tool_commit": TOOL_COMMIT, "tool_sha256_lf": tool_hashes, "mcp_version": sdk,
               "helper_sha256_lf": verifier_hash,
               "observation_bounds_only": True, "pixel_semantics": "pending_review",
               "capture_semantics": "window_client_printwindow_uncomposited",
               "native_layered_alpha_verified": False,
               "input_acknowledged": False, "server_exit_verified": False,
               "disposal_required": True, "intermediate_indices": []}
    # Exclusive invocation marker remains uncertain if the outer supervisor kills
    # this helper while a blocking capture or input server is still alive.
    marker = root / "temporal-pending.json"
    with marker.open("x", encoding="utf-8") as stream:
        json.dump({"nonce": binding["nonce"], "verifier_commit": binding["verifier_commit"]}, stream)
        stream.flush()
        os.fsync(stream.fileno())
    servers = []
    input_task = None
    try:
        async with AsyncExitStack() as stack:
            sessions = []
            for role in ("capture", "input"):
                started = time.time()
                before = {(p.pid, p.create_time()) for p in psutil.Process().children()}
                params = StdioServerParameters(command=sys.executable,
                    args=["-m", "lowlevel_computer_use_mcp.server"],
                    env={k: os.environ[k] for k in ("PATH", "SYSTEMROOT", "WINDIR", "TEMP", "TMP",
                         "USERPROFILE", "LOCALAPPDATA", "APPDATA", "HOMEDRIVE", "HOMEPATH") if k in os.environ})
                errors = stack.enter_context(open(os.devnull, "w"))
                read, write = await stack.enter_async_context(stdio_client(params, errlog=errors))
                session = await stack.enter_async_context(ClientSession(read, write,
                    read_timeout_seconds=timedelta(seconds=3)))
                await session.initialize()
                candidates = [p for p in psutil.Process().children() if
                    (p.pid, p.create_time()) not in before and p.create_time() >= started and
                    p.cmdline()[1:] == ["-m", "lowlevel_computer_use_mcp.server"] and
                    plain(p.exe()) == plain(sys.executable)]
                require(len(candidates) == 1)
                server = candidates[0]
                job_member(binding["job_name"], server.pid)
                identity = (server.pid, server.create_time())
                servers.append(identity)
                sessions.append((session, identity))

            async def call(index, name, params):
                session, identity = sessions[index]
                require(psutil.Process(identity[0]).create_time() == identity[1])
                response = await asyncio.wait_for(session.call_tool(name, {"params": params}), 3)
                require(not response.isError and len(response.content) == 1 and
                        response.content[0].type == "text" and len(response.content[0].text) <= 65536)
                data = json.loads(response.content[0].text)
                require(type(data) is dict and data.get("ok") is True)
                return data

            # Warm both servers before the measured action. Read-only desktop
            # enumeration is outside the capture burst, with no UIA traversal.
            for index in (0, 1):
                data = await call(index, "list_headless_windows", {"name": binding["desktop"]})
                require(data.get("name") == binding["desktop"] and
                        any(row.get("handle") == binding["hwnd"] and row.get("process_id") == binding["pid"]
                            for row in data.get("windows", [])))

            async def frame(index):
                require(observe(binding, executable) == original)
                path = root / f"frame-{index}.png"
                bounds = {"start": stamp()}
                data = await call(0, "screenshot", {"hwnd": binding["hwnd"], "client_only": True,
                                                     "output_path": str(path)})
                bounds["end"] = stamp()
                require(observe(binding, executable) == original and data.get("rendered_ok") is True and
                        data.get("mode") == "window" and data.get("window_hwnd") == binding["hwnd"] and
                        [data.get("width"), data.get("height")] == binding["client_size"] and
                        plain(data.get("path", "")) == path and 0 < path.stat().st_size <= MAX_PNG)
                receipt["frames"].append({"index": index, "requested_offset_ms": OFFSETS_MS[index],
                                          "path": path.name, "interval": bounds})

            await frame(0)
            require(observe(binding, executable, check_input=True) == original)
            click_bounds = {"start": stamp()}
            receipt["click_interval"] = click_bounds

            async def click():
                require(observe(binding, executable, check_input=True) == original)
                click_args = {key: binding["click"][key] for key in ("x", "y", "button")}
                data = await call(1, "mouse_click", {"hwnd": binding["hwnd"], **click_args})
                click_bounds["end"] = stamp()
                require(data.get("mode") == "background" and data.get("window_hwnd") == binding["hwnd"] and
                        data.get("target_hwnd") == binding["click"]["target_hwnd"] and
                        observe(binding, executable, check_input=True) == original)
                receipt["input_acknowledged"] = True

            async with joined_input(click()) as input_task:
                # Deadlines are relative to dispatch, not the end of a slow previous
                # screenshot. Waiting occurs outside the blocking capture operation.
                for index in (1, 2, 3):
                    deadline = click_bounds["start"]["monotonic_ns"] + OFFSETS_MS[index] * 1_000_000
                    await asyncio.sleep(max(0, (deadline - time.monotonic_ns()) / 1_000_000_000))
                    await frame(index)
            require(observe(binding, executable) == original)
        # Normal context exit closes the supported stdio sessions. A stuck exit
        # is bounded by the required outer Job deadline, never assumed complete.
        for pid, created in servers:
            try:
                require(psutil.Process(pid).create_time() != created)
            except psutil.NoSuchProcess:
                pass
        require(len(servers) == 2 and input_task.done() and not input_task.cancelled())
        input_task.result()
        receipt["server_exit_verified"] = True
        # File decoding/hashing is deliberately after the measured burst.
        for row in receipt["frames"]:
            path = plain(root / row["path"])
            require(path.stat().st_size <= MAX_PNG)
            with Image.open(path) as image:
                require(image.format == "PNG" and list(image.size) == binding["client_size"])
                pixels = image.convert("RGB").crop(tuple(binding["roi"])).tobytes()
            row.update({"sha256": digest(path), "bytes": path.stat().st_size,
                        "roi_sha256": hashlib.sha256(pixels).hexdigest()})
        receipt["intermediate_indices"] = intermediate_indices(click_bounds, receipt["frames"])
        receipt["status"] = "observed" if receipt["intermediate_indices"] else "not_observed"
        receipt["disposal_required"] = False
    except Exception:
        # Do not expose exception text, server output or private bindings.
        receipt["status"] = "unverified"
    finally:
        with (root / "temporal.json").open("x", encoding="utf-8") as stream:
            json.dump(receipt, stream, separators=(",", ":"))
            stream.flush()
            os.fsync(stream.fileno())
        if not receipt["disposal_required"]:
            # No deletion. Preserve the completed invocation alongside its images.
            marker.rename(root / "temporal-finished.json")
    return {"status": receipt["status"], "frames": len(receipt["frames"]),
            "disposal_required": receipt["disposal_required"], "temporal_acceptance": receipt["status"] == "observed"}
