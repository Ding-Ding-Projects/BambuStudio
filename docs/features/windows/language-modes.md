# English, Hong Kong Cantonese, and bilingual modes

## Behavior

The Windows fork defines three canonical UI-mode identifiers:

| Mode | Identifier | Presentation |
|---|---|---|
| English | `en` | The fork's English wording (ink and Ink Dispenser) |
| 廣東話（香港，預覽版） | `yue_HK` | Hong Kong Cantonese for every extracted message, English wording as the fallback |
| English + 廣東話（香港，預覽版） | `bilingual_en_yue_HK` | English first, with the Cantonese inline, stacked, or in the tooltip on every surface described below |

These are the fork's baseline modes, not replacements for Bambu Studio's existing locales. In
particular, formal written `zh_TW` remains a separate locale and is never relabeled as Cantonese.
Hyphenated compatibility aliases are normalized to the canonical underscore identifiers before they
are persisted.

Native mode resolution is centralized in `src/slic3r/GUI/LanguageMode.*`. It keeps formatting,
catalog, embedded-web, remote-service, and font routes separate. The two custom modes use their local
Cantonese resources but send English to cloud services and service language headers instead of
leaking `yue` or `bilingual` identifiers to external services.

## The Cantonese catalog

`bbl/i18n/yue_HK/BambuStudio_yue_HK.po` has an entry for every message in the current English
extraction (7,496 messages) and in the English override catalog. It holds 7,658 entries: 993 curated
and 6,665 carrying `#. review-status: agent-drafted` (a few cover messages the current source no
longer uses). `scripts/i18n/Test-LanguageModes.ps1` fails when a message has no Cantonese entry, so
a new string needs its translation in the same change. Every drafted entry has passed the
mechanical checks in `scripts/i18n/merge_cantonese_drafts.py`:

- placeholders match the English, and unnumbered placeholders keep the English order;
- line breaks and menu mnemonics are kept;
- no Simplified-only characters, and none of the disallowed terms in `glossary.json` (for example
  列印, 印表機, 專案, 軟體, 線材, 耗材, 您, 引數, 裡);
- written Hong Kong Cantonese register in sentences (嘅, 係, 冇, 咗 and so on);
- no "to be translated" stubs, and a trailing colon, ellipsis or question mark survives;
- alignment: where the zh_TW catalog has a reference for the same message, the draft shares a real
  part of its content characters with it, so a translation attached to the wrong message is rejected.

Drafted entries have not had a human review. `merge_cantonese_drafts.py --audit` re-runs every check
over the drafted entries in the catalog and exits non-zero on any failure; `--purge` removes failing
entries so they return to the translation queue. The alignment check exists because 826 early drafts
passed every other check while being attached to the wrong message; they were removed and translated
again.

In Cantonese mode a message without a Cantonese entry falls back to the English override catalog, so
the fallback text uses the fork's English wording rather than the raw source string.

## Bilingual mode in the native interface

Every legacy lookup (`_L`, `_u8L`, `_CTX` and the plural forms, all of which end in
`I18N::finish()`) shows English and records the Cantonese for that exact displayed English in
`BilingualRegistry`. Strings with format placeholders are recorded as templates, so a label built
with `format()` or `wxString::Format()` finds its Cantonese with the same values filled in.

`BilingualDecorator` runs only in bilingual mode. It watches windows as they are shown and gives
labels, buttons, check boxes, radio buttons, group boxes, section headers, list and table column
titles, tooltips and native menu items their second language:

- Single-line text reads "English · 廣東話" when that fits the space its layout can give it. A dialog
  may grow within the screen for this; if its content still does not fit, its labels stay English.
  On a page that scrolls (every Preferences page, the Prepare sidebar) only the room a label already
  has counts: a page scrolls instead of growing with its dialog, so a wider label would push the
  control at the end of its row out of sight.
- Text that does not fit stays English, and its tooltip adds "廣東話：…".
- Wrapped and multi-line text shows the Cantonese below the English. A label that the program wraps
  to a width, such as every Preferences row title and description, reads "English · 廣東話" when the
  pair fits that width on one line; otherwise the Cantonese goes below the English, wrapped to the
  same width.
- Once the layout settles, the decorator checks its own work: a pair that is cut short, reaches past
  what its parents show, or makes a scrolling page need more width than it shows goes back to English
  with the Cantonese in its tooltip, or below the English for a label the program wraps.
