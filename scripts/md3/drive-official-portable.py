#!/usr/bin/env python3
"""Observe a vendor portable build on a disposable hidden desktop.

This is process and window diagnosis. The vendor binary has no layout probe,
so a visible frame does not prove that the supplied 3MF was loaded.
"""
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
        raise RuntimeError(f"{tool} returned no readable JSON") from exc
    if result.returncode or payload.get("ok") is not True:
        raise RuntimeError(f"{tool} failed")
    return payload


def exit_code(handle: int | None) -> int | None:
    if not handle:
        return None
    value = ctypes.c_ulong()
    if not _kernel32.GetExitCodeProcess(handle, ctypes.byref(value)):
        return None
    return None if value.value == 259 else int(value.value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--datadir", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted":
        raise SystemExit("Vendor comparison requires a GitHub-hosted runner")
    exe, fixture, datadir = Path(args.exe), Path(args.fixture), Path(args.datadir)
    if not exe.is_file() or not fixture.is_file() or datadir.exists():
        raise SystemExit("The executable, fixture, or fresh profile preflight failed")
    datadir.mkdir(parents=True)
    desktop = "bambu-vendor-" + os.environ["GITHUB_RUN_ID"] + "-" + os.environ["GITHUB_RUN_ATTEMPT"]
    report = {
        "schema": 1,
        "method": "lowlevel-computer-use-cheap named hidden desktop",
        "runner": "github-hosted-windows",
        "run_id": os.environ["GITHUB_RUN_ID"],
        "run_attempt": os.environ["GITHUB_RUN_ATTEMPT"],
        "exe_sha256": sha256(exe),
        "fixture_sha256": sha256(fixture),
        "fixture_name": fixture.name,
        "file_open_attempted": False,
        "model_load_verified": False,
        "pixels_verified": False,
        "status": "not_started",
        "observations": [],
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    created = False
    launched_at = None
    launch_pid = None
    handle = None
    try:
        cheap("create_headless_desktop", name=desktop)
        created = True
        launched_at = datetime.now(timezone.utc)
        command = f'"{exe}" --datadir "{datadir}" "{fixture}"'
        launch_pid = int(cheap("launch_on_headless_desktop", name=desktop, command=command)["pid"])
        report["file_open_attempted"] = True
        handle = _kernel32.OpenProcess(0x101000, False, launch_pid)
        deadline = time.monotonic() + 120
        first_frame_at = None
        while time.monotonic() < deadline:
            processes = owned_process_inventory(process_snapshot(), exe=str(exe),
                datadir=str(datadir), launched_at=launched_at, launch_pid=launch_pid)
            windows = cheap("list_headless_windows", name=desktop)["windows"]
            owned_pids = {item["pid"] for item in processes}
            frames = [w for w in windows if int(w["process_id"]) in owned_pids
                      and w["class"] == "wxWindowNR" and w["width"] >= 800 and w["height"] >= 500]
            observation = {
                "elapsed_seconds": round((datetime.now(timezone.utc) - launched_at).total_seconds(), 1),
                "owned_process_count": len(processes),
                "owned_window_count": len(frames),
                "launch_exit_code": exit_code(handle),
                "fixture_name_in_window_title": any(fixture.stem.lower() in str(w.get("title", "")).lower() for w in frames),
            }
            if frames:
                observation["main_window_geometry"] = [int(frames[0]["width"]), int(frames[0]["height"])]
                first_frame_at = first_frame_at or time.monotonic()
            report["observations"].append(observation)
            if first_frame_at and time.monotonic() - first_frame_at >= 20:
                report["status"] = "owned_window_survived_file_open_attempt"
                break
            if not processes and observation["launch_exit_code"] is not None:
                report["status"] = "exited_after_file_open_attempt"
                break
            time.sleep(5)
        else:
            report["status"] = "window_or_liveness_timeout"
        report["model_load_limit"] = "No instrumented model-state probe exists in the vendor binary; process and window survival cannot prove 3MF load."
    except Exception as exc:
        report["status"] = "diagnostic_failed"
        report["failure_type"] = type(exc).__name__
    finally:
        if created:
            cleanup_errors = []
            try:
                if launch_pid is not None and launched_at is not None:
                    owned = owned_process_inventory(process_snapshot(), exe=str(exe),
                        datadir=str(datadir), launched_at=launched_at, launch_pid=launch_pid)
                    for item in owned:
                        cheap("kill_process", pid=item["pid"], force=True)
                    for _ in range(10):
                        remaining = owned_process_inventory(process_snapshot(), exe=str(exe),
                            datadir=str(datadir), launched_at=launched_at, launch_pid=launch_pid)
                        if not remaining:
                            break
                        time.sleep(1)
                    else:
                        raise RuntimeError("Owned application process remained after teardown")
            except Exception as exc:
                cleanup_errors.append(type(exc).__name__)
            try:
                cheap("close_headless_desktop", name=desktop)
            except Exception as exc:
                cleanup_errors.append(type(exc).__name__)
            report["owned_process_cleanup"] = "failed" if cleanup_errors else "completed"
            if cleanup_errors:
                report["cleanup_failure_type"] = cleanup_errors
        if handle:
            _kernel32.CloseHandle(handle)
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["status"] == "owned_window_survived_file_open_attempt" and report.get("owned_process_cleanup") == "completed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
