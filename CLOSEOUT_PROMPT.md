# Current implementation handoff

Latest work: lubricate mode and print melodies (10 November 2026). The full
record is the first section of HANDOFF.md and the first section of ROADMAP.md.

## State

Lubricate mode, the print dialog melody list, melody playback at every print
start, the design boards, the captures from the built app, the article with its
Cantonese twin and the catalog strings are all on main. The last source commit,
f9caf773c, was built with `build.bat -BuildOnly` and checked on a hidden
desktop: a changed preset no longer adds a horizontal scroll bar on the Process
page, the melody list uses the app's own list, the search field outline is
clean, the All Plates Stats tile draws its icon above its label, the Device
page stacks its columns instead of scrolling sideways, and a long preset name
ends in an ellipsis. The Cantonese light theme layouts and the regex builder
were captured too.

## Open

- Run lubricate mode and the melodies on a real H2C and confirm how `M1006 W`
  paces the moves.
- Capture the Lubricate confirmation and the button while printing (needs a
  connected printer) and the bilingual layouts.
- The earlier Cantonese song title wording remains in the history of
  fa929bfbb; changing it needs a history rewrite, which is not authorized.

Earlier open work from the 117-item required-feature plan is unchanged and is
listed in HANDOFF.md and ROADMAP.md.
