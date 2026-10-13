"""Offline startup-trace checks. No executable under inspection is run."""
import argparse
from contextlib import redirect_stdout
import ctypes
import importlib.util
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch
import uuid

SCRIPT = Path(__file__).resolve().parents[1] / "trace-local-startup.py"
spec = importlib.util.spec_from_file_location("trace_local", SCRIPT)
trace = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trace)
GUID = uuid.UUID("f76be281-b33e-4788-b9cc-bbd92a426057")
ACK = "LOCAL_TRACE_INITIAL\nLOCAL_TRACE_TARGET 17 18\n00001 00002 ntdll (pdb symbols) cache/ntdll.pdb\n    sls - Show loader snaps\nLOCAL_TRACE_READY\n"


def fixtures(root, age=1):
    # One real-shaped PE debug directory and a minimal MSF stream directory.
    image = bytearray(1536)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 60, 128)
    image[128:132] = b"PE\0\0"
    struct.pack_into("<HH", image, 132, 0x8664, 1)
    struct.pack_into("<H", image, 148, 240)
    struct.pack_into("<H", image, 152, 0x20b)
    struct.pack_into("<II", image, 152 + 112 + 48, 0x1000, 28)
    struct.pack_into("<4I", image, 400, 1024, 0x1000, 1024, 512)
    record = b"RSDS" + GUID.bytes_le + struct.pack("<I", age) + b"%bambu-studio.pdb%\0"
    struct.pack_into("<4I", image, 524, 2, len(record), 0x1040, 576)
    image[576:576+len(record)] = record
    exe = root / "bambu-studio.exe"
    exe.write_bytes(image)
    (root / "BambuStudio.dll").write_bytes(image)
    pdb = bytearray(512 * 5)
    pdb[:32] = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0"
    struct.pack_into("<6I", pdb, 32, 512, 1, 5, 16, 0, 2)
    struct.pack_into("<I", pdb, 1024, 3)
    struct.pack_into("<4I", pdb, 1536, 2, 0, 28, 4)
    struct.pack_into("<3I", pdb, 2048, 20000404, 0, age)
    pdb[2060:2076] = GUID.bytes_le
    symbols = root / "target.pdb"
    symbols.write_bytes(pdb)
    return exe, symbols


