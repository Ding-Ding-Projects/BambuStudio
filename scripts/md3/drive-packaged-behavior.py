#!/usr/bin/env python3
"""Drive a verified installed Bambu Studio build on an isolated hidden desktop.

This deliberately records observations rather than treating a successful click
as proof of a feature. It requires the hosted Squirrel installation receipt,
uses only the cheap Lowlevel route, and writes private raw captures to a caller
owned directory. Do not upload that directory without separate privacy review.
"""
from __future__ import annotations

import argparse
import base64
from collections import Counter
from contextlib import contextmanager
import ctypes
from ctypes import wintypes
import hashlib
import json
import ntpath
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path
from PIL import Image

from recapture import App, Runner, cheap, find_control
from hosted_process import (matching_wer_events, owned_process_inventory,
                            owned_processes, process_snapshot)

SHA = re.compile(r"^[0-9a-f]{40}$")
HEX256 = re.compile(r"^[0-9a-f]{64}$")
MODES = ("en", "yue_HK", "bilingual_en_yue_HK")


def helper_python_executable() -> Path:
    base = getattr(sys, "_base_executable", None)
    if not base:
        raise RuntimeError("The base CPython executable is unavailable for the hosted holder")
    path = Path(base)
    if path.name.lower() != "python.exe" or not path.is_file():
        raise RuntimeError("The hosted holder requires an existing base python.exe")
    return path


def validate_holder_receipt(receipt: dict, *, helper_pid: int, exe_hash: str,
                            helper_exe_hash: str,
                            datadir: str, desktop: str) -> tuple[int, datetime]:
    if (receipt.get("helper_pid") != helper_pid or receipt.get("exe_sha256") != exe_hash
            or receipt.get("helper_executable_sha256") != helper_exe_hash
            or receipt.get("desktop") != desktop
            or ntpath.normcase(ntpath.abspath(str(receipt.get("profile") or "")))
               != ntpath.normcase(ntpath.abspath(datadir))
            or receipt.get("status") not in ("app_launched", "app_exited_holder_alive")):
        raise RuntimeError("Hosted holder receipt identity differs from this launch")
    pid = receipt.get("app_pid")
    if not isinstance(pid, int) or pid <= 0 or pid == helper_pid:
        raise RuntimeError("Hosted holder receipt has no distinct app PID")
    try:
        started = datetime.fromisoformat(receipt["launch_started_at_utc"])
        if started.tzinfo is None:
            raise ValueError("missing timezone")
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError("Hosted holder receipt has no valid launch time") from exc
    return pid, started.astimezone(timezone.utc)


class HostedApp(App):
    """Bind a window only to this launch, installation, profile and desktop."""

    def __init__(self, exe, datadir, desktop, probe_dir):
        super().__init__(exe, datadir, desktop, probe_dir)
        self.launch_started = None
        self.launch_pid = None
        self.adopted_pids = []
        self.live_owned = []
        self.seen_owned = {}
        self.finished_at = None
        self.desktop_error = None
        self.startup_state = "not_started"
        self.launch_exit_code = None
        self.helper_pid = None
        self.helper_exe_hash = None
        self.holder_receipt_path = Path(probe_dir) / "hosted-launch.json"
        self.holder_stop_path = Path(probe_dir) / "hosted-launch.stop"
        self.holder_stdout_path = Path(probe_dir) / "hosted-stdout.log"
        self.holder_stderr_path = Path(probe_dir) / "hosted-stderr.log"
        self.holder_receipt = None
        self.cleanup_killed_pids = []
        self.natural_exit_observed_before_cleanup = False
        self.holder_lifetime = 600
        self.owned_teardown_verified = False
        self.desktop_closed_verified = False

    def _desktop_windows(self):
        return cheap("list_headless_windows", name=self.desktop)["windows"]

    def windows(self):
        # Snapshot identity first. If an early native exit releases the final
        # desktop handle, desktop enumeration must not hide that distinction.
        processes = process_snapshot()
        self.live_owned = owned_process_inventory(
            processes, exe=self.exe, datadir=self.datadir,
            launched_at=self.launch_started, launch_pid=self.launch_pid,
        )
        for item in self.live_owned:
            self.seen_owned[item["pid"]] = item
        try:
            windows = self._desktop_windows()
        except Exception as exc:
            self.desktop_error = f"{type(exc).__name__}: {exc}"
            self.startup_state = ("desktop_missing_with_owned_process" if self.live_owned
                                  else "desktop_missing_no_owned_process")
            raise RuntimeError(f"{self.startup_state}: {self.desktop_error}") from exc
        candidates = [item["pid"] for item in self.live_owned
                      if item["pid"] in {int(w["process_id"]) for w in windows}]
        self.adopted_pids = candidates
        if self.pid not in candidates:
            self.pid = candidates[0] if candidates else None
        self.startup_state = ("owned_window_available" if candidates else
                              "owned_process_without_window" if self.live_owned else
                              "no_owned_process_or_window")
        return [w for w in windows if int(w["process_id"]) in candidates]

    def launch_holder(self):
        os.environ["BAMBU_LAYOUT_PROBE"] = "1"
        os.environ["BAMBU_LAYOUT_PROBE_TAG"] = os.path.basename(self.datadir)
        holder = Path(__file__).with_name("hosted_launch_holder.py")
        helper_python = helper_python_executable()
        self.helper_exe_hash = sha256(helper_python)
        command = (f'"{helper_python}" "{holder}" --exe "{self.exe}" '
                   f'--datadir "{self.datadir}" --desktop "{self.desktop}" '
                   f'--receipt "{self.holder_receipt_path}" --stop "{self.holder_stop_path}" '
                   f'--stdout "{self.holder_stdout_path}" --stderr "{self.holder_stderr_path}" '
                   f'--timeout {int(self.holder_lifetime)}')
        self.helper_pid = cheap("launch_on_headless_desktop", name=self.desktop,
                                command=command)["pid"]
        receipt_deadline = time.monotonic() + 20
        while time.monotonic() < receipt_deadline:
            if self.holder_receipt_path.is_file():
                try:
                    self.holder_receipt = json.loads(self.holder_receipt_path.read_text(encoding="utf-8"))
                except (OSError, json.JSONDecodeError):
                    time.sleep(0.2)
                    continue
                self.launch_pid, self.launch_started = validate_holder_receipt(
                    self.holder_receipt, helper_pid=self.helper_pid,
                    exe_hash=sha256(Path(self.exe)), helper_exe_hash=self.helper_exe_hash,
                    datadir=self.datadir,
                    desktop=self.desktop)
                self.startup_state = "launch_pid_reported"
                break
            time.sleep(0.2)
        else:
            raise RuntimeError("Hosted launch holder produced no bounded startup receipt")

    def start(self, timeout=240):
        self.launch_holder()
        deadline = time.monotonic() + timeout
        no_process_since = None
        while time.monotonic() < deadline:
            frame = self.find(lambda w: w["class"] == "wxWindowNR"
                              and w["width"] >= 1000 and w["height"] >= 600)
            if frame:
                self.main = frame["handle"]
                self.pid = int(frame["process_id"])
                time.sleep(8)
                return
            if self.startup_state == "no_owned_process_or_window":
                no_process_since = no_process_since or time.monotonic()
                if time.monotonic() - no_process_since >= 10:
                    self.startup_state = "launch_pid_not_live_no_replacement"
                    raise RuntimeError("Launch PID and verified replacement are absent after startup grace period")
            else:
                no_process_since = None
            time.sleep(1)
        raise RuntimeError("No owned main frame appeared on the named hidden desktop")

    def stop(self):
        # Re-check live identity before terminating anything. An exited launch PID
        # is normal in a relaunch and must never obscure the drive's first error.
        errors = []
        try:
            if self.holder_receipt_path.is_file():
                previous = json.loads(self.holder_receipt_path.read_text(encoding="utf-8"))
                self.natural_exit_observed_before_cleanup = bool(
                    previous.get("helper_pid") == self.helper_pid
                    and previous.get("app_exited_at_utc")
                    and not previous.get("app_terminated_by_holder"))
        except (OSError, json.JSONDecodeError):
            pass
        try:
            self.live_owned = owned_process_inventory(
                process_snapshot(), exe=self.exe, datadir=self.datadir,
                launched_at=self.launch_started, launch_pid=self.launch_pid)
            for item in self.live_owned:
                self.seen_owned[item["pid"]] = item
            for pid in [item["pid"] for item in self.live_owned]:
                try:
                    cheap("kill_process", pid=pid, force=True)
                    self.cleanup_killed_pids.append(pid)
                except Exception as exc:
                    errors.append(f"owned PID {pid}: {type(exc).__name__}: {exc}")
        except Exception as exc:
            errors.append(f"ownership recheck: {type(exc).__name__}: {exc}")
        if self.helper_pid is not None:
            try:
                self.holder_stop_path.write_text("stop\n", encoding="ascii")
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    if self.holder_receipt_path.is_file():
                        receipt = json.loads(self.holder_receipt_path.read_text(encoding="utf-8"))
                        if receipt.get("helper_pid") == self.helper_pid:
                            self.holder_receipt = receipt
                            self.launch_exit_code = receipt.get("app_exit_code")
                            if receipt.get("holder_finished_at_utc"):
                                if (receipt.get("status") != "holder_stopped"
                                        or not receipt.get("app_exit_confirmed")
                                        or receipt.get("deadline_fired")):
                                    errors.append("Hosted holder did not confirm owned app teardown")
                                break
                    time.sleep(0.25)
                else:
                    errors.append("Hosted holder teardown unverified after fifteen seconds; helper PID was not killed without fresh identity proof")
            except Exception as exc:
                errors.append(f"holder closure: {type(exc).__name__}: {exc}")
        try:
            cheap("close_headless_desktop", name=self.desktop)
        except Exception as exc:
            errors.append(f"desktop closure: {type(exc).__name__}: {exc}")
        # The cheap CLI uses a new process for each call. Its close result may
        # say "not tracked" even when the named desktop has been released.
        # Confirm both no exact owned process and a missing named desktop.
        verification_deadline = time.monotonic() + 5
        while time.monotonic() < verification_deadline:
            try:
                remaining = owned_process_inventory(
                    process_snapshot(), exe=self.exe, datadir=self.datadir,
                    launched_at=self.launch_started, launch_pid=self.launch_pid)
                self.owned_teardown_verified = self.launch_pid is not None and not remaining
            except Exception:
                self.owned_teardown_verified = False
            try:
                cheap("list_headless_windows", name=self.desktop)
                self.desktop_closed_verified = False
            except Exception as exc:
                detail = str(exc)
                self.desktop_closed_verified = (
                    f"OpenDesktopW('{self.desktop}')" in detail and "GetLastError=2" in detail)
            if self.owned_teardown_verified and self.desktop_closed_verified:
                break
            time.sleep(0.25)
        if not self.owned_teardown_verified:
            errors.append("Exact owned process teardown was not verified")
        if not self.desktop_closed_verified:
            errors.append("Named desktop closure was not verified")
        self.finished_at = datetime.now(timezone.utc)
        if errors:
            raise RuntimeError("; ".join(errors))
