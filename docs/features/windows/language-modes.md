# English, Hong Kong Cantonese, and bilingual modes

## Behavior

The Windows fork defines three canonical UI-mode identifiers:

| Mode | Identifier | Presentation |
|---|---|---|
| English | `en` | English source copy |
| 廣東話（香港，預覽版） | `yue_HK` | Curated Hong Kong Cantonese where available, then English fallback |
| English + 廣東話（香港） | `bilingual_en_yue_HK` | English primary with compact, stacked, or progressively disclosed Cantonese |

These are the fork's baseline modes, not replacements for Bambu Studio's existing locales. In
particular, formal written `zh_TW` remains a separate locale and is never relabeled as Cantonese.
Hyphenated compatibility aliases are normalized to the canonical underscore identifiers before they
are persisted.

Native mode resolution is centralized in `src/slic3r/GUI/LanguageMode.*`. It keeps formatting,
catalog, embedded-web, remote-service, and font routes separate. The two custom modes use their local
Cantonese resources but route unsupported cloud and service language headers through documented
English fallbacks instead of leaking `yue` or `bilingual` identifiers to external services.

The native Cantonese catalog currently contains 239 curated messages covering navigation,
preferences, file actions, slicing and printing, connection recovery, destructive actions, and
security/error copy. The rest of the large native gettext surface falls back to English. Bilingual
native rendering is deliberately opt-in: migrated controls retain the English and Cantonese format
templates separately, format each variant once, and only then choose compact, stacked, or progressive
presentation. Legacy controls remain safe and English-first until they are migrated.

Embedded surfaces have dedicated resources:

- DeviceWeb has key parity for all 178 English entries and builds a bilingual English-first variant.
- The legacy local web bundle validates Cantonese and bilingual behavior for all 168 English keys.
- The Pages/browser MD3 prototype persists exactly the three modes, supports a non-persisting `lang`
  query override, and progressively discloses longer Cantonese copy to protect narrow layouts.

For Traditional CJK modes, native Windows labels prefer Microsoft JhengHei UI because the bundled
Roboto files do not contain Cantonese glyphs. ImGui receives the Traditional Chinese glyph range.
English and other Latin-script modes continue to use the privately registered Roboto resources.

## Configuration and installer hand-off

The application persists its selected mode in the normal `language` application setting. The first
three entries in Preferences are English, Cantonese preview, and bilingual; every pre-existing locale
that has an installed catalog remains listed after them. A mode change uses the existing restart/
recreate flow and preserves the normal modified-preset confirmation.

The NSIS installer exposes the same three choices and accepts the silent-install option
`/LANGMODE=en`, `/LANGMODE=yue_HK`, or `/LANGMODE=bilingual_en_yue_HK`. It writes the selection to
`HKCU\Software\codingmachineedge\BambuStudioMD3Preferences\LanguageMode`. On first launch, the app
imports that value only when its own language setting is empty; an existing application preference
always wins. The preference registry key intentionally survives uninstall so a reinstall keeps the
user's choice.

Installer errors use English, Cantonese, or English followed by Cantonese according to the selected
mode. Silent installs default to English unless a valid persisted or command-line mode is present;
unknown command-line values fail before an install directory is created.

## Fallback and safety rules

- A missing native Cantonese catalog or untranslated message falls back to its English source.
- An unknown Pages/browser mode falls back to English without overwriting a saved valid preference.
- DeviceWeb and legacy local web resources require exact English/Cantonese key parity and matching
  interpolation placeholders.
- Bilingual format placeholders are expanded in each language before the two presentations are
  combined, preventing duplicated or malformed runtime arguments.
- Friendly copy may be playful and compact. Destructive actions, privacy, certificates, unsigned
  software, fatal errors, and recovery instructions use restrained, literal Cantonese.

## Verification status

`scripts/i18n/Test-LanguageModes.ps1` checks canonical IDs, catalog presence and reproducibility,
native coverage metadata, DeviceWeb and legacy-web key parity, placeholders, and the Pages language
tests. `language_mode_tests` covers native normalization, route separation, and
format-before-presentation behavior. The Windows workflow is configured to build and run that test
target.

The resources and test gates exist in the candidate code, but this documentation does not claim that
the pending candidate Windows workflow or release has passed. The 239-message native catalog is a
preview, not complete native localization, and still needs broader independent human review—most
importantly for safety-critical print, account, networking, and destructive flows.
