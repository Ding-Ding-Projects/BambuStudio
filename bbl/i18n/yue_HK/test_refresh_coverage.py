#!/usr/bin/env python3
"""Coverage refresh regression tests; only the Python standard library is needed."""

from __future__ import annotations

import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import compile_translation
import refresh_coverage
from po_catalog import CatalogError, compile_mo, read_mo


HEADER = 'msgid ""\nmsgstr "Language: yue_HK\\nPlural-Forms: nplurals=1; plural=0;\\n"\n\n'


def message(index: int, category: str = "preferences", drafted: bool = True) -> str:
    status = "#. review-status: agent-drafted\n" if drafted else ""
    return (f"#. reviewed-category: {category}\n{status}"
            f'msgid "Message {index}"\nmsgstr "Translation {index}"\n\n')


class CoverageRefreshTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.po = self.root / "target.po"
        self.source = self.root / "source.po"
        self.coverage = self.root / "coverage.json"
        self.text = HEADER + "".join(message(i) for i in range(150))
        self.po.write_text(self.text, encoding="utf-8")
        self.source.write_text(self.text.replace("Language: yue_HK", "Language: en"), encoding="utf-8")
        self.metadata = {
            "schema_version": 1,
            "locale": "yue_HK",
            "translated_messages": 147,
            "categories": {"preferences": 147},
            "agent_drafted_messages": 147,
            "review_status": "curated-in-repository",
            "reviewed_on": "2026-09-26",
            "tone": {"safe_friendly_copy": "自然廣東話"},
            "required_flows": ["preferences"],
            "extension": {"preserve": True},
        }
        self.write_metadata(self.metadata)

    def write_metadata(self, metadata: object) -> None:
        self.coverage.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def refresh(self, **kwargs: bool) -> bool:
        return refresh_coverage.refresh_coverage(self.po, self.source, self.coverage, **kwargs)

    def validate(self) -> list:
        return compile_translation.validate_catalog(self.po, self.source, self.coverage)

    def assert_rejected(self, pattern: str, **kwargs: bool) -> None:
        before = self.coverage.read_bytes()
        with self.assertRaisesRegex(CatalogError, pattern):
            self.refresh(**kwargs)
        self.assertEqual(before, self.coverage.read_bytes())
        self.assertEqual([], list(self.root.glob(".coverage-*")))

    def cli(self, script: str, *options: str) -> subprocess.CompletedProcess:
        return subprocess.run(
            [sys.executable, str(Path(__file__).parent / script),
             "--po", str(self.po), "--source", str(self.source),
             "--coverage", str(self.coverage), *options],
            capture_output=True, text=True, cwd=self.root, check=False,
        )

    def test_refreshes_all_counts_and_preserves_authored_metadata(self) -> None:
        with self.assertRaisesRegex(CatalogError, "translated_messages is 147, catalog has 150"):
            self.validate()
        self.assertTrue(self.refresh())
        expected = dict(self.metadata, translated_messages=150,
                        categories={"preferences": 150}, agent_drafted_messages=150)
        self.assertEqual(expected, json.loads(self.coverage.read_text(encoding="utf-8")))
        self.assertEqual(151, len(self.validate()))
        self.assertEqual(self.text, self.po.read_text(encoding="utf-8"))
        self.assertEqual([], list(self.root.rglob("*.mo")))

    def test_reproduces_7709_to_7712_build_failure(self) -> None:
        # Synthetic messages at the exact failing count, not the product catalog.
        text = HEADER + "".join(message(i, drafted=i < 6719) for i in range(7712))
        self.po.write_text(text, encoding="utf-8")
        self.source.write_text(text, encoding="utf-8")
        self.write_metadata(dict(self.metadata, translated_messages=7709,
                                 categories={"preferences": 7709}, agent_drafted_messages=6716))
        with self.assertRaisesRegex(CatalogError, "translated_messages is 7709, catalog has 7712"):
            self.validate()
        self.refresh()
        updated = json.loads(self.coverage.read_text(encoding="utf-8"))
        self.assertEqual(7712, updated["translated_messages"])
        self.assertEqual(6719, updated["agent_drafted_messages"])
        self.assertEqual({"preferences": 7712}, updated["categories"])
        self.assertEqual(7713, len(self.validate()))

    def test_repairs_category_only_drift(self) -> None:
        self.write_metadata(dict(self.metadata, translated_messages=150, agent_drafted_messages=150))
        with self.assertRaisesRegex(CatalogError, "category counts"):
            self.validate()
        self.refresh()
        self.validate()

    def test_repairs_draft_only_drift(self) -> None:
        self.write_metadata(dict(self.metadata, translated_messages=150, categories={"preferences": 150}))
        with self.assertRaisesRegex(CatalogError, "agent_drafted_messages"):
            self.validate()
        self.refresh()
        self.validate()

    def test_idempotent_refresh_preserves_bytes_and_timestamp(self) -> None:
        self.refresh()
        # A semantically current file must not be reformatted either.
        data = json.loads(self.coverage.read_text(encoding="utf-8"))
        self.coverage.write_text(json.dumps(data, indent=4), encoding="utf-8")
        before, timestamp = self.coverage.read_bytes(), self.coverage.stat().st_mtime_ns
        self.assertFalse(self.refresh())
        self.assertEqual(before, self.coverage.read_bytes())
        self.assertEqual(timestamp, self.coverage.stat().st_mtime_ns)

    def test_rejects_duplicate_keys(self) -> None:
        self.po.write_text(self.text + message(0), encoding="utf-8")
        self.assert_rejected("duplicate msgid")

    def test_rejects_fuzzy_translation(self) -> None:
        self.po.write_text(self.text.replace('msgid "Message 0"', '#, fuzzy\nmsgid "Message 0"'), encoding="utf-8")
        self.assert_rejected("fuzzy entries")

    def test_rejects_empty_translation(self) -> None:
        self.po.write_text(self.text.replace('msgstr "Translation 0"', 'msgstr ""'), encoding="utf-8")
        self.assert_rejected("empty translation")

    def test_rejects_placeholder_mismatch(self) -> None:
        self.po.write_text(self.text.replace('msgstr "Translation 0"', 'msgstr "Translation %s"'), encoding="utf-8")
        self.assert_rejected("placeholder mismatch")

    def test_rejects_missing_category(self) -> None:
        self.po.write_text(self.text.replace("#. reviewed-category: preferences\n", "", 1), encoding="utf-8")
        self.assert_rejected("exactly one reviewed-category")

    def test_rejects_unknown_review_status(self) -> None:
        self.po.write_text(self.text.replace("review-status: agent-drafted", "review-status: unknown", 1), encoding="utf-8")
        self.assert_rejected("unknown review-status")

    def test_rejects_invalid_header(self) -> None:
        self.po.write_text(self.text.replace("Language: yue_HK", "Language: en"), encoding="utf-8")
        self.assert_rejected("header must declare")

    def test_source_membership_is_strict_by_default(self) -> None:
        self.po.write_text(self.text + message(150), encoding="utf-8")
        self.assert_rejected("absent from English")
        self.assertTrue(self.refresh(require_source_membership=False))

    def test_source_pending_exemption_is_preserved(self) -> None:
        self.po.write_text(self.text + "#. source-pending: pending.cpp\n" + message(150), encoding="utf-8")
        self.assertTrue(self.refresh())
        self.validate()

    def test_complete_mode_rejects_missing_messages(self) -> None:
        self.source.write_text(self.text + message(150), encoding="utf-8")
        self.assert_rejected("no Cantonese entry", require_complete=True)

    def test_minimum_translation_guard_is_preserved(self) -> None:
        self.po.write_text(HEADER + message(0), encoding="utf-8")
        self.assert_rejected("at least 150")

    def test_counts_contexts_plurals_and_spacers_but_not_obsolete_entries(self) -> None:
        extra = ('#. reviewed-category: navigation\nmsgctxt "menu"\n'
                 'msgid "Message 0"\nmsgstr "Open"\n\n'
                 '#. reviewed-category: navigation\nmsgid "%d item"\n'
                 'msgid_plural "%d items"\nmsgstr[0] "%d items"\n\n'
                 '#. reviewed-category: navigation\nmsgid " "\nmsgstr " "\n\n'
                 '#~ msgid "Obsolete"\n#~ msgstr "Old"\n')
        self.po.write_text(self.text + extra, encoding="utf-8")
        self.source.write_text(self.text + extra, encoding="utf-8")
        self.refresh()
        data = json.loads(self.coverage.read_text(encoding="utf-8"))
        self.assertEqual(153, data["translated_messages"])
        self.assertEqual({"navigation": 3, "preferences": 150}, data["categories"])
        decoded = read_mo(compile_mo(self.validate()))
        self.assertEqual("Open", decoded["menu\x04Message 0"])
        self.assertEqual("%d items", decoded["%d item\x00%d items"])

    def test_rejects_non_object_metadata_in_both_commands(self) -> None:
        for invalid in ([], None, "text", 3):
            with self.subTest(invalid=invalid):
                self.write_metadata(invalid)
                self.assert_rejected("must contain a JSON object")
                result = self.cli("compile_translation.py", "--check")
                self.assertEqual(1, result.returncode)
                self.assertIn("must contain a JSON object", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_cli_reports_malformed_json_without_writing(self) -> None:
        self.coverage.write_bytes(b"{not-json")
        before = self.coverage.read_bytes()
        result = self.cli("refresh_coverage.py")
        self.assertEqual(1, result.returncode)
        self.assertIn("error:", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(before, self.coverage.read_bytes())

    def test_cli_reports_invalid_utf8_without_traceback(self) -> None:
        self.po.write_bytes(b"\xff")
        before = self.coverage.read_bytes()
        for script in ("refresh_coverage.py", "compile_translation.py"):
            with self.subTest(script=script):
                result = self.cli(script)
                self.assertEqual(1, result.returncode)
                self.assertIn("error:", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(before, self.coverage.read_bytes())

    def test_concurrent_metadata_edit_is_not_overwritten(self) -> None:
        original_validate = refresh_coverage.validate_catalog
        replacement = dict(self.metadata, reviewed_on="2026-10-03")

        def edit_then_validate(*args, **kwargs):
            pairs = original_validate(*args, **kwargs)
            self.write_metadata(replacement)
            return pairs

        with mock.patch.object(refresh_coverage, "validate_catalog", side_effect=edit_then_validate):
            with self.assertRaisesRegex(CatalogError, "changed during refresh"):
                self.refresh()
        self.assertEqual(replacement, json.loads(self.coverage.read_text(encoding="utf-8")))
        self.assertEqual([], list(self.root.glob(".coverage-*")))

    def test_concurrent_catalog_edit_prevents_update(self) -> None:
        original_validate = refresh_coverage.validate_catalog

        def edit_then_validate(*args, **kwargs):
            pairs = original_validate(*args, **kwargs)
            self.po.write_text(self.text + message(150), encoding="utf-8")
            return pairs

        with mock.patch.object(refresh_coverage, "validate_catalog", side_effect=edit_then_validate):
            self.assert_rejected("catalog changed during coverage refresh")

    def test_failed_replace_preserves_original_and_removes_staging(self) -> None:
        before = self.coverage.read_bytes()
        with mock.patch.object(Path, "replace", side_effect=OSError("simulated write failure")):
            with self.assertRaisesRegex(OSError, "simulated write failure"):
                self.refresh()
        self.assertEqual(before, self.coverage.read_bytes())
        self.assertEqual([], list(self.root.glob(".coverage-*")))

    @unittest.skipIf(os.name == "nt", "POSIX permissions are not available on Windows")
    def test_preserves_file_permissions(self) -> None:
        self.coverage.chmod(0o640)
        self.refresh()
        self.assertEqual(0o640, stat.S_IMODE(self.coverage.stat().st_mode))

    def test_build_check_stays_read_only_and_supplies_recovery_hint(self) -> None:
        before = self.coverage.read_bytes()
        output = self.root / "output.mo"
        result = self.cli("compile_translation.py", "--check", "--output", str(output))
        self.assertEqual(1, result.returncode)
        self.assertIn("refresh_coverage.py", result.stderr)
        self.assertEqual(before, self.coverage.read_bytes())
        self.assertFalse(output.exists())

    def test_refresh_cli_then_compile_check(self) -> None:
        result = self.cli("refresh_coverage.py")
        self.assertEqual(0, result.returncode, result.stderr)
        output = self.root / "output.mo"
        check = self.cli("compile_translation.py", "--check", "--output", str(output))
        self.assertEqual(0, check.returncode, check.stderr)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
