# Current implementation handoff

Latest work: lubricate mode at print start (11 November 2026): on by default,
a switch in the print dialog, and the H2C routine moved outside its homing
window. Before that: door chimes (11 November 2026), and lubricate mode and
print melodies (10 November 2026). The full record is the first two sections of
HANDOFF.md and of ROADMAP.md.

## State

Door chimes: the printer plays a chime when its door opens and another when it
closes, chosen from seven of each plus Silent on a Device page card with a
Preview on the printer beside each pick. It is off by default and waits while
a print runs. Built and checked on a hidden desktop with no printer connected:
the card, its defaults, the switch, a keyboard change of a chime, both
settings after a restart, and the Cantonese light layout.

Lubricate mode and print melodies are on main and were checked on the built
app, including the Device page reflow, the preset name ellipsis, the regex
builder, the Cantonese light layouts and the bilingual melody list.

## Open

- Watch a real H2C print start run the lubricate routine, hear the door
  chimes and the lubricate melodies on it, confirm the
  door edges arrive when the door moves, and confirm how `M1006 W` paces the
  moves.
- Capture Preview with a printer connected, the Lubricate confirmation and the
  button while printing.
- The earlier Cantonese song title wording remains in the history of
  fa929bfbb; changing it needs a history rewrite, which is not authorized.

Earlier open work from the 117-item required-feature plan is unchanged and is
listed in HANDOFF.md and ROADMAP.md.
