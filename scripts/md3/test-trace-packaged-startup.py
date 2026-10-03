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
                    good.replace("123", "0"), good + "TRACE_TARGET 123 456\n",
                    good + "TRACE_CREATION_INITIAL\n"):
            self.assertIsNone(driver.creation_acknowledgement(bad))

    def test_creation_route_never_attaches_or_skips_initial_break(self):
        command = driver.creation_arguments("cdb.exe", "product.exe", "profile", "fixed.txt", "symbols")
        self.assertEqual(command[-3:], ["product.exe", "--datadir", "profile"])
        self.assertEqual(command[command.index("-cf") + 1], "fixed.txt")
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
