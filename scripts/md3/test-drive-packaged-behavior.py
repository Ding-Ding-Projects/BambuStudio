#!/usr/bin/env python3
"""Focused preflight and semantic-filter checks for the hosted behavior drive."""
import importlib.util
import hashlib
import json
import os
import tempfile
import unittest
import zipfile
from datetime import datetime, timedelta, timezone
from pathlib import Path
from unittest.mock import patch
from PIL import Image


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("behavior_drive", HERE / "drive-packaged-behavior.py")
drive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(drive)


class BehaviorDriveChecks(unittest.TestCase):
    def test_relaunch_requires_profile_lineage_desktop_and_creation(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = str(Path(temp) / "bambu-studio.exe")
            profile = str(Path(temp) / "profile")
            started = datetime.now(timezone.utc)
            def record(pid, parent, *, command=None, created=None, image=None):
                return {"ProcessId": pid, "ParentProcessId": parent,
                        "ExecutablePath": image or exe,
                        "CommandLine": command or f'"{exe}" --datadir "{profile}"',
                        "CreationDate": (created or started).isoformat()}
            processes = [record(10, 1), record(20, 10), record(21, 11),
                         record(22, 10, command=f'"{exe}" --datadir "{profile}-other"'),
                         record(23, 10, created=started - timedelta(minutes=1)),
                         record(24, 10, image=str(Path(temp) / "other.exe"))]
            self.assertEqual(drive.owned_processes(
                processes, exe=exe, datadir=profile, launched_at=started,
                launch_pid=10, desktop_pids={20, 21, 22, 23, 24}), [10, 20])

    def test_exited_launch_pid_does_not_mask_teardown(self):
        app = drive.HostedApp("exe", "profile", "desktop", "probe")
        app.pid = 6968
        app.launch_pid = 6968
        app.adopted_pids = []
        calls = []
        with patch.object(app, "windows", return_value=[]), patch.object(
            drive, "cheap", side_effect=lambda tool, **kwargs: calls.append(tool) or {"ok": True}
        ):
            app.stop()
        self.assertEqual(calls, ["close_headless_desktop"])

    def test_installation_must_match_host_source_package_and_executable(self):
        source = "a" * 40
        with tempfile.TemporaryDirectory() as temp:
            local = Path(temp)
            exe = local / "BambuStudioMD3" / "app-2.8.4120" / "bambu-studio.exe"
            exe.parent.mkdir(parents=True)
            exe.write_bytes(b"installed package executable")
            receipt = {"status": "verified", "runner": "github-hosted-windows",
                       "source_commit": source, "release_tag": "md3-v122",
                       "package_version": "2.8.4120", "installed_exe_sha256": drive.sha256(exe)}
            env = {"GITHUB_ACTIONS": "true", "RUNNER_ENVIRONMENT": "github-hosted",
                   "LOCALAPPDATA": str(local)}
            with patch.dict(os.environ, env):
                drive.validate_installation(receipt, exe, source, "md3-v122")
                with self.assertRaisesRegex(ValueError, "source or release"):
                    drive.validate_installation(receipt, exe, "b" * 40, "md3-v122")
                with self.assertRaisesRegex(ValueError, "differs"):
                    exe.write_bytes(b"different bytes")
                    drive.validate_installation(receipt, exe, source, "md3-v122")
                exe.write_bytes(b"installed package executable")
                with self.assertRaisesRegex(ValueError, "not inside"):
                    other = local / "unrelated.exe"
                    other.write_bytes(exe.read_bytes())
                    drive.validate_installation(receipt, other, source, "md3-v122")
            with patch.dict(os.environ, {"GITHUB_ACTIONS": "false", "RUNNER_ENVIRONMENT": "github-hosted"}):
                with self.assertRaisesRegex(ValueError, "disposable"):
                    drive.validate_installation(receipt, exe, source, "md3-v122")

    def test_offscreen_controls_cannot_satisfy_visible_postcondition(self):
        records = [
            {"kind": "toplevel", "hwnd": 1, "rect": {"x": 0, "y": 0, "w": 800, "h": 600}},
            {"kind": "window", "hwnd": 2, "top": 1, "parent": 1, "name": "Process",
             "label": "Process", "shown": True, "on_screen": False,
             "screen": {"x": 20, "y": 20, "w": 100, "h": 32}},
            {"kind": "window", "hwnd": 3, "top": 1, "parent": 1, "name": "Objects",
             "label": "Objects", "shown": True, "on_screen": True,
             "screen": {"x": 130, "y": 20, "w": 100, "h": 32}},
        ]
        self.assertIsNone(drive.visible(records, "Process", 1))
        self.assertIsNotNone(drive.visible(records, "Objects", 1))
        self.assertNotIn("Process", drive.visible_labels(records))

    def test_disposable_language_profiles_and_probe_header_must_match(self):
        with tempfile.TemporaryDirectory() as temp:
            for mode in drive.MODES:
                profile = Path(temp) / mode
                profile.mkdir()
                drive.seed_profile(profile, mode)
                content = (profile / "BambuStudio.conf").read_bytes().decode("utf-8")
                body, checksum = content.split("\n# MD5 checksum ")
                self.assertEqual(json.loads(body)["app"]["language"], mode)
                self.assertEqual(checksum.strip(), hashlib.md5(body.encode("utf-8")).hexdigest().upper())
                self.assertEqual(drive.probe_header([{"kind": "header", "language": mode,
                                                     "dpi_scale": 1.25}], mode)["dpi_scale"], 1.25)
                with self.assertRaisesRegex(RuntimeError, "native display scale"):
                    drive.probe_header([{"kind": "header", "language": mode,
                                        "dpi_scale": 1.0, "dark": False}], mode, "light", 1.25)
                with self.assertRaisesRegex(RuntimeError, "theme differs"):
                    drive.probe_header([{"kind": "header", "language": mode,
                                        "dpi_scale": 1.25, "dark": False}], mode, "dark", 1.25)
                with self.assertRaisesRegex(RuntimeError, "requested language"):
                    drive.probe_header([{"kind": "header", "language": "en", "dpi_scale": 1}],
                                       "yue_HK")
                with self.assertRaisesRegex(ValueError, "not new"):
                    drive.seed_profile(profile, mode)

    def test_false_printwindow_result_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            instance = drive.Drive(None, Path(temp), "a" * 40, "md3-v122", "b" * 64, "123", "en")
            with patch.object(drive, "cheap", return_value={"ok": True, "rendered_ok": False}):
                with self.assertRaisesRegex(RuntimeError, "PrintWindow"):
                    instance.capture("unrendered", 1)

    def test_uniform_rendered_image_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            instance = drive.Drive(None, Path(temp), "a" * 40, "md3-v122", "b" * 64, "123", "en")
            def render(_tool, **kwargs):
                Image.new("RGB", (1200, 800), "white").save(kwargs["output_path"])
                return {"ok": True, "rendered_ok": True}
            with patch.object(drive, "cheap", side_effect=render):
                with self.assertRaisesRegex(RuntimeError, "uniform"):
                    instance.capture("blank", 1)

    def test_workspace_roundtrip_requires_real_manifest_title(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle = Path(temp) / "fixture.bambu-workspace"
            with zipfile.ZipFile(bundle, "w") as archive:
                archive.writestr("Metadata/workspace.json", json.dumps({
                    "version": 1, "bundle_id": "test", "title": "Hosted verification workspace"}))
            good = drive.Drive.workspace_file_evidence(bundle)
            self.assertEqual(good["manifest_title"], "Hosted verification workspace")
            with zipfile.ZipFile(bundle, "w") as archive:
                archive.writestr("Metadata/workspace.json", json.dumps({"title": "wrong workspace"}))
            self.assertIsNone(drive.Drive.workspace_file_evidence(bundle))


if __name__ == "__main__":
    unittest.main()
