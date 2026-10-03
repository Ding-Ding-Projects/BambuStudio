#!/usr/bin/env python3
"""Fixed hosted native driver adapter with exact Job membership observations.

This is not a general action runner. All executable and evidence locations are
derived from the current hosted invocation; the request contains only bounded
identities, hashes and predefined runtime choices. Its receipt stays private.
"""
from __future__ import annotations

import ctypes
from ctypes import wintypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import sys


def require(condition):
    if not condition:
        raise RuntimeError("Scaled native adapter contract unavailable")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def strict_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result)
        result[key] = value
    return result


def read_json(path, limit):
    require(path.is_file() and 0 < path.stat().st_size <= limit)
    return json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=strict_object)


def plain_path(path, root=None):
    """Reject reparse points at every existing ancestor, including the root."""
    absolute = Path(os.path.abspath(path))
    for item in (absolute, *absolute.parents):
        if item.exists():
            require(not (item.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT))
    require(absolute.resolve() == absolute)
    if root is not None:
        require(absolute != root and absolute.is_relative_to(root))
    return absolute


def main():
    require(os.environ.get("GITHUB_ACTIONS") == "true"
            and os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted"
            and os.environ.get("RUNNER_OS") == "Windows")
    require(len(sys.argv) == 3 and sys.argv[1] in ("--job-name", "--validate-request"))
    validate_only = sys.argv[1] == "--validate-request"
    job_name = sys.argv[2]
    require(re.fullmatch(r"Local\\BambuNativeScale-[0-9a-f]{64}", job_name))
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    require(re.fullmatch(r"[0-9]{1,24}", run_id))
    root = plain_path(Path(os.environ["RUNNER_TEMP"]))
    request_path = plain_path(root / f"native-scale-request-{run_id}.json", root)
    receipt_path = plain_path(root / f"native-scale-adapter-{run_id}.json", root)
    require(not receipt_path.exists())
    request = read_json(request_path, 8192)
    fields = {"schema", "request_id", "source_commit", "release_tag", "run_id", "scope",
              "language", "theme", "viewport", "scale_percent", "exe_sha256", "cli_sha256",
              "install_sha256", "driver_sha256", "adapter_sha256", "verifier_sha256",
              "python_sha256", "cheap_sha256", "helper_sha256", "containment_sha256", "job_name",
              "resolution", "minimum_sha256", "display_mode_sha256"}
    require(isinstance(request, dict) and set(request) == fields)
    require(type(request["schema"]) is int and request["schema"] == 1)
    require(type(request["scale_percent"]) is int and request["scale_percent"] in (100, 125, 150, 200))
    require(all(isinstance(request[key], str) and len(request[key]) <= 128
                for key in fields - {"schema", "scale_percent"}))
    require(re.fullmatch(r"[0-9a-f]{32}", request["request_id"]))
    require(re.fullmatch(r"[0-9a-f]{40}", request["source_commit"]))
    require(re.fullmatch(r"md3-v[0-9]{1,12}", request["release_tag"]))
    require(request["run_id"] == run_id and request["job_name"] == job_name)
    require(request["scope"] in ("menus", "vocabulary", "vocabulary-persistence", "slice-controls",
                                  "combined-print", "combined-send", "cancellation", "minimum-resize"))
    require(request["language"] in ("en", "yue_HK", "bilingual_en_yue_HK"))
    require(request["theme"] in ("light", "dark"))
    require(request["viewport"] in ("1200x800", "1000x600", "measured-minimum"))
    minimum = request["scope"] == "minimum-resize"
    require((minimum and request["resolution"] == "1920x1080" and request["scale_percent"] == 100
             and request["viewport"] == "measured-minimum") or
            (not minimum and request["resolution"] == "unchanged" and request["scale_percent"] in (125, 150, 200)))
    for key in fields:
        if key.endswith("_sha256"):
            require(re.fullmatch(r"[0-9a-f]{64}", request[key]))
    here = plain_path(Path(__file__).parent)
    driver_path = plain_path(here.parent / "md3" / "drive-native-interface.py")
    verifier_path = plain_path(here / "Verify-HostedNativeInterface.ps1")
    python = plain_path(root / f"automation-python-{run_id}" / "Scripts" / "python.exe", root)
    cheap = plain_path(python.parent / "lowlevel-computer-use-cheap.exe", root)
    require(plain_path(Path(sys.executable)) == python)
    require(plain_path(Path(os.environ.get("LLCU_CHEAP", ""))) == cheap)
    raw = plain_path(root / f"native-interface-restricted-{run_id}", root)
    install_path = plain_path(raw / "install.json", root)
    runtime_path = plain_path(raw / "runtime.json", root)
    require(not runtime_path.exists() and set(raw.iterdir()) == {install_path})
    install = read_json(install_path, 1048576)
    version = install.get("package_version")
    require(isinstance(version, str) and re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?", version))
    version_root = plain_path(Path(os.environ["LOCALAPPDATA"]) / "BambuStudioMD3" / f"app-{version}")
    exe = plain_path(version_root / "bambu-studio.exe", version_root)
    cli = plain_path(version_root / "automation" / "bambu-automation.exe", version_root)
    paths = {"exe": exe, "cli": cli, "install": install_path, "driver": driver_path,
             "adapter": Path(__file__), "verifier": verifier_path, "python": python, "cheap": cheap,
             "helper": here / "Invoke-HostedDisplayScale.ps1", "containment": here / "HostedScaleProcess.cs",
             "minimum": here.parent / "md3" / "minimum_resize.py", "display_mode": here / "HostedDisplayMode.cs"}
    for key, path in paths.items():
        require(digest(plain_path(path)) == request[key + "_sha256"])
    request_hash = digest(request_path)
    scale_output = plain_path(root / f"native-scale-{run_id}", root)
    started_path = plain_path(scale_output / "native-input.started", root)
    restored_path = plain_path(scale_output / "native-input.restored", root)
    restored_temp = plain_path(scale_output / "native-input.restored.tmp", root)
    if minimum:
        require(not restored_path.exists() and not restored_temp.exists())
        if validate_only:
            require(not started_path.exists())
        else:
            require(started_path.is_file() and started_path.stat().st_size == 64
                    and started_path.read_text(encoding="ascii") == request_hash)
    if validate_only:
        return 0  # No product launch, UI access, Job query or receipt mutation.

    native = ctypes.WinDLL("kernel32", use_last_error=True)
    native.OpenJobObjectW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
    native.OpenJobObjectW.restype = wintypes.HANDLE
    native.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    native.OpenProcess.restype = wintypes.HANDLE
    native.IsProcessInJob.argtypes = [wintypes.HANDLE, wintypes.HANDLE, ctypes.POINTER(wintypes.BOOL)]
    native.IsProcessInJob.restype = wintypes.BOOL
    native.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD,
                                                wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    native.QueryFullProcessImageNameW.restype = wintypes.BOOL
    native.CloseHandle.argtypes = [wintypes.HANDLE]
    # These are real access attempts against this invocation's object. A
    # privileged or unexpectedly permissive environment is unavailable here.
    for access in (0x40000, 0x20000, 0x2, 0x1, 0x8):
        unexpected = native.OpenJobObjectW(access, False, job_name)
        reason = ctypes.get_last_error()
        if unexpected:
            native.CloseHandle(unexpected)
        require(not unexpected and reason == 5)  # ACCESS_DENIED, not a missing object.
    seen_holders, seen_products = set(), set()
    membership_failed = False

    def member(pid, expected, seen):
        nonlocal membership_failed
        job = native.OpenJobObjectW(4, False, job_name)  # JOB_OBJECT_QUERY only.
        handle = native.OpenProcess(0x1000, False, int(pid))
        try:
            require(job and handle)
            contained = wintypes.BOOL()
            require(native.IsProcessInJob(handle, job, ctypes.byref(contained)) and contained.value)
            name = ctypes.create_unicode_buffer(32768)
            size = wintypes.DWORD(len(name))
            require(native.QueryFullProcessImageNameW(handle, 0, name, ctypes.byref(size)))
            require(plain_path(Path(name.value)) == plain_path(expected))
            seen.add(int(pid))
        except Exception:
            membership_failed = True
            raise
        finally:
            if handle:
                native.CloseHandle(handle)
            if job:
                native.CloseHandle(job)

    result = {"schema": 1, "request_id": request["request_id"], "request_sha256": digest(request_path),
              "status": "failed", "holder_membership_count": 0, "product_membership_count": 0}
    code = 1
    try:
        member(os.getpid(), python, set())
        sys.path.insert(0, str(driver_path.parent))
        spec = importlib.util.spec_from_file_location("scaled_native_driver", driver_path)
        driver = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(driver)
        original = driver.behavior.HostedApp

        class ContainedApp(original):
            def launch_holder(self):
                plain_path(Path(self.datadir), root)
                super().launch_holder()
                member(self.helper_pid, driver.behavior.helper_python_executable(), seen_holders)
                member(self.launch_pid, exe, seen_products)

            def windows(self):
                windows = super().windows()
                for window in windows:
                    member(window["process_id"], exe, seen_products)
                return windows

        driver.behavior.HostedApp = ContainedApp
        scale = {100: "1", 125: "1.25", 150: "1.5", 200: "2"}[request["scale_percent"]]
        sys.argv = [str(driver_path), "--exe", str(exe), "--cli", str(cli),
                    "--install-receipt", str(install_path), "--source-commit", request["source_commit"],
                    "--release-tag", request["release_tag"], "--output", str(raw), "--scope", request["scope"],
                    "--language", request["language"], "--theme", request["theme"], "--scale", scale,
                    "--viewport", request["viewport"]]
        if minimum:
            sys.argv += ["--minimum-job-name", job_name]
        require(driver.main() == 0)
        runtime = read_json(runtime_path, 33554432)
        require(not membership_failed and seen_holders and seen_products)
        require(runtime.get("status") == "runtime_verified" and runtime.get("teardown_verified") is True)
        for key in ("source_commit", "release_tag", "run_id", "scope", "exe_sha256", "cli_sha256", "driver_sha256"):
            require(runtime.get(key) == request[key])
        require(runtime.get("install_receipt_sha256") == request["install_sha256"])
        require(runtime.get("requested_tuple") == {"language": request["language"], "theme": request["theme"],
                                                  "scale": request["scale_percent"] / 100.0,
                                                  "viewport": request["viewport"]})
        require(digest(request_path) == request_hash)
        if minimum:
            operations = [row for row in runtime.get("operations", [])
                          if row.get("operation") == "interactive-minimum-resize"]
            require(len(operations) == 1 and operations[0].get("status") == "interactive_clamp_observed")
            operation = operations[0]
            require(all(operation.get(key) is True for key in ("frame_restored", "input_desktop_restored",
                        "mouse_release_verified", "final_button_up_verified", "server_exit_verified")))
            require(operation.get("disposal_required") is False)
            require(runtime.get("minimum_helper_sha256") == request["minimum_sha256"])
            # The started marker is never removed. Restoration is a separate
            # atomic, invocation-bound proof, never inferred from Job exit.
            require(started_path.read_text(encoding="ascii") == request_hash and not restored_path.exists())
            with restored_temp.open("x", encoding="ascii") as stream:
                stream.write(request_hash)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(restored_temp, restored_path)
        result["runtime_sha256"] = digest(runtime_path)
        result["status"] = "runtime_and_membership_verified"
        code = 0
    except Exception:
        pass  # Raw exceptions and process identities are never printed.
    finally:
        result["holder_membership_count"] = len(seen_holders)
        result["product_membership_count"] = len(seen_products)
        with receipt_path.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, sort_keys=True)
    return code


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        raise SystemExit(2)
