# First-run funny-level disclosure

The funny level is disclosed when Bambu Studio is first set up, not only inside Preferences. The
first-run **Setup Wizard** has a **Funny levels** step that says plainly what the setting does
before the user goes any further, shows both current levels, and lets the user change or reset
either one on the spot.

## Behavior

- **Where:** the Setup Wizard, between **Please select your login region** and the User Experience
  Improvement Program page. The wizard runs on first start and again from **Help > Setup Wizard**,
  so the step can be revisited at any time.
- **What it states, in this order:**
  1. The funny level styles every message Bambu Studio shows, **including errors and warnings**.
     It only changes the tone: what happened, what is affected and what the user can do are stated
     exactly at every level.
  2. English and Cantonese each have their own funny level, from 1 (fully serious) to 5 (maximum
     playfulness), and **both start at level 5**. The number comes from the application
     (`I18N::FUNNY_LEVEL_DEFAULT`), so the page can never state a different default from the one
     that ships.
  3. Either level can be **changed or reset at any time**: on this step now, or later in
     **Preferences > General**.
- **Controls:** one slider per language (**Funny level (English)**, **Funny level (Cantonese)**),
  each with its value read back as "Level N of 5", one real message shown at the current level
  ("Slicing complete" from the funny-level copy table, in that slider's language), and a
  **Reset English to level 5** / **Reset Cantonese to level 5** button. Moving a slider saves the
  level when the move ends; resetting removes the stored value so the shipped default applies
  again, exactly as on a profile that never changed it. Both paths write the same keys as the
  Preferences sliders and update the running language service, so the rest of the application
  follows immediately.
- **Voice, not facts:** the opening line of the step is itself styled by the funny level (one line
  for levels 1-2, one for level 3, one for levels 4-5). The three statements above never change
  with the level.
- **Next and Back:** **Next** records that the disclosure was read and moves on to the experience
  program page; **Back** returns to the region page, and the program page's **Back** returns here.
  The selected region is carried through to the program page as before.

### Language modes

All wording lives in the guide's web text catalog (`resources/web/data/text.js`, keys `t300` to
`t313`) with an English and a Hong Kong Cantonese entry for every key; the page logic chooses keys
and fills the `{default}` and `{level}` placeholders.

- **English** and **Hong Kong Cantonese** show the step in that language.
- **Bilingual** shows each line in English with its Cantonese line beneath, using the guide's
  existing compact secondary line. The opening line follows each language's own level: the English
  part at the English level and the Cantonese part at the Cantonese level.
- Any other guide language, or a key with no Cantonese entry, falls back to the English text.

### School mode

While School mode is on, funny levels behave as if they were not installed. The step answers the
page with no levels, default or sample, nothing is shown, and the wizard moves straight on in the
direction the user was travelling (forward to the program page, or back to the region page). No
level is written while School mode is on.

### Outside the application

Opened in an ordinary browser, the page still shows the three statements with the shipped default
(level 5) and leaves the controls hidden, because only the application can store a level.

## Configuration

| Key | Location | Values |
| --- | --- | --- |
| `funny_level_en` | `BambuStudio.conf` (AppConfig) | 1-5; missing means the shipped default, 5 |
| `funny_level_yue` | `BambuStudio.conf` (AppConfig) | 1-5; missing means the shipped default, 5 |
| `funny_level_disclosed` | `BambuStudio.conf` (AppConfig) | `true` once the step has been read and left with **Next** |

## Implementation

- `src/slic3r/GUI/FirstRunFunnyDisclosure.hpp`: a wxWidgets-free model. `payload()` builds the
  `response_funny_disclosure` answer (levels, whether each is stored, the sample, the range and the
  default; only `available: false` in School mode). `parse_request()` accepts
  `request_funny_disclosure`, `save_funny_level` (`language` `en` or `yue`, a whole-number `level`,
  clamped to 1-5), `reset_funny_level` and `acknowledge_funny_disclosure`, and rejects anything
  else so nothing is written.
- `src/slic3r/GUI/WebGuideDialog.cpp`: `GuideFrame::OnScriptMessage` hands those messages to
  `apply_funny_disclosure_request()`, which saves or removes the key, updates
  `I18N::language_mode_service()`, records `funny_level_disclosed`, and answers the page. Its
  bounds are `static_assert`ed against `I18N::FUNNY_LEVEL_MIN`, `_MAX` and `_DEFAULT`.
- `resources/web/guide/12/`: the step page (`index.html`, `12.css`, `12.js`) and its DOM-free copy
  and routing logic (`funny-disclosure.js`). `resources/web/guide/11/11.js` now continues to the
  step, and the program page's **Back** returns to it.

## Accessibility

The sliders are native range inputs with visible labels, a spoken value ("Level 3 of 5" through
`aria-valuetext`), a polite live readout and a description pointing at the sample message. The reset
buttons name the language they reset. Everything works with the keyboard alone (arrow keys move a
slider, Space or Enter presses a button) and uses the guide's focus ring, reduced-motion rules and
light and dark colour tokens.

## Verification

- `g++ -std=c++17 -Isrc -Itests -Itests/catch2 tests/language_mode/first_run_disclosure_tests.cpp`
  then run the binary: the model's defaults, both funny-level extremes, School mode, and every
  accepted and rejected request (CTest target `first_run_disclosure_tests`).
- `node --test ui-md3/tests/first-run-funny-disclosure.test.mjs`: the wizard order, the page's
  controls, the native wiring, the catalog entries, and the rendered step on a fresh profile. The
  step is then rendered in English, Cantonese and bilingual mode with the English and Cantonese
  levels at 1 and 1, 5 and 5, 1 and 5, and 5 and 1: every mode must state all three facts in each
  language it shows, the facts must be word for word the same at levels 1 and 5 while the opening
  line changes, each slider must read back its own level, and no placeholder may be left unfilled.
  The fallbacks are covered too: other guide languages and missing Cantonese entries show English,
  a page without the application shows the facts with level 5 and no controls, out-of-range levels
  are clamped and School mode reports the step unavailable. The page logic must carry no words of
  its own, and negative regressions (a misstated default, a fact that no longer mentions errors and
  warnings, a missing Cantonese line, a fact that follows the level) must each turn the check red.
- `node resources/web/data/validate-text-locales.mjs`: Cantonese key parity for the new keys.

Still to be observed in a built Windows application: the step appearing in the first-run wizard,
the sliders and reset buttons saving to `BambuStudio.conf`, the sample message and opening line
following the slider, the step being skipped while School mode is on, and the layout at narrow
widths and high display scales in each language mode.
