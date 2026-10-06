#!/usr/bin/env python3
"""Inspect one locally built initial shell. Never manufacture build success.

Default execution validates provenance only. Live execution requires both
--execute and --desktop visible. Evidence stays in a fresh OS temporary folder.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes as wt
from datetime import datetime, timezone, timedelta
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
import threading
import time

HERE = Path(__file__).resolve().parent
REQUIRED = ("bambu-studio.exe", "BambuStudio.dll", "automation/bambu-automation.exe",
            "automation/build-identity.json")
HASH = re.compile(r"[0-9a-f]{64}")
SHA = re.compile(r"[0-9a-f]{40}")
WORKER_SECONDS = 225
TOOL_SECONDS = 270


def require(condition, message):
    if not condition:
        raise ValueError(message)


class LaunchFailure(ValueError):
    def __init__(self, message, teardown):
        super().__init__(message)
        self.teardown = teardown


def regular(path: Path) -> Path:
    require(path.is_absolute(), "Evidence paths must be absolute")
    for part in (path, *path.parents):
        info = part.lstat()
        require(not stat.S_ISLNK(info.st_mode) and not getattr(info, "st_file_attributes", 0) & 0x400,
                "Linked or redirected evidence path")
    require(path.is_file(), "Evidence file missing")
    return path


def digest(path: Path) -> str:
    regular(path)
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def read_json(path: Path, limit=1024 * 1024):
    regular(path)
    require(path.stat().st_size <= limit, "JSON exceeds evidence bound")
    return json.loads(path.read_text(encoding="utf-8-sig"))


def timestamp(value):
    require(isinstance(value, str) and value.endswith("Z"), "UTC timestamp must end in Z")
    return datetime.fromisoformat(value[:-1] + "+00:00")


def git(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True,
                            text=True, timeout=20, check=True)
    return result.stdout.strip()


def validate_build_transcript(text, start, end, expected_source, payload):
    """Bind the latest appended producer invocation, not a matching older pin."""
    lines = [line.strip() for line in text.splitlines()]
    sessions = [i for i, line in enumerate(lines)
                if re.fullmatch(r"(?:Windows )?PowerShell transcript start", line)]
    require(sessions, "Transcript invocation start missing")
    latest = lines[sessions[-1]:]
    markers = ("Bambu Studio one-click build started", "Pinned build source:",
               "Build-only workflow completed; runnable payload:", "One-click workflow completed successfully.")
    records = []
    for marker in markers:
        matches = [(i, line) for i, line in enumerate(latest) if marker in line]
        require(len(matches) == 1, "Latest invocation marker missing or repeated: " + marker)
        index, line = matches[0]
        logged = re.fullmatch(r"\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}Z)\] (.+)", line)
        require(logged is not None, "Invocation marker lacks producer UTC timestamp")
        moment = datetime.strptime(logged[1], "%Y-%m-%d %H:%M:%SZ").replace(tzinfo=timezone.utc)
        # Write-BuildLog truncates to whole UTC seconds. Compare at that precision;
        # transcript header/footer local-time fields are never treated as UTC.
        require(start.replace(microsecond=0) <= moment <= end, "Invocation marker outside receipt interval")
        records.append((index, moment, logged[2]))
    require(all(a[0] < b[0] and a[1] <= b[1] for a, b in zip(records, records[1:])),
            "Invocation markers are out of order")
    require(re.fullmatch(r"Bambu Studio one-click build started \(mode=(?:Incremental|Clean), install=False, plan=False\)\.", records[0][2]),
            "Transcript does not describe a real build-only invocation")
    require(records[1][2] == "Pinned build source: " + expected_source, "Latest invocation source differs")
    completion = "Build-only workflow completed; runnable payload: "
    require(records[2][2].startswith(completion), "Build-only completion evidence missing")
    require(Path(records[2][2][len(completion):]).resolve() == (payload / "bambu-studio.exe").resolve(),
            "Transcript completed another payload")
    require(records[3][2] == "One-click workflow completed successfully.", "Terminal success evidence missing")
    tail = [line for line in latest[records[3][0] + 1:] if line and not re.fullmatch(r"\*+", line)]
    require(len(tail) == 2 and re.fullmatch(r"(?:Windows )?PowerShell transcript end", tail[0])
            and re.fullmatch(r"End time: \d{14}", tail[1]), "Latest invocation did not close successfully")


def validate_receipt(receipt, root, expected_source, now=None, git_read=git):
    """An observer receipt is evidence, not a signature or a build invocation."""
    now = now or datetime.now(timezone.utc)
    require(type(receipt.get("schemaVersion")) is int and receipt["schemaVersion"] == 1 and receipt.get("kind") == "local-root-build",
            "Not a local root build receipt")
    require(SHA.fullmatch(expected_source or "") and receipt.get("sourceCommit") == expected_source,
            "Source identity mismatch")
    require(receipt.get("entrypoint") == "build.bat" and receipt.get("arguments") == ["/s"],
            "Receipt must describe the exact root build.bat /s invocation")
    require(type(receipt.get("exitCode")) is int and receipt["exitCode"] == 0,
            "Root build did not report success")
    require(receipt.get("sourceCleanBefore") is True and receipt.get("sourceCleanAfter") is True,
            "Build source cleanliness is unverified")
    require(isinstance(receipt.get("invocationId"), str) and 0 < len(receipt["invocationId"]) <= 128,
            "Invocation identity missing")
    start, end = timestamp(receipt["startedAtUtc"]), timestamp(receipt["finishedAtUtc"])
    require(start < end <= now and now - end <= timedelta(hours=24), "Stale or unfinished build receipt")
    require(git_read(root, "rev-parse", "HEAD") == expected_source and
            git_read(root, "rev-parse", "HEAD^{tree}") == receipt.get("sourceTree") and
            SHA.fullmatch(receipt.get("sourceTree", "")), "Producer source moved")
    require(not git_read(root, "status", "--porcelain=v1", "--untracked-files=normal"),
            "Producer source is not clean")
    require(digest(root / "build.bat") == receipt.get("entrypointSha256"), "Root entrypoint changed")
    transcript = Path(receipt["transcript"]["path"])
    require(digest(transcript) == receipt["transcript"]["sha256"], "Transcript changed")
    require(transcript.stat().st_size <= 64 * 1024 * 1024, "Transcript exceeds bound")
    raw = transcript.read_bytes()
    encoding = "utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig"
    text = raw.decode(encoding, errors="strict")
    payload = Path(receipt["payload"]["root"])
    require(payload.is_absolute() and payload.resolve() == (root / "install-dir").resolve(),
            "Payload is outside the recorded producer")
    validate_build_transcript(text, start, end, expected_source, payload)
    files = receipt["payload"]["files"]
    require(isinstance(files, dict) and set(files) == set(REQUIRED), "Required payload identities differ")
    for name in REQUIRED:
        require(isinstance(files[name], str) and HASH.fullmatch(files[name]) and
                digest(payload / name) == files[name], "Payload identity mismatch: " + name)
    companion = read_json(payload / "automation/build-identity.json")
    require(companion.get("sourceCommit") == expected_source and companion.get("buildRoute") == "local-windows"
            and companion.get("sha256") == files["automation/bambu-automation.exe"],
            "Automation companion identity mismatch")
    return payload / "bambu-studio.exe"


def seed_profile(root):
    profile = root / "profile"
    profile.mkdir()  # Never overwrite an existing profile.
    body = json.dumps({"app": {"language": "en", "dark_color_mode": "0",
        "ui_density": "comfortable", "motion_preference": "reduced",
        "single_instance": "false", "check_update": "false", "show_hints": "false"}}, indent=4)
    checksum = hashlib.md5(body.encode("utf-8")).hexdigest().upper()
    (profile / "BambuStudio.conf").write_bytes((body + "\n# MD5 checksum " + checksum + "\n").encode())
    return profile


def cheap(cli, tool, **arguments):
    require(tool in ("list_windows", "screenshot", "run_command"), "Unsupported desktop operation")
    result = subprocess.run([str(cli), tool, "--json", json.dumps(arguments)],
                            capture_output=True, text=True, timeout=300 if tool == "run_command" else 30, check=True,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    require(len(result.stdout) <= 2 * 1024 * 1024, "Desktop reply exceeds bound")
    reply = json.loads(result.stdout)
    require(reply.get("ok") is True, "Desktop operation failed")
    return reply


def worker_command(request):
    # Lowlevel's shell=False uses shlex(posix=False), which retains quotes in
    # argv on Windows. Use its documented shell route with a fixed command and
    # reject shell metacharacters in every path rather than interpolate them.
    values = (sys.executable, str(Path(__file__).resolve()), str(request))
    require(all(not re.search(r'["\r\n&|<>^%!]', value) for value in values), "Unsafe launcher path")
    return f'"{values[0]}" "{values[1]}" --worker "{values[2]}"'


def select_shell(windows, pid, native_identity):
    """Do not persist other applications' titles or borrow a title match."""
    matches = []
    for row in windows:
        hwnd = row.get("handle")
        if type(hwnd) is not int or hwnd <= 0:
            continue
        identity = native_identity(hwnd)
        if identity and identity["pid"] == pid and identity["class"] == "wxWindowNR" and identity["visible"]:
            matches.append({"hwnd": hwnd, **identity})
    require(len(matches) <= 1, "Owned shell window is ambiguous")
    return matches[0] if matches else None


