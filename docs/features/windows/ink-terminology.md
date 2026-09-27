# Ink terminology (filament → ink, AMS → Ink Dispenser)

This fork renames the user-facing terms in the UI:

| Legacy term | English UI | Cantonese UI |
| --- | --- | --- |
| Filament / filament(s) | Ink / ink(s) | 墨水 |
| AMS | Ink Dispenser | 墨水機 |
| Sync AMS (width-constrained sidebar button) | Sync | 同步 |

## Mechanism

The rename is implemented as **translation-catalog overrides only**. No source
`_L()` msgids, config keys, preset type names, file formats, CLI flags, or log
strings were changed, so slicing behavior, profile compatibility, 3MF/G-code
output, and upstream merges are unaffected.

- **English**: Bambu Studio ships an English override catalog that
  `wxTranslations` loads at runtime (`GUI_App::load_language` →
  `AddCatalog(SLIC3R_APP_KEY)`); because the catalog language ("en") differs
  from the msgid language ("en_US"), wxWidgets loads
  `resources/i18n/en/BambuStudio.mo`. The ink terminology lives in
  `bbl/i18n/en/BambuStudio_en.po` as `msgstr` overrides (msgids untouched) and
  is compiled into that MO.
- **Cantonese / bilingual**: `bbl/i18n/yue_HK/BambuStudio_yue_HK.po` msgstrs
  now use 墨水 / 墨水機; compiled deterministically by
  `bbl/i18n/yue_HK/compile_translation.py` into
  `resources/i18n/yue_HK/BambuStudio.mo` (validation gate: placeholders,
  reviewed categories, coverage.json, `--check` reproducibility).
- **Web surfaces**: `resources/web/data/text.js` (setup wizard filament
  selection, home-page "Ink Guide", user-preset filters) updated in the `en`
  and `yue_HK` sections; other languages retain their existing terms.

## Coverage

- Every `msgid` in the English catalog containing the standalone word
  "filament(s)" (549 entries) or "AMS" (97+ entries) now carries an ink /
  Ink Dispenser override, including strings the stale upstream PO was missing
  (e.g. the sidebar "Add filament" button, ConfigWizard filament pages,
  "Feed Filament", AMS/chamber temperature warnings) which were appended to the
  PO from a source scan of `src/slic3r` and `src/libslic3r`.
- Word-boundary replacement keeps technical literals intact: config keys such
  as `filament_start_gcode`/`nozzle_temperature` mentioned inside tooltips,
  URLs, and format placeholders (`%s`, `%1%`, `{}`) are untouched, and
  "a filament" became "an ink" where grammar required it.
- Destructive/error messages keep their exact meaning; only the two terms are
  substituted.

## Width-constrained labels

`Sidebar::priv::adjust_filament_title_layout()` squeezes the trailing buttons
in the INK section header, so the bare "Sync" (同步) is used for the `Sync AMS`
msgid instead of the full "Sync Ink Dispenser"; the tooltip
(`Sync AMS and nozzle information` → "Sync Ink Dispenser and nozzle
information") carries the full name. The first attempt at this shortening,
"Sync dispenser", still overran the panel edge and was cut back in `a4498bc72`.
Longer renamed labels worth watching at narrow widths (they reflow but were not
shortened): "Ink Dispenser Settings" (device status page) and
"Sync Ink Dispenser and nozzle information" (tooltip, unconstrained).

## Intentionally left

- **`AMS Materials Setting`** already displays as "Materials Setting" via an
  upstream copy-edit override, so no AMS remains visible in that title.
- Other display languages (de/fr/ja/…): upstream terminology retained.
- Internal/log-only strings, HMS cloud-served error texts, and any msgid text
  itself: unchanged by design.

Two surfaces were listed here as holdouts by the original rename and have since
been renamed; they are no longer exceptions:

- **Device-page webview** (`src/slic3r/GUI/DeviceWeb/device_page`): the `en` and
  `yue_HK` i18next catalogues now carry the ink values ("Ink Manager",
  "Ink Type", "Search Ink"). Its runtime bundle is generated, not committed —
  the CMake `device_page_build` target rebuilds `resources/web/device_page/dist/`
  from these locales — so a normal build ships the renamed page.
- **ui-md3 design-kit demo** (`ui-md3/app`): renamed with its lookup keys in one
  pass. Those keys are the rendered English string, so display text and key had
  to move together or every Cantonese lookup would silently miss.

## History and prose documentation

The rename is display-only in time as well as in scope. Prose that records
**what shipped and when** — the `## Landed` waves in `ROADMAP.md`, the commit
tables in `HANDOFF.md`, the parity-register rows — keeps the wording of its own
date, so a 2026-07-24 entry still reads "AI filament scanner" for a menu item
that reads "AI ink scanner" today. Rewriting a dated record to match today's
labels makes it a worse record without making anything easier to find; the same
reasoning is why `scripts/ci/Test-InkTerminology.ps1` skips obsolete `#~` PO
entries, which are merge history and are never loaded.

Prose that describes the **current** product — the README's feature and
screenshot sections, `ROADMAP.md`'s `## Remaining`, feature documentation — uses
the ink wording, because a reader is meant to find those words on screen.
Identifiers quoted in prose (`FilamentPicker`, `filamentRows`, `?view=filament`,
`filament_start_gcode`) keep the upstream spelling wherever they appear, current
or historical, for exactly the reason the rest of this document gives.

A screenshot is a dated record too. A capture taken before the rename is not
corrected by editing its caption: either retake it from a current build, or say
in the surrounding prose that it predates the rename. A caption that claims
"Ink" over an image that plainly reads "Filament" is worse than either.

## Verification

- `py -3 bbl/i18n/yue_HK/compile_translation.py --check` — green
  (620 reviewed translations, reproducible MO).
- `node resources/web/data/validate-text-locales.mjs` — green (168 web keys).
- `scripts/i18n/Test-LanguageModes.ps1` — green (13/13 ui-md3 i18n tests plus
  catalog, DeviceWeb, and legacy web checks).
- The English MO was regenerated deterministically (3748 entries) with the same
  writer layout the yue_HK compiler uses; probes confirm
  `Filament→Ink`, `Add filament→Add ink`, `Sync AMS→Sync`,
  `AMS Settings→Ink Dispenser Settings`, and zero residual
  "filament"/"AMS" words across all translated strings.
