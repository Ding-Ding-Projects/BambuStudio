"""Focused native workflow catalog and extraction checks, without launching the UI."""
from __future__ import annotations

import gettext
import hashlib
import io
import re
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "bbl/i18n"))
sys.path.insert(0, str(ROOT / "scripts/i18n"))
from po_catalog import compile_mo, entry_map, parse_po, placeholder_signature
from update_catalogs import append_hints, find_gettext, read_keywords, run_xgettext


# Independent delivery inventory. Extraction alone cannot detect a missing file.
REQUIRED = (
    "No plate selected",
    "Time estimate unavailable until slicing completes",
    "Material estimate unavailable until slicing completes",
    "Material",
    "Printer preset",
    "Wait for slicing to finish before sending output.",
    "Add a model in Prepare or open a sliced file.",
    "The selected output action is not available for the current plate.",
    "Print preparation",
    "Review before printing",
    "Review the selected plate and choose an output action. Opening this page does not slice or send a job.",
    "Plate readiness",
    "Slice and send",
    "Destination and mapping",
    "Printer destination and material mapping are reviewed in the selected action's existing confirmation flow.",
    "Output options",
    "Continue deliberately",
    "The selected action keeps its existing printer selection, validation, and confirmation steps.",
    "Slicing and combined output actions use the same checks and confirmation flow as Prepare.",
    "Monitor",
    "Print summary",
    "Estimates describe the selected plate. The output action determines whether one or all plates are included.",
    "No printable content",
    "Slicing in progress",
    "Slice required",
    "Ready for the selected output action",
    "Output unavailable",
    "Return to Prepare to review the plate, material settings, or slicing progress.",
    "Workspace navigation",
    "More workspaces",
)


def load(relative):
    file = ROOT / relative
    return entry_map(parse_po(file), file, allow_duplicates=True)


def require_inventory(catalog):
    missing = [key for key in REQUIRED if (None, key) not in catalog]
    if missing:
        raise AssertionError("Missing workflow messages: " + ", ".join(missing))


def require_translation(source, entry):
    if not entry.msgstr.strip() or entry.msgstr == source or entry.fuzzy:
        raise AssertionError("Missing reviewed draft translation: " + source)
    if placeholder_signature(source) != placeholder_signature(entry.msgstr):
        raise AssertionError("Placeholder mismatch: " + source)
    if source.count("\n") != entry.msgstr.count("\n"):
        raise AssertionError("Line-break mismatch: " + source)


class WorkflowLocalizationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.english = load("bbl/i18n/en/BambuStudio_en.po")
        cls.cantonese = load("bbl/i18n/yue_HK/BambuStudio_yue_HK.po")
        cls.pot = load("bbl/i18n/BambuStudio.pot")
        cls.gettext_bin = find_gettext(None)

    def test_registered_sources_are_actually_extracted(self):
        registration = (ROOT / "bbl/i18n/list.txt").read_text(encoding="utf-8").splitlines()
        for file in ("MainFrame.cpp", "WorkflowPrintPanel.cpp", "Notebook.cpp"):
            self.assertIn("src/slic3r/GUI/" + file, registration)
        self.assertEqual(len(REQUIRED), 30)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "workflow.pot"
            run_xgettext(self.gettext_bin, output, locations=False)
            require_inventory(entry_map(parse_po(output), output, allow_duplicates=True))
            append_hints(output)
            # The extractor's wall-clock header does not change message content.
            normalize = lambda text: re.sub(r'^"POT-Creation-Date:.*$', '', text, flags=re.M)
            self.assertEqual(normalize(output.read_text(encoding="utf-8")),
                             normalize((ROOT / "bbl/i18n/BambuStudio.pot").read_text(encoding="utf-8")))
        require_inventory(self.pot)

    def test_catalogs_compile_and_translate_every_workflow_key(self):
        require_inventory(self.english)
        require_inventory(self.cantonese)
        pairs = [("", "Content-Type: text/plain; charset=UTF-8\nLanguage: yue_HK\n")]
        for source in REQUIRED:
            entry = self.cantonese[(None, source)]
            require_translation(source, entry)
            self.assertEqual(len(entry.categories), 1, source)
            self.assertIn(entry.categories[0], ("navigation", "slicing-printing"), source)
            self.assertEqual(entry.review_status, "agent-drafted", source)
            self.assertFalse(entry.source_pending, source)
            self.assertEqual(self.english[(None, source)].msgstr, "", source)
            pairs.append((source, entry.msgstr))
        compiled = gettext.GNUTranslations(io.BytesIO(compile_mo(pairs)))
        for source in REQUIRED:
            self.assertEqual(compiled.gettext(source), self.cantonese[(None, source)].msgstr)

    def test_removing_each_required_catalog_record_is_detected(self):
        for catalog in (self.english, self.cantonese, self.pot):
            for source in REQUIRED:
                with self.subTest(source=source):
                    incomplete = dict(catalog)
                    del incomplete[(None, source)]
                    with self.assertRaisesRegex(AssertionError, "Missing workflow messages"):
                        require_inventory(incomplete)

    def test_missing_source_registration_fails_real_extraction(self):
        lines = (ROOT / "bbl/i18n/list.txt").read_text(encoding="utf-8").splitlines()
        broken = [line for line in lines if line != "src/slic3r/GUI/WorkflowPrintPanel.cpp"]
        self.assertEqual(len(broken), len(lines) - 1)
        with tempfile.TemporaryDirectory() as directory:
            registration = Path(directory) / "list.txt"
            registration.write_text("\n".join(broken) + "\n", encoding="utf-8")
            output = Path(directory) / "missing-panel.pot"
            subprocess.run([str(self.gettext_bin / "xgettext"), "--no-wrap", "--no-location",
                            "--from-code=UTF-8", "--boost",
                            *["--keyword=" + key for key in read_keywords()],
                            "-f", str(registration), "-o", str(output)],
                           cwd=ROOT, check=True, capture_output=True)
            with self.assertRaisesRegex(AssertionError, "Print preparation"):
                require_inventory(entry_map(parse_po(output), output, allow_duplicates=True))

    def test_invalid_translation_and_placeholder_are_detected(self):
        from dataclasses import replace
        source = REQUIRED[0]
        entry = self.cantonese[(None, source)]
        for text in ("", source, entry.msgstr + " %s"):
            with self.subTest(text=text), self.assertRaises(AssertionError):
                require_translation(source, replace(entry, msgstr=text))

    def test_feature_article_has_current_cantonese_pair_and_category_links(self):
        folder = ROOT / "docs/features/workspace"
        source = (folder / "print-preparation.md").read_bytes().replace(b"\r\n", b"\n")
        translated = (folder / "print-preparation.yue_HK.md").read_text(encoding="utf-8")
        self.assertIn("source-sha256: " + hashlib.sha256(source).hexdigest(), translated)
        self.assertIn("translation-of: print-preparation.md", translated)
        self.assertIn("print-preparation.md", (folder / "README.md").read_text(encoding="utf-8"))
        self.assertIn("print-preparation.yue_HK.md", (folder / "README.yue_HK.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
