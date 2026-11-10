# Rehearsal

The design tool cannot film its own boards, so every board was rendered in headless Chromium at
1440 px wide with the fonts this app ships (Roboto, Roboto Mono and Material Symbols Outlined from
`resources/fonts`), using [render/prep.py](render/prep.py) and [render/film.mjs](render/film.mjs)
from the previous pass. The frames in [frames](frames) are the final pass.

## Review findings

The review was done by the same author from the frames, against `references/review.md`; it was not
an independent reviewer.

| Board | Finding | Fix |
| --- | --- | --- |
| MelodyPicker | The dialog card stretched to the height of the state column, leaving about 120 px of empty card under Send | The grid aligns to the top, so the card ends under its buttons |
| LubricateDevice | The confirmation drew No before Yes; `MessageDialog` adds Yes first | Yes first and filled, then No |

Nothing fixable is left open.

## Not checked

- Light theme: the boards are drawn in the dark theme only.
- Cantonese and bilingual layouts on the boards.
- The boards against the built app side by side: the app captures in [captures](captures/README.md)
  were taken after this review and list where the app still differs.