MODE_LABELS = {
    "en": {"prepare": "Prepare", "ink": "Ink", "process": "Process", "objects": "Objects",
           "ink_search": "Search filaments", "settings": "Search settings", "preview": "Preview", "project": "Project"},
    "yue_HK": {"prepare": "準備", "ink": "耗材", "process": "打印設定", "objects": "物件",
               "ink_search": "搵墨水", "settings": "搜尋設定", "preview": "預覽", "project": "項目"},
    "bilingual_en_yue_HK": {"prepare": "Prepare", "ink": "Ink", "process": "Process",
                             "objects": "Objects", "ink_search": "Search filaments", "settings": "Search settings",
                             "preview": "Preview", "project": "Project"},
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def expected_3mf_objects(path: Path) -> list[str]:
    """Read only the bounded build object names of the checked-in 3MF fixture."""
    if not path.is_file() or path.stat().st_size > 20_000_000:
        raise ValueError("The public 3MF fixture is absent or exceeds its size bound")
    with zipfile.ZipFile(path) as bundle:
        if len(bundle.infolist()) > 128:
            raise ValueError("The public 3MF fixture has too many ZIP members")
        member = bundle.getinfo("3D/3dmodel.model")
        if member.file_size > 2_000_000:
            raise ValueError("The public 3MF model XML exceeds its size bound")
        xml = bundle.read(member)
    root = ET.fromstring(xml)
    resources = root.findall(".//{*}resources/{*}object")
    objects = {item.get("id"): item.get("name") for item in resources}
    if len(objects) != len(resources):
        raise ValueError("The public 3MF model has duplicate object IDs")
    items = root.findall(".//{*}build/{*}item")
    ids = [item.get("objectid") for item in items]
    if not ids or len(ids) > 64 or len(set(ids)) != len(ids):
        raise ValueError("The public 3MF build object inventory is empty or ambiguous")
    names = [objects.get(identifier) for identifier in ids]
    if any(not isinstance(name, str) or not name or len(name.encode("utf-8")) > 160
           for name in names):
        raise ValueError("The public 3MF build object names are unavailable or too long")
    return names


def model_snapshot(records: list[dict], *, pid: int, profile_tag: str,
                   main_hwnd: int) -> dict | None:
    """Accept only a complete probe owned by the selected packaged process."""
    if (not records or records[-1].get("kind") != "end"
            or sum(r.get("kind") == "end" for r in records) != 1):
        return None
    headers = [r for r in records if r.get("kind") == "header"]
    states = [r for r in records if r.get("kind") == "model_state"]
    if (len(headers) != 1 or len(states) != 1 or headers[0].get("pid") != pid
            or headers[0].get("tag") != profile_tag):
        return None
    state = states[0]
    plater = state.get("plater_hwnd")
    if (state.get("model_available") is not True
            or state.get("mainframe_hwnd") != main_hwnd
            or not isinstance(plater, int) or plater <= 0
            or not any(r.get("hwnd") == main_hwnd and r.get("kind") == "toplevel" for r in records)
            or not any(r.get("hwnd") == plater for r in records)):
        return None
    count = state.get("object_count")
    emitted = state.get("object_records")
    objects = [r for r in records if r.get("kind") == "model_object"]
    if (type(count) is not int or count < 0 or count > 64
            or type(emitted) is not int or emitted != count
            or state.get("objects_truncated") is not False
            or len(objects) != count):
        return None
    indices = [r.get("index") for r in objects]
    if any(type(value) is not int for value in indices) or sorted(indices) != list(range(count)):
        return None
    if any(r.get("object_available") is not True
           or r.get("name_available") is not True
           or r.get("name_valid_utf8") is not True
           or r.get("name_truncated") is not False
           or not isinstance(r.get("name"), str) or not r["name"]
           or type(r.get("instance_count")) is not int or r["instance_count"] < 1
           for r in objects):
        return None
    try:
        if any(len(r["name"].encode("utf-8")) > 160 for r in objects):
            return None
    except UnicodeEncodeError:
        return None
    return {"object_count": count,
            "names": [r["name"] for r in sorted(objects, key=lambda item: item["index"])],
            "path": state.get("project_path"),
            "path_available": state.get("project_path_available"),
            "path_truncated": state.get("project_path_truncated"),
            "active_plate_available": state.get("active_plate_available"),
            "active_plate_index": state.get("active_plate_index"),
            "active_plate_id": state.get("active_plate_id"),
            "active_plate_instance_count": state.get("active_plate_instance_count"),
            "active_plate_printable_instance_count": state.get("active_plate_printable_instance_count"),
            "plater_hwnd": plater, "mainframe_hwnd": main_hwnd,
            "probe_pid": pid, "profile_tag": profile_tag}


def fixture_model_transition(before: dict | None, after: dict | None,
                             expected_names: list[str], path: Path) -> dict | None:
    if before is None or after is None:
        return None
    expected = Counter(expected_names)
    if (before["object_count"] == len(expected_names)
            and Counter(before["names"]) == expected):
        return None
    if (after["object_count"] != len(expected_names)
            or Counter(after["names"]) != expected
            or after["path_available"] is not True or after["path_truncated"] is not False
            or not isinstance(after["path"], str)
            or not after["path"].lower().endswith(".3mf")
            or ntpath.normcase(ntpath.abspath(after["path"]))
               != ntpath.normcase(ntpath.abspath(str(path)))
            or after["active_plate_available"] is not True
            or not isinstance(after["active_plate_index"], int)
            or after["active_plate_index"] < 0
            or not isinstance(after["active_plate_id"], int)
            or after["active_plate_id"] < 0
            or not isinstance(after["active_plate_instance_count"], int)
            or after["active_plate_instance_count"] < 1
            or not isinstance(after["active_plate_printable_instance_count"], int)
            or after["active_plate_printable_instance_count"] < 1
            or after["active_plate_printable_instance_count"]
               > after["active_plate_instance_count"]):
        return None
    return {"before": before, "after": after,
            "fixture_sha256": sha256(path), "expected_object_count": len(expected_names),
            "expected_names": expected_names}


def preserve_logs(datadir: Path, output: Path) -> list[dict]:
    """Copy only bounded original app logs into the restricted output inventory."""
    log_dir = datadir / "log"
    if not log_dir.is_dir():
        return []
    records = []
    destination = output / "restricted-logs"
    for source in sorted(log_dir.glob("*.log"))[:3]:
        if (not re.fullmatch(r"[A-Za-z0-9_.-]{1,100}", source.name)
                or source.is_symlink() or not source.is_file()
                or source.stat().st_size > 10_000_000):
            continue
        destination.mkdir(exist_ok=True)
        target = destination / source.name
        shutil.copyfile(source, target)
        records.append({"file": target.name,
                        "sha256": sha256(target), "bytes": target.stat().st_size,
                        "privacy": "restricted original log; do not print or publish"})
    return records


def preserve_holder_streams(app: HostedApp, output: Path) -> list[dict]:
    records = []
    for source in (app.holder_stdout_path, app.holder_stderr_path):
        if not source.is_file():
            continue
        if source.stat().st_size == 0:
            continue
        if source.stat().st_size > 1_048_576:
            raise RuntimeError("Hosted output stream exceeded its bounded capture size")
        destination = output / "restricted-logs"
        destination.mkdir(exist_ok=True)
        target = destination / source.name
        shutil.copyfile(source, target)
        records.append({"file": target.name, "sha256": sha256(target),
                        "bytes": target.stat().st_size,
                        "privacy": "restricted original process output; do not print or publish"})
    return records


def holder_stream_sources(app: HostedApp) -> list[tuple[str, Path]]:
    """Distinguish confirmed empty pipes from missing or incomplete capture."""
    streams = (app.holder_receipt or {}).get("streams") or {}
    sources = []
    for role, path in (("stdout", app.holder_stdout_path),
                       ("stderr", app.holder_stderr_path)):
        if streams.get(role, {}).get("bytes_total") == 0 and not path.is_file():
            continue
        sources.append((role, path))
    return sources


@contextmanager
def startup_case_environment(case_temp: Path, profile: Path):
    """Give each launched child a new trace location without changing other cases."""
    previous = {key: os.environ.get(key) for key in
                ("TEMP", "TMP", "BAMBU_LAYOUT_PROBE", "BAMBU_LAYOUT_PROBE_TAG")}
    case_temp.mkdir()
    os.environ.update(TEMP=str(case_temp), TMP=str(case_temp),
                      BAMBU_LAYOUT_PROBE="1", BAMBU_LAYOUT_PROBE_TAG=profile.name)
    try:
        yield
    finally:
        for key, value in previous.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


class OriginalProcessHandle:
    """Retain a direct launch handle only after matching WMI and handle identity."""

    def __init__(self, pid: int, exe: Path, created_at: datetime):
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel = kernel
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD,
                                                      wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        kernel.GetProcessTimes.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                           ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
        kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.handle = kernel.OpenProcess(0x00100000 | 0x1000, False, pid)
        if not self.handle:
            raise OSError(ctypes.get_last_error(), "OpenProcess failed")
        try:
            name = ctypes.create_unicode_buffer(32768)
            length = wintypes.DWORD(len(name))
            if not kernel.QueryFullProcessImageNameW(self.handle, 0, name, ctypes.byref(length)):
                raise OSError(ctypes.get_last_error(), "QueryFullProcessImageNameW failed")
            values = [ctypes.c_ulonglong() for _ in range(4)]
            if not kernel.GetProcessTimes(self.handle, *(ctypes.byref(value) for value in values)):
                raise OSError(ctypes.get_last_error(), "GetProcessTimes failed")
            actual = datetime.fromtimestamp((values[0].value - 116444736000000000) / 10_000_000,
                                            timezone.utc)
            if (ntpath.normcase(ntpath.abspath(name.value)) !=
                    ntpath.normcase(ntpath.abspath(str(exe)))
                    or abs((actual - created_at).total_seconds()) > 1):
                raise RuntimeError("Direct launch handle image or creation time differs from owned PID")
        except BaseException:
            self.close()
            raise

    def exit_code(self) -> int | None:
        if self.kernel.WaitForSingleObject(self.handle, 0) != 0:
            return None
        code = wintypes.DWORD()
        return int(code.value) if self.kernel.GetExitCodeProcess(self.handle, ctypes.byref(code)) else None

    def close(self) -> None:
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def startup_case_exited(case: dict) -> bool:
    """An absent HWND or desktop never stands in for a confirmed original exit."""
    return (case.get("original_exit_confirmed") is True
            and case.get("owned_replacement_seen") is False
            and case.get("owned_process_live_at_end") is False)


def bundle_case_diagnostics(name: str, sources: list[tuple[str, Path]],
                            exe: Path, pid_times: dict[int, datetime],
                            output: Path) -> tuple[dict, list[str]]:
    """One encrypted-only bounded file per case, with exact source provenance."""
    payload = {"schema": 1, "case": name, "files": []}
    missing = []
    for role, path in sources:
        if not path.is_file() or path.is_symlink():
            missing.append(role)
            continue
        total_bytes = path.stat().st_size
        with path.open("rb") as stream:
            content = stream.read(1_048_576)
        payload["files"].append({"role": role, "bytes": len(content),
                                 "source_bytes": total_bytes,
                                 "truncated": total_bytes > len(content),
                                 "sha256": hashlib.sha256(content).hexdigest(),
                                 "base64": base64.b64encode(content).decode("ascii")})
    if pid_times:
        try:
            wer_events = matching_wer_events(str(exe), pid_times, datetime.now(timezone.utc))[:2]
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError):
            wer_events = []
            missing.append("exact_wer_query_failed")
        for xml in wer_events:
            raw = xml.encode("utf-8")
            payload["files"].append({"role": "exact_pid_image_time_wer", "bytes": len(raw),
                                     "sha256": hashlib.sha256(raw).hexdigest(),
                                     "base64": base64.b64encode(raw).decode("ascii")})
    destination = output / "restricted-logs"
    destination.mkdir(exist_ok=True)
    target = destination / (name + "-startup.log")
    target.write_text(json.dumps(payload, separators=(",", ":")), encoding="utf-8")
    if target.stat().st_size > 16_000_000:
        raise RuntimeError("Bounded startup diagnostic bundle exceeded sixteen MB")
    return ({"file": target.name, "bytes": target.stat().st_size,
             "sha256": sha256(target), "privacy": "restricted case diagnostics"}, missing)