- Tooltips show the Cantonese below the English. The decorator remembers the application's own text,
  so decorations never accumulate.
- List and table column titles read "English · 廣東話" when the pair fits the column. A column
  header has no tooltip, so a title that does not fit stays English.
- Placeholder hints in text and search fields read "English · 廣東話" when the pair fits the field;
  otherwise they stay English.
- Typed text, list values and combo box values are never changed. Windows that already render both
  languages themselves (`apply_localized_text()`) are left alone.

The settings labels, which `OG_CustomCtrl` paints itself, use the same rule at paint time through
`I18N::fit_bilingual()`; the label column keeps its English width, and the label tooltip carries the
Cantonese description. Material menus show the Cantonese as the secondary line of each item.

Self-drawn widgets (tab strips, step indicators, switches and similar controls) and the 3D canvas
(ImGui text, gizmos and notifications) use the same rule at paint time. A notification's link, such as
"Restart to install update" or "Retry", reads "English · 廣東話" when the pair fits a line of the
notification, and stays English otherwise; the notification's text itself shows the Cantonese below.

Known limits: a label that the program rewrites while it is on screen shows English again until the
next pass, which runs about every three seconds. The camera view's small LIVE badge stays English.

## Documentation and changelog

Every article under `docs/features` has a Hong Kong Cantonese translation beside it, named
`<article>.yue_HK.md`. Its front matter records `review-status: agent-drafted` and the
`source-sha256` of the English article it translates (the SHA-256 of the English text with CRLF
line endings normalised to LF), and its first line links back to the English article. In Cantonese
mode the command palette opens the translation; in English and bilingual modes it opens the English
article.

The in-app changelog reads `resources/changelog/changelog.yue_HK.json` beside `changelog.json`: a map
from each entry's commit SHA to its Cantonese text. English mode shows the English entry, Cantonese
mode the Cantonese (English where an entry has none), and bilingual mode both, the Cantonese below.
Search matches either language, and Copy and Export carry the text the viewer shows. A missing or
broken translation file leaves the changelog English rather than empty.

`scripts/i18n/check_translated_content.py` fails when an article has no translation, when an English
article changed after its translation was made (its hash no longer matches), or when a changelog
entry has no Cantonese text. `scripts/i18n/Test-LanguageModes.ps1` runs it; run that script before
pushing a change to the docs or the changelog.

## Embedded web surfaces

- DeviceWeb has key parity for all 204 English entries and builds a bilingual English-first variant.
- The legacy local web bundle (`resources/web/data/text.js`) has key parity for all 281 English keys.
  Every page element marked `.trans` must name a key the English table has; image alternative text
  (`data-alt-tid`) and input placeholders (`data-ph-tid`) follow the language mode too. In bilingual
  mode a placeholder shows both languages when they fit the field, and otherwise English with both
  languages in the field's tooltip.
- Local pages receive the local web language, including `yue_HK` and the bilingual mode: the setup
  wizard, ink creation and editing, the project tab, the plug-in download page, and the printer
  connection page. Remote pages (MakerWorld, sign-in) receive the service language instead.
- The `fila_manager` page is not translated: its panel is never constructed, and the ink manager the
  application shows lives in DeviceWeb.
- The Pages/browser MD3 prototype persists exactly the three modes, supports a non-persisting `lang`
  query override, and progressively discloses longer Cantonese copy to protect narrow layouts.

### Home webview bilingual layout

The Home webview (`resources/web/homepage3` plus the shared `resources/web/data/text.js` catalog)
renders bilingual mode with two rules that protect both sentence integrity and layout:

- **Whole-string annotation only.** `GetLocalizedTextByKey` annotates a `.trans` element once, at the
  element level. Sentences that the legacy markup composes from several `.trans` fragments (for
  example the network-plugin banner: "Please " + "install" + " the network plugin before logging in")
  are wrapped in a `.bi-group` container: in bilingual mode the fragments render English-only and
  `AnnotateBilingualGroups` appends a single composed Cantonese line for the whole sentence. If any
  fragment of the composed whole lacks a real Cantonese translation, or the Cantonese equals the
  English, the surface shows English only instead of interleaving fragment-wise Cantonese.
