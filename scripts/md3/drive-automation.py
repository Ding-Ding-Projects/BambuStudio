#!/usr/bin/env python3
"""Verify packaged automation on an owned hidden desktop, with restricted evidence."""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
import zipfile
from datetime import datetime, timezone

from PIL import Image, ImageStat
from recapture import cheap
from hosted_process import owned_process_inventory, process_snapshot

# Reuse the established holder, process identity proof and native profile encoding.
spec = importlib.util.spec_from_file_location("packaged_behavior", Path(__file__).with_name("drive-packaged-behavior.py"))
behavior = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = behavior
spec.loader.exec_module(behavior)


class Driver:
    def __init__(self, args, app, workspace):
        self.args, self.app, self.workspace = args, app, workspace
        self.rows, self.images = [], []

    def protocol(self, denied_path):
        completed = subprocess.run([sys.executable, str(Path(__file__)), "--pipe-probe", str(self.app.pid), str(denied_path)],
                                   capture_output=True, text=True, timeout=20,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        require(completed.returncode == 0 and completed.stdout.strip() == "protocol_verified",
                "Packaged native pipe version and request identity were not verified")
        self.rows.append({"operation": "native_pipe_protocol", "status": "verified", "version": 1})
        self.rows.append({"operation": "native_workspace_rejection", "status": "verified", "expected_error": "path_denied"})

    def capture(self, label):
        frames = [w for w in self.app.windows() if w["handle"] == self.app.main]
        if len(frames) != 1 or int(frames[0]["process_id"]) != self.app.pid:
            raise RuntimeError("Capture window is no longer owned by this run")
        filename = f"{len(self.images):03d}-{label}.png"
        path = self.args.output / filename
        cheap("screenshot", hwnd=self.app.main, output_path=str(path))
        with Image.open(path) as image:
            image.verify()
        with Image.open(path) as image:
            rgb = image.convert("RGB")
            if rgb.width < 700 or rgb.height < 500:
                raise RuntimeError("Captured native frame is undersized")
            rgb.thumbnail((128, 128))
            if max(ImageStat.Stat(rgb).stddev) < 10:
                raise RuntimeError("Captured native frame is blank or uniform")
        self.images.append({"file": filename, "sha256": behavior.sha256(path),
                            "captured_at_utc": datetime.now(timezone.utc).isoformat(),
                            "privacy": "restricted_pixel_review_pending",
                            "window": {k: frames[0].get(k) for k in ("width", "height", "dpi", "class")}})

    def call(self, operation, arguments=None, *, error_code=None):
        # Revalidate exact executable, profile, launch timestamp and PID for every command.
        inventory = owned_process_inventory(process_snapshot(), exe=self.app.exe,
                                            datadir=self.app.datadir,
                                            launched_at=self.app.launch_started,
                                            launch_pid=self.app.launch_pid)
        if self.app.pid not in {p["pid"] for p in inventory}:
            raise RuntimeError("Automation target process identity is no longer owned")
        command = [str(self.args.cli), "command", operation, "--arguments",
                   json.dumps(arguments or {}, separators=(",", ":")), "--json",
                   "--workspace", str(self.workspace), "--instance", str(self.app.pid)]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=90,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        if len(completed.stdout) > 1_048_576 or len(completed.stderr) > 65536:
            raise RuntimeError("CLI response exceeds bounded output limits")
        envelope = json.loads(completed.stdout)
        if error_code:
            if completed.returncode != 1 or envelope.get("ok") is not False or envelope.get("error", {}).get("code") != error_code:
                raise RuntimeError("CLI did not reject the forbidden workspace path")
            result = {}
        else:
            if completed.returncode != 0 or envelope.get("ok") is not True or not isinstance(envelope.get("result"), dict):
                raise RuntimeError("Packaged CLI operation did not return a successful object")
            result = envelope["result"]
        self.rows.append({"operation": operation, "status": "verified", "expected_error": error_code})
        return result

    def headless_slice(self, project):
        output = self.workspace / "headless-sliced.3mf"
        command = [str(self.args.cli), "command", "slice_start", "--arguments",
                   json.dumps({"headless": True, "path": str(project), "output": str(output),
                               "plate": 0, "overwrite": False}),
                   "--json", "--workspace", str(self.workspace)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=420,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        require(len(result.stdout) <= 1_048_576 and len(result.stderr) <= 65536,
                "Headless CLI output exceeds bounded limits")
        envelope = json.loads(result.stdout)
        job = envelope.get("result", {})
        require(result.returncode == 0 and envelope.get("ok") is True and
                job.get("state") == "completed" and job.get("exitCode") == 0,
                "Isolated headless slice did not complete")
        require(output.is_file() and zipfile.is_zipfile(output), "Headless output is not a real 3MF archive")
        require(job.get("sha256") == behavior.sha256(output), "Headless output hash differs from CLI receipt")
        with zipfile.ZipFile(output) as archive:
            entries = [e for e in archive.infolist() if e.filename.endswith(".gcode")]
            require(entries and all(e.file_size > 0 for e in entries), "Headless output contains no nonempty toolpath")
            require(sum(e.file_size for e in entries) <= 64_000_000, "Headless toolpath exceeds fixture bound")
            for entry in entries:
                with archive.open(entry) as stream:
                    while stream.read(65536):
                        pass
        self.rows.append({"operation": "slice_start_headless", "status": "verified", "output_sha256": behavior.sha256(output)})


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ("exe", "cli", "install-receipt", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--release-tag", required=True)
    args = parser.parse_args()
    require(os.environ.get("GITHUB_ACTIONS") == "true" and os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted",
            "Only disposable hosted execution is authorized")
    require(behavior.SHA.fullmatch(args.source_commit) is not None, "Source SHA malformed")
    receipt = json.loads(args.install_receipt.read_text(encoding="utf-8-sig"))
    behavior.validate_installation(receipt, args.exe, args.source_commit, args.release_tag)
    root = Path(os.environ["RUNNER_TEMP"]).resolve()
    require(args.output.resolve().is_relative_to(root), "Evidence output escapes runner temporary root")
    scratch = Path(tempfile.mkdtemp(prefix="automation-owned-", dir=root))
    workspace, profile, probe = (scratch / name for name in ("workspace", "profile", "probe"))
    for directory in (workspace, profile, probe):
        directory.mkdir()
    behavior.seed_profile(profile, "en", "light")
    fixture = Path(__file__).parents[2] / "tests/automation-fixtures/cube.stl"
    shutil.copyfile(fixture, workspace / "cube.stl")
    os.environ["BAMBU_AUTOMATION"] = "1"
    os.environ["BAMBU_AUTOMATION_ROOTS"] = str(workspace)
    app = behavior.HostedApp(str(args.exe), str(profile), "bsautomation-" + str(os.getpid()), str(probe))
    app.holder_lifetime = 1200
    drive = Driver(args, app, workspace)
    status = "failed"
    failure = None
    teardown = False
    try:
        app.start(timeout=240)
        drive.capture("native-ready")
        drive.protocol(scratch / "native-outside.3mf")
        require(not (scratch / "native-outside.3mf").exists(), "Native workspace rejection still wrote a file")
        capabilities = drive.call("capabilities")
        required_operations = {"project_inspect", "project_new", "project_open", "project_save",
                               "model_import", "presets_list", "slice_start", "export_file"}
        require(required_operations.issubset(set(capabilities.get("operations", []))), "Native capability inventory is incomplete")
        drive.call("project_new")
        empty = drive.call("project_inspect")
        require(empty.get("objects") == [], "Native new project is not empty")
        drive.capture("empty-project")
        drive.call("model_import", {"path": str(workspace / "cube.stl")})
        imported = drive.call("project_inspect")
        require(len(imported.get("objects", [])) == 1, "Native import did not produce exactly one object")
        drive.capture("imported-cube")
        project = workspace / "roundtrip.3mf"
        drive.call("project_save", {"path": str(project)})
        require(project.is_file() and zipfile.is_zipfile(project), "Native project save produced no real 3MF")
        drive.call("project_new")
        drive.call("project_open", {"path": str(project)})
        reopened = drive.call("project_inspect")
        require(len(reopened.get("objects", [])) == 1, "Native saved project did not reopen with one object")
        drive.capture("reopened-cube")
        drive.call("project_save", {"path": str(scratch / "outside.3mf")}, error_code="outside_workspace")
        require(not (scratch / "outside.3mf").exists(), "Workspace refusal still wrote outside the allowed root")
        presets = drive.call("presets_list")
        # A missing bundled preset is an unresolved runtime result, never a passing skip.
        require(presets.get("printer") and presets.get("print") and presets.get("filament"),
                "Bundled presets unavailable; slicing remains unverified")
        job = drive.call("slice_start")
        job_id = job.get("jobId")
        require(isinstance(job_id, str) and job_id, "Native slice returned no job identity")
        deadline = time.monotonic() + 300
        while time.monotonic() < deadline:
            job = drive.call("job_status", {"jobId": job_id})
            if job.get("state") == "result_available":
                break
            time.sleep(2)
        require(job.get("state") == "result_available", "Native slice produced no result within deadline")
        drive.rows.append({"operation": "slice_job_identity", "status": "unverified",
                           "reason": "native background slice has no operation-correlated completion identity"})
        sliced = workspace / "sliced.gcode.3mf"
        drive.call("export_file", {"path": str(sliced), "overwrite": False})
        require(sliced.is_file() and zipfile.is_zipfile(sliced), "Native sliced archive was not written")
        with zipfile.ZipFile(sliced) as archive:
            entries = [e for e in archive.infolist() if e.filename.endswith(".gcode")]
            require(entries and all(e.file_size > 0 for e in entries), "Sliced 3MF has no nonempty toolpath")
            require(sum(e.file_size for e in entries) <= 64_000_000, "Native toolpath exceeds fixture bound")
            for entry in entries:
                with archive.open(entry) as stream:
                    while stream.read(65536):
                        pass
        drive.capture("sliced-project")
        # The packaged one-shot service waits; no process-local job registry is borrowed.
        drive.headless_slice(project)
        drive.call("printer_list")
        status = "runtime_verified"
    except Exception as exc:
        failure = f"{type(exc).__name__}: {exc}"
    finally:
        try:
            app.stop()
            teardown = bool(app.owned_teardown_verified and app.desktop_closed_verified)
            require(teardown, "Owned process and desktop teardown is unverified")
        except Exception as exc:
            failure = failure or f"{type(exc).__name__}: {exc}"
            status = "failed"
        report = {"schema": 1, "status": status, "source_commit": args.source_commit,
                  "release_tag": args.release_tag, "run_id": os.environ["GITHUB_RUN_ID"],
                  "exe_sha256": behavior.sha256(args.exe), "cli_sha256": behavior.sha256(args.cli),
                  "fixture_sha256": behavior.sha256(fixture), "operations": drive.rows,
                  "captures": drive.images, "capture_method": "lowlevel-computer-use-cheap hidden desktop",
                  "privacy": "isolated_profile_public_cube_only_restricted_pixel_review_pending",
                  "hardware": "unverified_no_printer_mutations", "teardown_verified": teardown,
                  "failure": failure}
        (args.output / "runtime.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return 0 if status == "runtime_verified" and teardown else 1


def pipe_probe(pid, denied_path):
    require(pid.isdecimal() and int(pid) > 0, "Invalid pipe instance")
    identifier = "runtime-" + uuid.uuid4().hex
    request = {"version": 1, "id": identifier, "operation": "capabilities", "arguments": {}}
    def exchange(request):
        # Parent owns a twenty-second timeout, including pipe opening and reading.
        with open(r"\\.\pipe\BambuStudio.Automation.v1." + pid, "r+b", buffering=0) as pipe:
            payload = (json.dumps(request, separators=(",", ":")) + "\n").encode("utf-8")
            require(pipe.write(payload) == len(payload), "Incomplete native request write")
            raw = bytearray()
            while len(raw) <= 1_048_576:
                byte = pipe.read(1)
                require(byte, "Native pipe closed before response newline")
                if byte == b"\n":
                    break
                raw.extend(byte)
            else:
                raise RuntimeError("Native pipe response exceeds maximum length")
        return json.loads(raw.decode("utf-8"))
    response = exchange(request)
    require(response.get("version") == 1 and response.get("id") == identifier and
            response.get("ok") is True and isinstance(response.get("result"), dict),
            "Native pipe response identity or protocol mismatch")
    denied_id = "runtime-" + uuid.uuid4().hex
    response = exchange({"version": 1, "id": denied_id, "operation": "project_save",
                         "arguments": {"path": denied_path, "overwrite": False}})
    require(response.get("version") == 1 and response.get("id") == denied_id and
            response.get("ok") is False and response.get("error", {}).get("code") == "path_denied",
            "Native workspace restriction did not reject the outside path")
    print("protocol_verified")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--pipe-probe":
        raise SystemExit(pipe_probe(sys.argv[2], sys.argv[3]))
    raise SystemExit(main())
