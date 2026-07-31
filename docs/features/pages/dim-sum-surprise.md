# Dim sum surprise

A one-in-a-hundred chance, per visit, that a dim sum dish appears in the bottom-left corner. Data:
[`ui-md3/site/dimsum.data.js`](../../../ui-md3/site/dimsum.data.js). Behaviour: `maybeDimSum()` in
[`ui-md3/site/boot.js`](../../../ui-md3/site/boot.js).

## The dishes

Ten, each named in English and Cantonese: shrimp dumpling 蝦餃, pork and shrimp dumpling 燒賣,
barbecue pork bun 叉燒包, rice noodle roll 腸粉, egg tart 蛋撻, lotus leaf sticky rice 糯米雞,
turnip cake 蘿蔔糕, egg waffle 雞蛋仔, braised chicken feet 鳳爪, and mango pomelo sago 楊枝甘露.

Artwork is hand-authored inline SVG held in the repository — no network fetch, no third-party CDN,
no tracking pixel. Food colours are deliberately fixed rather than derived from the active Material
palette: they are data, and a har gow tinted by the accent seed stops being a har gow.

## When it appears

- The draw is a single `Math.random() < 0.01` per launch. It is never re-rolled, so it cannot fire
  twice in one visit, and it is never made more frequent than the stated odds.
- **Never on a first visit.** A visitor with nothing stored yet is mid-onboarding; the surprise
  waits until they have been here before. This is the web analogue of "not during first run".
- Never when local storage is unavailable, because the first-visit check cannot be made honestly.
- Never blocking: the card is a corner `role="note"`, it does not gate startup, does not steal
  focus, and dismisses itself after twelve seconds.

## Accessibility and control

The SVG carries `role="img"` and an `aria-label` naming the dish in both languages, so a
screen-reader user gets the same delight rather than an unlabelled decoration. The entry animation
is disabled under `prefers-reduced-motion`. The card offers "Turn this off" inline, and the same
switch lives in Settings under Surprises; turning it off is honoured absolutely and persists.

The surrounding copy follows the active language mode and funny level; the dish's own name stays
correct at every level.
