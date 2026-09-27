#!/usr/bin/env python3
"""Focused preflight and semantic-filter checks for the hosted behavior drive."""
import importlib.util
import hashlib
import json
import os
import subprocess
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
    def test_file_open_requires_object_and_enabled_slice_state(self):
        tab_title = {"kind": "toplevel", "name": "flowrate-test-pass1", "shown": True,
                     "on_screen": True}
        object_row = {"kind": "window", "name": "flowrate-test-pass1", "shown": True,
                      "on_screen": True, "parent": 9, "top": 1}
        slice_disabled = {"kind": "tool", "name": "Slice plate", "shown": True,
                          "on_screen": True, "enabled": False}
        self.assertIsNone(drive.Drive.fixture_loaded_state([tab_title, slice_disabled],
                                                            "flowrate-test-pass1"))
        self.assertIsNone(drive.Drive.fixture_loaded_state([object_row, slice_disabled],
                                                            "flowrate-test-pass1"))
        result = drive.Drive.fixture_loaded_state(
            [object_row, {**slice_disabled, "enabled": True}], "flowrate-test-pass1")
        self.assertEqual(result["object_parent"], 9)
        class ExitedApp:
            main = 7
            def windows(self):
                return []
        with tempfile.TemporaryDirectory() as temp:
            instance = drive.Drive(ExitedApp(), Path(temp), "a" * 40, "md3-v122",
                                   "b" * 64, "123", "en")
            with self.assertRaisesRegex(RuntimeError, "exited"):
                instance.wait_fixture_loaded("flowrate-test-pass1", timeout=1)

    def test_client_resize_compensates_borders_and_refuses_minimum_clamp(self):
        class FakeApp:
            main = 7
            def __init__(self, clamp=False):
                self.outer = [1216, 839]
                self.clamp = clamp
                self.commands = []
            def windows(self):
                return [{"handle": 7, "width": self.outer[0], "height": self.outer[1]}]
            def probe(self):
                return [{"kind": "header", "language": "en", "dark": False, "dpi_scale": 1.0},
                        {"kind": "toplevel", "hwnd": 7,
                         "client": {"w": self.outer[0] - 16, "h": self.outer[1] - 39}}]
            def command(self, payload):
                self.commands.append(payload)
                if not self.clamp:
                    self.outer = [int(value) for value in payload.split()[-2:]]
        with tempfile.TemporaryDirectory() as temp:
            app = FakeApp()
            instance = drive.Drive(app, Path(temp), "a" * 40, "md3-v122", "b" * 64, "123", "en")
            with patch.object(drive.time, "sleep"):
                _, client = instance.resize_client_exact((1000, 600))
            self.assertEqual(client, {"w": 1000, "h": 600})
            self.assertEqual(app.commands, ["resize 7 1016 639"])
            clamped = FakeApp(clamp=True)
            instance = drive.Drive(clamped, Path(temp), "a" * 40, "md3-v122", "b" * 64, "123", "en")
            with patch.object(drive.time, "sleep"), self.assertRaisesRegex(RuntimeError, "exact"):
                instance.resize_client_exact((1000, 600))

    def test_verifier_identity_must_match_checked_out_driver(self):
        good = [subprocess.CompletedProcess([], 0, "a" * 40 + "\n"),
                subprocess.CompletedProcess([], 0, "tracked\n"),
                subprocess.CompletedProcess([], 0, "")]
        with patch.object(drive.subprocess, "run", side_effect=good):
            drive.validate_verifier("a" * 40)
        with patch.object(drive.subprocess, "run", side_effect=[good[0]]):
            with self.assertRaisesRegex(ValueError, "differs"):
                drive.validate_verifier("b" * 40)
        modified = [good[0], good[1], subprocess.CompletedProcess([], 0, " M scripts/md3/hosted_process.py\n")]
        with patch.object(drive.subprocess, "run", side_effect=modified):
            with self.assertRaisesRegex(ValueError, "Tracked driver inputs"):
                drive.validate_verifier("a" * 40)

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
            self.assertEqual([item["pid"] for item in drive.owned_process_inventory(
                processes, exe=exe, datadir=profile, launched_at=started,
                launch_pid=10)], [10, 20])
            self.assertEqual(drive.owned_processes(
                processes, exe=exe, datadir=profile, launched_at=started,
                launch_pid=10, desktop_pids={20, 21, 22, 23, 24}), [20])

    def test_exited_launch_pid_does_not_mask_teardown(self):
        app = drive.HostedApp("exe", "profile", "desktop", "probe")
        app.pid = 6968
        app.launch_pid = 6968
        app.adopted_pids = []
        calls = []
        app.launch_started = datetime.now(timezone.utc)
        with patch.object(drive, "process_snapshot", return_value=[]), patch.object(
            drive, "cheap", side_effect=lambda tool, **kwargs: calls.append(tool) or {"ok": True}
        ):
            app.stop()
        self.assertEqual(calls, ["close_headless_desktop"])

    def test_owned_child_without_window_is_still_cleaned_up(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = str(Path(temp) / "bambu-studio.exe")
            profile = str(Path(temp) / "profile")
            app = drive.HostedApp(exe, profile, "desktop", "probe")
            app.launch_pid = 10
            app.launch_started = datetime.now(timezone.utc)
            child = {"ProcessId": 20, "ParentProcessId": 10,
                     "ExecutablePath": exe,
                     "CommandLine": f'"{exe}" --datadir "{profile}"',
                     "CreationDate": app.launch_started.isoformat()}
            calls = []
            with patch.object(drive, "process_snapshot", return_value=[child]), patch.object(
                drive, "cheap", side_effect=lambda tool, **kwargs: calls.append((tool, kwargs)) or {"ok": True}
            ):
                app.stop()
            self.assertEqual([name for name, _ in calls], ["kill_process", "close_headless_desktop"])
            self.assertEqual(calls[0][1]["pid"], 20)
            self.assertEqual(app.seen_owned[20]["parent_pid"], 10)

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