class IdentityTests(unittest.TestCase):
    def test_matching_pdb_is_bound_to_image_guid_and_age(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            exe, pdb = fixtures(root)
            result = trace.validate_symbols(exe, (pdb, pdb))
            self.assertEqual(result[0]["guid"], str(GUID))
            self.assertEqual(result[0]["age"], 1)
            for offset in (2056, 2060):
                original = pdb.read_bytes()
                damaged = bytearray(original)
                damaged[offset] ^= 1
                pdb.write_bytes(damaged)
                with self.assertRaisesRegex(ValueError, "does not match"):
                    trace.validate_symbols(exe, (pdb, pdb))
                pdb.write_bytes(original)

    def test_symbol_container_and_pe_bounds_fail_closed(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            exe, pdb = fixtures(root)
            for path, offset, value in ((pdb, 52, 99), (pdb, 44, 0xffffffff), (exe, 60, 0xffffffff), (exe, 536, 0xfffffff0)):
                before = path.read_bytes()
                changed = bytearray(before)
                struct.pack_into("<I", changed, offset, value)
                path.write_bytes(changed)
                with self.assertRaises((ValueError, struct.error)):
                    trace.pdb_identity(path) if path == pdb else trace.pe_identity(path)
                path.write_bytes(before)

    def test_debugger_manifest_requires_every_exact_tool(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            rows = []
            for name in trace.TOOLS:
                path = root / name
                path.write_bytes(name.encode())
                rows.append(dict(Name=name, Signature="Valid", Machine="0x8664", Publisher="CN=Microsoft Corporation", SHA256=trace.digest(path)))
            receipt = root / "identity.json"
            receipt.write_text(json.dumps(rows))
            trace.validate_tools(root / "cdb.exe", receipt)
            (root / "dbgeng.dll").write_bytes(b"different")
            with self.assertRaisesRegex(ValueError, "identity differs"):
                trace.validate_tools(root / "cdb.exe", receipt)
            receipt.write_text(json.dumps(rows[:-1]))
            with self.assertRaisesRegex(ValueError, "incomplete"):
                trace.validate_tools(root / "cdb.exe", receipt)

    def test_fixed_commands_stop_before_any_continuation(self):
        commands = trace.commands()
        self.assertIn("!gflag +sls\n!gflag\n", commands)
        self.assertIn(".symopt- 0x40", commands)
        self.assertIn(".symopt+ 0x400", commands)
        self.assertNotIn("g", commands.splitlines())
        self.assertNotIn("bu ", commands)
        self.assertEqual(trace.acknowledgement(ACK), (17, 18))
        for text in (ACK.replace("sls - Show loader snaps", ""), ACK.replace("(pdb symbols)", "(export symbols)"),
                     ACK.replace("LOCAL_TRACE_READY", ""), ACK + "LOCAL_TRACE_TARGET 21 22\n", ACK + "mismatched pdb\n"):
            self.assertIsNone(trace.acknowledgement(text))

    def test_environment_and_directory_are_fresh(self):
        with tempfile.TemporaryDirectory() as folder, patch.dict(os.environ, {"Temp":"old", "TMP":"old", "_NT_SYMBOL_PATH":"foreign", "BAMBU_AUTOMATION":"1"}):
            root = Path(folder).resolve()
            env = trace.isolated_environment(root)
            self.assertEqual(env["TEMP"], str(root / "temp"))
            self.assertEqual(env["TMP"], env["TEMP"])
            self.assertNotIn("Temp", env)
            self.assertNotIn("_NT_SYMBOL_PATH", env)
            self.assertNotIn("BAMBU_AUTOMATION", env)
            with self.assertRaises(FileExistsError):
                trace.isolated_environment(root)

    def test_validation_only_cannot_dispatch(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder).resolve() / "file"
            path.write_text("fixture")
            argv = [str(SCRIPT), "--source-commit", "a" * 40]
            for name in trace.PATH_ARGS:
                argv.extend(["--" + name.replace("_", "-"), str(path)])
            with patch.object(sys, "argv", argv), patch.object(trace, "readiness", return_value=(path, {}, [])), \
                 patch.object(trace, "dispatch", side_effect=AssertionError("must not launch")), redirect_stdout(io.StringIO()):
                self.assertEqual(trace.main(), 0)

    def test_existing_provenance_validator_is_required(self):
        args = argparse.Namespace(build_receipt=Path("receipt"), producer=Path("producer"), source_commit="a"*40)
        with patch.object(trace.review, "read_json", return_value={}), \
             patch.object(trace.review, "validate_receipt", side_effect=ValueError("stale build")), \
             patch.object(trace, "validate_tools") as tools:
            with self.assertRaisesRegex(ValueError, "stale build"):
                trace.readiness(args)
            tools.assert_not_called()


class OwnershipTests(unittest.TestCase):
    def native(self, interrupt=False, reject_key=None):
        k, u, state = MagicMock(), MagicMock(), {"jobs": [], "handles": [], "children": {}}
        k.CreateJobObjectW.return_value = 11
        k.SetInformationJobObject.return_value = True
        k.TerminateJobObject.return_value = True
        k.WaitForSingleObject.return_value = 0
        u.OpenInputDesktop.return_value = 44
        def desktop(handle, kind, text, length, needed):
            text.value = "Default"
            return True
        u.GetUserObjectInformationW.side_effect = desktop
        def initialize(attributes, count, flags, size):
            self.assertEqual(count, 2)
            size._obj.value = 128
            return attributes is not None
        k.InitializeProcThreadAttributeList.side_effect = initialize
        def update(attributes, flags, key, value, size, prev, returned):
            if key == 0x2000D:
                state["jobs"] = list(value)
            elif key == 0x20002:
                state["handles"] = list(value)
            else:
                self.fail("Unexpected process attribute")
            state["attributes"] = ctypes.addressof(attributes)
            return key != reject_key
        k.UpdateProcThreadAttribute.side_effect = update
        class Abrupt(BaseException): pass
        def create(exe, cmd, pa, ta, inherit, flags, env, cwd, si, pi):
            base = si.contents
            class Extended(ctypes.Structure):
                _fields_ = [("base", type(base)), ("attributes", ctypes.c_void_p)]
            extended = ctypes.cast(si, ctypes.POINTER(Extended)).contents
            self.assertEqual(base.desktop, "WinSta0\\Default")
            self.assertEqual(flags, 0x8080404)
            self.assertTrue(inherit)
            self.assertEqual(state["handles"], [51, 52])
            atomic = (extended.attributes == state["attributes"] and state["jobs"] == [11])
            state["children"][22] = 11 if atomic else None
            pi._obj.process, pi._obj.thread, pi._obj.pid = 22, 33, 7
            if interrupt:
                state["children"] = {pid: job for pid, job in state["children"].items() if job != 11}
                raise Abrupt()
            return True
        k.CreateProcessW.side_effect = create
        k.ResumeThread.return_value = 1
        def account(job, kind, buffer, length, returned):
            buffer._obj.active = 0
            return True
        k.QueryInformationJobObject.side_effect = account
        return k, u, state, Abrupt

    def test_atomic_creation_and_only_owned_job_teardown(self):
        for reject in (None, 0x2000D, 0x20002):
            k, u, state, abrupt = self.native(reject_key=reject)
            session = trace.NativeDebugger.__new__(trace.NativeDebugger)
            session.pipe_handles = (51, 52, 52)
            with patch.object(trace.ctypes, "WinDLL", create=True, side_effect=lambda name, **kw: k if name == "kernel32" else u), patch.object(trace.os, "name", "nt"):
                if reject:
                    with self.assertRaises(trace.LaunchFailure):
                        session._start(Path("C:/cdb.exe"), ["cdb.exe"], {}, Path("C:/"))
                    k.CreateProcessW.assert_not_called()
                else:
                    session._start(Path("C:/cdb.exe"), ["cdb.exe"], {}, Path("C:/"))
                    self.assertEqual(state["children"], {22: 11})
                    self.assertTrue(session.close())
                k.TerminateJobObject.assert_called_once_with(11, 0)
                k.AssignProcessToJobObject.assert_not_called()

    def test_interruption_at_creation_boundary_has_no_orphan(self):
        k, u, state, abrupt = self.native(interrupt=True)
        session = trace.NativeDebugger.__new__(trace.NativeDebugger)
        session.pipe_handles = (51, 52, 52)
        with patch.object(trace.ctypes, "WinDLL", create=True, side_effect=lambda name, **kw: k if name == "kernel32" else u), patch.object(trace.os, "name", "nt"):
            with self.assertRaises(abrupt):
                session._start(Path("C:/cdb.exe"), ["cdb.exe"], {}, Path("C:/"))
        self.assertEqual(state["children"], {})
        k.ResumeThread.assert_not_called()
        k.TerminateJobObject.assert_not_called()

    def test_wrong_job_image_thread_or_desktop_never_retains_target(self):
        for bad in ("job", "image", "thread", "desktop", None):
            session = trace.NativeDebugger.__new__(trace.NativeDebugger)
            k, u = MagicMock(), MagicMock()
            session.k, session.u, session.job, session.process = k, u, 11, 22
            k.OpenProcess.return_value = 55
            k.OpenThread.return_value = 66
            k.WaitForSingleObject.return_value = 258
            k.GetProcessIdOfThread.return_value = 9 if bad == "thread" else 17
            def member(handle, job, yes):
                yes._obj.value = bad != "job"
                return True
            def image(handle, flags, buffer, size):
                buffer.value = "C:/foreign.exe" if bad == "image" else "C:/target.exe"
                return True
            def desktop(handle, kind, buffer, size, length):
                buffer.value = "Other" if bad == "desktop" else "Default"
                return True
            k.IsProcessInJob.side_effect = member
            k.QueryFullProcessImageNameW.side_effect = image
            u.GetUserObjectInformationW.side_effect = desktop
            if bad:
                with self.assertRaises(ValueError):
                    session.retain_target(17, 18, Path("C:/target.exe"))
                k.CloseHandle.assert_any_call(55)
            else:
                self.assertEqual(session.retain_target(17, 18, Path("C:/target.exe")).process, 55)
                self.assertNotIn(unittest.mock.call(55), k.CloseHandle.call_args_list)

    def test_timeout_never_claims_teardown_or_target_result(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            args = argparse.Namespace(source_commit="a"*40, **{key:root / key for key in trace.PATH_ARGS})
            def call(*a, **kw):
                self.assertEqual(a[1], "run_command")
                self.assertGreater(kw["timeout"], trace.WORKER_SECONDS + 10)
                raise subprocess.TimeoutExpired("mock", 270)
            report = trace.dispatch(args, root, call)
            self.assertEqual(report["teardown"], "unverified")
            self.assertEqual(report["targetExit"], {"status":"unavailable"})
            self.assertTrue((root / "stop").is_file())

    def test_worker_rejects_foreign_request_before_native_work(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(trace, "trace", side_effect=AssertionError("must not launch")):
            with self.assertRaisesRegex(ValueError, "Unowned"):
                trace.worker(Path(folder) / "request.json")


class LifecycleTests(unittest.TestCase):
    def run_trace(self, *, refuse=False, output=ACK + "LOCAL_TRACE_EXIT\n", timeout=False):
        events = []
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            exe, pdb = fixtures(root)
            args = argparse.Namespace(source_commit="a"*40, build_receipt=exe, cdb=root / "cdb.exe")
            class Target:
                pid, process = 17, 55
                def observe_target_exit(self):
                    events.append("target-exit")
                    return {"status":"exited", "exitCode":0xffffffff, "observedBeforeTeardown":True}
            class Session:
                pid = 7
                stdin, stdout, k = io.BytesIO(), io.BytesIO(output.encode()), MagicMock()
                def alive(self): return timeout
                def retain_target(self, *a):
                    events.append("ownership")
                    if refuse: raise ValueError("Wrong owner")
                    return Target()
                def observe_target_exit(self): return {"status":"exited", "exitCode":0}
                def close(self): events.append("teardown"); return True
            session = Session()
            with patch.object(trace, "readiness", return_value=(exe, {}, [])), patch.object(trace, "TRACE_SECONDS", 0.02):
                report = trace.trace(args, root, lambda *a: session)
            return report, events

    def test_target_exit_is_observed_before_teardown(self):
        report, events = self.run_trace()
        self.assertEqual(events, ["ownership", "target-exit", "teardown"])
        self.assertEqual(report["targetExit"]["exitCode"], 0xffffffff)
        self.assertEqual(report["loaderTrace"], "collected")
        self.assertEqual(report["runtimeAcceptance"], "unverified")

    def test_wrong_owner_never_continues(self):
        report, events = self.run_trace(refuse=True)
        self.assertEqual(report["continuation"], "not_attempted")
        self.assertEqual(events, ["ownership", "teardown"])
        self.assertEqual(report["targetExit"], {"status":"not_observed"})

    def test_partial_startup_and_deadline_remain_incomplete(self):
        for kwargs in ({"output":""}, {"timeout":True}):
            report, events = self.run_trace(**kwargs)
            self.assertIn("failure", report)
            self.assertEqual(report["loaderTrace"], "incomplete")
            self.assertEqual(events[-1], "teardown")


if __name__ == "__main__":
    unittest.main()
