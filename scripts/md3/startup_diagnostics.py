"""Preserve bounded startup diagnostics only inside encrypted runtime evidence."""
from __future__ import annotations

import base64
import hashlib
import json
import ntpath
import os
import re
from pathlib import Path
import stat
from datetime import datetime, timezone

VERIFIER_FILES = (
    ".github/workflows/hosted-startup-diagnostic.yml",
    "scripts/ci/Verify-HostedNativeInterface.ps1",
    "scripts/ci/Verify-HostedSquirrelInstall.ps1",
    "scripts/md3/drive-native-interface.py", "scripts/md3/startup_diagnostics.py",
    "scripts/md3/drive-packaged-behavior.py", "scripts/md3/hosted_launch_holder.py",
    "scripts/md3/hosted_process.py", "scripts/md3/behavior_contract.py",
    "scripts/md3/recapture.py", "scripts/md3/hosted-automation-public-v1.pem",
)


def verifier_binding(source: str) -> dict:
    root = Path(__file__).resolve().parents[2]
    files = {}
    for name in VERIFIER_FILES:
        # Normalize checkout line endings so independent Git-object manifests
        # and Windows checkouts identify the same committed source bytes.
        data = _read(root / name, 2097152).replace(b"\r\n", b"\n")
        files[name] = hashlib.sha256(data).hexdigest()
    return {"source_commit": source, "hash_format": "sha256-lf-v1", "files": files}


def _hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1048576), b""):
            digest.update(block)
    return digest.hexdigest()


