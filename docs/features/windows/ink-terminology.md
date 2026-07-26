# Ink terminology (filament → ink, AMS → Ink Dispenser)

This fork renames the user-facing terms in the UI:

| Legacy term | English UI | Cantonese UI |
| --- | --- | --- |
| Filament / filament(s) | Ink / ink(s) | 墨水 |
| AMS | Ink Dispenser | 墨水機 |
| Sync AMS (width-constrained sidebar button) | Sync dispenser | 同步墨水機 |

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
in the INK section header, so the compact "Sync dispenser" (同步墨水機) is used
for the `Sync AMS` msgid instead of the full "Sync Ink Dispenser". Longer
renamed labels worth watching at narrow widths (they reflow but were not
shortened): "Ink Dispenser Settings" (device status page) and
"Sync Ink Dispenser and nozzle information" (tooltip, unconstrained).

## Intentionally left

- **Device-page webview** (`src/slic3r/GUI/DeviceWeb/device_page`): its i18next
  locale JSONs still say Filament/耗材 because the shipped page is the
  pre-built `dist/` bundle with strings baked in; an edit-only pass cannot
  rebuild it. Rename the locales and rebuild `dist/` in a follow-up.
- **ui-md3 design-kit demo** (`ui-md3/app`): English strings double as i18n
  lookup keys there; renaming them is a key migration, out of scope for a
  catalog-level rename.
- **`AMS Materials Setting`** already displays as "Materials Setting" via an
  upstream copy-edit override, so no AMS remains visible in that title.
- Other display languages (de/fr/ja/…): upstream terminology retained.
- Internal/log-only strings, HMS cloud-served error texts, and any msgid text
  itself: unchanged by design.

## Verification

- `py -3 bbl/i18n/yue_HK/compile_translation.py --check` — green
  (620 reviewed translations, reproducible MO).
- `node resources/web/data/validate-text-locales.mjs` — green (168 web keys).
- `scripts/i18n/Test-LanguageModes.ps1` — green (13/13 ui-md3 i18n tests plus
  catalog, DeviceWeb, and legacy web checks).
- The English MO was regenerated deterministically (3748 entries) with the same
  writer layout the yue_HK compiler uses; probes confirm
  `Filament→Ink`, `Add filament→Add ink`, `Sync AMS→Sync dispenser`,
  `AMS Settings→Ink Dispenser Settings`, and zero residual
  "filament"/"AMS" words across all translated strings.
