"""Focused native motion extraction and English-source membership regression."""
from __future__ import annotations

import gettext
import io
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "bbl/i18n"))
sys.path.insert(0, str(ROOT / "scripts/i18n"))
from po_catalog import compile_mo, entry_map, parse_po, placeholder_signature
from compile_catalog import compile_override
from check_ink_overrides import uses_old_word
from update_catalogs import entry_block, find_gettext, merge_into_english, run_xgettext
sys.path.insert(0, str(ROOT / "bbl/i18n/yue_HK"))
from compile_translation import validate_catalog
from po_catalog import CatalogError

MOTION = (
    "Interface motion",
    "Reduce motion settles supported transitions immediately. System follows your operating system preference.",
    "Reduce motion",
)

INK_MESSAGES = (
    "add %d new filament(s)",
    "kept (at least one filament must remain); preset/colour still applied",
    "at least one filament must remain",
    "Add %d new filament(s)",
    "The preset files are removed from the user preset folder and cannot be recovered except from a config-profile snapshot. Deleting a custom printer also deletes the filament and process presets attached to it.",
    "Also deletes %1% filament and %2% process presets attached to this printer",
)


def load(path):
    return entry_map(parse_po(path), path, allow_duplicates=True)


def require_motion(catalog):
    for message in MOTION:
        if (None, message) not in catalog:
            raise AssertionError("Motion key is absent from English source catalog: " + message)


class NativeMotionCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.english = load(ROOT / "bbl/i18n/en/BambuStudio_en.po")
        cls.cantonese = load(ROOT / "bbl/i18n/yue_HK/BambuStudio_yue_HK.po")
        cls.directory = tempfile.TemporaryDirectory(prefix="native-motion-catalog-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.extracted_path = Path(cls.directory.name) / "native.pot"
        run_xgettext(find_gettext(None), cls.extracted_path, locations=True)
        cls.extracted = load(cls.extracted_path)

    def test_live_extraction_and_both_catalogs_include_motion(self):
        self.assertIn("src/slic3r/GUI/Preferences.cpp", (ROOT / "bbl/i18n/list.txt").read_text(encoding="utf-8").splitlines())
        for catalog in (self.extracted, self.english, self.cantonese):
            require_motion(catalog)
            self.assertIn((None, "System"), catalog)

    def test_existing_translations_compile_without_rewording(self):
        require_motion(self.english)
        pairs = [("", "Content-Type: text/plain; charset=UTF-8\nLanguage: yue_HK\n")]
        for message in MOTION:
            entry = self.cantonese[(None, message)]
            self.assertTrue(entry.msgstr.strip())
            self.assertFalse(entry.fuzzy)
            pairs.append((message, entry.msgstr))
            self.assertEqual(self.english[(None, message)].msgstr, "")
        translated = gettext.GNUTranslations(io.BytesIO(compile_mo(pairs)))
        for message in MOTION:
            self.assertEqual(translated.gettext(message), self.cantonese[(None, message)].msgstr)

    def test_missing_each_motion_source_entry_is_detected(self):
        require_motion(self.english)
        for message in MOTION:
            reduced = dict(self.english)
            del reduced[(None, message)]
            with self.subTest(message=message), self.assertRaisesRegex(AssertionError, "absent from English source catalog"):
                require_motion(reduced)

    def test_every_current_extracted_key_has_an_english_source_entry(self):
        missing = set(self.extracted) - set(self.english)
        self.assertFalse(missing, sorted(missing, key=str))

    def test_strict_cantonese_source_membership_and_completeness(self):
        pairs = validate_catalog(
            ROOT / "bbl/i18n/yue_HK/BambuStudio_yue_HK.po",
            ROOT / "bbl/i18n/en/BambuStudio_en.po",
            ROOT / "bbl/i18n/yue_HK/coverage.json",
            require_complete=True,
        )
        translated = gettext.GNUTranslations(io.BytesIO(compile_mo(pairs)))
        self.assertEqual(translated.gettext("Settings draft"), "設定草稿")
        self.assertEqual(translated.gettext("Connection interrupted. Retrying in %d seconds."),
                         "連線中斷。%d 秒後重試。")

    def test_real_validator_rejects_missing_motion_source(self):
        source = Path(self.directory.name) / "missing-motion-source.po"
        source.write_text("\n\n".join("\n".join(entry_block(entry))
                                    for key, entry in self.english.items()
                                    if key != (None, MOTION[0])) + "\n", encoding="utf-8")
        with self.assertRaisesRegex(CatalogError, "absent from English source catalog"):
            validate_catalog(
                ROOT / "bbl/i18n/yue_HK/BambuStudio_yue_HK.po", source,
                ROOT / "bbl/i18n/yue_HK/coverage.json", require_complete=True,
            )

    def test_six_display_overrides_compile_with_original_placeholders(self):
        translated = gettext.GNUTranslations(io.BytesIO(compile_override(ROOT / "bbl/i18n/en/BambuStudio_en.po")))
        for message in INK_MESSAGES:
            with self.subTest(message=message):
                shown = translated.gettext(message)
                self.assertEqual(shown, message.replace("filament", "ink"))
                self.assertFalse(uses_old_word(shown))
                self.assertEqual(placeholder_signature(message), placeholder_signature(shown))
                self.assertIn((None, message), self.extracted)
        # An empty override is omitted by the real compiler, so the original
        # wording returns. This is the regression the display check must catch.
        path = Path(self.directory.name) / "missing-override.po"
        path.write_text('msgid ""\nmsgstr "Content-Type: text/plain; charset=UTF-8\\n"\n\n' +
                        "\n".join(entry_block(self.extracted[(None, INK_MESSAGES[0])])) + "\n", encoding="utf-8")
        broken = gettext.GNUTranslations(io.BytesIO(compile_override(path)))
        self.assertTrue(uses_old_word(broken.gettext(INK_MESSAGES[0])))

    def test_supported_merge_is_scoped_preserving_and_idempotent(self):
        scoped = Path(self.directory.name) / "motion.pot"
        scoped.write_text("\n\n".join("\n".join(entry_block(self.extracted[(None, message)])) for message in MOTION) + "\n", encoding="utf-8")
        english = Path(self.directory.name) / "english.po"
        original = b'msgid "System"\nmsgstr "Preserved override"\n'
        english.write_bytes(original)
        added, retained = merge_into_english(scoped, english)
        self.assertEqual((added, retained), (3, 1))
        self.assertTrue(english.read_bytes().startswith(original))
        require_motion(load(english))
        self.assertEqual(load(english)[(None, "System")].msgstr, "Preserved override")
        first = english.read_bytes()
        self.assertEqual(merge_into_english(scoped, english), (0, 1))
        self.assertEqual(english.read_bytes(), first)


if __name__ == "__main__":
    unittest.main(verbosity=2)
