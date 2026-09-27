#!/usr/bin/env python3
"""Trace one verified installed executable without exposing debugger output."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

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
    return f"OpenDesktopW('{desktop}')" in detail and "GetLastError=2" in detail


def teardown_verified(*, owned_absent: bool, desktop_absent: bool,
                      holder_verified: bool, cleanup_errors: list[str]) -> bool:
    return owned_absent and desktop_absent and holder_verified and not cleanup_errors


def cdb_arguments(cdb: str, pid: int, script: str, log: str, symbols: str) -> list[str]:
    if pid <= 0:
        raise ValueError("An exact positive process ID is required")
    return [cdb, "-pd", "-y", symbols, "-logo", log, "-cf", script, "-p", str(pid)]


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
            except subprocess.TimeoutExpired:
                report["debugger_detach_status"] = "quit_command_timed_out"
                try:
                    debugger.kill()
                    debugger.wait(timeout=8)
                    report["debugger_detach_status"] = "debugger_stopped_with_pd_protection"
                except (OSError, subprocess.TimeoutExpired):
                    report["debugger_detach_status"] = "debugger_stop_unverified"
        if created:
            cleanup_errors = []
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