def run_startup_case(name: str, route: str, seeded: bool, args, scratch: Path,
                     exe_hash: str) -> tuple[dict, dict]:
    """Compare launch routes without interpreting mere liveness as GUI success."""
    case_root = scratch / name
    profile = case_root / "profile"
    probe = case_root / "probe"
    case_temp = case_root / "temp"
    profile.mkdir(parents=True)
    probe.mkdir()
    if seeded:
        seed_profile(profile, args.language, args.theme)
    seed_config = profile / "BambuStudio.conf"
    initial_seed_config_sha256 = sha256(seed_config) if seeded else None
    desktop = f"bsdiag-{os.getpid()}-{name}"
    app = HostedApp(str(args.exe), str(profile), desktop, str(probe))
    app.holder_lifetime = 120
    original_handle: OriginalProcessHandle | None = None
    samples = []
    primary_error = None
    cleanup_error = None
    original_exit = False
    exit_code = None
    replacement_seen = False
    mainframe_seen = False
    started = datetime.now(timezone.utc)
    try:
        with startup_case_environment(case_temp, profile):
            if route == "holder":
                app.launch_holder()
            elif route == "direct":
                app.launch_started = datetime.now(timezone.utc)
                command = f'"{args.exe}" --datadir "{profile}"'
                app.launch_pid = int(cheap("launch_on_headless_desktop", name=desktop,
                                           command=command)["pid"])
                app.startup_state = "direct_launch_pid_reported"
            else:
                raise ValueError("Unknown startup route")
        deadline = time.monotonic() + 45
        absent_since = None
        while time.monotonic() < deadline:
            try:
                windows = app.windows()
                desktop_state = "available"
            except Exception:
                windows = []
                desktop_state = "enumeration_failed"
            if route == "direct" and original_handle is None:
                original = next((item for item in app.live_owned
                                 if item["pid"] == app.launch_pid), None)
                if original is not None:
                    try:
                        original_handle = OriginalProcessHandle(
                            app.launch_pid, args.exe,
                            datetime.fromisoformat(original["created_at_utc"]))
                    except (OSError, RuntimeError):
                        pass
            if original_handle is not None:
                exit_code = original_handle.exit_code()
                original_exit = exit_code is not None
            elif route == "holder" and app.holder_receipt_path.is_file():
                try:
                    app.holder_receipt = json.loads(app.holder_receipt_path.read_text(encoding="utf-8"))
                    original_exit = bool(app.holder_receipt.get("app_exit_confirmed")
                                         and app.holder_receipt.get("app_exited_at_utc")
                                         and not app.holder_receipt.get("app_terminated_by_holder"))
                    if original_exit:
                        exit_code = app.holder_receipt.get("app_exit_code")
                except (OSError, json.JSONDecodeError):
                    pass
            replacement_seen |= any(item["pid"] != app.launch_pid for item in app.seen_owned.values())
            mainframe_seen |= any(w.get("class") == "wxWindowNR" and
                                  w.get("width", 0) >= 1000 and w.get("height", 0) >= 600
                                  for w in windows)
            samples.append({"elapsed_seconds": round((datetime.now(timezone.utc) - started).total_seconds(), 2),
                            "owned_pids": [item["pid"] for item in app.live_owned],
                            "owned_window_count": len(windows),
                            "desktop_state": desktop_state,
                            "original_exit_confirmed": original_exit,
                            "original_exit_code": exit_code})
            if original_exit and not app.live_owned:
                absent_since = absent_since or time.monotonic()
                if time.monotonic() - absent_since >= 5:
                    break
            else:
                absent_since = None
            time.sleep(1)
    except Exception as exc:
        primary_error = f"{type(exc).__name__}: case launch or observation failed"
    finally:
        if original_handle is not None:
            original_handle.close()
        try:
            app.stop()
        except Exception as exc:
            cleanup_error = f"{type(exc).__name__}: case teardown failed"
    # Teardown may set an exit code. Only the observation before teardown is
    # evidence of a natural exit.
    case = {"case": name, "route": route, "profile_state": "seeded" if seeded else "empty",
            "desktop": desktop, "installed_exe_sha256": exe_hash,
            "started_at_utc": started.isoformat(),
            "ended_at_utc": app.finished_at.isoformat() if app.finished_at else None,
            "initial_pid": app.launch_pid, "helper_pid": app.helper_pid,
            "helper_executable_sha256": app.helper_exe_hash,
            "original_exit_confirmed": original_exit, "original_exit_code": exit_code,
            "owned_replacement_seen": replacement_seen,
            "owned_process_live_at_end": bool(samples and samples[-1]["owned_pids"]),
            "mainframe_seen": mainframe_seen,
            "owned_processes": sorted(app.seen_owned.values(), key=lambda item: item["pid"]),
            "timeline": samples, "startup_state": app.startup_state,
            "desktop_state": samples[-1]["desktop_state"] if samples else "not_enumerated",
            "cleanup_killed_pids": app.cleanup_killed_pids,
            "owned_teardown_verified": app.owned_teardown_verified,
            "desktop_closed_verified": app.desktop_closed_verified,
            "primary_error": primary_error, "cleanup_error": cleanup_error,
            "holder_status": (app.holder_receipt or {}).get("status"),
            "holder_deadline_fired": (app.holder_receipt or {}).get("deadline_fired"),
            "stream_capture_complete": (app.holder_receipt or {}).get("stream_capture_complete"),
            "stream_counters": (app.holder_receipt or {}).get("streams"),
            "gui_verified": False, "behavior_verified": False}
    sources = [("launcher_trace", case_temp / "bbs-launcher-trace.log")]
    if route == "holder":
        sources.append(("holder_receipt", app.holder_receipt_path))
        sources.extend(holder_stream_sources(app))
    app_log_dir = profile / "log"
    if app_log_dir.is_dir() and not app_log_dir.is_symlink():
        sources.extend(("app_log", path) for path in sorted(app_log_dir.glob("*.log"))[:3])
    pid_times = {item["pid"]: datetime.fromisoformat(item["created_at_utc"])
                 for item in app.seen_owned.values()} if original_exit else {}
    if route == "holder" and app.launch_pid and app.launch_started and original_exit:
        pid_times[app.launch_pid] = app.launch_started
    restricted, missing = bundle_case_diagnostics(name, sources, args.exe,
                                                   pid_times, args.output)
    case["diagnostic_source_status"] = {"missing_roles": missing,
                                        "restricted_bundle": restricted["file"]}
    case["exit_code_status"] = ("observed_before_cleanup" if original_exit and exit_code is not None
                                else "exit_confirmed_code_unavailable" if original_exit
                                else "unavailable_without_verified_process_handle_or_exit")
    if route == "direct":
        case["stream_status"] = "stdout_and_stderr_unavailable_from_direct_lowlevel_launch"
    else:
        case["stream_status"] = ("captured_or_explicitly_empty" if case["stream_capture_complete"]
                                 else "incomplete_or_unavailable")
    case["cleanup_verified"] = (cleanup_error is None and app.owned_teardown_verified
                                and app.desktop_closed_verified)
    case["seed_config_sha256"] = initial_seed_config_sha256
    case["post_run_config_sha256"] = (sha256(seed_config) if seed_config.is_file()
                                      and not seed_config.is_symlink() else None)
    return case, restricted


