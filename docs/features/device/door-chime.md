# Door chimes

The printer plays a short chime through its buzzer when its door opens, and another when it
closes. Pick each chime on the Device page, hear it on the printer with **Preview**, and turn the
feature on with one switch. It is off until you turn it on.

## The Door chime card

The card sits on the Device page under the **Move** card.

- **Play a chime when the door opens or closes** turns the feature on or off. It is labelled beta
  in its tooltip, because the door reading comes from a status bit the printer maker does not
  document.
- **When the door opens** and **When the door closes** pick a chime for each edge. Every list ends
  with **Silent**, so one edge can stay quiet.
- **Preview** beside each list plays the chime chosen there on the printer straight away. It works
  on any connected printer that is not printing, with or without a door sensor, and is off for
  **Silent**.
- The line under the lists says what will happen now: no printer connected, no door sensor (only
  Preview plays), waiting because the printer is printing, or ready.

The switch and both choices are kept in the application settings (`door_chime_enabled`,
`door_chime_open`, `door_chime_close`) and apply to whichever printer is selected on the Device
page.

## The chimes

| Door opens | Notes | Door closes | Notes |
| --- | --- | --- | --- |
| Ding-dong (default) | E5 then C5, the two-note doorbell | Dong-ding (default) | C5 then E5 |
| Welcome rise | C5, E5, G5, C6 | Falling arpeggio | C6, G5, E5, C5 |
| Shop bell | two quick E6 pings | Westminster, closing phrase | G4, D5, E5, C5 |
| Westminster, first phrase | E5, C5, D5, G4 | Goodnight | E5, D5, C5 |
| Hello chirp | G5, C6 | Soft latch | C4, then G3 |
| Marimba up | C5, D5, E5, G5, A5 | Double tap | two short C5 |
| Single ping | C6 | Bye chirp | C6, G5 |
| Silent | nothing | Silent | nothing |

Each note plays on all three buzzer voices, the same layout as the lubricate melodies. The
Westminster phrases are from the public domain Westminster Quarters.

## When a chime plays

The app watches the door reading of the selected printer twice a second. A change from closed to
open sends the open chime; a change from open to closed sends the close chime. The first reading
after the app connects to a printer, or after you select another printer, only records the state,
so a door that is already open stays quiet. A printer that has not reported its door yet is
skipped until it does.

A chime is sent as live commands, the same way **Play on printer** in the
[Melody creator](printer-melody.md) sends a melody:

```
M17
M400 S1
M1006 S1
M1006 A76 B25 L100 C76 D25 M100 E76 F25 N100
M1006 A72 B35 L100 C72 D35 M100 E72 F35 N100
M1006 W
M18
```

The tune turns the motors on before it plays and releases them afterwards (`M18`), and `M400`
waits for the motion queue. That must never happen in the middle of a print, so chimes wait while
the printer is printing, paused, preparing or slicing; a door opened then gets no chime. After a
chime the motors are released, so a later jog on the Device page may ask to home first.

## Requirements and limits

- The door reading only exists on printers that report door-open detection, such as the Bambu Lab
  H2C. On other printers only **Preview** plays.
- The door reading arrives with the printer's status reports, so a chime can follow the door by a
  moment.
- Only the printer selected on the Device page is watched.

## Files and tests

- `src/slic3r/GUI/DoorChime/DoorChime.hpp`: the chime library, the G-code, the door edge detector
  and the printing rule, free of wxWidgets.
- `src/slic3r/GUI/DoorChime/DoorChimeService.cpp`: the watcher, the settings and Preview.
- `src/slic3r/GUI/DoorChime/DoorChimeCard.cpp`: the Device page card.
- `tests/door_chime_test.cpp` (196 assertions, run by `tests/door_chime.test.mjs`).

Verified: the assertions above with MSVC, the Cantonese catalog check, and the card in the built
app on a hidden desktop with no printer connected. Not verified: a chime played by a real printer
and the door edge on a real H2C, which need the printer.

## Related

- [Printer melodies](printer-melody.md): compose your own tune and play it on the printer.
- [Live readings](live-readings.md): the door reading shown on the Device page.