def validate_probe(rows, pid, hwnd, tag):
    require(rows and rows[-1] == {"kind": "end"}, "Native probe is incomplete")
    headers = [r for r in rows if r.get("kind") == "header"]
    require(len(headers) == 1, "Native probe header is ambiguous")
    header = headers[0]
    require(header.get("pid") == pid and header.get("tag") == tag, "Native probe ownership mismatch")
    require(header.get("language") == "en" and header.get("dark") is False and
            header.get("density") == "comfortable", "Native profile tuple mismatch")
    scale = header.get("dpi_scale")
    require(type(scale) in (int, float) and math.isfinite(scale) and scale > 0, "Invalid measured DPI")
    frames = [r for r in rows if r.get("kind") == "toplevel" and r.get("hwnd") == hwnd]
    require(len(frames) == 1 and frames[0].get("shown") is True, "Owned shell is not in native probe")
    client = frames[0].get("client", {})
    require(all(type(client.get(k)) is int and client[k] > 0 for k in ("w", "h")), "Invalid client geometry")
    findings = [{k: r[k] for k in ("hwnd", "kind", "flags") if k in r}
                for r in rows if r.get("flags")]
    other_windows = [r["hwnd"] for r in rows if r.get("kind") == "toplevel" and
                     r.get("shown") is True and r.get("hwnd") != hwnd]
    return {"client": client, "dpi_scale": scale, "findings": findings,
            "otherVisibleWindows": other_windows, "shellUnobstructed": "unverified"}