def run_startup_comparison(args, receipt: dict, source: str, verifier: str,
                           scratch: Path) -> int:
    """Diagnostic scope is intentionally incomplete even when a frame appears."""
    cases = []
    logs = []
    comparison_stopped_reason = None
    for name, route, seeded in (("seeded-direct", "direct", True),
                                ("seeded-holder", "holder", True)):
        case, record = run_startup_case(name, route, seeded, args, scratch,
                                        receipt["installed_exe_sha256"])
        cases.append(case)
        logs.append(record)
        if not case.get("cleanup_verified"):
            comparison_stopped_reason = "owned_process_or_named_desktop_teardown_unverified"
            break
    empty_fallback = (comparison_stopped_reason is None and len(cases) == 2
                      and all(startup_case_exited(case) for case in cases))
    if empty_fallback:
        for name, route in (("empty-direct", "direct"), ("empty-holder", "holder")):
            case, record = run_startup_case(name, route, False, args, scratch,
                                            receipt["installed_exe_sha256"])
            cases.append(case)
            logs.append(record)
            if not case.get("cleanup_verified"):
                comparison_stopped_reason = "owned_process_or_named_desktop_teardown_unverified"
                break
    requested = {"language": args.language, "theme": args.theme,
                 "scale": args.scale, "viewport": [int(x) for x in args.viewport.split("x")]}
    rows = [{"name": case["case"],
             "status": "blocked" if case.get("primary_error") or not case.get("cleanup_verified")
             else "diagnostic_only",
             "original_exit_confirmed": case["original_exit_confirmed"],
             "original_exit_code": case["original_exit_code"],
             "mainframe_seen": case["mainframe_seen"],
             "reason": "Startup observation or teardown was unverified"
             if case.get("primary_error") or not case.get("cleanup_verified")
             else "Startup comparison does not verify rendering or behavior"}
            for case in cases]
    report = {"schema": 2, "scope": "diagnostic", "source_commit": source,
              "verification_commit": verifier, "release_tag": args.release_tag,
              "hosted_run_id": args.hosted_run_id,
              "installed_exe_sha256": receipt["installed_exe_sha256"],
              "package_version": receipt["package_version"],
              "runner": "github-hosted-windows", "requested_tuple": requested,
              "measured_tuple": None, "startup_comparison": cases,
              "empty_profile_fallback_run": empty_fallback,
              "comparison_stopped_reason": comparison_stopped_reason,
              "rows": rows, "images": [], "restricted_logs": logs,
              "rendered_ok": False, "behavior_verified": False,
              "failed_rows": [row["name"] for row in rows],
              "privacy": "restricted; startup diagnostics require review before publication",
              "verdict": "blocked", "cleanup": "failed" if any(case["cleanup_error"] for case in cases)
              else "verified"}
    (args.output / "behavior-report.json").write_text(json.dumps(report, indent=2) + "\n",
                                                     encoding="utf-8")
    return 2


def preserve_holder_receipt(source: Path, output: Path) -> dict | None:
    if not source.is_file():
        return None
    if source.stat().st_size > 65536:
        raise RuntimeError("Hosted launch lifetime receipt exceeded its size limit")
    destination = output / "restricted-logs"
    destination.mkdir(exist_ok=True)
    target = destination / "hosted-launch-lifetime.log"
    shutil.copyfile(source, target)
    return {"file": target.name, "sha256": sha256(target),
            "bytes": target.stat().st_size,
            "privacy": "restricted original startup receipt; do not print or publish"}


def preserve_wer(exe: str, app: HostedApp, output: Path) -> tuple[list[dict], str]:
    receipt = app.holder_receipt or {}
    if (app.launch_pid is None or app.launch_started is None or app.finished_at is None
            or receipt.get("app_exited_at_utc") is None
            or receipt.get("app_terminated_by_holder")
            or (app.launch_pid in app.cleanup_killed_pids
                and not app.natural_exit_observed_before_cleanup)):
        return [], "not_applicable_without_observed_natural_exit"
    pid_times = {app.launch_pid: app.launch_started}
    for item in app.seen_owned.values():
        pid_times[item["pid"]] = datetime.fromisoformat(item["created_at_utc"])
    events = matching_wer_events(exe, pid_times, datetime.now(timezone.utc))
    if not events:
        return [], "no_exact_pid_image_time_match_or_query_unavailable"
    destination = output / "restricted-logs"
    destination.mkdir(exist_ok=True)
    records = []
    for index, xml in enumerate(events[:2]):
        target = destination / f"wer-exact-{index}.log"
        target.write_text(xml, encoding="utf-8")
        records.append({"file": target.name, "sha256": sha256(target),
                        "bytes": target.stat().st_size,
                        "privacy": "restricted exact-match WER event; do not print or publish"})
    return records, "exact_match_restricted"


