#!/usr/bin/env python3
"""Trace one verified installed executable without exposing debugger output."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

from hosted_process import owned_process_inventory, process_snapshot

_kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_kernel32.OpenProcess.argtypes = (ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong)
_kernel32.OpenProcess.restype = ctypes.c_void_p
_kernel32.GetExitCodeProcess.argtypes = (ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong))
_kernel32.GetExitCodeProcess.restype = ctypes.c_int
_kernel32.CloseHandle.argtypes = (ctypes.c_void_p,)
_kernel32.CloseHandle.restype = ctypes.c_int


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
        raise RuntimeError(f"{tool} did not succeed")
    return payload


def cdb_arguments(cdb: str, pid: int, script: str, log: str, symbols: str) -> list[str]:
    if pid <= 0:
        raise ValueError("An exact positive process ID is required")
    return [cdb, "-pd", "-y", symbols, "-logo", log, "-cf", script, "-p", str(pid)]


def process_exit_code(handle: int | None) -> int | None:
    if not handle:
        return None
    value = ctypes.c_ulong()
    if not _kernel32.GetExitCodeProcess(handle, ctypes.byref(value)):
        return None
    return None if value.value == 259 else int(value.value)


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("exe", "cdb", "install-receipt", "source-commit", "verification-commit", "tag", "output"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted":
        raise SystemExit("This trace requires a GitHub-hosted runner")
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
    }
    created = False
    launch_pid = None
    launched_at = None
    handle = None
    debugger = None
    stage = "create_desktop"
    try:
        cheap("create_headless_desktop", name=desktop)
        created = True
        launched_at = datetime.now(timezone.utc)
        stage = "launch_installed_executable"
        launch_pid = int(cheap("launch_on_headless_desktop", name=desktop,
            command=f'"{exe}" --datadir "{profile}"')["pid"])
        handle = _kernel32.OpenProcess(0x101000, False, launch_pid)
        if not handle:
            raise RuntimeError("Cannot retain the launched process handle")
        stage = "revalidate_exact_process"
        expected = None
        for _ in range(12):
            inventory = owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
                launched_at=launched_at, launch_pid=launch_pid)
            expected = next((row for row in inventory if row["pid"] == launch_pid and row["launch_pid"]), None)
            if expected:
                break
            time.sleep(0.25)
        if not expected or process_exit_code(handle) is not None:
            raise RuntimeError("The launched process exited or failed exact ownership validation")
        report["pid_revalidated"] = True
        report["launch_creation_utc"] = expected["created_at_utc"]
        # Recheck immediately before attaching; the retained handle also keeps
        # the original process object distinct from later PID reuse.
        stage = "attach_exact_pid"
        inventory = owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
            launched_at=launched_at, launch_pid=launch_pid)
        if not any(row["pid"] == launch_pid and row["created_at_utc"] == expected["created_at_utc"] for row in inventory):
            raise RuntimeError("Process identity changed before debugger attachment")
        debugger = subprocess.Popen(cdb_arguments(str(cdb), launch_pid, str(script), str(raw_log), str(symbols)),
            stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        stage = "observe_debugger_and_exit"
        deadline = time.monotonic() + 65
        while time.monotonic() < deadline and debugger.poll() is None:
            if process_exit_code(handle) is not None:
                break
            time.sleep(0.5)
        report["natural_exit_code"] = process_exit_code(handle)
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
            except subprocess.TimeoutExpired:
                report["debugger_detach_status"] = "quit_command_timed_out"
                debugger.kill()
                debugger.wait(timeout=8)
                report["debugger_detach_status"] = "debugger_stopped_with_pd_protection"
        if created:
            cleanup_errors = []
            try:
                if launch_pid is not None and launched_at is not None:
                    owned = owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
                        launched_at=launched_at, launch_pid=launch_pid)
                    for row in owned:
                        cheap("kill_process", pid=row["pid"], force=True)
                    remaining = owned_process_inventory(process_snapshot(), exe=str(exe), datadir=str(profile),
                        launched_at=launched_at, launch_pid=launch_pid)
                    if remaining:
                        raise RuntimeError("Owned process remained after teardown")
            except Exception as exc:
                cleanup_errors.append(type(exc).__name__)
            try:
                cheap("close_headless_desktop", name=desktop)
            except Exception as exc:
                cleanup_errors.append(type(exc).__name__)
            report["owned_process_cleanup"] = "failed" if cleanup_errors else "verified"
            if cleanup_errors:
                report["cleanup_failure_type"] = cleanup_errors
        if handle:
            _kernel32.CloseHandle(handle)
        if raw_log.is_file():
            if 0 < raw_log.stat().st_size <= 10_000_000:
                report["restricted_logs"] = [{"file": raw_log.name, "bytes": raw_log.stat().st_size,
                    "sha256": sha256(raw_log)}]
                raw = raw_log.read_text(encoding="utf-8", errors="replace")
                report["debugger_attach_marker"] = "TRACE_ATTACH" in raw
                report["exit_breakpoint_marker"] = (
                    "TRACE_RTL_EXIT" in raw or "TRACE_EXIT_PROCESS" in raw)
                report["exception_marker"] = "TRACE_EXCEPTION_80070057" in raw
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
