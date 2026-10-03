"""Focused exact-PID and detach command contract for the hosted trace driver."""
from __future__ import annotations

import importlib.util
import pathlib
import unittest
from startup_desktop_holder import absent_response


MODULE = pathlib.Path(__file__).with_name("trace-packaged-startup.py")
SPEC = importlib.util.spec_from_file_location("startup_trace_driver", MODULE)
assert SPEC and SPEC.loader
driver = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(driver)


class CdbAttachmentContract(unittest.TestCase):
    def test_missing_desktop_is_a_semantic_zero_exit_response(self):
        name = 'startup-loader-1-1'
        response = {'ok': False, 'error': "OpenDesktopW('startup-loader-1-1') failed (GetLastError=2: missing)"}
        self.assertTrue(absent_response(0, response, name))
        self.assertFalse(absent_response(1, response, name))

    def test_other_desktop_and_native_codes_never_prove_absence(self):
        response = {'ok': False, 'error': "OpenDesktopW('startup-loader-1-1') failed (GetLastError=2: missing)"}
        for bad in ({**response, 'ok': True}, {**response, 'ok': 0},
                    {**response, 'error': response['error'].replace('=2:', '=20:')},
                    {**response, 'error': response['error'].replace('=2:', '=5:')},
                    {**response, 'error': response['error'].replace('1-1', '1-2')}):
            self.assertFalse(absent_response(0, bad, 'startup-loader-1-1'))
    def test_creation_acknowledgement_needs_emitted_identity_and_flag(self):
        good = "TRACE_CREATION_INITIAL\nTRACE_TARGET 123 456\n  sls - Show loader snaps\n"
        self.assertEqual(driver.creation_acknowledgement(good), (123, 456))
        for bad in (good.replace("TRACE_CREATION_INITIAL", "0:000> .echo TRACE_CREATION_INITIAL"),
                    good.replace("  sls - Show loader snaps", ""),
                    good.replace("  sls - Show loader snaps", "Could not find NtGlobalFlag in nt!_PEB"),
                    good.replace("123", "0"), good + "TRACE_TARGET 123 456\n",
                    good + "TRACE_CREATION_INITIAL\n"):
            self.assertIsNone(driver.creation_acknowledgement(bad))
        # Exercise the production short-circuit chain without native processes.
        for rows, members, desktop, expected_stage, accepted in (
                ([], [True, True], "owned", "inventory_cardinality", False),
                ([{"pid": 123}, {"pid": 123}], [True, True], "owned", "inventory_cardinality", False),
                ([{"pid": 124}], [True, True], "owned", "target_identity", False),
                ([{"pid": 123}], [False, True], "owned", "debugger_membership", False),
                ([{"pid": 123}], [True, False], "owned", "target_membership", False),
                ([{"pid": 123}], [True, True], "other", "target_desktop", False),
                ([{"pid": 123}], [True, True], "owned", "ownership_verified", True)):
            observation, calls = {}, []
            values = iter(members)
            def member(pid):
                calls.append(pid)
                return next(values)
            actual = driver.creation_ownership_observation(observation, (123, 456), 10,
                lambda: rows, member, lambda thread: desktop, "owned")
            self.assertEqual(actual, accepted)
            self.assertEqual(observation["stage"], expected_stage)
            self.assertEqual(observation["inventory_count"], len(rows))
            expected_calls = [] if expected_stage in ("inventory_cardinality", "target_identity") else [10]
            if expected_stage in ("target_membership", "target_desktop", "ownership_verified"):
                expected_calls.append(123)
            self.assertEqual(calls, expected_calls)
            self.assertTrue(all(type(v) in (str, int, bool) for v in observation.values()))
        observation = {}
        def unavailable_inventory():
            raise RuntimeError("private detail must not enter observations")
        with self.assertRaises(RuntimeError):
            driver.creation_ownership_observation(observation, (123, 456), 10,
                unavailable_inventory, lambda pid: True, lambda thread: "owned", "owned")
        self.assertEqual(observation, {"stage": "inventory_query"})
        # Exercise actual observation logic with native calls replaced, not processes.
        import ctypes
        from ctypes import wintypes
        for handle, success, required in ((0, False, 0), (7, False, 1024),
                                          (7, False, 100000), (7, True, 12)):
            observed, calls = {}, []
            def get_desktop(thread):
                calls.append("handle")
                self.assertEqual(thread, 456)
                return handle
            def get_information(native_handle, index, name, capacity, needed):
                calls.append("information")
                self.assertEqual((native_handle, index), (7, 2))
                self.assertEqual(capacity, ctypes.sizeof(name))
                ctypes.cast(needed, ctypes.POINTER(wintypes.DWORD))[0] = required
                name.value = "owned"
                return success
            def last_error():
                calls.append("error")
                return 5
            if handle and success:
                self.assertEqual(driver.desktop_lookup_observation(observed, 456,
                    get_desktop, get_information, last_error), "owned")
                self.assertEqual(calls, ["handle", "information"])
                self.assertEqual(observed["stage"], "target_desktop")
                self.assertIsNone(observed["desktop_information_error"])
            else:
                with self.assertRaises(ValueError):
                    driver.desktop_lookup_observation(observed, 456,
                        get_desktop, get_information, last_error)
                self.assertEqual(calls, ["handle", "information", "error"] if handle
                                 else ["handle", "error"])
                self.assertEqual(observed["stage"], "target_desktop_name" if handle
                                 else "target_thread_desktop_handle")
            if handle:
                self.assertEqual(observed["desktop_required_bytes"], min(required, 65536))
                self.assertEqual(observed["desktop_required_bytes_capped"], required > 65536)
            self.assertNotIn("owned", observed.values())
        for failing_stage in ("handle", "information"):
            observed = {}
            def get_desktop(thread):
                if failing_stage == "handle":
                    raise RuntimeError("private native detail")
                return 7
            def get_information(*args):
                raise RuntimeError("private native detail")
            with self.assertRaises(RuntimeError):
                driver.desktop_lookup_observation(observed, 456, get_desktop,
                    get_information, lambda: self.fail("No return, no native error read"))
            self.assertEqual(observed["stage"], "target_thread_desktop_handle"
                             if failing_stage == "handle" else "target_desktop_name")

    def test_creation_route_never_attaches_or_skips_initial_break(self):
        cache = r"C:\owned cache\symbols"
        command = driver.creation_arguments("cdb.exe", "product.exe", "profile", "fixed.txt", cache)
        self.assertEqual(command[-3:], ["product.exe", "--datadir", "profile"])
        self.assertEqual(command[command.index("-cf") + 1], "fixed.txt")
        self.assertIn("-sins", command)
        self.assertIn("-ses", command)
        self.assertEqual(command[command.index("-y") + 1],
                         "srv*" + cache + "*https://msdl.microsoft.com/download/symbols")
        for bad in ("symbols", r"\\other\share\symbols", r"C:\cache*https://other.invalid",
                    r"C:\cache;C:\other", 'C:\\cache"', "C:\\cache\n", r"C:\cache\..\other"):
            with self.assertRaises(ValueError):
                driver.creation_arguments("cdb.exe", "product.exe", "profile", "fixed.txt", bad)
        lines = driver.creation_commands().splitlines()
        self.assertEqual(lines[2:7], [".symopt- 0x40", ".reload /f ntdll.dll", "lmv m ntdll",
                                     "!gflag +sls", "!gflag"])
        self.assertNotIn("g", lines)
        self.assertNotIn(".reload /i", driver.creation_commands())
        for forbidden in ("-p", "-pd", "-g", "-G", "-pn", "-logo"):
            self.assertNotIn(forbidden, command)

    def test_exact_pid_and_detach_are_required(self):
        command = driver.cdb_arguments("cdb.exe", 4912, "commands.txt", "restricted.log", "empty-symbols")
        self.assertEqual(command[-2:], ["-p", "4912"])
        self.assertIn("-pd", command)
        self.assertEqual(command[command.index("-y") + 1], "empty-symbols")
        self.assertNotIn("-pn", command)

    def test_missing_pid_is_rejected_before_attach(self):
        with self.assertRaises(ValueError):
            driver.cdb_arguments("cdb.exe", 0, "commands.txt", "restricted.log", "empty-symbols")

    def test_desktop_absence_requires_exact_missing_desktop_result(self):
        self.assertTrue(driver.named_desktop_absent(
            {"error": "OpenDesktopW('startup-trace-1') GetLastError=2"}, "startup-trace-1"))
        self.assertFalse(driver.named_desktop_absent(
            {"error": "OpenDesktopW('startup-trace-2') GetLastError=2"}, "startup-trace-1"))
        self.assertFalse(driver.named_desktop_absent(
            {"error": "OpenDesktopW('startup-trace-1') GetLastError=5"}, "startup-trace-1"))
        for code in (20, 299):
            self.assertFalse(driver.named_desktop_absent(
                {"error": f"OpenDesktopW('startup-trace-1') GetLastError={code}"},
                "startup-trace-1"))

    def test_breakpoint_marker_needs_emitted_line_and_stack_frame(self):
        marker = "TRACE_RTL_EXIT"
        stack = " # Child-SP          RetAddr               Call Site\n00 00000000`001ff000 00007fff`12345678 ntdll!RtlExitUserProcess"
        self.assertTrue(driver.emitted_breakpoint_with_stack(f"{marker}\n{stack}\n", marker))
        self.assertFalse(driver.emitted_breakpoint_with_stack(
            f'0:000> bu ntdll!RtlExitUserProcess ".echo {marker}; k 24; gc"\n{stack}\n', marker))
        self.assertFalse(driver.emitted_breakpoint_with_stack(f"{marker}\nno stack\n", marker))
        self.assertFalse(driver.emitted_breakpoint_with_stack(f"{marker}\n # Child-SP\n", marker))

    def test_teardown_requires_fresh_absence_and_holder_receipt(self):
        state = {"owned_absent": True, "desktop_absent": True,
                 "holder_verified": True, "debugger_stopped": True,
                 "cleanup_errors": []}
        self.assertTrue(driver.teardown_verified(**state))
        for changed in ({"owned_absent": False}, {"desktop_absent": False},
                        {"holder_verified": False}, {"debugger_stopped": False},
                        {"cleanup_errors": ["holder_unverified"]}):
            self.assertFalse(driver.teardown_verified(**(state | changed)))


if __name__ == "__main__":
    unittest.main()