def validate_installation(receipt: dict, exe: Path, source: str, tag: str) -> None:
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted":
        raise ValueError("The packaged GUI drive requires a disposable GitHub-hosted runner")
    if receipt.get("status") != "verified" or receipt.get("runner") != "github-hosted-windows":
        raise ValueError("The isolated Squirrel installation is not verified")
    if receipt.get("source_commit") != source or receipt.get("release_tag") != tag:
        raise ValueError("Installation receipt source or release differs from the requested build")
    expected = receipt.get("installed_exe_sha256", "")
    if not HEX256.fullmatch(expected) or sha256(exe) != expected:
        raise ValueError("Installed executable differs from the verified Squirrel package")
    version = receipt.get("package_version", "")
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("Installation receipt has no valid package version")
    expected_path = Path(os.environ["LOCALAPPDATA"]) / "BambuStudioMD3" / ("app-" + version) / "bambu-studio.exe"
    if exe.resolve() != expected_path.resolve():
        raise ValueError("Executable is not inside this runner's isolated Squirrel installation")


def validate_verifier(commit: str) -> None:
    checkout = Path(__file__).resolve().parents[2]
    inputs = ["scripts/md3/drive-packaged-behavior.py", "scripts/md3/hosted_process.py",
              "scripts/md3/hosted_launch_holder.py",
              "scripts/md3/recapture.py", "scripts/md3/send-layout-probe.py"]
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=checkout,
                            capture_output=True, text=True, timeout=15, check=False)
    if result.returncode or result.stdout.strip().lower() != commit:
        raise ValueError("Verification commit differs from the checked-out driver source")
    tracked = subprocess.run(["git", "ls-files", "--error-unmatch", "--", *inputs],
                             cwd=checkout, capture_output=True, text=True,
                             timeout=15, check=False)
    status = subprocess.run(["git", "status", "--porcelain", "--untracked-files=all", "--", *inputs],
                            cwd=checkout, capture_output=True, text=True,
                            timeout=15, check=False)
    if tracked.returncode or status.returncode or status.stdout.strip():
        raise ValueError("Tracked driver inputs differ from the verification commit")


def visible_labels(records: list[dict]) -> list[str]:
    return sorted({str(r.get("label") or r.get("name")) for r in records
                   if r.get("kind") in ("window", "tool") and r.get("shown")
                   and r.get("on_screen") and (r.get("label") or r.get("name"))})


def visible(records: list[dict], label: str, owner: int | None = None) -> dict | None:
    return find_control(records, label, owner)


def seed_profile(datadir: Path, mode: str, theme: str = "light") -> None:
    """Create only a disposable config, using AppConfig's JSON and checksum format."""
    if mode not in MODES or theme not in ("light", "dark") or any(datadir.iterdir()):
        raise ValueError("Language profile is not new or the mode is unsupported")
    body = json.dumps({"app": {"language": mode, "dark_color_mode": "1" if theme == "dark" else "0"}},
                      ensure_ascii=False, indent=4)
    digest = hashlib.md5(body.encode("utf-8")).hexdigest().upper()
    (datadir / "BambuStudio.conf").write_bytes(
        (body + "\n# MD5 checksum " + digest + "\n").encode("utf-8"))


def probe_header(records: list[dict], mode: str, theme: str | None = None,
                 scale: float | None = None) -> dict:
    header = next((r for r in records if r.get("kind") == "header"), None)
    if header is None or header.get("language") != mode:
        raise RuntimeError(f"Layout probe did not confirm requested language {mode}")
    if not isinstance(header.get("dpi_scale"), (int, float)) or header["dpi_scale"] <= 0:
        raise RuntimeError("Layout probe did not report a valid display scale")
    if theme is not None and (not isinstance(header.get("dark"), bool)
                              or header["dark"] != (theme == "dark")):
        raise RuntimeError("Layout probe theme differs from requested theme")
    if scale is not None and abs(float(header["dpi_scale"]) - scale) > 0.02:
        raise RuntimeError("Measured native display scale differs from requested scale")
    return {key: header.get(key) for key in ("language", "dpi_scale", "dark", "density")}


