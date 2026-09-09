# Funny levels and dialog emojis

## Behavior

Two independent **funny level** sliders and one **Show emojis in dialogs and message boxes**
switch live in **Preferences ▸ General**, directly under the Language picker.

| Control | Range / values | Default | AppConfig key |
|---|---|---|---|
| Funny level (English) | 1 (fully serious) … 5 (maximum playfulness) | 2 | `funny_level_en` |
| Funny level (Cantonese) | 1 … 5 | 2 | `funny_level_yue` |
| Show emojis in dialogs and message boxes | on / off | off | `dialog_emojis` |
| First-run disclosure shown | internal flag | unset | `funny_level_disclosed` |

The level changes the **voice** of a message, never its **facts**. Every variant keeps the same
printf placeholders as the source string, so file names, counts, and the action a button performs
are identical at level 5 and at level 1. The two ladders are independent: English can stay
dead straight while Cantonese is at full volume, exactly as on the documentation site
(`ui-md3/site/copy.js`), whose ladder semantics this feature mirrors:

- 5 variants map one-to-one onto levels 1–5.
- 3 variants: levels 1–2 use the first, level 3 the second, levels 4–5 the third.
- 2 variants: levels 1–2 use the first, levels 3–5 the second.
- 1 variant: the same text at every level (control labels, proper nouns).

### Copy resolution

`Slic3r::GUI::I18N::LanguageModeService::translate()` (see `src/slic3r/GUI/LanguageMode.cpp`)
consults the copy table before the Cantonese catalog:

1. The English primary is the English ladder entry for the active English level when the
   source string has a ladder; otherwise the unchanged source string.
2. The Cantonese text is the Cantonese ladder entry for the active Cantonese level when one
   exists; otherwise the curated `yue_HK` catalog entry; otherwise English.
3. Bilingual mode combines the two exactly as before (English primary, Cantonese secondary).

Strings currently carrying a ladder: `Slicing complete`, `Export successfully.`,
`Model file downloaded.`, `Setting saved: %s`, `Deleted: %s`, `Error`, `Warning`,
`Unsaved Changes`, `Do you want to continue?`, plus the feature's own settings labels as
single-entry (level-invariant) ladders so they render bilingually without a catalog entry.
Anything not in the table renders its base string at every level.

### Dialog emojis

When the switch is on, `MsgDialog` (and therefore `MessageDialog`, `ErrorDialog`,
`WarningDialog`, `InfoDialog`, and their subclasses in `src/slic3r/GUI/MsgDialog.cpp`) prefixes
the **headline** with one non-semantic emoji chosen from the dialog's style flags:

| Style | Emoji |
|---|---|
| `wxICON_ERROR` | ❌ |
| `wxICON_WARNING` | ⚠️ |
| `wxICON_QUESTION` | ❓ |
| `wxAPPLY` (success) | ✅ |
| anything else | ℹ️ |

The status meaning stays on the Material header glyph and the copy; the emoji is decoration. It
never appears in buttons, action labels, field labels, or accessible names: `add_button()` and
`SetButtonLabel()` pass every label through `strip_dialog_emoji()`, so even a caller that passes a
decorated string gets a plain button. Decorating an already-decorated headline is a no-op, so
repeated re-titling never stacks glyphs.

### First-run disclosure

The first time the application starts with either level above 1 (the compiled default of 2
qualifies), `GUI_App::show_funny_level_disclosure_once()` pushes one non-blocking notification
(`NotificationType::FunnyLevelDisclosure`, regular level, auto-fading) saying that the funny level
styles every message including errors and warnings, that facts never change, and that the
setting lives in Preferences ▸ General. It then records `funny_level_disclosed = true`, so the
notice fires once per configuration directory. It never gates startup or steals focus.

## Configuration

- Both sliders are Material Design 3 `Slider` widgets (`src/slic3r/GUI/Widgets/Slider.hpp`):
  keyboard focusable, arrow / Page / Home / End operable, and exposed to assistive technology
  through `SliderAccessible`, which reads the row title set with `SetName()`.
- Each row shows `Level N of 5`, a **provenance line** (`Stored in BambuStudio.conf as N.` or
  `Not stored yet; using the compiled default 2.`), and a **What does this change?** Text button
  that reveals the full explanation on demand (progressive disclosure). The caption states plainly
  that the level styles all messages, including errors, warnings and destructive confirmations.
- The keys are deliberately **not** seeded by `AppConfig::set_defaults()`. A missing key means
  "compiled default in effect", which is what makes the provenance line truthful; the key is
  written only when the user moves the slider or flips the switch.
- Changes apply live: the slider writes AppConfig, saves, and updates the in-process
  `LanguageModeService`, so the next `translate_mode()` call and the next dialog use the new
  value without a restart. On startup `GUI_App` loads all three values right after the language
  mode is configured.
- Labels honor the language modes: English, Cantonese, and bilingual (compact
  `English · 廣東話`) presentation via `render_localized_text_compact()`.

## Failure modes

- Malformed or out-of-range persisted values (`"loud"`, `"12"`, `"-2"`) never reach the UI:
  `parse_funny_level()` falls back to the compiled default for non-numbers and clamps numbers to
  1–5. `set_funny_level()` clamps again, so no code path can hold an invalid level.
- A source string without a ladder is unaffected by the level. There is no partial state: either
  the whole ladder exists for that language or the base copy is used.
- If the Cantonese catalog is missing, Cantonese ladder entries still render (they do not depend
  on the catalog); everything else falls back to English exactly as documented in
  [language modes](language-modes.md).
- If the notification manager is not yet available at `post_init()` the disclosure is skipped and
  the flag is left unset, so it is retried at the next start rather than silently lost.
- The dialog emoji is skipped for empty headlines and when the switch is off; a dialog never shows
  a bare glyph with no text.

## Security considerations

- No network, file, or clipboard access is involved. The copy table is compiled in; no external
  text is interpolated into it.
- Format arguments (file names, counts) are supplied by the caller after variant selection, so a
  variant can never reorder or drop a `%s`/`%d` placeholder relative to the source string — the
  tests assert that every ladder entry retains its placeholders.
- Emoji decoration is confined to the headline. Accessible names, button labels, and any text
  that could be read back as a command or identifier stay plain.

## Verification

Catch2 tests in `tests/language_mode/funny_level_tests.cpp` (target `language_mode_tests`):

- `Funny levels clamp to the 1..5 range`
- `Persisted funny levels parse with a compiled default and stable keys`
- `Copy variants follow the level ladder per language`
- `Level variants keep their format placeholders so facts stay exact`
- `translate() picks the level variant and otherwise the base string`
- `Dialog emojis decorate headlines only and never action labels`

Manual check: open Preferences ▸ General, move **Funny level (English)** to 5, then trigger any
warning dialog (for example close a project with unsaved changes). The headline reads
`Unsaved changes are waiting for a decision`; the buttons keep their exact labels. Switch
**Show emojis in dialogs and message boxes** on and reopen the dialog: the headline gains a
leading ⚠️ and the buttons do not.

## Suggested articles

- [English, Hong Kong Cantonese, and bilingual modes](language-modes.md) — the modes the
  ladders plug into.
- [Native Material Design 3 UI](md3-native-ui.md) — the Slider and Switch widgets used here.
- [Keyboard, assistive, and responsive GUI accessibility](gui-accessibility.md) — how the slider
  and disclosure button stay keyboard and screen-reader operable.
- [Appearance customization](appearance-customization.md) — the neighboring Preferences tab.
