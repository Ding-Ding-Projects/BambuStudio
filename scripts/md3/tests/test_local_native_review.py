"""Non-window regressions for local provenance and exact-target inspection."""
import argparse
from contextlib import redirect_stdout
from copy import deepcopy
from datetime import datetime, timezone, timedelta
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock

SCRIPT = Path(__file__).resolve().parents[1] / "local-native-review.py"
spec = importlib.util.spec_from_file_location("local_review", SCRIPT)
review = importlib.util.module_from_spec(spec)
spec.loader.exec_module(review)
SOURCE = "a" * 40
TREE = "b" * 40


class ReceiptTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.payload = self.root / "install-dir"
        (self.payload / "automation").mkdir(parents=True)
        (self.root / "build.bat").write_text("@echo off\nexit /b 0\n")
        for name in review.REQUIRED[:-1]:
            (self.payload / name).write_bytes(name.encode())
        companion = {"sourceCommit": SOURCE, "buildRoute": "local-windows",
                     "sha256": review.digest(self.payload / "automation/bambu-automation.exe")}
        (self.payload / "automation/build-identity.json").write_text(json.dumps(companion))
        self.transcript = self.root / "transcript.log"
        self.transcript.write_text("Pinned build source: " + SOURCE + "\nBuild-only workflow completed; runnable payload: " + str(self.payload / "bambu-studio.exe"))
        self.now = datetime.now(timezone.utc)
        utc = lambda value: value.isoformat().replace("+00:00", "Z")
        self.receipt = {"schemaVersion": 1, "kind": "local-root-build", "invocationId": "fixture-only",
            "entrypoint": "build.bat", "arguments": ["/s"], "exitCode": 0,
            "sourceCommit": SOURCE, "sourceTree": TREE, "sourceCleanBefore": True, "sourceCleanAfter": True,
            "startedAtUtc": utc(self.now - timedelta(minutes=2)), "finishedAtUtc": utc(self.now - timedelta(minutes=1)),
            "entrypointSha256": review.digest(self.root / "build.bat"),
            "transcript": {"path": str(self.transcript), "sha256": review.digest(self.transcript)},
            "payload": {"root": str(self.payload), "files": {name: review.digest(self.payload / name) for name in review.REQUIRED}}}

    def git(self, root, *args):
        return {("rev-parse", "HEAD"): SOURCE, ("rev-parse", "HEAD^{tree}"): TREE,
                ("status", "--porcelain=v1", "--untracked-files=normal"): ""}[args]

    def validate(self, receipt=None, git=None):
        return review.validate_receipt(receipt or self.receipt, self.root, SOURCE, self.now, git or self.git)

    def test_complete_observer_receipt(self):
        self.assertEqual(self.validate(), self.payload / "bambu-studio.exe")

    def test_failed_unknown_and_non_root_invocations_are_rejected(self):
        for key, value in (("exitCode", 1), ("exitCode", None), ("exitCode", False),
                           ("entrypoint", "helper.ps1"), ("arguments", ["/s", "-Plan"]),
                           ("sourceCleanBefore", False), ("sourceCleanAfter", None),
                           ("sourceCommit", "c" * 40), ("kind", "hosted-install")):
            with self.subTest(key=key, value=value):
                bad = deepcopy(self.receipt)
                bad[key] = value
                with self.assertRaises(ValueError):
                    self.validate(bad)

    def test_stale_future_and_incomplete_receipts_are_rejected(self):
        for start, end in (("2020-01-01T00:00:00Z", "2020-01-01T00:01:00Z"),
                           ("2099-01-01T00:00:00Z", "2099-01-01T00:01:00Z"),
                           (self.receipt["finishedAtUtc"], self.receipt["startedAtUtc"])):
            bad = deepcopy(self.receipt)
            bad.update(startedAtUtc=start, finishedAtUtc=end)
            with self.assertRaises(ValueError):
                self.validate(bad)

    def test_moved_and_dirty_producers_are_rejected(self):
        for altered in (("rev-parse", "HEAD"), ("rev-parse", "HEAD^{tree}"),
                        ("status", "--porcelain=v1", "--untracked-files=normal")):
            with self.subTest(altered=altered), self.assertRaises(ValueError):
                self.validate(git=lambda root, *args: "changed" if args == altered else self.git(root, *args))

    def test_each_payload_hash_is_bound(self):
        for name in review.REQUIRED:
            path = self.payload / name
            original = path.read_bytes()
            path.write_bytes(original + b"changed")
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "Payload identity mismatch"):
                self.validate()
            path.write_bytes(original)
        self.validate()

    def test_transcript_requires_real_completion_after_latest_pin(self):
        original = self.transcript.read_text()
        self.transcript.write_text(original + "\nPinned build source: " + SOURCE + "\nbuild failed")
        self.receipt["transcript"]["sha256"] = review.digest(self.transcript)
        with self.assertRaisesRegex(ValueError, "completion evidence missing"):
            self.validate()
        self.transcript.write_text(original)
        self.receipt["transcript"]["sha256"] = review.digest(self.transcript)
        self.validate()

    def test_entrypoint_and_companion_are_bound(self):
        self.receipt["entrypointSha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "entrypoint changed"):
            self.validate()
        self.receipt["entrypointSha256"] = review.digest(self.root / "build.bat")
        path = self.payload / "automation/build-identity.json"
        data = json.loads(path.read_text())
        data["sourceCommit"] = "d" * 40
        path.write_text(json.dumps(data))
        self.receipt["payload"]["files"]["automation/build-identity.json"] = review.digest(path)
        with self.assertRaisesRegex(ValueError, "companion identity mismatch"):
            self.validate()

    def test_validation_only_never_launches(self):
        receipt = self.root / "receipt.json"
        receipt.write_text(json.dumps(self.receipt))
        argv = [str(SCRIPT), "--producer", str(self.root), "--build-receipt", str(receipt), "--source-commit", SOURCE]
        with patch.object(sys, "argv", argv), patch.object(review, "validate_receipt", return_value=self.payload / "bambu-studio.exe"), \
             patch.object(review, "dispatch_worker", side_effect=AssertionError("must not launch")), redirect_stdout(io.StringIO()):
            self.assertEqual(review.main(), 0)


def probe(pid=7, hwnd=20, tag="fixture"):
    return [{"kind": "header", "pid": pid, "tag": tag, "language": "en", "dark": False,
             "density": "comfortable", "dpi_scale": 1.25},
            {"kind": "toplevel", "hwnd": hwnd, "shown": True, "client": {"w": 1200, "h": 800}},
            {"kind": "end"}]


class OwnershipTests(unittest.TestCase):
    def test_native_process_is_suspended_until_contained(self):
        for assigned in (True, False):
            with self.subTest(assigned=assigned):
                kernel, user, events = MagicMock(), MagicMock(), []
                kernel.CreateJobObjectW.return_value = 11
                kernel.SetInformationJobObject.return_value = True
                kernel.TerminateJobObject.return_value = True
                kernel.WaitForSingleObject.return_value = 0
                user.OpenInputDesktop.return_value = 44
                def desktop(handle, kind, buffer, length, needed):
                    buffer.value = "Default"
                    return True
                user.GetUserObjectInformationW.side_effect = desktop
                def create(exe, command, pa, ta, inherit, flags, env, cwd, si, pi):
                    events.append("create-suspended")
                    self.assertEqual(flags & 4, 4)
                    self.assertEqual(si._obj.desktop, "WinSta0\\Default")
                    self.assertFalse(inherit)
                    pi._obj.process, pi._obj.thread, pi._obj.pid = 22, 33, 7
                    return True
                kernel.CreateProcessW.side_effect = create
                kernel.AssignProcessToJobObject.side_effect = lambda job, process: events.append("assign") or assigned
                kernel.ResumeThread.side_effect = lambda thread: events.append("resume") or 1
                def accounting(job, kind, buffer, length, returned):
                    buffer._obj.active = 0
                    return True
                kernel.QueryInformationJobObject.side_effect = accounting
                with patch.object(review.ctypes, "WinDLL", create=True, side_effect=lambda name, **kw: kernel if name == "kernel32" else user), \
                     patch.object(review.os, "name", "nt"):
                    if assigned:
                        session = review.NativeSession(Path("C:/fixture/app.exe"), Path("C:/fixture/profile"), "fixture")
                        self.assertEqual(events, ["create-suspended", "assign", "resume"])
                        self.assertTrue(session.close())
                    else:
                        with self.assertRaises(review.LaunchFailure) as context:
                            review.NativeSession(Path("C:/fixture/app.exe"), Path("C:/fixture/profile"), "fixture")
                        self.assertEqual(events, ["create-suspended", "assign"])
                        self.assertEqual(context.exception.teardown, "verified")
                        kernel.TerminateProcess.assert_called_once_with(22, 1)
                kernel.TerminateJobObject.assert_called_once_with(11, 0)

    def test_foreign_windows_are_never_adopted_by_title(self):
        windows = [{"handle": 10, "title": "Bambu Studio"}, {"handle": 20, "title": ""}]
        identity = lambda hwnd: {"pid": 8 if hwnd == 10 else 7, "class": "wxWindowNR", "visible": True}
        self.assertEqual(review.select_shell(windows, 7, identity)["hwnd"], 20)
        self.assertIsNone(review.select_shell(windows[:1], 7, identity))
        with self.assertRaisesRegex(ValueError, "ambiguous"):
            review.select_shell(windows, 7, lambda hwnd: identity(20))

    def test_probe_end_identity_tuple_and_geometry_are_required(self):
        good = probe()
        self.assertEqual(review.validate_probe(good, 7, 20, "fixture")["client"], {"w": 1200, "h": 800})
        bads = [good[:-1], probe(pid=8), probe(hwnd=99), probe(tag="old")]
        for field, value in (("language", "yue_HK"), ("dark", True), ("density", "compact"), ("dpi_scale", float("nan"))):
            bad = deepcopy(good)
            bad[0][field] = value
            bads.append(bad)
        bad = deepcopy(good)
        bad[1]["client"]["w"] = 0
        bads.append(bad)
        for bad in bads:
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                review.validate_probe(bad, 7, 20, "fixture")

    def test_existing_profile_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = review.seed_profile(root)
            before = (profile / "BambuStudio.conf").read_bytes()
            with self.assertRaises(FileExistsError):
                review.seed_profile(root)
            self.assertEqual((profile / "BambuStudio.conf").read_bytes(), before)

    def test_worker_timeout_and_partial_startup_remain_unverified(self):
        for outcome in (subprocess.TimeoutExpired("mock", 270), {"returncode": 124, "timed_out": False}):
            with self.subTest(outcome=type(outcome).__name__), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                args = argparse.Namespace(producer=root, build_receipt=root / "receipt.json", source_commit=SOURCE,
                                          lowlevel_cli=root / "cli.exe", desktop="visible", capture=False)
                def call(*a, **kw):
                    self.assertEqual(a[1], "run_command")
                    self.assertGreater(kw["timeout"], review.WORKER_SECONDS + 10)
                    if isinstance(outcome, Exception):
                        raise outcome
                    return outcome
                result = review.dispatch_worker(args, root, call)
                self.assertEqual(result["launch"]["status"], "unverified")
                self.assertEqual(result["teardown"], "unverified")
                self.assertTrue((root / "stop").exists())

    def test_partial_launch_tears_down_only_its_session(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            receipt = root / "receipt.json"
            receipt.write_text("{}")
            args = argparse.Namespace(build_receipt=receipt, source_commit=SOURCE, producer=root,
                                      lowlevel_cli=root / "cli.exe", capture=False)
            class Session:
                pid = 7
                closed = 0
                def alive(self): return False
                def close(self): self.closed += 1; return True
            owned = Session()
            with patch.object(review, "validate_receipt", return_value=root / "app.exe"):
                result = review.inspect_shell(args, {}, root, root / "app.exe", session_factory=lambda *a: owned,
                                              call=lambda *a, **kw: self.fail("No window operation after exit"))
            self.assertEqual(owned.closed, 1)
            self.assertEqual(result["launch"]["status"], "started")
            self.assertEqual(result["probe"]["status"], "not_attempted")
            self.assertEqual(result["teardown"], "verified")
            self.assertIn("failure", result)

    def test_teardown_failure_cannot_be_reported_as_verified(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            receipt = root / "receipt.json"
            receipt.write_text("{}")
            args = argparse.Namespace(build_receipt=receipt, source_commit=SOURCE, producer=root,
                                      lowlevel_cli=root / "cli.exe", capture=False)
            class Session:
                pid = 7
                def alive(self): return False
                def close(self): raise OSError("mock")
            with patch.object(review, "validate_receipt", return_value=root / "app.exe"):
                result = review.inspect_shell(args, {}, root, root / "app.exe", session_factory=lambda *a: Session())
            self.assertEqual(result["teardown"], "unverified")

    def test_complete_initial_probe_keeps_capture_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            receipt = root / "receipt.json"
            receipt.write_text("{}")
            args = argparse.Namespace(build_receipt=receipt, source_commit=SOURCE, producer=root,
                                      lowlevel_cli=root / "cli.exe", capture=False)
            class Session:
                pid = 7
                closed = False
                def alive(self): return True
                def identity(self, hwnd): return {"pid": 7, "class": "wxWindowNR", "visible": True}
                def close(self): self.closed = True; return True
            owned = Session()
            def sender(command, **kw):
                self.assertTrue(command[1].endswith("send-layout-probe.py"))
                self.assertNotIn("--command", command)
                Path(command[3]).write_text("\n".join(json.dumps(row) for row in probe(tag=root.name)))
            def call(cli, operation, **kw):
                self.assertEqual(operation, "list_windows")
                return {"windows": [{"handle": 20}]}
            with patch.object(review, "validate_receipt", return_value=root / "app.exe"), \
                 patch.object(review.subprocess, "run", side_effect=sender):
                result = review.inspect_shell(args, {}, root, root / "app.exe", lambda *a: owned, call)
            self.assertEqual(result["probe"]["status"], "received")
            self.assertEqual(result["screenshot"]["status"], "not_attempted")
            self.assertEqual(result["runtimeAcceptance"], "unverified")
            self.assertNotIn("failure", result)
            self.assertTrue(owned.closed)

    def test_image_bytes_target_and_geometry_require_separate_evidence(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory).resolve() / "fixture.png"
            image = Image.new("RGB", (2, 2), "white")
            image.putpixel((0, 0), (0, 0, 0))
            image.save(path)
            response = {"rendered_ok": True, "mode": "window", "window_hwnd": 20, "path": str(path)}
            self.assertEqual(review.validate_image(path, response, 20, {"w": 2, "h": 2})["visualAcceptance"], "unverified")
            for bad in ({**response, "window_hwnd": 99}, {**response, "rendered_ok": False}):
                with self.assertRaises(ValueError):
                    review.validate_image(path, bad, 20, {"w": 2, "h": 2})
            with self.assertRaisesRegex(ValueError, "dimensions"):
                review.validate_image(path, response, 20, {"w": 3, "h": 2})
            Image.new("RGB", (2, 2), "white").save(path)
            with self.assertRaisesRegex(ValueError, "blank"):
                review.validate_image(path, response, 20, {"w": 2, "h": 2})

    def test_launcher_rejects_shell_metacharacters(self):
        with self.assertRaisesRegex(ValueError, "Unsafe launcher"):
            review.worker_command(Path("C:/temp/%EXPANSION%/request.json"))


if __name__ == "__main__":
    unittest.main()
