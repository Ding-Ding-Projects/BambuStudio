"""One hosted baseline resize proof using a persistent compatibility MCP handoff.

The handoff server retains its original input-desktop handle and banner. The
actual drag uses the pinned cheap CLI from the existing owned-desktop worker.
All observations and identities belong only in restricted runtime evidence.
"""
import asyncio
import ctypes
from ctypes import wintypes
from datetime import timedelta
from importlib.metadata import version
import json
import os
from pathlib import Path
import re
import sys
import time


def require(value, message):
    if not value:
        raise RuntimeError(message)


def desktop_api():
    user = ctypes.WinDLL("user32", use_last_error=True)
    user.OpenInputDesktop.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    user.OpenInputDesktop.restype = wintypes.HANDLE
    user.CloseDesktop.argtypes = [wintypes.HANDLE]
    user.GetThreadDesktop.argtypes = [wintypes.DWORD]
    user.GetThreadDesktop.restype = wintypes.HANDLE
    user.GetUserObjectInformationW.argtypes = [wintypes.HANDLE, ctypes.c_int,
        ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    return user


def desktop_name(user, handle):
    text, needed = ctypes.create_unicode_buffer(256), wintypes.DWORD()
    require(handle and user.GetUserObjectInformationW(handle, 2, text, ctypes.sizeof(text),
            ctypes.byref(needed)), "Desktop identity unavailable")
    return text.value


def input_desktop():
    user = desktop_api()
    handle = user.OpenInputDesktop(0, False, 1)
    try:
        return desktop_name(user, handle)
    finally:
        if handle:
            user.CloseDesktop(handle)


def native_minimum_operation(request, user):
    """Read-only ownership queries, followed by one existing cheap pointer drag."""
    import psutil
    from recapture import cheap
    pid, hwnd = request["pid"], request["hwnd"]
    require(psutil.Process(pid).create_time() == request["process_started"], "Product identity changed")
    desktops = desktop_api()
    kernel = ctypes.WinDLL("kernel32")
    require(desktop_name(desktops, desktops.GetThreadDesktop(kernel.GetCurrentThreadId())) == request["desktop"],
            "Resize observer is not on the owned desktop")
    user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.GetDpiForWindow.argtypes = [wintypes.HWND]
    user.GetForegroundWindow.restype = wintypes.HWND
    user.IsZoomed.argtypes = [wintypes.HWND]
    user.GetAsyncKeyState.argtypes = [ctypes.c_int]
    user.GetAsyncKeyState.restype = ctypes.c_short
    user.WindowFromPoint.argtypes = [wintypes.POINT]
    user.WindowFromPoint.restype = wintypes.HWND
    user.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
    user.GetAncestor.restype = wintypes.HWND
    user.SendMessageTimeoutW.argtypes = [wintypes.HWND, wintypes.UINT, ctypes.c_size_t,
        ctypes.c_ssize_t, wintypes.UINT, wintypes.UINT, ctypes.POINTER(ctypes.c_size_t)]
    def geometry():
        actual = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(actual))
        require(actual.value == pid and psutil.Process(pid).create_time() == request["process_started"],
                "Resize target ownership changed")
        rect = wintypes.RECT()
        require(user.GetWindowRect(hwnd, ctypes.byref(rect)), "Frame geometry unavailable")
        return [rect.left, rect.top, rect.right, rect.bottom]
    before = geometry()
    result = {"rect": before, "dpi": user.GetDpiForWindow(hwnd), "pid": pid,
              "process_started": request["process_started"], "hwnd": hwnd}
    if request["operation"] == "minimum-geometry":
        return result
    started = time.monotonic()
    require(input_desktop() == request["desktop"] and user.GetForegroundWindow() == hwnd,
            "Owned resize frame is not the foreground input target")
    width, height = request["minimum"]
    require(before[2] - before[0] == width + 80 and before[3] - before[1] == height + 80,
            "Resize must begin above the observed minimum")
    require(result["dpi"] == 96, "Interactive baseline DPI changed")
    require(not user.IsZoomed(hwnd) and not (user.GetAsyncKeyState(1) & 0x8000),
            "Frame is maximized or a mouse button is already held")
    point = None
    for inset in (2, 4, 6, 8):
        x, y = before[2] - inset, before[3] - inset
        hit = user.WindowFromPoint(wintypes.POINT(x, y))
        hit_pid = wintypes.DWORD()
        user.GetWindowThreadProcessId(hit, ctypes.byref(hit_pid))
        if hit_pid.value != pid or user.GetAncestor(hit, 2) != hwnd:
            continue
        code = ctypes.c_size_t()
        # WM_NCHITTEST is observation only. No sizing command or input message is sent.
        if user.SendMessageTimeoutW(hwnd, 0x84, 0, (y << 16) | (x & 0xffff), 2, 200,
                                    ctypes.byref(code)) and code.value == 17:
            point = (x, y)
            break
    require(point is not None, "Owned bottom-right sizing edge unavailable")
    require(geometry() == before and input_desktop() == request["desktop"] and
            user.GetForegroundWindow() == hwnd and time.monotonic() - started < 0.5,
            "Resize target observation expired")
    x, y = point
    require(min(x, y, x - 120, y - 120) >= 0, "Resize coordinates exceed the supported screen range")
    # Every point along the straight drag remains over the owned frame. This
    # also excludes an overlapping handoff banner before any button-down.
    for offset in range(121):
        hit = user.WindowFromPoint(wintypes.POINT(x - offset, y - offset))
        require(hit and user.GetAncestor(hit, 2) == hwnd, "Resize path is obscured")
    require(geometry() == before and input_desktop() == request["desktop"] and
            user.GetForegroundWindow() == hwnd and time.monotonic() - started < 0.5,
            "Resize path observation expired")
    cheap("mouse_drag", start_x=x, start_y=y, end_x=x - 120, end_y=y - 120,
          button="left", duration=0.5, confirm_focus_disruption=True)
    require(input_desktop() == request["desktop"] and user.GetForegroundWindow() == hwnd,
            "Resize input ownership changed")
    require(not (user.GetAsyncKeyState(1) & 0x8000), "Mouse release was not observed")
    result.update({"after_rect": geometry(), "start": [x, y], "end": [x - 120, y - 120],
                   "hit_test": 17, "input_route": "cheap_foreground_drag_owned_desktop",
                   "atomic_input_binding": False, "mouse_release_verified": True})
    return result


