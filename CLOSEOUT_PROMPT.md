# Current implementation handoff

Latest work: lubricate mode and print melodies (10 November 2026). The full
record is the first section of HANDOFF.md and the first section of ROADMAP.md.

## State

Lubricate mode, the print dialog melody list, melody playback at every print
start, the design boards, the captures from the built app, the article with its
Cantonese twin and the catalog strings are all on main. The last source commit,
1b10b4887, was built with `build.bat -BuildOnly` and checked on a hidden
desktop: a changed preset no longer adds a horizontal scroll bar on the Process
page, the melody list uses the app's own list, the search field outline is
clean, and the All Plates Stats tile draws its icon above its label.

## Open

- Run lubricate mode and the melodies on a real H2C and confirm how `M1006 W`
  paces the moves.
- Capture the Lubricate confirmation and the button while printing (needs a
  connected printer), the regex builder popover, and the light theme and
  Cantonese layouts.
- Reflow the Device page, which scrolls sideways at 1200x800 because its ink
  column sits right of the Control column.
- A long preset name with the changed marker is cut at the right edge of its
  box.
- The earlier Cantonese song title wording remains in the history of
  fa929bfbb; changing it needs a history rewrite, which is not authorized.

Earlier open work from the 117-item required-feature plan is unchanged and is
listed in HANDOFF.md and ROADMAP.md.
