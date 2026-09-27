"""Focused exact-PID and detach command contract for the hosted trace driver."""
from __future__ import annotations

import importlib.util
import pathlib
import unittest


MODULE = pathlib.Path(__file__).with_name("trace-packaged-startup.py")
SPEC = importlib.util.spec_from_file_location("startup_trace_driver", MODULE)
assert SPEC and SPEC.loader
driver = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(driver)


class CdbAttachmentContract(unittest.TestCase):
    def test_exact_pid_and_detach_are_required(self):
        command = driver.cdb_arguments("cdb.exe", 4912, "commands.txt", "restricted.log", "empty-symbols")
        self.assertEqual(command[-2:], ["-p", "4912"])
        self.assertIn("-pd", command)
        self.assertEqual(command[command.index("-y") + 1], "empty-symbols")
        self.assertNotIn("-pn", command)

    def test_missing_pid_is_rejected_before_attach(self):
        with self.assertRaises(ValueError):
            driver.cdb_arguments("cdb.exe", 0, "commands.txt", "restricted.log", "empty-symbols")


if __name__ == "__main__":
    unittest.main()