class Drive:
    def __init__(self, app: App, output: Path, source: str, tag: str, exe_hash: str, run_id: str,
                 mode: str, verification_commit: str = "", theme: str = "light",
                 scale: float = 1.0, viewport: tuple[int, int] = (1200, 800)):
        self.app = app
        self.runner = Runner(app, "en")
        self.output = output
        self.rows: list[dict] = []
        self.images: list[dict] = []
        self.mode = mode
        self.theme = theme
        self.scale = scale
        self.viewport = viewport
        self.labels = MODE_LABELS[mode]
        self.identity = {"source_commit": source, "release_tag": tag, "hosted_run_id": run_id,
                         "verification_commit": verification_commit,
                         "installed_exe_sha256": exe_hash,
                         "requested_tuple": {"language": mode, "theme": theme,
                                             "scale": scale, "viewport": list(viewport)}}

    def checked_header(self, records: list[dict]) -> dict:
        return probe_header(records, self.mode, self.theme, self.scale)

    def resize_client_exact(self, requested: tuple[int, int]) -> tuple[dict, dict]:
        """Use probe client size to compensate for frame borders, with a bound."""
        for _attempt in range(4):
            windows = self.app.windows()
            frame = next((w for w in windows if w["handle"] == self.app.main), None)
            records = self.app.probe()
            self.checked_header(records)
            top = next((r for r in records if r.get("kind") == "toplevel"
                        and r.get("hwnd") == self.app.main), None)
            client = (top or {}).get("client") or {}
            actual = (client.get("w"), client.get("h"))
            if not frame or not all(isinstance(value, int) and value > 0 for value in actual):
                raise RuntimeError("The owned main frame has no measured client area")
            if actual == requested:
                return frame, client
            target_outer = (frame["width"] + requested[0] - actual[0],
                            frame["height"] + requested[1] - actual[1])
            if min(target_outer) < 400 or max(target_outer) > 4000:
                raise RuntimeError("Calculated outer frame size is outside the safe resize range")
            self.app.command(f"resize {self.app.main} {target_outer[0]} {target_outer[1]}")
            time.sleep(0.5)
        raise RuntimeError(f"Main client area did not reach exact {requested[0]}x{requested[1]}")

    def capture(self, label: str, hwnd: int) -> dict:
        name = f"{len(self.images):03d}-{re.sub('[^a-z0-9-]+', '-', label.lower()).strip('-')}.png"
        path = self.output / name
        result = cheap("screenshot", hwnd=hwnd, output_path=str(path))
        if result.get("rendered_ok") is not True:
            raise RuntimeError(f"PrintWindow did not confirm a rendered image for {label}")
        if not path.is_file() or path.stat().st_size < 2000:
            raise RuntimeError(f"No substantial screenshot file was written for {label}")
        with Image.open(path) as captured:
            captured.verify()
        with Image.open(path) as captured:
            rgb = captured.convert("RGB")
            if rgb.width < 200 or rgb.height < 150:
                raise RuntimeError(f"Captured frame dimensions are too small for {label}")
            extrema = rgb.getextrema()
            if all(low == high for low, high in extrema):
                raise RuntimeError(f"Captured frame is uniform for {label}")
        record = {"file": name, "sha256": sha256(path), "bytes": path.stat().st_size,
                  "captured_at_utc": datetime.now(timezone.utc).isoformat(),
                  "privacy": "restricted; visual review required before publication"}
        self.images.append(record)
        return record

    def observe(self, label: str, action, expected: tuple[str, ...], owner=None,
                *, timeout: float = 12, require_change: bool = False,
                require_new_marker: bool = False, require_enabled: bool = False) -> bool:
        """Capture both sides and require visible probe evidence after the action."""
        row = {"name": label, "status": "unverified", "expected_visible": list(expected)}
        self.rows.append(row)
        try:
            before = self.app.probe()
            row["before_header"] = self.checked_header(before)
            row["before_visible"] = visible_labels(before)
            marker_was_visible = any(visible(before, name, owner) for name in expected)
            row["before_image"] = self.capture(label + "-before", owner or self.app.main)
            t0 = time.monotonic()
            input_at = action()
            if not isinstance(input_at, float):
                input_at = t0
            row["action_ms"] = round((time.monotonic() - t0) * 1000)
            deadline = time.monotonic() + timeout
            after = []
            while time.monotonic() < deadline:
                after = self.app.probe()
                self.checked_header(after)
                if all(visible(after, name, owner) for name in expected) and (
                    not require_enabled or all(visible(after, name, owner).get("enabled") for name in expected)
                ):
                    break
                time.sleep(0.4)
            row["ready_ms"] = round((time.monotonic() - t0) * 1000)
            row["input_to_ready_ms"] = round((time.monotonic() - input_at) * 1000)
            row["after_visible"] = visible_labels(after)
            first_hit = next((visible(after, name, owner) for name in expected
                              if visible(after, name, owner)), None)
            after_window = (first_hit or {}).get("top") or owner or self.app.main
            row["after_window"] = after_window
            row["after_image"] = self.capture(label + "-after", after_window)
            present = all(visible(after, name, owner) for name in expected)
            enabled = not require_enabled or all(visible(after, name, owner).get("enabled")
                                                for name in expected if visible(after, name, owner))
            changed = row["before_visible"] != row["after_visible"]
            row["status"] = "probe_confirmed" if (present and enabled and
                (changed or not require_change) and (not require_new_marker or not marker_was_visible)) else "unverified"
            row["visual_review"] = "pending"
            if not present:
                row["reason"] = "Expected controls were not visible in the post-action layout probe"
            elif not enabled:
                row["reason"] = "Expected post-action control remained disabled"
            elif require_new_marker and marker_was_visible:
                row["reason"] = "The expected marker was already visible before the action"
            elif require_change and not changed:
                row["reason"] = "The visible control set did not change after the action"
            return row["status"] == "probe_confirmed"
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"
            return False

    def nav(self, label: str, expected: tuple[str, ...], timeout=12) -> bool:
        def action():
            records = self.app.probe()
            rail = next((r for r in records if r.get("kind") == "window"
                         and r.get("name") == "Navigation rail" and r.get("on_screen")), None)
            if not rail:
                raise RuntimeError("No on-screen navigation rail")
            target = visible(records, label, rail["hwnd"])
            if not target:
                raise RuntimeError(f"No navigation control labelled {label!r}")
            input_at = time.monotonic()
            self.runner.click_control(self.app.main, target)
            return input_at
        observed = self.observe("open-" + label.lower(), action, expected, self.app.main,
                                timeout=timeout, require_change=True)
        if not expected and observed:
            self.rows[-1]["status"] = "unverified"
            self.rows[-1]["reason"] = "Navigation changed visible controls, but no page-specific marker was established"
        return observed and bool(expected)

    def click(self, label: str, expected: tuple[str, ...], *, scope=None,
              timeout=12, require_change=True, require_enabled=False) -> bool:
        def action():
            records = self.app.probe()
            parent = next((r for r in records if r.get("kind") == "window"
                           and r.get("name") == scope and r.get("on_screen")), None) if scope else None
            if scope and not parent:
                raise RuntimeError(f"No on-screen scope named {scope!r}")
            target = visible(records, label, parent["hwnd"] if parent else self.app.main)
            if not target:
                raise RuntimeError(f"No on-screen control labelled {label!r}")
            input_at = time.monotonic()
            self.runner.click_control(target.get("top") or self.app.main, target)
            return input_at
        return self.observe("click-" + label.lower(), action, expected, self.app.main,
                            timeout=timeout, require_change=require_change,
                            require_enabled=require_enabled)

    def unavailable(self, name: str, reason: str) -> None:
        self.rows.append({"name": name, "status": "unavailable", "reason": reason})

    def unverified(self, name: str, reason: str) -> None:
        self.rows.append({"name": name, "status": "unverified", "reason": reason})

    def optional_nav(self, label: str) -> None:
        records = self.app.probe()
        rail = next((r for r in records if r.get("kind") == "window"
                     and r.get("name") == "Navigation rail" and r.get("on_screen")), None)
        if not rail or not visible(records, label, rail["hwnd"]):
            self.unavailable("open-" + label.lower(), "The navigation control is not present in this installed build")
            return
        self.nav(label, (), timeout=15)

    def dialog_text(self, name: str, button: str, dialog_title: str, value: str,
                    postcondition) -> bool:
        """Use a discovered native edit control, then inspect an independent result."""
        row = {"name": name, "status": "unverified", "action": button,
               "dialog_title": dialog_title}
        self.rows.append(row)
        try:
            before = self.app.probe()
            self.checked_header(before)
            row["before_visible"] = visible_labels(before)
            row["before_image"] = self.capture(name + "-before", self.app.main)
            target = visible(before, button, self.app.main)
            if not target:
                raise RuntimeError(f"No on-screen button labelled {button!r}")
            old_dialogs = {w["handle"] for w in self.app.windows() if w["class"] == "#32770"}
            input_at = time.monotonic()
            self.runner.click_control(target.get("top") or self.app.main, target)
            dialog = self.app.wait(lambda w: w["class"] == "#32770"
                                   and w["handle"] not in old_dialogs
                                   and dialog_title.lower() in w["title"].lower(),
                                   12, dialog_title)
            row["dialog_image"] = self.capture(name + "-dialog", dialog["handle"])
            children = cheap("list_child_windows", hwnd=dialog["handle"])["children"]
            edits = [c for c in children if c["class"].lower() == "edit" and c.get("visible")]
            if not edits:
                raise RuntimeError("The dialog exposes no visible native edit control")
            # In a file picker the filename edit is below its search field.
            edit = max(edits, key=lambda c: c.get("top", 0))
            cheap("win_set_control_text", hwnd=edit["handle"], text=value)
            cheap("win_send_keys", hwnd=dialog["handle"], keys=["enter"])
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                if all(w["handle"] != dialog["handle"] for w in self.app.windows()):
                    break
                time.sleep(0.4)
            else:
                buttons = [c for c in cheap("list_child_windows", hwnd=dialog["handle"])["children"]
                           if c["class"].lower() == "button" and c.get("visible")
                           and c.get("text", "").replace("&", "").strip().lower() in ("ok", "save", "open")]
                if len(buttons) != 1:
                    raise RuntimeError("The dialog stayed open and has no unique submit button")
                submit = buttons[0]
                cheap("mouse_click", hwnd=dialog["handle"],
                      x=submit["left"] + submit["width"] // 2,
                      y=submit["top"] + submit["height"] // 2)
                deadline = time.monotonic() + 12
                while time.monotonic() < deadline:
                    if all(w["handle"] != dialog["handle"] for w in self.app.windows()):
                        break
                    time.sleep(0.4)
                else:
                    raise RuntimeError("The dialog remained open after its submit button was clicked")
            row["input_to_ready_ms"] = round((time.monotonic() - input_at) * 1000)
            row["after_image"] = self.capture(name + "-after", self.app.main)
            after = self.app.probe()
            row["after_visible"] = visible_labels(after)
            evidence = postcondition(after)
            if evidence is None:
                raise RuntimeError("The expected result did not appear after dialog submission")
            row["result"] = evidence
            row["status"] = "probe_confirmed"
            row["visual_review"] = "pending"
            return True
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"
            return False

    def workspace_roundtrip(self) -> None:
        title = "Hosted verification workspace"
        output_file = self.workspace_scratch_file()
        self.click("Overview", ("Rename workspace",), timeout=10)
        renamed = self.dialog_text(
            "rename-workspace", "Rename workspace", "Workspace title", title,
            lambda records: {"title": title} if visible(records, title, self.app.main) else None)
        if not renamed:
            self.unavailable("workspace-save-reopen", "Workspace rename did not establish a distinct source state")
            return
        saved = self.dialog_text(
            "save-workspace", "Save workspace", "Save workspace", str(output_file),
            lambda _records: self.workspace_file_evidence(output_file))
        if not saved:
            self.unavailable("workspace-reopen", "No verified workspace file was saved")
            return
        self.click("New workspace", ("Rename workspace",), timeout=10)
        if visible(self.app.probe(), title, self.app.main):
            self.unavailable("workspace-reopen", "New workspace did not remove the prior title")
            return
        self.dialog_text(
            "reopen-workspace", "Open workspace", "Open workspace", str(output_file),
            lambda records: {"title": title, "file_sha256": sha256(output_file)}
            if visible(records, title, self.app.main) else None)

    def workspace_scratch_file(self) -> Path:
        """Keep the editable fixture outside the encrypted tuple root."""
        profile = Path(self.app.datadir)
        if profile.name != "profile" or not profile.is_dir():
            raise RuntimeError("Workspace scratch requires an owned isolated profile")
        candidate = profile.parent / "fixture.bambu-workspace"
        if candidate.exists() or candidate.is_symlink():
            raise RuntimeError("Workspace scratch fixture already exists")
        return candidate

    def open_project_file(self, path: Path) -> bool:
        """Exercise File > Open Project using a checked-in, non-private 3MF."""
        row = {"name": "file-menu-open-project", "status": "unverified",
               "fixture_sha256": sha256(path) if path.is_file() else None,
               "action": "File > Open Project"}
        self.rows.append(row)
        if not path.is_file():
            row["status"] = "blocked"
            row["reason"] = "The checked-in 3MF fixture is absent"
            return False
        try:
            expected_names = expected_3mf_objects(path)
            row["expected_object_count"] = len(expected_names)
            row["expected_names"] = expected_names
            before = self.app.probe()
            row["before_header"] = self.checked_header(before)
            row["before_visible"] = visible_labels(before)
            before_model = model_snapshot(before, pid=self.app.pid,
                                          profile_tag=Path(self.app.datadir).name,
                                          main_hwnd=self.app.main)
            row["before_model_evidence"] = before_model
            row["before_image"] = self.capture("file-open-before", self.app.main)
            old_dialogs = {w["handle"] for w in self.app.windows() if w["class"] == "#32770"}
            start = time.monotonic()
            self.app.command("invoke Open Project")
            dialog = self.app.wait(lambda w: w["class"] == "#32770"
                                   and w["handle"] not in old_dialogs
                                   and w["width"] >= 400, 20, "Open Project dialog")
            row["dialog_image"] = self.capture("file-open-dialog", dialog["handle"])
            children = cheap("list_child_windows", hwnd=dialog["handle"])["children"]
            edits = [c for c in children if c["class"].lower() == "edit" and c.get("visible")]
            if not edits:
                raise RuntimeError("Open Project dialog has no visible native file edit")
            edit = max(edits, key=lambda c: c.get("top", 0))
            cheap("win_set_control_text", hwnd=edit["handle"], text=str(path))
            cheap("win_send_keys", hwnd=dialog["handle"], keys=["enter"])
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                if all(w["handle"] != dialog["handle"] for w in self.app.windows()):
                    break
                time.sleep(0.5)
            else:
                raise RuntimeError("Open Project dialog did not close after file submission")
            after, result = self.wait_fixture_loaded(path, expected_names, before_model)
            row["after_header"] = self.checked_header(after)
            row["after_visible"] = visible_labels(after)
            row["after_image"] = self.capture("file-open-after", self.app.main)
            row["input_to_ready_ms"] = round((time.monotonic() - start) * 1000)
            row["result"] = result
            row["status"] = "probe_confirmed" if result else "unverified"
            if row["status"] == "unverified":
                row["reason"] = "The frame survived File > Open, but no complete owned fixture model transition appeared"
            return row["status"] == "probe_confirmed"
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"
            return False

    def wait_fixture_loaded(self, path: Path, expected_names: list[str],
                            before_model: dict | None,
                            timeout: float = 60) -> tuple[list[dict], dict | None]:
        deadline = time.monotonic() + timeout
        after = []
        while time.monotonic() < deadline:
            if not any(w["handle"] == self.app.main for w in self.app.windows()):
                raise RuntimeError("The main frame exited during 3MF loading")
            after = self.app.probe()
            self.checked_header(after)
            after_model = model_snapshot(after, pid=self.app.pid,
                                         profile_tag=Path(self.app.datadir).name,
                                         main_hwnd=self.app.main)
            result = fixture_model_transition(before_model, after_model, expected_names, path)
            if result:
                result["slice_control_enabled"] = any(
                    r.get("kind") in ("window", "tool") and r.get("shown")
                    and r.get("on_screen") and r.get("enabled")
                    and any("slice plate" in str(r.get(key) or "").lower()
                            for key in ("name", "label")) for r in after)
                return after, result
            time.sleep(0.5)
        return after, None

    @staticmethod
    def workspace_file_evidence(path: Path) -> dict | None:
        if not path.is_file() or path.stat().st_size < 100:
            return None
        with zipfile.ZipFile(path) as bundle:
            names = set(bundle.namelist())
            if "Metadata/workspace.json" not in names:
                return None
            manifest = json.loads(bundle.read("Metadata/workspace.json"))
            if manifest.get("title") != "Hosted verification workspace":
                return None
        return {"file_sha256": sha256(path), "manifest_title": manifest["title"]}

    def prepare_tabs(self) -> None:
        l = self.labels
        self.nav(l["prepare"], (l["ink"], l["process"], l["objects"]), timeout=30)
        for name, marker in ((l["ink"], l["ink_search"]), (l["process"], l["settings"]),
                             (l["objects"], "Search plate, object and part.")):
            self.click(name, (marker,), scope="Sidebar", timeout=10,
                       require_change=(name != l["ink"]))

    def narrow_prepare(self) -> None:
        row = {"name": "narrow-prepare-tabs", "status": "unverified",
               "requested_size": [1000, 600]}
        self.rows.append(row)
        try:
            frame, client = self.resize_client_exact((1000, 600))
            records = self.app.probe()
            row["header"] = self.checked_header(records)
            row["actual_client_size"] = [client["w"], client["h"]]
            row["outer_frame_size"] = [frame["width"], frame["height"]]
            sidebar = next((r for r in records if r.get("kind") == "window"
                            and r.get("name") == "Sidebar" and r.get("on_screen")), None)
            if not sidebar:
                raise RuntimeError("Sidebar is not on screen after narrow resize")
            tabs = []
            for name in (self.labels["ink"], self.labels["process"], self.labels["objects"]):
                tab = visible(records, name, sidebar["hwnd"])
                if not tab:
                    raise RuntimeError(f"Prepare tab {name!r} is not on screen at narrow width")
                if any(tab.get(flag) for flag in ("text_clipped", "clipped_by_parent", "starved", "zero_sized")):
                    raise RuntimeError(f"Prepare tab {name!r} has a layout defect at narrow width")
                tabs.append({"label": name, "screen": tab["screen"]})
            row["tabs"] = tabs
            row["image"] = self.capture("narrow-prepare-tabs", self.app.main)
            row["status"] = "probe_confirmed"
            row["visual_review"] = "pending"
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"

    def run_localized(self) -> None:
        self.prepare_tabs()
        self.narrow_prepare()
        self.unavailable("localized-print-workspace-device-flows",
                         "This language tuple drove Prepare only; the remaining flows require their own localized action ledger")

    def run(self):
        self.observe("installed-shell", lambda: None, ("Home",), self.app.main)
        self.rows[-1]["status"] = "capture_only"
        self.rows[-1]["reason"] = "A generic shell label does not prove a feature"
        project_fixture = Path(__file__).resolve().parents[2] / "resources" / "calib" / "filament_flow" / "flowrate-test-pass1.3mf"
        if not self.open_project_file(project_fixture):
            self.unverified("recent-project-tile", "A fixture-specific File > Open result was not established, so the recent tile was not clicked")
            return
        self.unverified("recent-project-tile", "The recent tile is inside a webview and has no verified cheap-route target yet")
        self.prepare_tabs()
        self.narrow_prepare()
        self.resize_client_exact((1200, 800))
        fixture = Path(__file__).resolve().parents[2] / ".claude" / "skills" / "run-bambustudio" / "cube.stl"
        if fixture.is_file():
            loaded = self.observe("load-print-fixture", lambda: self.command_at(f"load {fixture}"),
                                  ("cube",), self.app.main, timeout=30, require_change=True,
                                  require_new_marker=True)
            if loaded:
                self.click("Slice plate", ("Print plate",), timeout=120, require_enabled=True)
            else:
                self.unavailable("slice-print-fixture", "The cube load was not confirmed in the layout probe")
        else:
            self.unavailable("load-print-fixture", "The checked-in cube STL fixture is unavailable")
        self.nav("Preview", ("Print plate",), timeout=30)
        self.unavailable("print-and-nozzle-send",
                         "Preview print affordance was inspected; no paired dual-nozzle printer is supplied and no send action was attempted")
        self.nav("Device", (), timeout=30)
        device_records = self.app.probe()
        device_controls = [label for label in visible_labels(device_records)
                           if any(part in label.lower() for part in ("camera", "liveview", "live view", "lan", "nozzle"))]
        device_image = self.capture("device-unpaired-state", self.app.main)
        self.rows.append({"name": "device-no-hardware-control-inventory", "status": "unavailable",
                          "candidate_controls": device_controls, "image": device_image,
                          "source_control": "Play or stop the camera live view",
                          "camera_control_visible": any("play or stop the camera live view" in label.lower()
                                                        for label in device_controls),
                          "reason": "The no-printer message is custom drawn; visible controls and pixels require review and do not prove a paired printer, transfer, or video stream"})
        self.optional_nav("Multi-device")
        self.unavailable("lan-transfer-and-cancel",
                         "Unpaired Device state and optional Multi-device navigation were inspected; pairing, transfer and cancellation need a LAN printer")
        self.unavailable("camera-autoplay",
                         "Camera control exposure was inspected without clicking Play; autoplay and streaming need a paired camera")
        self.nav("Project", ("Workspace",), timeout=30)
        self.click("Workspace", ("New workspace", "Open workspace", "Save workspace"), timeout=20)
        for tab, marker in (("Checklist", "Edit / due date / link"),
                            ("Calendar", "Add planned print"), ("Files", "Add project 3MF")):
            self.click(tab, (marker,), timeout=10)
        self.workspace_roundtrip()
        self.unavailable("project-portable-history",
                         "Project version history requires a saved 3MF member; the isolated workspace round trip does not prove embedded history")
        self.observe("open-model-creator", lambda: self.command_at("invoke Model Creator"),
                     ("Describe the model", "Provider executable path"), None,
                     timeout=30, require_change=True)
        self.unavailable("model-generation",
                         "Provider sign-in and renderer execution were not supplied; no generation or plate import was attempted")
        self.unavailable("localization-three-modes",
                         "See the separate en, yue_HK, and bilingual_en_yue_HK profile reports")

    def command_at(self, text: str) -> float:
        input_at = time.monotonic()
        self.app.command(text)
        return input_at


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--install-receipt", type=Path, required=True)
    ap.add_argument("--source-commit", required=True)
    ap.add_argument("--verification-commit", required=True)
    ap.add_argument("--release-tag", required=True)
    ap.add_argument("--hosted-run-id", required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--language", choices=MODES, default="en")
    ap.add_argument("--theme", choices=("light", "dark"), default="light")
    ap.add_argument("--scale", type=float, default=1.0)
    ap.add_argument("--viewport", choices=("1200x800", "1000x600"), default="1200x800")
    ap.add_argument("--scope", choices=("diagnostic", "behavior", "layout"), default="behavior")
    args = ap.parse_args()
    source = args.source_commit.lower()
    verifier = args.verification_commit.lower()
    if not SHA.fullmatch(source) or not SHA.fullmatch(verifier) or not re.fullmatch(r"md3-v\d+", args.release_tag):
        ap.error("source commit, verification commit or release tag is malformed")
    if args.scale not in (1.0, 1.25, 1.5, 2.0):
        ap.error("scale must be one of 1.0, 1.25, 1.5, 2.0")
    if args.scope == "behavior" and (args.language, args.theme, args.scale, args.viewport) != (
            "en", "light", 1.0, "1200x800"):
        ap.error("the full behavior ledger is bound to the English light 100% 1200x800 baseline")
    if not re.fullmatch(r"[0-9]{1,20}", args.hosted_run_id) or args.hosted_run_id != os.environ.get("GITHUB_RUN_ID"):
        ap.error("hosted run ID must match the current runner")
    if args.output.exists():
        ap.error("output directory already exists; this drive never overwrites prior evidence")
    receipt = json.loads(args.install_receipt.read_text(encoding="utf-8-sig"))
    validate_installation(receipt, args.exe, source, args.release_tag)
    validate_verifier(verifier)
    runner_temp = os.environ.get("RUNNER_TEMP")
    if not runner_temp or not args.output.resolve().is_relative_to(Path(runner_temp).resolve()):
        ap.error("output must be a new child of the disposable runner's temporary directory")
    args.output.mkdir(parents=True)
    scratch = Path(tempfile.mkdtemp(prefix="bsbehavior-private-", dir=runner_temp))
    datadir = scratch / "profile"
    probe_dir = scratch / "probe"
    datadir.mkdir()
    probe_dir.mkdir()
    seed_profile(datadir, args.language, args.theme)
    if args.scope == "diagnostic":
        return run_startup_comparison(args, receipt, source, verifier, scratch)
    desktop = "bsbehavior-" + str(os.getpid())
    app = HostedApp(str(args.exe), str(datadir), desktop, str(probe_dir))
    app.holder_lifetime = 1800 if args.scope == "behavior" else 900 if args.scope == "layout" else 360
    requested_size = tuple(int(part) for part in args.viewport.split("x"))
    drive = Drive(app, args.output, source, args.release_tag,
                  receipt["installed_exe_sha256"], args.hosted_run_id, args.language,
                  verifier, args.theme, args.scale, requested_size)
    cleanup_error = None
    try:
        app.start()
        frame = next((w for w in app.windows() if w["handle"] == app.main), {})
        drive.identity["window"] = {key: frame.get(key) for key in ("title", "class", "width", "height")}
        drive.identity["process"] = {"initial_pid": app.launch_pid,
                                     "selected_pid": app.pid,
                                     "relaunched": app.pid != app.launch_pid}
        frame, client = drive.resize_client_exact(requested_size)
        expected_dpi = round(96 * args.scale)
        if frame.get("dpi") != expected_dpi:
            raise RuntimeError("Native window DPI differs from requested scale")
        drive.identity["measured_tuple"] = {**drive.checked_header(app.probe()),
                                             "viewport": [client["w"], client["h"]],
                                             "outer_frame": [frame["width"], frame["height"]],
                                             "native_dpi": frame["dpi"]}
        if args.scope == "diagnostic":
            image = drive.capture("installed-shell-diagnostic", app.main)
            drive.rows.append({"name": "installed-shell-diagnostic", "status": "capture_only",
                               "image": image, "reason": "Launch and a rendered frame do not prove behavior"})
        elif args.scope == "layout":
            drive.run_localized()
        else:
            drive.run()
    except Exception as exc:
        drive.rows.append({"name": "launch-or-drive", "status": "blocked",
                           "reason": f"{type(exc).__name__}: {exc}"})
    finally:
        try:
            app.stop()
        except Exception as exc:
            cleanup_error = f"{type(exc).__name__}: {exc}"
    drive.identity["process"] = {
        "helper_pid": app.helper_pid,
        "helper_executable_sha256": app.helper_exe_hash,
        "initial_pid": app.launch_pid,
        "selected_pid": app.pid,
        "relaunched": bool(app.launch_pid and app.pid and app.pid != app.launch_pid),
        "launch_started_at_utc": app.launch_started.isoformat() if app.launch_started else None,
        "run_ended_at_utc": app.finished_at.isoformat() if app.finished_at else None,
        "owned_processes": sorted(app.seen_owned.values(), key=lambda item: item["pid"]),
        "cleanup_killed_pids": app.cleanup_killed_pids,
        "natural_exit_observed_before_cleanup": app.natural_exit_observed_before_cleanup,
        "startup_state": app.startup_state,
        "desktop_error": app.desktop_error,
        "launch_exit_code": app.launch_exit_code,
        "exit_code_status": "unavailable from holder" if app.launch_exit_code is None else "observed",
        "app_exited_at_utc": (app.holder_receipt or {}).get("app_exited_at_utc"),
        "holder_status": (app.holder_receipt or {}).get("status"),
        "holder_deadline_seconds": (app.holder_receipt or {}).get("holder_deadline_seconds"),
        "holder_deadline_fired": (app.holder_receipt or {}).get("deadline_fired"),
        "app_exit_confirmed": (app.holder_receipt or {}).get("app_exit_confirmed"),
    }
    logs = []
    wer_status = "not_checked"
    try:
        logs.extend(preserve_logs(datadir, args.output))
        holder_log = preserve_holder_receipt(app.holder_receipt_path, args.output)
        if holder_log is not None:
            logs.append(holder_log)
        logs.extend(preserve_holder_streams(app, args.output))
    except Exception as exc:
        drive.rows.append({"name": "restricted-log-preservation", "status": "blocked",
                           "reason": f"{type(exc).__name__}: {exc}"})
    try:
        wer_logs, wer_status = preserve_wer(str(args.exe), app, args.output)
        logs.extend(wer_logs)
    except Exception as exc:
        wer_status = "diagnostic_collection_failed"
        drive.rows.append({"name": "restricted-wer-preservation", "status": "blocked",
                           "reason": f"{type(exc).__name__}: {exc}"})
    failed_rows = [r["name"] for r in drive.rows if r["status"] in ("blocked", "unverified")]
    verdict = "blocked" if cleanup_error or failed_rows else ("diagnostic_only" if args.scope == "diagnostic" else "pending_visual_review")
    report = {"schema": 2, **drive.identity, "scope": args.scope,
              "package_version": receipt["package_version"], "runner": "github-hosted-windows",
              "desktop": desktop,
              "rows": drive.rows, "images": drive.images, "restricted_logs": logs,
              "wer_status": wer_status,
              "failed_rows": failed_rows,
              "privacy": "restricted; inspect pixels and metadata before publication",
              "cleanup": "verified" if cleanup_error is None else "failed: " + cleanup_error,
              "verdict": verdict}
    (args.output / "behavior-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 2 if verdict == "blocked" else 0


if __name__ == "__main__":
    sys.exit(main())