def job_member(job_name, pid):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenJobObjectW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
    kernel.OpenJobObjectW.restype = wintypes.HANDLE
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.IsProcessInJob.argtypes = [wintypes.HANDLE, wintypes.HANDLE, ctypes.POINTER(wintypes.BOOL)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    job, process = kernel.OpenJobObjectW(4, False, job_name), kernel.OpenProcess(0x1000, False, pid)
    try:
        member = wintypes.BOOL()
        require(job and process and kernel.IsProcessInJob(process, job, ctypes.byref(member)) and member.value,
                "Minimum proof process is outside its owned Job")
    finally:
        if process:
            kernel.CloseHandle(process)
        if job:
            kernel.CloseHandle(job)


def run_minimum_resize(drive):
    import psutil
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    sdk = version("mcp")
    parsed = re.fullmatch(r"1\.(\d+)\.(\d+)", sdk)
    require(parsed and int(parsed[1]) >= 2, "Unsupported installed MCP client")
    require(input_desktop() == "Default", "Original input desktop is not Default")
    desktops = desktop_api()
    kernel = ctypes.WinDLL("kernel32")
    require(desktop_name(desktops, desktops.GetThreadDesktop(kernel.GetCurrentThreadId())) == "Default",
            "Persistent handoff client must start on Default")
    job_member(drive.args.minimum_job_name, os.getpid())
    job_member(drive.args.minimum_job_name, drive.app.pid)
    process_started = psutil.Process(drive.app.pid).create_time()
    identity = {"process_started": process_started, "desktop": drive.app.desktop}
    def geometry():
        drive.worker("minimum-geometry", **identity)
        return drive.last_input["native_input_target"]
    original = geometry()
    root = next(r for r in drive.probe if r.get("kind") == "window" and
                r.get("hwnd") == drive.app.main and r.get("depth") == 0)
    minimum = [root["min"]["w"], root["min"]["h"]]
    receipt = {"operation": "interactive-minimum-resize", "status": "unverified", "mcp_version": sdk,
        "transport": "persistent_compatibility_mcp_stdio", "original": original,
        "minimum_outer": minimum, "frame_restored": False, "input_desktop_restored": False,
        "server_exit_verified": False, "mouse_release_verified": True, "disposal_required": True}
    drive.rows.append(receipt)
    async def exercise():
        server_identity = None
        parameters = StdioServerParameters(command=sys.executable,
            args=["-m", "lowlevel_computer_use_mcp.server"],
            env={key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TEMP", "TMP",
                "USERPROFILE", "LOCALAPPDATA", "APPDATA", "HOMEDRIVE", "HOMEPATH") if key in os.environ})
        started = time.time()
        with open(os.devnull, "w") as errors:
            async with stdio_client(parameters, errlog=errors) as (read, write):
                async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=15)) as session:
                    await session.initialize()
                    servers = [p for p in psutil.Process().children() if p.create_time() >= started and
                        p.cmdline()[1:] == ["-m", "lowlevel_computer_use_mcp.server"] and
                        Path(p.exe()).resolve() == Path(sys.executable).resolve()]
                    require(len(servers) == 1, "Persistent handoff server identity is ambiguous")
                    server_identity = (servers[0].pid, servers[0].create_time())
                    job_member(drive.args.minimum_job_name, servers[0].pid)
                    async def call(name, arguments):
                        require(psutil.Process(server_identity[0]).create_time() == server_identity[1],
                                "Persistent server identity changed")
                        response = await session.call_tool(name, {"params": arguments})
                        require(not response.isError and len(response.content) == 1 and
                                response.content[0].type == "text" and len(response.content[0].text) < 65536,
                                "Bounded handoff response unavailable")
                        require(json.loads(response.content[0].text).get("ok") is True, "Handoff tool unavailable")
                    handed_off = False
                    try:
                        drive.worker("resize", size=[minimum[0] + 80, minimum[1] + 80])
                        before = geometry()
                        require(before["rect"][2] - before["rect"][0] == minimum[0] + 80 and
                                before["rect"][3] - before["rect"][1] == minimum[1] + 80,
                                "Above-minimum frame could not be established")
                        receipt["before"] = before
                        receipt["before_capture"] = drive.capture("minimum-before-drag")
                        require(input_desktop() == "Default", "Original input desktop changed before handoff")
                        handed_off = True  # Even an interrupted response requires restoration.
                        await call("show_headless_desktop", {"name": drive.app.desktop,
                            "instruction": "Hosted minimum-size verification in progress.", "confirm_focus_disruption": True})
                        require(input_desktop() == drive.app.desktop, "Owned desktop handoff was not observed")
                        receipt["mouse_release_verified"] = False
                        drive.worker("minimum-drag", minimum=minimum, **identity)
                        receipt["drag"] = drive.last_input["native_input_target"]
                        receipt["mouse_release_verified"] = receipt["drag"].get("mouse_release_verified") is True
                        after = geometry()
                        receipt["after"] = after
                        receipt["after_capture"] = drive.capture("minimum-after-drag")
                        root_after = next(r for r in drive.probe if r.get("kind") == "window" and
                            r.get("hwnd") == drive.app.main and r.get("depth") == 0)
                        require(after["rect"] != before["rect"] and after["rect"][:2] == before["rect"][:2] and
                                [after["rect"][2] - after["rect"][0], after["rect"][3] - after["rect"][1]] == minimum and
                                [root_after["min"]["w"], root_after["min"]["h"]] == minimum and after["dpi"] == 96,
                                "Interactive sizing did not clamp at the measured minimum")
                        receipt["status"] = "interactive_clamp_observed"
                    finally:
                        # A timeout may interrupt the drag's own mouseUp finally.
                        # Never carry an uncertain held button to another desktop.
                        require(receipt["mouse_release_verified"], "Input release unknown; host disposal required")
                        try:
                            rect = original["rect"]
                            drive.worker("resize", size=[rect[2] - rect[0], rect[3] - rect[1]])
                            restored = geometry()
                            receipt["frame_restored"] = restored["rect"] == rect and restored["dpi"] == original["dpi"]
                        finally:
                            if handed_off:
                                await call("hide_headless_desktop", {"name": drive.app.desktop})
                            receipt["input_desktop_restored"] = input_desktop() == "Default"
        if server_identity:
            try:
                receipt["server_exit_verified"] = psutil.Process(server_identity[0]).create_time() != server_identity[1]
            except psutil.NoSuchProcess:
                receipt["server_exit_verified"] = True
    try:
        asyncio.run(exercise())
        require(receipt["frame_restored"] and receipt["input_desktop_restored"] and
                receipt["server_exit_verified"], "Minimum proof restoration or server shutdown unverified")
        receipt["disposal_required"] = False
        drive.viewport_observations[-1]["interactive_resize_clamp"] = "observed_in_dedicated_scope"
        drive.viewport_observations[-1]["interactive_operation"] = receipt["operation"]
    except BaseException:
        receipt["status"] = "unverified"
        raise
