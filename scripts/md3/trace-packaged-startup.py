#!/usr/bin/env python3
"""Trace one verified installed executable without exposing debugger output."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import threading
import time
from datetime import datetime, timezone
from pathlib import Path, PureWindowsPath

from hosted_process import owned_process_inventory, process_snapshot

_behavior_spec = importlib.util.spec_from_file_location(
    "hosted_behavior_identity", Path(__file__).with_name("drive-packaged-behavior.py"))
if _behavior_spec is None or _behavior_spec.loader is None:
    raise RuntimeError("Existing hosted process identity checks are unavailable")
_behavior = importlib.util.module_from_spec(_behavior_spec)
_behavior_spec.loader.exec_module(_behavior)
OriginalProcessHandle = _behavior.OriginalProcessHandle
helper_python_executable = _behavior.helper_python_executable
validate_holder_receipt = _behavior.validate_holder_receipt


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def cheap(tool: str, **kwargs):
    args = [os.environ["LLCU_CHEAP"], tool]
    for key, value in kwargs.items():
        args += ["--" + key, value if isinstance(value, str) else json.dumps(value)]
    result = subprocess.run(args, capture_output=True, text=True, timeout=60, check=False)
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"{tool} did not return JSON") from exc
    if result.returncode or payload.get("ok") is not True:
        if tool == "list_headless_windows" and named_desktop_absent(payload, kwargs.get("name", "")):
            raise NamedDesktopAbsent("Named desktop is absent")
        raise RuntimeError(f"{tool} did not succeed")
    return payload


class NamedDesktopAbsent(RuntimeError):
    pass


def named_desktop_absent(payload: dict, desktop: str) -> bool:
    detail = json.dumps(payload, ensure_ascii=False)
    return (f"OpenDesktopW('{desktop}')" in detail
            and re.search(r"GetLastError=2(?!\d)", detail) is not None)


def emitted_breakpoint_with_stack(raw: str, marker: str) -> bool:
    """Require an emitted marker followed by a real CDB stack header and frame."""
    lines = raw.splitlines()
    for index, line in enumerate(lines):
        if line.strip() != marker:
            continue
        following = lines[index + 1:index + 81]
        for offset, candidate in enumerate(following):
            if "Child-SP" not in candidate:
                continue
            frames = following[offset + 1:offset + 31]
            if any(re.match(r"^\s*[0-9a-fA-F]{2,3}\s+[0-9a-fA-F`]+\s+[0-9a-fA-F`]+\s+", frame)
                   for frame in frames):
                return True
    return False


def teardown_verified(*, owned_absent: bool, desktop_absent: bool,
                      holder_verified: bool, debugger_stopped: bool,
                      cleanup_errors: list[str]) -> bool:
    return (owned_absent and desktop_absent and holder_verified
            and debugger_stopped and not cleanup_errors)


def cdb_arguments(cdb: str, pid: int, script: str, log: str, symbols: str) -> list[str]:
    if pid <= 0:
        raise ValueError("An exact positive process ID is required")
    return [cdb, "-pd", "-y", symbols, "-logo", log, "-cf", script, "-p", str(pid)]


def creation_arguments(cdb: str, exe: str, profile: str, script: str, symbols: str) -> list[str]:
    # The caller creates this private cache exclusively. Never accept a symbol
    # expression, UNC share or inherited search path as the cache argument.
    cache = PureWindowsPath(symbols)
    if (not cache.is_absolute() or len(cache.drive) != 2 or cache.drive[1] != ":"
            or any(ord(char) < 32 or char in '*;"' for char in symbols)
            or ".." in cache.parts):
        raise ValueError("A local absolute symbol cache is required")
    symbol_path = "srv*" + symbols + "*https://msdl.microsoft.com/download/symbols"
    return [cdb, "-sins", "-ses", "-y", symbol_path, "-cf", script, exe, "--datadir", profile]


def creation_commands() -> str:
    # Strict PDB matching is enforced by CDB, not an independent PDB hash check.
    # Keep the target stopped until the caller validates identity and readback.
    return ('.echo TRACE_CREATION_INITIAL\n.printf "TRACE_TARGET %u %u\\n", @$tpid, @$tid\n'
            '.symopt- 0x40\n.reload /f ntdll.dll\nlmv m ntdll\n'
            '!gflag +sls\n!gflag\n'
            'sxe -c ".echo TRACE_EXCEPTION; .lastevent; k 24; gn" av\n'
            'sxe -c ".echo TRACE_PROCESS_EXIT; .lastevent; q" epr\n')


def creation_acknowledgement(text: str):
    targets = re.findall(r"(?m)^TRACE_TARGET ([1-9]\d*) ([1-9]\d*)\s*$", text)
    if (text.splitlines().count("TRACE_CREATION_INITIAL") != 1 or len(targets) != 1
            or not re.search(r"(?im)^\s*sls\s*-\s*Show loader snaps\s*$", text)):
        return None
    return tuple(map(int, targets[0]))


def desktop_lookup_observation(observation, thread, get_desktop, get_information, last_error):
    """Observe native return boundaries without publishing names or handles."""
    from ctypes import wintypes
    observation["stage"] = "target_thread_desktop_handle"
    handle = get_desktop(thread)
    error = last_error() if not handle else None
    observation["desktop_handle_present"] = bool(handle)
    observation["desktop_handle_error"] = error
    if not handle:
        raise ValueError("Target desktop handle unavailable")
    name = ctypes.create_unicode_buffer(256)
    needed = wintypes.DWORD()
    observation["stage"] = "target_desktop_name"
    ok = get_information(handle, 2, name, ctypes.sizeof(name), ctypes.byref(needed))
    error = last_error() if not ok else None
    observation["desktop_information_success"] = bool(ok)
    observation["desktop_information_error"] = error
    observation["desktop_required_bytes"] = min(int(needed.value), 65536)
    observation["desktop_required_bytes_capped"] = needed.value > 65536
    if not ok:
        raise ValueError("Target desktop information unavailable")
    observation["stage"] = "target_desktop"
    return name.value


def thread_desktop_observation(observation, thread, expected_pid, open_thread, process_id,
                               exit_code, close_handle, last_error, desktop_name):
    """Hold the exact live target thread through its existing desktop observation."""
    from ctypes import wintypes
    observation["stage"] = "target_thread_open"
    handle = open_thread(0x0800, False, thread)  # THREAD_QUERY_LIMITED_INFORMATION
    error = last_error() if not handle else None
    observation["thread_handle_present"] = bool(handle)
    observation["thread_open_error"] = error
    if not handle:
        raise ValueError("Target thread unavailable")
    try:
        observation["stage"] = "target_thread_process"
        pid = process_id(handle)
        error = last_error() if not pid else None
        observation["thread_process_query_success"] = bool(pid)
        observation["thread_process_error"] = error
        if not pid:
            raise ValueError("Target thread process unavailable")
        observation["thread_process_matches"] = pid == expected_pid
        if not observation["thread_process_matches"]:
            raise ValueError("Target thread process mismatch")
        observation["stage"] = "target_thread_exit_state"
        code = wintypes.DWORD()
        ok = exit_code(handle, ctypes.byref(code))
        error = last_error() if not ok else None
        observation["thread_exit_query_success"] = bool(ok)
        observation["thread_exit_error"] = error
        if not ok:
            raise ValueError("Target thread exit state unavailable")
        observation["thread_alive"] = code.value == 259  # STILL_ACTIVE
        if not observation["thread_alive"]:
            raise ValueError("Target thread is not active")
        return desktop_name(thread)
    finally:
        ok = close_handle(handle)
        error = last_error() if not ok else None
        observation["thread_handle_closed"] = bool(ok)
        observation["thread_close_error"] = error
        if not ok:
            observation["stage"] = "target_thread_close"
            raise ValueError("Target thread handle cleanup unavailable")


def creation_ownership_observation(observation, target, debugger_pid, inventory, member,
                                   desktop_name, expected_desktop):
    """Preserve short-circuit ownership checks, recording no native identities."""
    observation["stage"] = "inventory_query"
    rows = inventory()
    observation["inventory_count"] = len(rows)
    observation["stage"] = "inventory_cardinality"
    if len(rows) != 1:
        return False
    observation["stage"] = "target_identity"
    observation["target_pid_matches"] = rows[0]["pid"] == target[0]
    if not observation["target_pid_matches"]:
        return False
    observation["stage"] = "debugger_membership"
    observation["debugger_job_member"] = bool(member(debugger_pid))
    if not observation["debugger_job_member"]:
        return False
    observation["stage"] = "target_membership"
    observation["target_job_member"] = bool(member(rows[0]["pid"]))
    if not observation["target_job_member"]:
        return False
    observation["stage"] = "target_desktop"
    observation["target_desktop_matches"] = desktop_name(target[1]) == expected_desktop
    if not observation["target_desktop_matches"]:
        return False
    observation["stage"] = "ownership_verified"
    return True


def creation_trace(args) -> int:
    """Fixed instrumented startup, supervised externally by a nonbreakaway job."""
    from ctypes import wintypes
    if (args.tag != "md3-v190" or args.source_commit != "35d1074faea221fa4f289f1db1e0ee428a90d701"
            or not re.fullmatch(r"Local\\BambuNativeScale-[0-9a-f]{64}", args.job_name or "")
            or not re.fullmatch(r"startup-loader-[0-9]+-[0-9]+", args.desktop or "")):
        raise ValueError("Unsupported loader diagnostic identity")
    exe, cdb, output = Path(args.exe), Path(args.cdb), Path(args.output)
    install = json.loads(Path(args.install_receipt).read_text(encoding="utf-8-sig"))
    if (install.get("status") != "verified" or install.get("source_commit") != args.source_commit
            or install.get("release_tag") != args.tag or sha256(exe) != install.get("installed_exe_sha256")):
        raise ValueError("Installed diagnostic identity mismatch")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Diagnostic output must be fresh")
    scratch = output / "private-work"
    scratch.mkdir()
    profile, symbols, temp = (scratch / name for name in ("profile", "symbols", "temp"))
    for path in (profile, symbols, temp):
        path.mkdir()
    commands = scratch / "initial.txt"
    # No software breakpoint, registry mutation or arbitrary command input.
    commands.write_text(creation_commands(), encoding="ascii")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    user = ctypes.WinDLL("user32", use_last_error=True)
    kernel.OpenJobObjectW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
    kernel.OpenJobObjectW.restype = wintypes.HANDLE
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.IsProcessInJob.argtypes = [wintypes.HANDLE, wintypes.HANDLE, ctypes.POINTER(wintypes.BOOL)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    kernel.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenThread.restype = wintypes.HANDLE
    kernel.GetProcessIdOfThread.argtypes = [wintypes.HANDLE]
    kernel.GetProcessIdOfThread.restype = wintypes.DWORD
    kernel.GetExitCodeThread.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetExitCodeThread.restype = wintypes.BOOL
    user.GetThreadDesktop.argtypes = [wintypes.DWORD]
    user.GetThreadDesktop.restype = wintypes.HANDLE
    user.GetUserObjectInformationW.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p,
                                               wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    def member(pid):
        handle = kernel.OpenProcess(0x1000, False, pid)
        try:
            yes = wintypes.BOOL()
            return bool(handle and kernel.IsProcessInJob(handle, job, ctypes.byref(yes)) and yes.value)
        finally:
            if handle:
                kernel.CloseHandle(handle)
    def desktop_name(thread=None):
        if thread is not None:
            return desktop_lookup_observation(observation, thread, user.GetThreadDesktop,
                user.GetUserObjectInformationW, ctypes.get_last_error)
        name = ctypes.create_unicode_buffer(256)
        needed = wintypes.DWORD()
        if not user.GetUserObjectInformationW(user.GetThreadDesktop(thread or kernel.GetCurrentThreadId()),
                2, name, ctypes.sizeof(name), ctypes.byref(needed)):
            raise ValueError("Worker desktop unavailable")
        return name.value
    job = kernel.OpenJobObjectW(4, False, args.job_name)
    debugger = None
    data = bytearray()
    lock = threading.Lock()
    overflow = threading.Event()
    report = {"schema": 1, "scope": "diagnostic", "verdict": "blocked",
        "source_commit": args.source_commit, "verification_commit": args.verification_commit,
        "release_tag": args.tag, "hosted_run_id": os.environ["GITHUB_RUN_ID"],
        "installed_exe_sha256": sha256(exe), "debugger_sha256": sha256(cdb),
        "requested_tuple": {"language": "en", "theme": "light", "scale": 1, "viewport": [1200,800]},
        "measured_tuple": None, "images": [], "restricted_logs": [],
        "execution_class": "instrumented_from_creation_separate_from_baseline",
        "symbol_route": "fixed_microsoft_server_fresh_local_cache",
        "symbol_identity": "debugger_enforced_exact_matching_not_independent_pdb_hash",
        "status": "unavailable", "initial_marker_observed": False, "job_membership_verified": False}
    observation = {"stage": "worker_containment", "acknowledgement_observed": False,
        "observed_output_bytes": 0, "inventory_count": None, "target_pid_matches": None,
        "debugger_job_member": None, "target_job_member": None, "target_desktop_matches": None,
          "continuation_written": False, "desktop_handle_present": None,
          "desktop_handle_error": None, "desktop_information_success": None,
          "desktop_information_error": None, "desktop_required_bytes": None,
          "desktop_required_bytes_capped": None,
          "thread_handle_present": None, "thread_open_error": None,
          "thread_process_query_success": None, "thread_process_error": None,
          "thread_process_matches": None, "thread_exit_query_success": None,
          "thread_exit_error": None, "thread_alive": None,
          "thread_handle_closed": None, "thread_close_error": None}
    report["creation_observation"] = observation
    try:
        if not job or not member(os.getpid()) or desktop_name() != args.desktop:
            raise ValueError("Worker containment unavailable")
        env = {key: value for key, value in os.environ.items() if key.upper() not in {"TEMP","TMP"}}
        env.update(TEMP=str(temp), TMP=str(temp))
        launched = datetime.now(timezone.utc)
        arguments = creation_arguments(str(cdb), str(exe), str(profile), str(commands), str(symbols))
        command_line = subprocess.list2cmdline(arguments[:-3]) + f' "{exe}" --datadir "{profile}"'
        observation["stage"] = "debugger_launch"
        debugger = subprocess.Popen(command_line,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd=exe.parent, env=env, creationflags=subprocess.CREATE_NO_WINDOW)
        def drain():
            try:
                while True:
                    block = debugger.stdout.read1(4096)
                    if not block:
                        break
                    with lock:
                        if len(data) + len(block) > 1048576:
                            overflow.set()
                            return
                        data.extend(block)
            except Exception:
                overflow.set()
        reader = threading.Thread(target=drain, daemon=True)
        reader.start()
        deadline = time.monotonic() + 90
        observation["stage"] = "await_acknowledgement"
        while time.monotonic() < deadline and debugger.poll() is None and not overflow.is_set():
            with lock:
                observation["observed_output_bytes"] = len(data)
                text = bytes(data).decode("utf-8", errors="replace")
            target = creation_acknowledgement(text)
            if not report["initial_marker_observed"] and target:
                observation["acknowledgement_observed"] = True
                if not creation_ownership_observation(observation, target, debugger.pid,
                        lambda: owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
                            launched_at=launched, launch_pid=debugger.pid), member,
                        lambda thread: thread_desktop_observation(observation, thread, target[0],
                            kernel.OpenThread, kernel.GetProcessIdOfThread, kernel.GetExitCodeThread,
                            kernel.CloseHandle, ctypes.get_last_error, desktop_name), args.desktop):
                    raise ValueError("Debug target ownership unavailable")
                report["initial_marker_observed"] = True
                report["job_membership_verified"] = True
                observation["stage"] = "continuation_write"
                debugger.stdin.write(b"g\n")
                observation["stage"] = "continuation_flush"
                debugger.stdin.flush()
                observation["continuation_written"] = True
                observation["stage"] = "await_debugger_exit"
            time.sleep(0.1)
        if debugger.poll() is None:
            observation["stage"] = "output_limit" if overflow.is_set() else "debugger_deadline"
            raise ValueError("Debugger deadline or output limit")
        observation["stage"] = "output_drain"
        reader.join(timeout=3)
        if reader.is_alive() or overflow.is_set():
            raise ValueError("Debugger output incomplete")
        report["debugger_exit_code"] = debugger.returncode
        report["status"] = "creation_trace_collected" if report["initial_marker_observed"] else "initial_observation_unavailable"
        observation["stage"] = "debugger_exited"
    except Exception:
        report["status"] = "unavailable"
    finally:
        if debugger and debugger.poll() is None:
            debugger.kill()
            try:
                debugger.wait(timeout=3)
            except subprocess.TimeoutExpired:
                pass
        report["debugger_stop_verified"] = debugger is None or debugger.poll() is not None
        report["output_overflow"] = overflow.is_set()
        # The supervisor must prove the entire job empty and desktop closed
        # before moving these bounded bytes into the encrypted transport.
        if report["debugger_stop_verified"] and not overflow.is_set() and data:
            restricted = output / "restricted-logs"
            restricted.mkdir()
            raw = restricted / "cdb-loader.log"
            with lock:
                raw.write_bytes(bytes(data))
            report["restricted_logs"] = [{"file": raw.name, "bytes": raw.stat().st_size, "sha256": sha256(raw)}]
        if job:
            kernel.CloseHandle(job)
        (output / "behavior-report.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    return 0 if report["status"] == "creation_trace_collected" else 2


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("exe", "cdb", "install-receipt", "source-commit", "verification-commit", "tag", "output"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--from-creation", action="store_true")
    parser.add_argument("--job-name")
    parser.add_argument("--desktop")
    args = parser.parse_args()
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted":
        raise SystemExit("This trace requires a GitHub-hosted runner")
    if args.from_creation:
        return creation_trace(args)
    exe, cdb, output = Path(args.exe), Path(args.cdb), Path(args.output)
    install = json.loads(Path(args.install_receipt).read_text(encoding="utf-8-sig"))
    if (install.get("status") != "verified" or install.get("source_commit") != args.source_commit.lower()
            or install.get("release_tag") != args.tag or sha256(exe) != install.get("installed_exe_sha256")):
        raise SystemExit("Installed release identity differs from the verified receipt")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise SystemExit("Startup trace output directory is not empty")
    restricted = output / "restricted-logs"
    restricted.mkdir()
    raw_log = restricted / "cdb-startup.log"
    symbols = Path(os.environ["RUNNER_TEMP"]) / ("startup-empty-symbols-" + os.environ["GITHUB_RUN_ID"])
    symbols.mkdir(exist_ok=False)
    script = Path(os.environ["RUNNER_TEMP"]) / ("cdb-startup-" + os.environ["GITHUB_RUN_ID"] + ".txt")
    script.write_text(
        '.echo TRACE_ATTACH\n.lastevent\nlm\n~*k 12\n'
        'bu ntdll!RtlExitUserProcess ".echo TRACE_RTL_EXIT; k 24; lm; gc"\n'
        'bu kernel32!ExitProcess ".echo TRACE_EXIT_PROCESS; k 24; gc"\n'
        'sxe -c ".echo TRACE_EXCEPTION_80070057; .lastevent; k 24; gn" 80070057\n'
        'g\n', encoding="ascii")
    profile = Path(os.environ["RUNNER_TEMP"]) / ("startup-profile-" + os.environ["GITHUB_RUN_ID"])
    profile.mkdir(exist_ok=False)
    desktop = "startup-trace-" + os.environ["GITHUB_RUN_ID"] + "-" + os.environ["GITHUB_RUN_ATTEMPT"]
    report = {
        "schema": 1, "scope": "diagnostic", "verdict": "blocked",
        "source_commit": args.source_commit.lower(), "verification_commit": args.verification_commit.lower(),
        "release_tag": args.tag, "hosted_run_id": os.environ["GITHUB_RUN_ID"],
        "installed_exe_sha256": sha256(exe),
        "requested_tuple": {"language": "en", "theme": "light", "scale": 1, "viewport": [1200, 800]},
        "measured_tuple": None, "images": [], "restricted_logs": [],
        "execution_class": "instrumented_startup_separate_from_baseline",
        "symbols": "empty_local_path; matching_project_pdb_unavailable", "status": "not_started",
        "pid_revalidated": False, "owned_process_cleanup": "not_started",
        "named_desktop_closed_verified": False,
    }
    created = False
    launch_pid = None
    launched_at = None
    handle = None
    helper_pid = None
    holder_receipt = output.parent / "startup-holder-receipt.json"
    holder_stop = output.parent / "startup-holder-stop.txt"
    holder_stdout = output.parent / "startup-holder-stdout.log"
    holder_stderr = output.parent / "startup-holder-stderr.log"
    debugger = None
    stage = "create_desktop"
    try:
        cheap("create_headless_desktop", name=desktop)
        created = True
        stage = "launch_retained_desktop_holder"
        helper_python = helper_python_executable()
        helper_hash = sha256(helper_python)
        holder = Path(__file__).with_name("hosted_launch_holder.py")
        command = (f'"{helper_python}" "{holder}" --exe "{exe}" '
                   f'--datadir "{profile}" --desktop "{desktop}" '
                   f'--receipt "{holder_receipt}" --stop "{holder_stop}" '
                   f'--stdout "{holder_stdout}" --stderr "{holder_stderr}" --timeout 120')
        helper_pid = int(cheap("launch_on_headless_desktop", name=desktop, command=command)["pid"])
        deadline = time.monotonic() + 20
        receipt = None
        while time.monotonic() < deadline:
            if holder_receipt.is_file():
                try:
                    receipt = json.loads(holder_receipt.read_text(encoding="utf-8"))
                    launch_pid, launched_at = validate_holder_receipt(
                        receipt, helper_pid=helper_pid, exe_hash=sha256(exe),
                        helper_exe_hash=helper_hash, datadir=str(profile), desktop=desktop)
                    break
                except (OSError, json.JSONDecodeError):
                    pass
            time.sleep(0.2)
        if launch_pid is None:
            raise RuntimeError("Retained desktop holder produced no valid launch receipt")
        report["holder_receipt_status"] = receipt["status"]
        stage = "revalidate_exact_process"
        expected = None
        for _ in range(12):
            inventory = owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
                launched_at=launched_at, launch_pid=launch_pid)
            expected = next((row for row in inventory if row["pid"] == launch_pid and row["launch_pid"]), None)
            if expected:
                break
            time.sleep(0.25)
        if not expected:
            raise RuntimeError("The launched process exited or failed exact ownership validation")
        handle = OriginalProcessHandle(launch_pid, exe,
            datetime.fromisoformat(expected["created_at_utc"]))
        if handle.exit_code() is not None:
            raise RuntimeError("The launched process exited before debugger attachment")
        cheap("list_headless_windows", name=desktop)
        report["pid_revalidated"] = True
        report["launch_creation_utc"] = expected["created_at_utc"]
        # Check WMI and the retained kernel handle immediately before attaching.
        stage = "attach_exact_pid"
        inventory = owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
            launched_at=launched_at, launch_pid=launch_pid)
        if (not any(row["pid"] == launch_pid and row["created_at_utc"] == expected["created_at_utc"]
                    for row in inventory) or handle.exit_code() is not None):
            raise RuntimeError("Process identity changed before debugger attachment")
        debugger = subprocess.Popen(cdb_arguments(str(cdb), launch_pid, str(script), str(raw_log), str(symbols)),
            stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        stage = "observe_debugger_and_exit"
        deadline = time.monotonic() + 65
        while time.monotonic() < deadline and debugger.poll() is None:
            if handle.exit_code() is not None:
                break
            time.sleep(0.5)
        report["natural_exit_code"] = handle.exit_code()
        report["debugger_exit_code"] = debugger.poll()
        report["status"] = "process_exited_during_trace" if report["natural_exit_code"] is not None else "trace_timeout_or_process_live"
    except Exception as exc:
        report["status"] = "trace_failed"
        report["failure_stage"] = stage
        report["failure_type"] = type(exc).__name__
    finally:
        if debugger and debugger.poll() is None:
            try:
                debugger.communicate(input="qd\n", timeout=8)
            except (OSError, subprocess.TimeoutExpired):
                report["debugger_detach_status"] = "quit_command_timed_out"
                try:
                    debugger.kill()
                    debugger.wait(timeout=8)
                    report["debugger_detach_status"] = "debugger_stopped_with_pd_protection"
                except (OSError, subprocess.TimeoutExpired):
                    report["debugger_detach_status"] = "debugger_stop_unverified"
        report["debugger_stop_verified"] = debugger is None or debugger.poll() is not None
        if created:
            cleanup_errors = []
            if not report["debugger_stop_verified"]:
                cleanup_errors.append("debugger_stop_unverified")
            try:
                if launch_pid is not None and launched_at is not None:
                    owned = owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
                        launched_at=launched_at, launch_pid=launch_pid)
                    for row in owned:
                        cheap("kill_process", pid=row["pid"], force=True)
            except Exception as exc:
                cleanup_errors.append(type(exc).__name__)
            if helper_pid is not None:
                try:
                    holder_stop.write_text("stop\n", encoding="ascii")
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        if holder_receipt.is_file():
                            receipt = json.loads(holder_receipt.read_text(encoding="utf-8"))
                            if receipt.get("helper_pid") == helper_pid and receipt.get("holder_finished_at_utc"):
                                report["holder_teardown_verified"] = (
                                    receipt.get("status") == "holder_stopped"
                                    and receipt.get("app_exit_confirmed") is True
                                    and receipt.get("deadline_fired") is False)
                                break
                        time.sleep(0.25)
                    if not report.get("holder_teardown_verified"):
                        cleanup_errors.append("holder_teardown_unverified")
                except (OSError, ValueError, json.JSONDecodeError) as exc:
                    cleanup_errors.append(type(exc).__name__)
            try:
                cheap("close_headless_desktop", name=desktop)
            except Exception as exc:
                cleanup_errors.append(type(exc).__name__)
            # A fresh process snapshot and an exact missing-desktop error are
            # required. A successful close request alone is not teardown proof.
            deadline = time.monotonic() + 5
            owned_absent = False
            desktop_absent = False
            while time.monotonic() < deadline:
                try:
                    remaining = owned_process_inventory(process_snapshot(), exe=str(exe),
                        datadir=str(profile), launched_at=launched_at, launch_pid=launch_pid)
                    owned_absent = launch_pid is not None and not remaining
                except Exception:
                    owned_absent = False
                try:
                    cheap("list_headless_windows", name=desktop)
                    desktop_absent = False
                except NamedDesktopAbsent:
                    desktop_absent = True
                except Exception:
                    desktop_absent = False
                if owned_absent and desktop_absent:
                    break
                time.sleep(0.25)
            report["named_desktop_closed_verified"] = desktop_absent
            report["owned_process_cleanup"] = ("verified" if teardown_verified(
                owned_absent=owned_absent, desktop_absent=desktop_absent,
                holder_verified=report.get("holder_teardown_verified") is True,
                debugger_stopped=report["debugger_stop_verified"],
                cleanup_errors=cleanup_errors) else "failed")
            if cleanup_errors:
                report["cleanup_failure_type"] = cleanup_errors
        if handle:
            handle.close()
        if raw_log.is_file():
            if 0 < raw_log.stat().st_size <= 10_000_000:
                report["restricted_logs"] = [{"file": raw_log.name, "bytes": raw_log.stat().st_size,
                    "sha256": sha256(raw_log)}]
                raw = raw_log.read_text(encoding="utf-8", errors="replace")
                report["debugger_attach_marker"] = any(
                    line.strip() == "TRACE_ATTACH" for line in raw.splitlines())
                report["exit_breakpoint_marker"] = (
                    emitted_breakpoint_with_stack(raw, "TRACE_RTL_EXIT")
                    or emitted_breakpoint_with_stack(raw, "TRACE_EXIT_PROCESS"))
                report["exception_marker"] = emitted_breakpoint_with_stack(
                    raw, "TRACE_EXCEPTION_80070057")
            else:
                raw_log.unlink()
                report["raw_debugger_output_limit"] = "empty_or_over_10_mb_excluded"
        if report["status"] == "process_exited_during_trace":
            report["status"] = ("exact_pid_trace_finished" if report.get("debugger_attach_marker")
                and report.get("exit_breakpoint_marker") else "process_exited_without_complete_debugger_trace")
        report["raw_debugger_output"] = "restricted_encrypted_transport_only"
        (output / "behavior-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["status"] == "exact_pid_trace_finished" and report["owned_process_cleanup"] == "verified" else 1


if __name__ == "__main__":
    raise SystemExit(main())