def validate_image(path, reply, hwnd, geometry):
    require(reply.get("rendered_ok") is True and reply.get("mode") == "window" and
            reply.get("window_hwnd") == hwnd and Path(reply.get("path", "")).resolve() == path.resolve(),
            "Screenshot target or rendering is unverified")
    from PIL import Image
    regular(path)
    with Image.open(path) as im:
        im.load()
        require(im.format == "PNG" and im.size == (geometry["w"], geometry["h"]),
                "Screenshot dimensions differ from measured client")
        extrema = im.convert("RGB").getextrema()
        require(any(low != high for low, high in extrema), "Screenshot is blank")
    return {"status": "produced", "sha256": digest(path), "path": str(path),
            "privacy": "unreviewed", "visualAcceptance": "unverified"}


class NativeSession:
    """Suspended launch is assigned to an owned kill-on-close job before running."""
    def __init__(self, exe, profile, tag):
        require(os.name == "nt", "Native review requires Windows")
        from ctypes import wintypes as w
        self.k = ctypes.WinDLL("kernel32", use_last_error=True)
        self.u = ctypes.WinDLL("user32", use_last_error=True)
        class SI(ctypes.Structure):
            _fields_ = [("cb", w.DWORD), ("reserved", w.LPWSTR), ("desktop", w.LPWSTR),
                ("title", w.LPWSTR), *[(n, w.DWORD) for n in ("x", "y", "cx", "cy", "cols", "rows", "fill", "flags")],
                ("show", w.WORD), ("reserved2size", w.WORD), ("reserved2", ctypes.c_void_p),
                ("stdin", w.HANDLE), ("stdout", w.HANDLE), ("stderr", w.HANDLE)]
        class PI(ctypes.Structure):
            _fields_ = [("process", w.HANDLE), ("thread", w.HANDLE), ("pid", w.DWORD), ("tid", w.DWORD)]
        class Basic(ctypes.Structure):
            _fields_ = [("processTime", ctypes.c_int64), ("jobTime", ctypes.c_int64), ("flags", w.DWORD),
                ("minWorking", ctypes.c_size_t), ("maxWorking", ctypes.c_size_t), ("activeLimit", w.DWORD),
                ("affinity", ctypes.c_size_t), ("priority", w.DWORD), ("scheduling", w.DWORD)]
        class Extended(ctypes.Structure):
            _fields_ = [("basic", Basic), ("io", ctypes.c_uint64 * 6),
                *[(n, ctypes.c_size_t) for n in ("processMemory", "jobMemory", "peakProcess", "peakJob")]]
        signatures = {"CreateJobObjectW": ([ctypes.c_void_p, w.LPCWSTR], w.HANDLE),
            "SetInformationJobObject": ([w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD], w.BOOL),
            "CreateProcessW": ([w.LPCWSTR, w.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, w.BOOL, w.DWORD,
                ctypes.c_void_p, w.LPCWSTR, ctypes.POINTER(SI), ctypes.POINTER(PI)], w.BOOL),
            "AssignProcessToJobObject": ([w.HANDLE, w.HANDLE], w.BOOL),
            "ResumeThread": ([w.HANDLE], w.DWORD), "TerminateProcess": ([w.HANDLE, w.UINT], w.BOOL),
            "TerminateJobObject": ([w.HANDLE, w.UINT], w.BOOL),
            "WaitForSingleObject": ([w.HANDLE, w.DWORD], w.DWORD),
            "QueryInformationJobObject": ([w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD, ctypes.c_void_p], w.BOOL),
            "CloseHandle": ([w.HANDLE], w.BOOL)}
        for name, (arguments, result) in signatures.items():
            getattr(self.k, name).argtypes, getattr(self.k, name).restype = arguments, result
        self.u.OpenInputDesktop.argtypes, self.u.OpenInputDesktop.restype = [w.DWORD, w.BOOL, w.DWORD], w.HANDLE
        self.u.GetUserObjectInformationW.argtypes = [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD)]
        self.u.CloseDesktop.argtypes = [w.HANDLE]
        desktop = self.u.OpenInputDesktop(0, False, 1)
        require(desktop, "Input desktop is unavailable")
        name, length = ctypes.create_unicode_buffer(256), w.DWORD()
        try:
            require(self.u.GetUserObjectInformationW(desktop, 2, name, ctypes.sizeof(name), ctypes.byref(length))
                    and name.value.casefold() == "default", "Visible default desktop is not active")
        finally:
            self.u.CloseDesktop(desktop)
        self.job, self.process = self.k.CreateJobObjectW(None, None), None
        require(self.job, "Cannot create owned process job")
        info = Extended()
        info.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        try:
            require(self.k.SetInformationJobObject(self.job, 9, ctypes.byref(info), ctypes.sizeof(info)), "Cannot contain process")
            env = dict(os.environ)
            for key in list(env):
                if key.upper().startswith("BAMBU_AUTOMATION") or key.upper().startswith("BAMBU_LAYOUT_PROBE"):
                    del env[key]
            env.update(BAMBU_LAYOUT_PROBE="1", BAMBU_LAYOUT_PROBE_TAG=tag)
            environment = ctypes.create_unicode_buffer("\0".join(k + "=" + v for k, v in sorted(env.items())) + "\0\0")
            startup, process = SI(), PI()
            startup.cb, startup.desktop, startup.flags, startup.show = ctypes.sizeof(SI), "WinSta0\\Default", 1, 1
            command = ctypes.create_unicode_buffer(subprocess.list2cmdline([str(exe), "--datadir", str(profile)]))
            require(self.k.CreateProcessW(str(exe), command, None, None, False, 0x404, environment,
                        str(exe.parent), ctypes.byref(startup), ctypes.byref(process)), "Native launch failed")
            self.process, self.pid = process.process, int(process.pid)
            try:
                require(self.k.AssignProcessToJobObject(self.job, self.process), "Process containment failed")
                require(self.k.ResumeThread(process.thread) != 0xFFFFFFFF, "Process resume failed")
            except Exception:
                self.k.TerminateProcess(self.process, 1)
                raise
            finally:
                self.k.CloseHandle(process.thread)
        except Exception as exc:
            try:
                teardown = "verified" if self.close() else "unverified"
            except Exception:
                teardown = "unverified"
            raise LaunchFailure(str(exc), teardown) from exc

    def alive(self):
        return self.k.WaitForSingleObject(self.process, 0) == 258

    def identity(self, hwnd):
        self.u.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
        self.u.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
        self.u.IsWindowVisible.argtypes = [wt.HWND]
        pid, name = wt.DWORD(), ctypes.create_unicode_buffer(256)
        self.u.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value != self.pid:
            return None
        self.u.GetClassNameW(hwnd, name, 256)
        return {"pid": pid.value, "class": name.value, "visible": bool(self.u.IsWindowVisible(hwnd))}

    def close(self):
        verified = True
        if self.job:
            verified = bool(self.k.TerminateJobObject(self.job, 0))
            if self.process:
                verified = self.k.WaitForSingleObject(self.process, 5000) == 0 and verified
            class Accounting(ctypes.Structure):
                _fields_ = [("times", ctypes.c_int64 * 4), ("pageFaults", wt.DWORD),
                            ("total", wt.DWORD), ("active", wt.DWORD), ("terminated", wt.DWORD)]
            accounting = Accounting()
            deadline = time.monotonic() + 5
            while True:
                observed = self.k.QueryInformationJobObject(self.job, 1, ctypes.byref(accounting), ctypes.sizeof(accounting), None)
                if not observed or accounting.active == 0 or time.monotonic() >= deadline:
                    break
                time.sleep(0.05)
            verified = bool(observed) and accounting.active == 0 and verified
            self.k.CloseHandle(self.job)
            self.job = None
        if self.process:
            self.k.CloseHandle(self.process)
            self.process = None
        return verified


