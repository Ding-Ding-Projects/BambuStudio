"""Offline synthetic fixtures only. No launch, input, capture or acceptance evidence."""
from copy import deepcopy
from datetime import timedelta
import importlib.util
import io
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

    def test_strict_json_rejects_nested_duplicates_and_nonfinite_numbers(self):
        for raw in ('{"schemaVersion":0,"schemaVersion":1}',
                    '{"privacy":{"status":"unreviewed","status":"reviewed-safe"}}',
                    '{"value":NaN}', '{"value":Infinity}', '{"value":-Infinity}',
                    '{"nested":[1e999]}'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                ledger.strict_json(raw)
        self.assertEqual(ledger.strict_json('{"value":1.25,"nested":[2]}'),
                         {"value": 1.25, "nested": [2]})

    def test_every_referenced_json_ingress_is_strict(self):
        companion = self.build.payload / "automation/build-identity.json"
        shell, post = self.root / "shell.jsonl", self.root / "post.jsonl"
        originals = {path: path.read_text() for path in
                     (companion, shell, post, self.build_path, self.review_path)}
        document = deepcopy(self.document)
        for ingress in ("build", "session", "companion", "initial-probe", "step-probe"):
            for fragment in ('"duplicate":0,"duplicate":1,', '"nonfinite":NaN,', '"overflow":1e999,'):
                with self.subTest(ingress=ingress, fragment=fragment):
                    for path, content in originals.items():
                        path.write_text(content)
                    self.document = deepcopy(document)
                    build = deepcopy(self.build.receipt)
                    review = deepcopy(self.review)
                    corrupt = lambda raw: "{" + fragment + raw[1:]
                    if ingress == "companion":
                        companion.write_text(corrupt(originals[companion]))
                        build["payload"]["files"]["automation/build-identity.json"] = ledger.native.digest(companion)
                    if ingress == "initial-probe":
                        shell.write_text(corrupt(originals[shell]))
                        review["probe"]["sha256"] = ledger.native.digest(shell)
                    if ingress == "step-probe":
                        post.write_text(corrupt(originals[post]))
                        self.document["steps"][0]["post"]["probe"] = self.ref(post)
                    build_text = json.dumps(build)
                    self.build_path.write_text(corrupt(build_text) if ingress == "build" else build_text)
                    self.document["buildReceipt"] = self.ref(self.build_path)
                    review["buildReceiptSha256"] = self.document["buildReceipt"]["sha256"]
                    self.document["steps"][0]["buildReceiptSha256"] = review["buildReceiptSha256"]
                    review_text = json.dumps(review)
                    self.review_path.write_text(corrupt(review_text) if ingress == "session" else review_text)
                    self.document["session"]["review"] = self.ref(self.review_path)
                    with self.assertRaisesRegex(ValueError, "Duplicate JSON key|Nonfinite JSON"):
                        self.validate()

    def test_read_is_bounded_even_if_file_grows_after_stat(self):
        class TrackingStream(io.BytesIO):
            def read(self, size=-1):
                self.requested = size
                return super().read(size)
        stream = TrackingStream(b'{"a":1}' + b" " * 100)
        with patch.object(ledger.native, "regular"), patch.object(Path, "open", return_value=stream):
            with self.assertRaisesRegex(ValueError, "exceeds read bound"):
                ledger.read_json(self.root / "grown.json", limit=8)
        self.assertEqual(stream.requested, 9)
        valid = TrackingStream(b'{"a":1}')
        with patch.object(ledger.native, "regular"), patch.object(Path, "open", return_value=valid):
            self.assertEqual(ledger.read_json(self.root / "bounded.json", limit=8), {"a": 1})
        self.assertEqual(valid.requested, 9)

    def test_initial_probe_and_step_probe_have_same_read_bound(self):
        with patch.object(ledger, "bounded_read", wraps=ledger.bounded_read) as reads:
            self.validate()
        observed = {call.args[0]: call.args[1] for call in reads.call_args_list}
        for name in ("shell.jsonl", "pre.jsonl", "post.jsonl"):
            self.assertEqual(observed[self.root / name], 16 * 1024 * 1024)
        path = self.root / "shell.jsonl"
        with path.open("wb") as stream:
            stream.truncate(ledger.PROBE_LIMIT + 1)
        self.review["probe"]["sha256"] = ledger.native.digest(path)
        self.review_path.write_text(json.dumps(self.review))
        self.document["session"]["review"] = self.ref(self.review_path)
        with self.assertRaisesRegex(ValueError, "exceeds size bound"):
            self.validate()

    def test_input_reader_rejects_duplicate_keys_before_validation(self):
        path = self.root / "observations.json"
        path.write_text('{"schemaVersion":0,' + json.dumps(self.document)[1:])
        with self.assertRaisesRegex(ValueError, "Duplicate JSON key"):
            ledger.read_json(path, ledger.PROBE_LIMIT)


if __name__ == "__main__":
    unittest.main()
