#!/usr/bin/env python3
"""Focused preflight and semantic-filter checks for the hosted behavior drive."""
import importlib.util
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("behavior_drive", HERE / "drive-packaged-behavior.py")
drive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(drive)


class BehaviorDriveChecks(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
