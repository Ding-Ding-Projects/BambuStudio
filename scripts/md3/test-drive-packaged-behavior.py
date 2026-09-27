#!/usr/bin/env python3
"""Focused preflight and semantic-filter checks for the hosted behavior drive."""
import importlib.util
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import threading
import unittest
import zipfile
from types import SimpleNamespace
from datetime import datetime, timedelta, timezone
from pathlib import Path
from unittest.mock import patch
from PIL import Image


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("behavior_drive", HERE / "drive-packaged-behavior.py")
drive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(drive)
holder_spec = importlib.util.spec_from_file_location("hosted_launch_holder", HERE / "hosted_launch_holder.py")
holder = importlib.util.module_from_spec(holder_spec)
holder_spec.loader.exec_module(holder)


class BehaviorDriveChecks(unittest.TestCase):
    def test_contract_binding_rejects_missing_stale_mismatched_and_partial_inventory(self):
        contract = drive.behavior_contract
        images = [{"file": "before.png"}, {"file": "after.png"}]
        rows = []
        for flow in contract.LAYOUT_FLOWS:
            rows.append({"id": flow.id, "status": "probe_confirmed", "proof": {
                "source": "installed-process-probe", "predicate_id": flow.predicate_id,
                "input_action": "owned native input",
                "before_state": {field: "before" for field in flow.state_fields},
                "after_state": {field: "after" for field in flow.state_fields},
                "capture_ids": ["before.png", "after.png"]}})
        valid_result = drive.asdict(contract.validate_behavior_rows("layout", "en", rows))
        source_hash = drive.sha256(Path(contract.__file__))
        state = lambda items, result, digest, captures=images: drive.contract_report_state(
            items, result, digest, "layout", "en", captures)
        self.assertEqual(state(rows, valid_result, source_hash), "complete")
        self.assertEqual(state(None, valid_result, source_hash), "missing_contract_inventory")
        self.assertEqual(state(rows, valid_result, "0" * 64), "stale_contract_source")
        self.assertEqual(state(rows, {**valid_result, "version": 1}, source_hash),
                         "mismatched_contract_result")
        partial_rows = rows[:-1]
        partial_result = drive.asdict(contract.validate_behavior_rows("layout", "en", partial_rows))
        self.assertEqual(state(partial_rows, partial_result, source_hash),
                         "partial_contract_inventory")
        self.assertEqual(state(rows, valid_result, source_hash, images[:1]),
                         "unreported_contract_capture")

    def test_contract_does_not_promote_visible_labels_to_file_open_proof(self):
        row = {"name": "file-menu-open-project", "status": "probe_confirmed",
               "result": {"before_visible": [], "after_visible": ["fixture.3mf"]},
               "before_image": {"file": "before.png"},
               "after_image": {"file": "after.png"}}
        self.assertIsNone(drive.project_file_open_contract(
            row, [{"file": "before.png"}, {"file": "after.png"}],
            SimpleNamespace(pid=42, datadir="profile")))

    def test_startup_fallback_needs_confirmed_exit_without_replacement(self):
        exited = {"original_exit_confirmed": True, "owned_replacement_seen": False,
                  "owned_process_live_at_end": False}
        self.assertTrue(drive.startup_case_exited(exited))
        for change in ({"original_exit_confirmed": False},
                       {"owned_replacement_seen": True},
                       {"owned_process_live_at_end": True}):
            self.assertFalse(drive.startup_case_exited({**exited, **change}))

    def test_startup_comparison_uses_separate_seeded_routes_then_strict_empty_fallback(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            exe = root / "bambu-studio.exe"
            exe.write_bytes(b"same installed executable")
            args = SimpleNamespace(exe=exe, output=root, language="en", theme="light",
                                   scale=1.0, viewport="1200x800", release_tag="md3-v125",
                                   hosted_run_id="123")
            receipt = {"installed_exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
                       "package_version": "2.8.4124"}
            calls = []
            def case(name, route, seeded, actual_args, scratch, exe_hash):
                calls.append((name, route, seeded, actual_args.exe, exe_hash))
                return ({"case": name, "original_exit_confirmed": True,
                         "original_exit_code": 2147942487, "mainframe_seen": False,
                         "owned_replacement_seen": False, "owned_process_live_at_end": False,
                         "cleanup_error": None, "cleanup_verified": True},
                        {"file": name + ".log", "bytes": 1, "sha256": "a" * 64})
            with patch.object(drive, "run_startup_case", side_effect=case):
                self.assertEqual(drive.run_startup_comparison(
                    args, receipt, "a" * 40, "b" * 40, root), 2)
            self.assertEqual([(name, route, seeded) for name, route, seeded, _, _ in calls],
                             [("seeded-direct", "direct", True),
                              ("seeded-holder", "holder", True),
                              ("empty-direct", "direct", False),
                              ("empty-holder", "holder", False)])
            self.assertEqual({item[3] for item in calls}, {exe})
            self.assertEqual({item[4] for item in calls}, {receipt["installed_exe_sha256"]})
            report = json.loads((root / "behavior-report.json").read_text())
            self.assertEqual(report["images"], [])
            self.assertFalse(report["behavior_verified"])
            self.assertEqual(report["verdict"], "blocked")

    def test_startup_comparison_does_not_fallback_from_missing_desktop(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            args = SimpleNamespace(exe=root / "bambu-studio.exe", output=root,
                                   language="en", theme="light", scale=1.0,
                                   viewport="1200x800", release_tag="md3-v125",
                                   hosted_run_id="123")
            calls = []
            def case(name, route, seeded, *_args):
                calls.append(name)
                return ({"case": name, "original_exit_confirmed": route == "holder",
                         "original_exit_code": None, "mainframe_seen": False,
                         "owned_replacement_seen": False, "owned_process_live_at_end": False,
                         "cleanup_error": None, "cleanup_verified": True},
                        {"file": name + ".log", "bytes": 1,
                                                  "sha256": "a" * 64})
            with patch.object(drive, "run_startup_case", side_effect=case):
                drive.run_startup_comparison(args, {"installed_exe_sha256": "a" * 64,
                                             "package_version": "2.8.4124"},
                                             "a" * 40, "b" * 40, root)
            self.assertEqual(calls, ["seeded-direct", "seeded-holder"])

    def test_unverified_teardown_stops_comparison_and_retains_first_case(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            args = SimpleNamespace(exe=root / "bambu-studio.exe", output=root,
                                   language="en", theme="light", scale=1.0,
                                   viewport="1200x800", release_tag="md3-v125",
                                   hosted_run_id="123")
            calls = []
            def case(name, *_args):
                calls.append(name)
                return ({"case": name, "original_exit_confirmed": True,
                         "original_exit_code": 1, "mainframe_seen": False,
                         "owned_replacement_seen": False, "owned_process_live_at_end": False,
                         "cleanup_error": "desktop still open", "cleanup_verified": False},
                        {"file": name + ".log", "bytes": 1, "sha256": "a" * 64})
            with patch.object(drive, "run_startup_case", side_effect=case):
                self.assertEqual(drive.run_startup_comparison(
                    args, {"installed_exe_sha256": "a" * 64, "package_version": "2.8.4124"},
                    "a" * 40, "b" * 40, root), 2)
            self.assertEqual(calls, ["seeded-direct"])
            report = json.loads((root / "behavior-report.json").read_text())
            self.assertEqual(len(report["startup_comparison"]), 1)
            self.assertEqual(len(report["restricted_logs"]), 1)
            self.assertEqual(report["comparison_stopped_reason"],
                             "owned_process_or_named_desktop_teardown_unverified")

    def test_empty_stream_is_explicit_and_omitted_from_restricted_inventory(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            app = drive.HostedApp("exe", "profile", "desktop", str(root))
            app.holder_receipt = {"streams": {"stdout": {"bytes_total": 0, "bytes_saved": 0},
                                              "stderr": {"bytes_total": 7, "bytes_saved": 7}}}
            self.assertEqual(drive.holder_stream_sources(app),
                             [("stderr", app.holder_stderr_path)])
            app.holder_stderr_path.write_bytes(b"content")
            app.holder_stdout_path.write_bytes(b"")
            self.assertEqual([item["file"] for item in drive.preserve_holder_streams(app, root)],
                             ["hosted-stderr.log"])

    def test_stream_prefix_is_flushed_before_inherited_writer_closes(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "stdout.log"
            reader, writer = os.pipe()
            result = {}
            with patch.object(holder.msvcrt, "open_osfhandle", return_value=reader):
                thread = threading.Thread(target=holder.drain_pipe, args=(123, path, result))
                thread.start()
                os.write(writer, b"early output")
                for _ in range(100):
                    if path.is_file() and path.read_bytes() == b"early output":
                        break
                    threading.Event().wait(0.01)
                self.assertEqual(path.read_bytes(), b"early output")
                self.assertTrue(thread.is_alive())
                os.close(writer)
                thread.join(timeout=2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(result["bytes_saved"], 12)

    def test_zero_byte_stream_records_counter_without_creating_file(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "stderr.log"
            reader, writer = os.pipe()
            os.close(writer)
            result = {}
            with patch.object(holder.msvcrt, "open_osfhandle", return_value=reader):
                holder.drain_pipe(123, path, result)
            self.assertFalse(path.exists())
            self.assertEqual(result["bytes_total"], 0)
            self.assertEqual(result["bytes_saved"], 0)

    def test_case_temp_environment_restores_parent_and_bundle_excludes_shared_trace(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            profile = root / "profile"
            profile.mkdir()
            case_temp = root / "case-temp"
            with patch.dict(os.environ, {"TEMP": "shared-temp", "TMP": "shared-temp"}):
                with drive.startup_case_environment(case_temp, profile):
                    self.assertEqual(os.environ["TEMP"], str(case_temp))
                    self.assertEqual(os.environ["TMP"], str(case_temp))
                    (case_temp / "bbs-launcher-trace.log").write_bytes(b"case trace")
                self.assertEqual(os.environ["TEMP"], "shared-temp")
                self.assertEqual(os.environ["TMP"], "shared-temp")
            exe = root / "bambu-studio.exe"
            exe.write_bytes(b"installed")
            with patch.object(drive, "matching_wer_events", return_value=[]):
                record, missing = drive.bundle_case_diagnostics(
                    "seeded-direct", [("launcher_trace", case_temp / "bbs-launcher-trace.log")],
                    exe, {}, root)
            self.assertEqual(missing, [])
            payload = json.loads((root / "restricted-logs" / record["file"]).read_text())
            self.assertEqual(payload["files"][0]["role"], "launcher_trace")
            self.assertNotIn("case trace", json.dumps(record))

    def test_workspace_fixture_stays_outside_tuple_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            profile = root / "private" / "profile"
            profile.mkdir(parents=True)
            output = root / "behavior" / "en-light-1-1200x800"
            output.mkdir(parents=True)
            instance = drive.Drive(SimpleNamespace(datadir=str(profile)), output,
                                   "a" * 40, "md3-v125", "b" * 64, "123", "en")
            candidate = instance.workspace_scratch_file()
            self.assertEqual(candidate, profile.parent / "fixture.bambu-workspace")
            self.assertNotIn(output, candidate.parents)
            candidate.write_bytes(b"existing")
            with self.assertRaisesRegex(RuntimeError, "already exists"):
                instance.workspace_scratch_file()

    def test_holder_uses_existing_base_python_not_venv_redirector(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp) / "python.exe"
            base.write_bytes(b"base interpreter fixture")
            with patch.object(drive.sys, "_base_executable", str(base), create=True):
                self.assertEqual(drive.helper_python_executable(), base)
            with patch.object(drive.sys, "_base_executable", str(Path(temp) / "missing.exe"), create=True):
                with self.assertRaisesRegex(RuntimeError, "base python.exe"):
                    drive.helper_python_executable()

    def test_holder_refuses_non_hosted_execution_before_launch(self):
        argv = ["holder", "--exe", "unused.exe", "--datadir", "unused-profile",
                "--desktop", "hidden", "--receipt", "receipt.json", "--stop", "stop.file",
                "--stdout", "stdout.log", "--stderr", "stderr.log"]
        with patch.object(sys, "argv", argv), patch.dict(holder.os.environ, {
                "GITHUB_ACTIONS": "false", "RUNNER_ENVIRONMENT": "github-hosted"}):
            with self.assertRaises(SystemExit):
                holder.main()

    def test_holder_receipt_requires_exact_launcher_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            profile = str(Path(temp) / "isolated profile")
            started = datetime.now(timezone.utc).isoformat()
            receipt = {"helper_pid": 10, "app_pid": 20, "exe_sha256": "a" * 64,
                       "helper_executable_sha256": "c" * 64,
                       "profile": profile, "desktop": "owned-desktop",
                       "launch_started_at_utc": started, "status": "app_exited_holder_alive"}
            self.assertEqual(drive.validate_holder_receipt(
                receipt, helper_pid=10, exe_hash="a" * 64, helper_exe_hash="c" * 64,
                datadir=profile, desktop="owned-desktop")[0], 20)
            for change in ({"helper_pid": 11}, {"app_pid": 10},
                           {"exe_sha256": "b" * 64}, {"helper_executable_sha256": "d" * 64},
                           {"profile": profile + "-other"},
                           {"desktop": "visible"}, {"status": "desktop_open_failed"},
                           {"launch_started_at_utc": "not a time"}):
                with self.subTest(change=change), self.assertRaises(RuntimeError):
                    drive.validate_holder_receipt(
                        {**receipt, **change}, helper_pid=10, exe_hash="a" * 64,
                        helper_exe_hash="c" * 64,
                        datadir=profile, desktop="owned-desktop")

    def test_missing_desktop_preserves_process_snapshot_and_rejects_stranger(self):
        app = drive.HostedApp("exe", "profile", "owned-desktop", "probe")
        app.launch_pid = 20
        app.launch_started = datetime.now(timezone.utc)
        unrelated = {"ProcessId": 30, "ParentProcessId": 20,
                     "ExecutablePath": "other.exe",
                     "CommandLine": '"other.exe" --datadir "profile"',
                     "CreationDate": app.launch_started.isoformat()}
        order = []
        def snapshot():
            order.append("snapshot")
            return [unrelated]
        def missing(_tool, **_kwargs):
            order.append("desktop")
            raise RuntimeError("OpenDesktopW failed")
        with patch.object(drive, "process_snapshot", side_effect=snapshot), patch.object(
                drive, "cheap", side_effect=missing):
            with self.assertRaisesRegex(RuntimeError, "desktop_missing_no_owned_process"):
                app.windows()
        self.assertEqual(order, ["snapshot", "desktop"])
        self.assertEqual(app.seen_owned, {})
        self.assertEqual(app.startup_state, "desktop_missing_no_owned_process")

    def test_missing_desktop_with_owned_child_has_distinct_diagnostic(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = str(Path(temp) / "bambu-studio.exe")
            profile = str(Path(temp) / "profile")
            app = drive.HostedApp(exe, profile, "owned-desktop", str(Path(temp) / "probe"))
            app.launch_pid = 20
            app.launch_started = datetime.now(timezone.utc)
            child = {"ProcessId": 21, "ParentProcessId": 20,
                     "ExecutablePath": exe,
                     "CommandLine": f'"{exe}" --datadir "{profile}"',
                     "CreationDate": app.launch_started.isoformat()}
            with patch.object(drive, "process_snapshot", return_value=[child]), patch.object(
                    drive, "cheap", side_effect=RuntimeError("OpenDesktopW failed")):
                with self.assertRaisesRegex(RuntimeError, "desktop_missing_with_owned_process"):
                    app.windows()
            self.assertIn(21, app.seen_owned)

    def test_wer_evidence_requires_exact_pid_image_and_time(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = str(Path(temp) / "bambu-studio.exe")
            started = datetime.now(timezone.utc) - timedelta(seconds=10)
            ended = datetime.now(timezone.utc)
            def event(pid, path, at):
                return (f'<Event><System><TimeCreated SystemTime="{at.isoformat()}" />'
                        f'</System><EventData><Data Name="ProcessId">0x{pid:x}</Data>'
                        f'<Data Name="AppPath">{path}</Data></EventData></Event>')
            events = [event(99, exe, ended), event(20, str(Path(temp) / "other.exe"), ended),
                      event(20, exe, started - timedelta(seconds=1)), event(20, exe, ended)]
            output = subprocess.CompletedProcess([], 0, json.dumps(events), "")
            with patch.object(drive.subprocess, "run", return_value=output):
                self.assertEqual(drive.matching_wer_events(exe, {20: started}, ended), [events[-1]])

    def test_wer_query_uses_full_bounded_run_interval(self):
        started = datetime.now(timezone.utc) - timedelta(minutes=15)
        ended = datetime.now(timezone.utc)
        with patch.object(drive.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "")) as run:
            self.assertEqual(drive.matching_wer_events("C:\\verified.exe", {20: started}, ended), [])
        self.assertEqual(run.call_args.kwargs["env"]["BS_WER_START_UTC"], started.isoformat())
        with patch.object(drive.subprocess, "run") as run:
            self.assertEqual(drive.matching_wer_events(
                "C:\\verified.exe", {20: ended - timedelta(minutes=41)}, ended), [])
        run.assert_not_called()

    def test_unverified_holder_teardown_never_kills_unchecked_helper_pid(self):
        with tempfile.TemporaryDirectory() as temp:
            probe = Path(temp) / "probe"
            probe.mkdir()
            app = drive.HostedApp("exe", "profile", "desktop", str(probe))
            app.helper_pid = 99
            app.launch_pid = 20
            app.launch_started = datetime.now(timezone.utc)
            app.holder_receipt_path.write_text(json.dumps({"helper_pid": 99,
                                                          "app_exit_confirmed": False}), encoding="utf-8")
            calls = []
            with patch.object(drive, "process_snapshot", return_value=[]), patch.object(
                    drive, "cheap", side_effect=lambda tool, **kwargs: calls.append(tool) or {"ok": True}), patch.object(
                    drive.time, "monotonic", side_effect=[0, 16, 20, 26]):
                with self.assertRaisesRegex(RuntimeError, "helper PID was not killed"):
                    app.stop()
            self.assertEqual(calls, ["close_headless_desktop"])

    def test_teardown_requires_absent_owned_pid_and_missing_named_desktop(self):
        with tempfile.TemporaryDirectory() as temp:
            probe = Path(temp) / "probe"
            probe.mkdir()
            app = drive.HostedApp("exe", "profile", "owned-desktop", str(probe))
            app.launch_pid = 20
            app.launch_started = datetime.now(timezone.utc)
            def absent(tool, **_kwargs):
                if tool == "list_headless_windows":
                    raise RuntimeError("OpenDesktopW('owned-desktop') failed (GetLastError=2)")
                return {"ok": True, "closed": False}
            with patch.object(drive, "process_snapshot", return_value=[]), patch.object(
                    drive, "cheap", side_effect=absent):
                app.stop()
            self.assertTrue(app.owned_teardown_verified)
            self.assertTrue(app.desktop_closed_verified)
            with patch.object(drive, "process_snapshot", return_value=[]), patch.object(
                    drive, "cheap", return_value={"ok": True, "windows": []}), patch.object(
                    drive.time, "monotonic", side_effect=[0, 1, 6]), patch.object(
                    drive.time, "sleep"):
                with self.assertRaisesRegex(RuntimeError, "Named desktop closure was not verified"):
                    app.stop()
            self.assertFalse(app.desktop_closed_verified)

    def test_wer_collection_skips_holder_terminated_app(self):
        with tempfile.TemporaryDirectory() as temp:
            app = drive.HostedApp("exe", "profile", "desktop", str(Path(temp) / "probe"))
            app.launch_pid = 20
            app.launch_started = datetime.now(timezone.utc) - timedelta(seconds=10)
            app.finished_at = datetime.now(timezone.utc)
            app.holder_receipt = {"app_exited_at_utc": app.finished_at.isoformat(),
                                  "app_terminated_by_holder": True}
            with patch.object(drive, "matching_wer_events") as query:
                records, status = drive.preserve_wer("exe", app, Path(temp))
            self.assertEqual(records, [])
            self.assertEqual(status, "not_applicable_without_observed_natural_exit")
            query.assert_not_called()

    def test_windows_profile_argument_spelling_and_rejections(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = str(Path(temp) / "bambu-studio.exe")
            profile = str(Path(temp) / "profile with spaces")
            started = datetime.now(timezone.utc)
            def candidate(command, *, image=None):
                return [{"ProcessId": 20, "ParentProcessId": 10,
                         "ExecutablePath": image or exe, "CommandLine": command,
                         "CreationDate": started.isoformat()}]
            def selected(command, *, image=None):
                return drive.owned_processes(
                    candidate(command, image=image), exe=exe, datadir=profile,
                    launched_at=started, launch_pid=10, desktop_pids={20})
            alternate_profile = profile.upper().replace("\\", "/")
            alternate_image = exe.upper().replace("\\", "/")
            self.assertEqual(selected(
                f'"{exe}" --DATADIR "{alternate_profile}"', image=alternate_image), [20])
            self.assertEqual(selected(f'"{exe}" --datadir "{profile}"'), [20])
            self.assertEqual(selected(f'"{exe}" --datadir "{profile}-other"'), [])
            self.assertEqual(selected(f'"{exe}" --datadir {profile}'), [])
            self.assertEqual(selected(f'"{exe}" --datadir "{profile}'), [])
            self.assertEqual(selected(f'"{exe}" --datadir "{profile}" --datadir other'), [])
            self.assertEqual(selected(f'"{exe}" --datadir "{profile}" --datadir="other"'), [])
            with patch.object(Path, "resolve", side_effect=AssertionError("must stay lexical")):
                self.assertEqual(selected(f'"{exe}" --datadir "{profile}"'), [20])

    def test_file_open_requires_complete_owned_model_transition(self):
        fixture = HERE.parents[1] / "resources" / "calib" / "filament_flow" / "flowrate-test-pass1.3mf"
        expected = drive.expected_3mf_objects(fixture)
        self.assertEqual(expected, ["flowrate_0", "flowrate_10", "flowrate_15",
                                    "flowrate_20", "flowrate_5", "flowrate_m10",
                                    "flowrate_m15", "flowrate_m20", "flowrate_m5"])
        def rows(names, *, path=None, pid=42, count=None, printable=1):
            count = len(names) if count is None else count
            return ([{"kind": "header", "pid": pid, "tag": "profile"},
                     {"kind": "toplevel", "hwnd": 7}, {"kind": "window", "hwnd": 8},
                     {"kind": "model_state", "model_available": True,
                      "mainframe_hwnd": 7, "plater_hwnd": 8,
                      "object_count": count, "object_records": len(names),
                      "objects_truncated": False, "project_path": path,
                      "project_path_available": bool(path), "project_path_truncated": False,
                      "active_plate_available": True, "active_plate_index": 0,
                      "active_plate_id": 0, "active_plate_instance_count": max(1, len(names)),
                      "active_plate_printable_instance_count": printable}]
                    + [{"kind": "model_object", "index": i, "name": name,
                        "object_available": True, "name_available": True,
                        "name_valid_utf8": True, "name_truncated": False,
                        "instance_count": 1} for i, name in enumerate(names)]
                    + [{"kind": "end"}])
        before_records = rows([])
        after_records = rows(expected, path=str(fixture))
        before = drive.model_snapshot(before_records, pid=42, profile_tag="profile", main_hwnd=7)
        after = drive.model_snapshot(after_records, pid=42, profile_tag="profile", main_hwnd=7)
        result = drive.fixture_model_transition(before, after, expected, fixture)
        self.assertEqual(result["expected_object_count"], len(expected))
        self.assertEqual(result["after"]["names"], expected)
        self.assertIsNone(drive.model_snapshot(after_records[:-1], pid=42,
                                                profile_tag="profile", main_hwnd=7))
        self.assertIsNone(drive.model_snapshot(
            [item for item in after_records if item.get("kind") != "model_state"],
            pid=42, profile_tag="profile", main_hwnd=7))
        self.assertIsNone(drive.model_snapshot(after_records, pid=43,
                                                profile_tag="profile", main_hwnd=7))
        self.assertIsNone(drive.model_snapshot(after_records, pid=42,
                                                profile_tag="other", main_hwnd=7))
        duplicate = [dict(item) for item in after_records]
        duplicate[5]["index"] = 0
        self.assertIsNone(drive.model_snapshot(duplicate, pid=42, profile_tag="profile", main_hwnd=7))
        incomplete = rows(expected[:-1], path=str(fixture), count=len(expected))
        self.assertIsNone(drive.model_snapshot(incomplete, pid=42, profile_tag="profile", main_hwnd=7))
        truncated = [dict(item) for item in after_records]
        truncated[4]["name_truncated"] = True
        self.assertIsNone(drive.model_snapshot(truncated, pid=42, profile_tag="profile", main_hwnd=7))
        invalid_utf8 = [dict(item) for item in after_records]
        invalid_utf8[4]["name_valid_utf8"] = False
        self.assertIsNone(drive.model_snapshot(invalid_utf8, pid=42,
                                                profile_tag="profile", main_hwnd=7))
        self.assertIsNone(drive.fixture_model_transition(
            before, drive.model_snapshot(rows(expected, path=str(fixture) + "-other"),
                                         pid=42, profile_tag="profile", main_hwnd=7), expected, fixture))
        self.assertIsNone(drive.fixture_model_transition(
            before, drive.model_snapshot(rows(expected, path=str(fixture), printable=0),
                                         pid=42, profile_tag="profile", main_hwnd=7), expected, fixture))
        self.assertIsNone(drive.fixture_model_transition(after, after, expected, fixture))
        class ExitedApp:
            main = 7
            def windows(self):
                return []
        with tempfile.TemporaryDirectory() as temp:
            instance = drive.Drive(ExitedApp(), Path(temp), "a" * 40, "md3-v122",
                                   "b" * 64, "123", "en")
            with self.assertRaisesRegex(RuntimeError, "exited"):
                instance.wait_fixture_loaded(fixture, expected, before, timeout=1)

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
        def closed(tool, **_kwargs):
            calls.append(tool)
            if tool == "list_headless_windows":
                raise RuntimeError("OpenDesktopW('desktop') failed (GetLastError=2)")
            return {"ok": True}
        with patch.object(drive, "process_snapshot", return_value=[]), patch.object(
            drive, "cheap", side_effect=closed):
            app.stop()
        self.assertEqual(calls, ["close_headless_desktop", "list_headless_windows"])
        self.assertTrue(app.owned_teardown_verified)
        self.assertTrue(app.desktop_closed_verified)

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
            def closed(tool, **kwargs):
                calls.append((tool, kwargs))
                if tool == "list_headless_windows":
                    raise RuntimeError("OpenDesktopW('desktop') failed (GetLastError=2)")
                return {"ok": True}
            with patch.object(drive, "process_snapshot", side_effect=[[child], []]), patch.object(
                    drive, "cheap", side_effect=closed):
                app.stop()
            self.assertEqual([name for name, _ in calls],
                             ["kill_process", "close_headless_desktop", "list_headless_windows"])
            self.assertEqual(calls[0][1]["pid"], 20)
            self.assertEqual(app.seen_owned[20]["parent_pid"], 10)
            self.assertTrue(app.owned_teardown_verified)
            self.assertTrue(app.desktop_closed_verified)

    def test_owned_child_still_present_after_kill_blocks_teardown(self):
        with tempfile.TemporaryDirectory() as temp:
            exe = str(Path(temp) / "bambu-studio.exe")
            profile = str(Path(temp) / "profile")
            app = drive.HostedApp(exe, profile, "desktop", str(Path(temp) / "probe"))
            app.launch_pid = 10
            app.launch_started = datetime.now(timezone.utc)
            child = {"ProcessId": 20, "ParentProcessId": 10,
                     "ExecutablePath": exe,
                     "CommandLine": f'"{exe}" --datadir "{profile}"',
                     "CreationDate": app.launch_started.isoformat()}
            def closed(tool, **_kwargs):
                if tool == "list_headless_windows":
                    raise RuntimeError("OpenDesktopW('desktop') failed (GetLastError=2)")
                return {"ok": True}
            with patch.object(drive, "process_snapshot", return_value=[child]), patch.object(
                    drive, "cheap", side_effect=closed), patch.object(
                    drive.time, "monotonic", side_effect=[0, 1, 6]), patch.object(
                    drive.time, "sleep"):
                with self.assertRaisesRegex(RuntimeError, "Exact owned process teardown was not verified"):
                    app.stop()
            self.assertFalse(app.owned_teardown_verified)
            self.assertTrue(app.desktop_closed_verified)

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
