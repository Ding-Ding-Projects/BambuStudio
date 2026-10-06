"""Offline synthetic fixtures only. No launch, input, capture or acceptance evidence."""
from copy import deepcopy
from datetime import timedelta
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ledger = load("review_ledger", HERE.parent / "review-ledger.py")
fixtures = load("build_fixtures", HERE / "test_local_native_review.py")


class LedgerTests(unittest.TestCase):
    def setUp(self):
        self.build = fixtures.ReceiptTests()
        self.build.setUp()
        self.addCleanup(self.build.doCleanups)
        self.temp = tempfile.TemporaryDirectory(prefix="bambu-local-review-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.now = self.build.now
        self.build_path = self.root / "build.json"
        self.build_path.write_text(json.dumps(self.build.receipt))
        self.probe = [{"kind": "header", "pid": 123, "tag": self.root.name,
                       "language": "en", "dark": False, "density": "comfortable", "dpi_scale": 1.0},
                      {"kind": "toplevel", "hwnd": 456, "shown": True, "client": {"w": 8, "h": 6}},
                      {"kind": "end"}]
        self.write_probe("shell.jsonl")
        self.review_path = self.root / "review.json"
        self.review = {"schemaVersion": 1, "kind": "local-initial-shell", "sourceCommit": fixtures.SOURCE,
                       "buildReceiptSha256": ledger.native.digest(self.build_path),
                       "driverSha256": ledger.native.digest(HERE.parent / "local-native-review.py"),
                       "desktop": "visible", "teardown": "verified",
                       "launch": {"status": "started", "pid": 123, "shell": {"pid": 123,
                                  "hwnd": 456, "class": "wxWindowNR", "visible": True}},
                       "probe": {"status": "received", "sha256": ledger.native.digest(self.root / "shell.jsonl")}}
        self.review_path.write_text(json.dumps(self.review))
        self.document = {"schemaVersion": 1, "kind": "local-native-interactions",
                         "producer": str(self.build.root), "sourceCommit": fixtures.SOURCE,
                         "buildReceipt": self.ref(self.build_path),
                         "session": {"id": self.root.name, "review": self.ref(self.review_path)}, "steps": []}
        self.document["steps"] = [self.step(1, self.snapshot("pre", 40), self.snapshot("post", 20))]
        original = ledger.native.validate_receipt
        self.validation = patch.object(ledger.native, "validate_receipt", side_effect=lambda receipt, root, source, now:
                                       original(receipt, root, source, now, self.build.git))
        self.validation.start()
        self.addCleanup(self.validation.stop)

    def at(self, seconds):
        return (self.now - timedelta(seconds=seconds)).isoformat().replace("+00:00", "Z")

    def ref(self, path):
        return {"path": str(path), "sha256": ledger.native.digest(path)}

    def write_probe(self, name):
        path = self.root / name
        path.write_text("\n".join(json.dumps(row) for row in self.probe))
        return path

    def snapshot(self, name, seconds):
        from PIL import Image
        path = self.root / (name + ".png")
        # Generated pixels exercise file validation only, never runtime evidence.
        image = Image.new("RGB", (8, 6), "white")
        image.putpixel((0, 0), (0, 0, 0))
        image.save(path)
        return {"atUtc": self.at(seconds), "semanticState": "Synthetic fixture state " + name,
                "hwnd": 456, "tuple": {"language": "en", "dark": False, "density": "comfortable",
                                       "dpiScale": 1.0, "client": {"w": 8, "h": 6}, "motion": "reduced"},
                "probe": self.ref(self.write_probe(name + ".jsonl")),
                "capture": {"file": self.ref(path), "reply": {"rendered_ok": True, "mode": "window",
                                                              "window_hwnd": 456, "path": str(path)}},
                "privacy": {"status": "reviewed-safe", "reviewer": "synthetic-fixture",
                            "reviewedAtUtc": self.at(0)}}

    def step(self, sequence, pre, post):
        return {"id": "step-" + str(sequence), "sequence": sequence, "status": "observed",
                "sourceCommit": fixtures.SOURCE, "buildReceiptSha256": self.document["buildReceipt"]["sha256"],
                "sessionId": self.root.name, "pid": 123,
                "action": {"method": "native-keyboard", "target": "Synthetic target", "atUtc": self.at(30)},
                "pre": pre, "post": post}

    def validate(self, document=None):
        return ledger.validate(self.document if document is None else document, self.now)

    def test_valid_fixture_never_claims_acceptance(self):
        result = self.validate()
        self.assertEqual(result["observedSteps"], 1)
        self.assertEqual(result["status"], "evidence-consistent")
        self.assertEqual(result["runtimeAcceptance"], "unverified")
        self.assertEqual(result["visualAcceptance"], "unverified")
        self.assertEqual(result["publication"], "not_authorized")
        self.assertEqual(result["document"], self.document)

    def test_empty_plan_is_incomplete(self):
        self.document["steps"] = []
        self.assertEqual(self.validate()["status"], "incomplete")

    def test_missing_hash_changed_bytes_and_source_are_rejected(self):
        for mutation in (lambda d: d.update(sourceCommit="c" * 40),
                         lambda d: d["buildReceipt"].update(sha256="0" * 64),
                         lambda d: d["steps"][0]["post"]["capture"]["file"].update(path=str(self.root / "missing.png"))):
            bad = deepcopy(self.document)
            mutation(bad)
            with self.assertRaises((ValueError, OSError)):
                self.validate(bad)
        (self.root / "post.png").write_bytes(b"changed")
        with self.assertRaises(ValueError):
            self.validate()

    def test_planned_mismatched_repeated_or_out_of_order_steps_rejected(self):
        for key, value in (("status", "planned"), ("sequence", 2), ("sequence", True),
                           ("pid", 999), ("sourceCommit", "d" * 40), ("sessionId", "other-session"),
                           ("buildReceiptSha256", "f" * 64)):
            bad = deepcopy(self.document)
            bad["steps"][0][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.validate(bad)
        self.document["steps"].append(deepcopy(self.document["steps"][0]))
        with self.assertRaises(ValueError):
            self.validate()

    def test_observation_privacy_tuple_and_input_contracts(self):
        for mutation in (lambda s: s["post"].update(semanticState=""),
                         lambda s: s["post"].update(semanticState="planned"),
                         lambda s: s["post"].update(atUtc=self.at(50)),
                         lambda s: s["post"]["tuple"].update(language="yue_HK"),
                         lambda s: s["post"]["tuple"].update(dpiScale=1.5),
                         lambda s: s["post"]["privacy"].update(status="unreviewed"),
                         lambda s: s["post"]["privacy"].update(reviewedAtUtc=self.at(55)),
                         lambda s: s["post"]["capture"]["reply"].update(window_hwnd=999),
                         lambda s: s["action"].update(method="planned-hook"),
                         lambda s: s["post"].update(profileContents="not permitted")):
            bad = deepcopy(self.document)
            mutation(bad["steps"][0])
            with self.assertRaises(ValueError):
                self.validate(bad)

    def test_probe_requires_end_and_session_ownership(self):
        for rows in (self.probe[:-1], [{**self.probe[0], "pid": 999}, *self.probe[1:]]):
            path = self.root / "post.jsonl"
            path.write_text("\n".join(map(json.dumps, rows)))
            self.document["steps"][0]["post"]["probe"] = self.ref(path)
            with self.assertRaises(ValueError):
                self.validate()

    def test_session_requires_completed_launch_and_current_driver(self):
        for mutation in (lambda r: r.update(teardown="unverified"),
                         lambda r: r.update(driverSha256="a" * 64),
                         lambda r: r.update(failure="fixture failure"),
                         lambda r: r["launch"].update(status="not_attempted")):
            bad = deepcopy(self.review)
            mutation(bad)
            self.review_path.write_text(json.dumps(bad))
            self.document["session"]["review"] = self.ref(self.review_path)
            with self.assertRaises(ValueError):
                self.validate()

    def test_continuity_and_distinct_observation_files(self):
        second = self.step(2, deepcopy(self.document["steps"][0]["post"]), self.snapshot("last", 5))
        second["action"]["atUtc"] = self.at(10)
        self.document["steps"].append(second)
        self.assertEqual(self.validate()["observedSteps"], 2)
        second["pre"]["semanticState"] = "Different state without recorded transition"
        with self.assertRaises(ValueError):
            self.validate()
        self.document["steps"] = self.document["steps"][:1]
        self.document["steps"][0]["post"]["probe"] = self.document["steps"][0]["pre"]["probe"]
        with self.assertRaises(ValueError):
            self.validate()

    def test_output_is_new_private_temp_only(self):
        report = self.validate()
        for target in (Path("relative"), HERE / "bambu-ledger-public", self.root):
            with self.assertRaises(ValueError):
                ledger.save(report, target)
        with tempfile.TemporaryDirectory(prefix="bambu-ledger-") as directory:
            path = Path(directory).resolve()
            with self.assertRaises(FileExistsError):
                ledger.save(report, path)

    def test_output_preserves_provenance_without_modifying_originals(self):
        report = self.validate()
        with tempfile.TemporaryDirectory(prefix="bambu-ledger-") as directory:
            path = Path(directory).resolve()
            path.rmdir()  # Empty directory owned by this fixture only.
            ledger.save(report, path)
            saved = json.loads((path / "ledger.json").read_text(encoding="utf-8"))
            self.assertEqual(saved, report)
            for original, digest in saved["retainedOriginals"].items():
                self.assertEqual(ledger.native.digest(Path(original)), digest)

    def test_cli_failure_does_not_echo_private_path(self):
        result = subprocess.run([sys.executable, "-B", str(HERE.parent / "review-ledger.py"),
                                 "--input", str(self.root / "private-profile-marker.json"),
                                 "--evidence-root", str(self.root / "unused")],
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 2)
        self.assertNotIn("private-profile-marker", result.stdout + result.stderr)
        self.assertNotIn(str(self.root), result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stderr)["runtimeAcceptance"], "unverified")
        self.assertFalse((self.root / "unused").exists())


if __name__ == "__main__":
    unittest.main()