- **Compact, non-overlapping secondary line.** The Cantonese annotation is a single smaller block
  span (`0.85em`, ellipsized when its container is single-line, full text preserved in a `title`
  tooltip). Sidebar rows (`.BtnItem`) and the login area use `min-height` with a flex-column
  `#LeftBoard`, so two-line items and the network-plugin banner grow downward and push the menu
  instead of overlaying it. Inside the error banner the secondary line keeps full
  `--md-on-error-container` contrast and may wrap.

Attribute and `innerText` contexts (icon `title` tooltips, the model search placeholder, confirm
dialogs) use `GetCurrentPlainTextByKey`, which yields `English ／ 粵語：Cantonese` plain text so no
markup leaks into tooltips or dialogs. Pure English and pure Cantonese modes are byte-identical to
the catalog strings.

For Traditional CJK modes, native Windows labels prefer Microsoft JhengHei UI because the bundled
Roboto files do not contain Cantonese glyphs. ImGui receives the Traditional Chinese glyph range.
English and other Latin-script modes continue to use the privately registered Roboto resources.

## Configuration and installer hand-off

The application persists its selected mode in the normal `language` application setting. The first
three entries in Preferences are English, Cantonese preview, and bilingual; every pre-existing locale
that has an installed catalog remains listed after them. A mode change uses the existing restart/
recreate flow and preserves the normal modified-preset confirmation.

The supported installer is the unsigned Squirrel.Windows `Setup.exe` package. It does not expose a
language command-line switch or write a separate installer registry preference: the first launch
uses the application's persisted `language` setting, while a new profile starts in English and can
choose Cantonese or bilingual mode from Preferences. Squirrel updates keep the application-data
setting in place, so an update does not reset the user's language choice.

Installer and update failures are reported in the app's current language mode after launch. The
installer itself remains language-neutral and never pretends that a command-line language value was
accepted when the Squirrel package has no such contract.

## Fallback and safety rules

- A missing native Cantonese catalog or untranslated message falls back to the English wording.
- An empty message stays empty in every mode. A catalog answers the empty message with its header
  ("Project-Id-Version: ..."), so every direct catalog lookup skips it; before this, Cantonese mode
  gave every spin field without a unit that header as its label.
- Both fork catalogs are compiled at build time: the CMake `fork_catalogs` target (which needs
  Python) turns the English override PO and the Cantonese PO into `BambuStudio.mo` files, and no MO
  is tracked in Git. The Windows workflow compiles the installed Cantonese catalog again with
  `--check` before packaging, so a stale catalog cannot ship silently.
- An unknown Pages/browser mode falls back to English without overwriting a saved valid preference.
- DeviceWeb and legacy local web resources require exact English/Cantonese key parity and matching
  interpolation placeholders.
- Bilingual format placeholders are expanded in each language before the two presentations are
  combined, preventing duplicated or malformed runtime arguments.
- Friendly copy may be playful and compact. Destructive actions, privacy, certificates, unsigned
  software, fatal errors, and recovery instructions use restrained, literal Cantonese.

## Verification status

`scripts/i18n/Test-LanguageModes.ps1` compiles both catalogs strictly, runs the drafted-entry audit
and the docs and changelog check, and checks canonical IDs, DeviceWeb and legacy-web key parity,
page keys, placeholders, and the ui-md3 language tests. `scripts/ci/Test-InkTerminology.ps1` checks
the ink vocabulary in every catalog. These scripts run locally before a push: the Windows workflow
builds and publishes releases and runs no tests or checks. `language_mode_tests` covers native
normalization, route separation, format-before-presentation behavior and the bilingual registry; the
workflow configures with `SLIC3R_BUILD_TESTS=OFF`, so it is not built or run there.

Released builds carrying the bilingual decorator (`md3-v148` to `md3-v153`) have been run unmodified
on a hidden desktop in all three modes; the captures and their provenance are listed in
`docs/screenshots/md3-everything/README.md`, and each defect they showed is a row of the clipping
inventory. On `md3-v153`, the layout probe's `language-audit` found no English-only label with a
Cantonese translation on the Prepare page or on Preferences > General in bilingual mode: every such
label showed both languages or, where the pair did not fit, carried the Cantonese in its tooltip
(`docs/screenshots/md3-everything/probe/language-audit--*--md3-v153.json`). Other surfaces have not
been audited that way yet. The drafted Cantonese still needs independent human review, most
importantly for safety-critical print, account, networking, and destructive flows.