def _read(path: Path, limit: int) -> bytes:
    for ancestor in (path, *path.parents):
        info = ancestor.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError("Unsafe diagnostic path")
    with path.open("rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            raise ValueError("Diagnostic size exceeds bound")
        data = stream.read(limit + 1)
        if len(data) > limit or len(data) != info.st_size:
            raise ValueError("Diagnostic changed during read")
    return data


def _profile_logs(app, root: Path) -> dict:
    """Read only the verified launch profile's immediate native log directory."""
    try:
        # HostedApp names its exact launch profile datadir, not profile_path.
        profile = Path(app.datadir)
        if not profile.is_absolute():
            raise ValueError("Profile must be absolute")
        for ancestor in (profile, *profile.parents):
            info = ancestor.lstat()
            if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
                raise ValueError("Unsafe profile path")
        if not profile.resolve().is_relative_to(root) or profile.resolve() == root:
            raise ValueError("Profile escapes temporary root")
        directory = profile / "log"
        try:
            info = directory.lstat()
        except FileNotFoundError:
            return {"status": "absent", "files": []}
        if (not stat.S_ISDIR(info.st_mode) or stat.S_ISLNK(info.st_mode)
                or getattr(info, "st_file_attributes", 0) & 0x400):
            raise ValueError("Unsafe log directory")
        # Exact native filename shapes from LogSink.cpp and BaseException.cpp.
        stamp = r"[A-Za-z]{3}_[A-Za-z]{3}_\d{2}_\d{2}_\d{2}_\d{2}_"
        studio = re.compile(r"studio_" + stamp + re.escape(str(app.launch_pid))
                            + r"(?:_enc(?:_cn|_dc)?)?\.log\.\d{1,10}")
        crash = re.compile(r"crash_" + stamp + r"\d{1,10}\.log")
        candidates = []
        with os.scandir(directory) as entries:
            for index, entry in enumerate(entries):
                if index >= 64:
                    raise ValueError("Log enumeration bound exceeded")
                if studio.fullmatch(entry.name) or crash.fullmatch(entry.name):
                    candidates.append(Path(entry.path))
                    if len(candidates) > 3:
                        raise ValueError("Log count bound exceeded")
        files, total = [], 0
        for path in sorted(candidates):
            data = _read(path, 524288)
            total += len(data)
            if total > 1572864:
                raise ValueError("Log total bound exceeded")
            files.append({"name": path.name, "bytes": len(data),
                          "sha256": hashlib.sha256(data).hexdigest(),
                          "base64": base64.b64encode(data).decode("ascii")})
        return {"status": "preserved" if files else "no_matching_logs",
                "bytes": total, "files": files}
    except Exception:
        return {"status": "unavailable", "reason": "profile_log_validation_failed", "files": []}


def collect_startup(app, *, teardown: bool, operations: int) -> dict:
    """Never print contents, broaden process ownership, or read live writer output."""
    states = {"not_started", "launch_pid_reported", "owned_window_available",
              "owned_process_without_window", "no_owned_process_or_window",
              "launch_pid_not_live_no_replacement", "desktop_missing_with_owned_process",
              "desktop_missing_no_owned_process"}
    state = getattr(app, "startup_state", None)
    result = {"status": "unavailable", "reason": "not_collected",
              "startup_state": state if state in states else "unknown",
              "launch_exit_code": None, "holder_status": None,
              "natural_exit_observed_before_cleanup":
                  getattr(app, "natural_exit_observed_before_cleanup", False) is True}
    if operations:
        result["reason"] = "not_initial_startup"
        return result
    if not teardown:
        result["reason"] = "teardown_unverified"
        return result
    try:
        result["helper_sha256"] = hashlib.sha256(_read(Path(__file__), 65536)).hexdigest()
        root = Path(os.environ["RUNNER_TEMP"]).resolve()
        if not app.holder_receipt_path.resolve().is_relative_to(root):
            raise ValueError("Diagnostic path escapes temporary root")
        if any(path.parent != app.holder_receipt_path.parent for path in
               (app.holder_stdout_path, app.holder_stderr_path)):
            raise ValueError("Stream directory mismatch")
        receipt_bytes = _read(app.holder_receipt_path, 65536)
        receipt = json.loads(receipt_bytes)
        if not isinstance(receipt, dict):
            raise ValueError("Invalid holder receipt")
        expected = {"helper_pid": app.helper_pid, "app_pid": app.launch_pid,
                    "helper_executable_sha256": app.helper_exe_hash,
                    "desktop": app.desktop}
        if any(receipt.get(key) != value or value is None for key, value in expected.items()):
            raise ValueError("Holder identity mismatch")
        started = datetime.fromisoformat(receipt["launch_started_at_utc"])
        if (started.tzinfo is None or receipt.get("exe_sha256") != _hash_file(Path(app.exe))
                or ntpath.normcase(ntpath.abspath(receipt.get("profile", ""))) !=
                   ntpath.normcase(ntpath.abspath(app.datadir))
                or started.astimezone(timezone.utc) != app.launch_started
                or receipt.get("status") != "holder_stopped"
                or receipt.get("app_exit_confirmed") is not True
                or receipt.get("stream_capture_complete") is not True
                or receipt.get("deadline_fired") is not False
                or not receipt.get("holder_finished_at_utc")):
            raise ValueError("Holder lifetime is not verified")
        if type(receipt.get("app_exit_code")) is not int or not 0 <= receipt["app_exit_code"] <= 4294967295:
            raise ValueError("Exit code unavailable")
        streams = {}
        for role, path in (("stdout", app.holder_stdout_path), ("stderr", app.holder_stderr_path)):
            metadata = receipt.get("streams", {}).get(role, {})
            total = metadata.get("bytes_total")
            if type(total) is not int or total < 0 or "capture_error" in metadata:
                raise ValueError("Stream completion unavailable")
            data = _read(path, 1048576) if path.exists() else b""
            if len(data) != min(total, 1048576) or metadata.get("bytes_saved") != len(data):
                raise ValueError("Stream length mismatch")
            streams[role] = {"bytes": len(data), "bytes_total": total,
                             "truncated": total > len(data),
                             "sha256": hashlib.sha256(data).hexdigest(),
                             "base64": base64.b64encode(data).decode("ascii")}
        result.update(status="preserved", reason="verified_initial_startup",
                      launch_exit_code=receipt.get("app_exit_code"),
                      natural_exit_observed_before_cleanup=bool(app.natural_exit_observed_before_cleanup),
                      holder_status="holder_stopped", holder_receipt=receipt,
                      holder_receipt_sha256=hashlib.sha256(receipt_bytes).hexdigest(), streams=streams)
        result["profile_logs"] = _profile_logs(app, root)
    except Exception:
        # Keep original runtime failure and never expose paths or stream contents.
        result["reason"] = "startup_diagnostics_unavailable"
    return result
