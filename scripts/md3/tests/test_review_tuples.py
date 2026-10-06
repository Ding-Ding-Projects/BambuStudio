"""Offline tuple accounting tests. Synthetic input never establishes live coverage."""
from copy import deepcopy
from contextlib import redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "review-tuples.py"
spec = importlib.util.spec_from_file_location("review_tuples", SCRIPT)
tuples = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tuples)


def requested(**changes):
    return {"boundary": "shared-control-callers", "language": "en", "theme": "light",
            "scalePercent": 125, "density": "comfortable", "motion": "reduced",
            "geometry": "normal", **changes}


def probe():
    return [{"kind": "header", "pid": 7, "tag": "fixture", "language": "en",
             "dark": False, "density": "comfortable", "dpi_scale": 1.25},
            {"kind": "toplevel", "hwnd": 20, "shown": True, "client": {"w": 1500, "h": 1000}},
            {"kind": "end"}]


def compare(rows=None, **changes):
    return tuples.compare(requested(**changes), rows, pid=7, hwnd=20, tag="fixture")


class TupleTests(unittest.TestCase):
    def test_inventory_is_unique_complete_and_never_observed(self):
        rows = tuples.inventory()
        self.assertEqual(len(rows), 1728)
        self.assertEqual(len({json.dumps(row["requested"], sort_keys=True) for row in rows}), 1728)
        for boundary in tuples.BOUNDARIES:
            self.assertEqual(sum(row["requested"]["boundary"] == boundary for row in rows), 192)
        self.assertTrue(all(row["status"] == "pending" and row["observed"] is None for row in rows))

    def test_no_probe_stays_pending(self):
        result = compare()
        self.assertEqual(result["status"], "pending")
        self.assertFalse(result["acceptance"])
        self.assertIn("nativeProbe", result["unavailable"])

    def test_matching_probe_reuses_validator_but_cannot_pass_motion_or_minimum(self):
        rows = probe()
        original = deepcopy(rows)
        validator = tuples.local_validator()
        with patch.object(tuples, "local_validator", return_value=validator):
            result = compare(rows)
        self.assertEqual(rows, original)
        self.assertEqual(result["probeValidation"], "native-validator-passed")
        self.assertEqual(result["status"], "pending")
        self.assertEqual(result["mismatches"], [])
        self.assertIn("effectiveMotion", result["unavailable"])
        self.assertIn("measuredNativeEm", result["unavailable"])
        self.assertFalse(result["acceptance"])

    def test_each_observable_mismatch_is_explicit(self):
        for field, value in (("language", "yue_HK"), ("theme", "dark"),
                             ("density", "compact"), ("scalePercent", 150)):
            with self.subTest(field=field):
                result = compare(probe(), **{field: value})
                self.assertEqual(result["status"], "mismatch")
                self.assertIn(field, result["mismatches"])
        rows = probe()
        rows[1]["client"]["w"] += 1
        self.assertIn("normalGeometry", compare(rows)["mismatches"])

    def test_expanded_tuple_does_not_forge_initial_profile(self):
        rows = probe()
        rows[0].update(language="bilingual_en_yue_HK", dark=True, density="compact")
        validator = tuples.local_validator()
        with patch.object(tuples, "local_validator", return_value=validator):
            result = compare(rows, language="bilingual_en_yue_HK", theme="dark", density="compact")
        self.assertEqual(result["status"], "pending")
        self.assertEqual(result["probeValidation"], "native-validator-passed")
        self.assertEqual(rows[0]["language"], "bilingual_en_yue_HK")
        self.assertFalse(result["acceptance"])

    def test_invalid_numbers_rejected_in_measurement_and_request(self):
        for value in (True, False, 0, -1, float("nan"), float("inf"), -float("inf"), "1.25", None):
            with self.subTest(value=value):
                rows = probe()
                rows[0]["dpi_scale"] = value
                with self.assertRaises(ValueError):
                    compare(rows)
                with self.assertRaises(ValueError):
                    tuples.minimum_geometry(value)
        for value in (True, 0, -1, 100.0, float("nan"), float("inf"), "100"):
            with self.assertRaises(ValueError):
                compare(probe(), scalePercent=value)
        for value in (True, 0, -1, 1.2, float("nan"), float("inf")):
            rows = probe()
            rows[1]["client"]["w"] = value
            with self.assertRaises(ValueError):
                compare(rows)

    def test_minimum_is_measured_native_rule_not_fixed_dip_size(self):
        self.assertEqual(tuples.minimum_geometry(10), {"w": 1000, "h": 600})
        self.assertEqual(tuples.minimum_geometry(20), {"w": 1520, "h": 980})
        with self.assertRaises(ValueError):
            tuples.minimum_geometry(1e308)
        with self.assertRaises(ValueError):
            tuples.minimum_geometry(10 ** 1000)
        rows = probe()
        rows[1]["client"] = {"w": 1000, "h": 600}
        result = compare(rows, geometry="minimum")
        self.assertEqual(result["status"], "pending")
        self.assertIn("minimumGeometry", result["unavailable"])
        # Unsupported fields in a raw dump are not trusted as observations.
        rows[0].update(em=10, effectiveMotion="reduced")
        self.assertIn("minimumGeometry", compare(rows, geometry="minimum")["unavailable"])

    def test_incomplete_ambiguous_wrong_owner_and_hidden_probes_rejected(self):
        mutations = [lambda rows: rows.pop(), lambda rows: rows.append({"kind": "end"}),
                     lambda rows: rows.insert(1, deepcopy(rows[0])),
                     lambda rows: rows.insert(2, deepcopy(rows[1])),
                     lambda rows: rows[0].update(pid=8), lambda rows: rows[0].update(tag="other"),
                     lambda rows: rows[1].update(shown=False), lambda rows: rows[1].update(hwnd=21)]
        for mutate in mutations:
            rows = probe()
            mutate(rows)
            with self.assertRaises(ValueError):
                compare(rows)

    def test_json_duplicate_and_nonfinite_values_rejected(self):
        for text in ('{"language":"en","language":"yue_HK"}', '{"x": NaN}', '{"x": Infinity}'):
            with self.assertRaises(ValueError):
                tuples.parse_json(text)

    def test_unknown_request_fields_and_unknown_boundaries_rejected(self):
        for change in ({"boundary": "invented"}, {"motion": "system"}, {"extra": "observation"}):
            with self.assertRaises(ValueError):
                compare(probe(), **change)

    def test_cli_never_exits_success_for_pending_or_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "requested.json"
            dump = Path(directory) / "native.jsonl"
            path.write_text(json.dumps(requested()), encoding="utf-8")
            dump.write_text("\n".join(json.dumps(row) for row in probe()), encoding="utf-8")
            arguments = ["compare", "--requested", str(path), "--pid", "7", "--hwnd", "20", "--tag", "fixture"]
            for extra in ([], ["--probe", str(dump)]):
                with redirect_stdout(io.StringIO()) as output:
                    self.assertEqual(tuples.main(arguments + extra), 3)
                self.assertFalse(json.loads(output.getvalue())["acceptance"])
            path.write_text(json.dumps(requested(theme="dark")), encoding="utf-8")
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(tuples.main(arguments + ["--probe", str(dump)]), 2)
            self.assertEqual(json.loads(output.getvalue())["status"], "mismatch")
            dump.write_text('{"kind": NaN}', encoding="utf-8")
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(tuples.main(arguments + ["--probe", str(dump)]), 1)


if __name__ == "__main__":
    unittest.main()