def inspect_shell(args, receipt, root, exe, session_factory=NativeSession, call=cheap):
    report = {"schemaVersion": 1, "kind": "local-initial-shell", "sourceCommit": args.source_commit,
        "buildReceiptSha256": digest(args.build_receipt), "driverSha256": digest(Path(__file__).resolve()),
        "desktop": "visible", "launch": {"status": "not_attempted"}, "probe": {"status": "not_attempted"},
        "screenshot": {"status": "not_attempted"}, "teardown": "not_attempted", "runtimeAcceptance": "unverified"}
    session = None
    try:
        profile = seed_profile(root)
        tag = root.name
        # Repeat provenance immediately before process creation, after profile setup.
        validate_receipt(receipt, args.producer, args.source_commit)
        report["launch"] = {"status": "failed"}
        session = session_factory(exe, profile, tag)
        report["launch"] = {"status": "started", "pid": session.pid}
        deadline, shell = time.monotonic() + 120, None
        while time.monotonic() < deadline:
            require(session.alive(), "Owned process exited before inspection")
            shell = select_shell(call(args.lowlevel_cli, "list_windows", include_empty_titles=True)["windows"],
                                 session.pid, session.identity)
            if shell:
                break
            time.sleep(0.5)
        require(shell is not None, "Initial shell was not observed within 120 seconds")
        report["launch"]["shell"] = shell
        report["probe"] = {"status": "failed"}
        probe_path = root / "shell.jsonl"
        require(not probe_path.exists(), "Probe path must be new")
        require(session.identity(shell["hwnd"]) == {k: shell[k] for k in ("pid", "class", "visible")}, "Window ownership changed")
        subprocess.run([sys.executable, str(HERE / "send-layout-probe.py"), str(shell["hwnd"]), str(probe_path), "--timeout", "10"],
                       capture_output=True, timeout=15, check=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        end = time.monotonic() + 10
        while True:
            regular(probe_path)
            require(probe_path.stat().st_size <= 16 * 1024 * 1024, "Probe exceeds bound")
            try:
                rows = [json.loads(line) for line in probe_path.read_text(encoding="utf-8").splitlines()]
                if rows and rows[-1] == {"kind": "end"}:
                    break
            except json.JSONDecodeError:
                pass
            require(time.monotonic() < end, "Native probe did not complete")
            time.sleep(0.1)
        geometry = validate_probe(rows, session.pid, shell["hwnd"], tag)
        report["probe"] = {"status": "received", "sha256": digest(probe_path), **geometry}
        if args.capture:
            report["screenshot"] = {"status": "failed"}
            require(session.alive() and session.identity(shell["hwnd"]) ==
                    {k: shell[k] for k in ("pid", "class", "visible")}, "Capture ownership changed")
            target = root / "shell.png"
            reply = call(args.lowlevel_cli, "screenshot", hwnd=shell["hwnd"], client_only=True, output_path=str(target))
            report["screenshot"] = validate_image(target, reply, shell["hwnd"], geometry["client"])
        validate_receipt(receipt, args.producer, args.source_commit)
    except Exception as exc:
        report["failure"] = str(exc)
        if isinstance(exc, LaunchFailure):
            report["teardown"] = exc.teardown
    finally:
        if session is not None:
            try:
                report["teardown"] = "verified" if session.close() else "unverified"
            except Exception:
                report["teardown"] = "unverified"
        (root / "review.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def run_worker(request):
    root = request.parent
    require(root.parent.resolve() == Path(tempfile.gettempdir()).resolve() and
            root.name.startswith("bambu-local-review-") and request.name == "request.json",
            "Worker request is outside its temporary owner")
    data = read_json(request)
    require(data.get("desktop") == "visible" and data.get("execute") is True,
            "Worker lacks explicit visible-desktop selection")
    require(type(data.get("capture")) is bool, "Invalid screenshot selection")
    require(timedelta(0) <= datetime.now(timezone.utc) - timestamp(data["requestedAtUtc"]) <= timedelta(seconds=30),
            "Worker request is stale")
    require(not (root / "profile").exists() and not (root / "stop").exists(), "Worker request was already used or stopped")
    with (root / "worker.claim").open("x", encoding="ascii") as claim:
        claim.write(str(os.getpid()))
    args = argparse.Namespace(**{key: data[key] for key in ("source_commit", "capture")},
        **{key: Path(data[key]) for key in ("producer", "build_receipt", "lowlevel_cli")})
    finished = threading.Event()

    def watchdog():
        deadline = time.monotonic() + WORKER_SECONDS
        while not finished.wait(0.1):
            if (root / "stop").exists() or time.monotonic() >= deadline:
                # OS handle closure terminates this worker's unnamed job only.
                # No post-mortem verification is inferred from that containment.
                os._exit(124)

    timer = threading.Thread(target=watchdog, daemon=True)
    timer.start()
    try:
        receipt = read_json(args.build_receipt)
        exe = validate_receipt(receipt, args.producer, args.source_commit)
        report = inspect_shell(args, receipt, root, exe)
        return 0 if "failure" not in report and report["teardown"] == "verified" else 1
    finally:
        finished.set()


def dispatch_worker(args, root, call=cheap):
    request = root / "request.json"
    data = {key: str(getattr(args, key)) for key in ("producer", "build_receipt", "source_commit", "lowlevel_cli")}
    data.update(desktop=args.desktop, execute=True, capture=args.capture,
                requestedAtUtc=datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"))
    request.write_text(json.dumps(data), encoding="utf-8")
    try:
        result = call(args.lowlevel_cli, "run_command", command=worker_command(request), shell=True,
                      cwd=str(HERE), timeout=TOOL_SECONDS)
        require(result.get("timed_out") is False and type(result.get("returncode")) is int,
                "Worker invocation did not return a terminal result")
        require((root / "review.json").is_file(), "Worker ended without inspection evidence")
        report = read_json(root / "review.json", 16 * 1024 * 1024)
        require(report.get("sourceCommit") == args.source_commit, "Worker source receipt mismatch")
        require(report.get("kind") == "local-initial-shell" and
                all(isinstance(report.get(key), dict) and isinstance(report[key].get("status"), str)
                    for key in ("launch", "probe", "screenshot")) and
                report.get("teardown") in ("verified", "unverified", "not_attempted"),
                "Worker verdict is incomplete")
        report["workerExitCode"] = result["returncode"]
        return report
    except BaseException:
        # An interrupted wrapper cannot claim that its contained worker ended.
        # Request its bounded watchdog exit, retain evidence, never kill by name.
        (root / "stop").write_text("stop", encoding="ascii")
        report = {"schemaVersion": 1, "kind": "local-initial-shell", "sourceCommit": args.source_commit,
            "launch": {"status": "unverified"}, "probe": {"status": "unverified"},
            "screenshot": {"status": "unverified"}, "teardown": "unverified",
            "runtimeAcceptance": "unverified", "failure": "Worker interrupted, timed out or lacked valid evidence"}
        (root / "wrapper-failure.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--build-receipt", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--lowlevel-cli", type=Path)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--desktop", choices=("visible",))
    parser.add_argument("--capture", action="store_true")
    args = parser.parse_args()
    require(args.producer.is_absolute(), "Producer path must be absolute")
    receipt = read_json(args.build_receipt)
    exe = validate_receipt(receipt, args.producer, args.source_commit)
    if not args.execute:
        require(not args.capture and args.desktop is None, "Live options require explicit execution")
        print(json.dumps({"provenance": "validated", "launch": "not_attempted", "probe": "not_attempted", "screenshot": "not_attempted"}))
        return 0
    require(args.desktop == "visible" and os.name == "nt", "Explicit Windows visible-desktop selection is required")
    require(args.lowlevel_cli is not None, "Explicit Lowlevel CLI path is required")
    regular(args.lowlevel_cli)
    if args.capture:
        from PIL import Image  # Check before launch, never install dependencies here.
    root = Path(tempfile.mkdtemp(prefix="bambu-local-review-"))
    report = dispatch_worker(args, root)
    print(json.dumps({"evidenceDirectory": str(root), "launch": report["launch"]["status"],
        "probe": report["probe"]["status"], "screenshot": report["screenshot"]["status"], "teardown": report["teardown"]}))
    return 0 if "failure" not in report and report["teardown"] == "verified" and report.get("workerExitCode") == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(run_worker(Path(sys.argv[2])) if len(sys.argv) == 3 and sys.argv[1] == "--worker" else main())
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as exc:
        print(json.dumps({"status": "blocked", "reason": str(exc), "launch": "not_attempted"}), file=sys.stderr)
        raise SystemExit(2)
