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

## Door monitor: chimes while Bambu Studio is closed

Bambu Studio only rings the chimes while it is running. The **Door monitor** section at the bottom
of the card sets up the [Printer Door Monitor](https://github.com/Ding-Ding-Projects/printer-door-monitor)
web app, which keeps a log of every opening and closing and rings the chimes around the clock.

1. Under **Run it on**, choose **This computer** or **A Docker host over SSH**. For SSH, enter
   the host as `user@host`; typing a host picks the SSH target by itself. SSH uses your existing
   key, and Docker Desktop on this computer provides the `docker` command.
2. Press **Set up door monitor**. Bambu Studio checks Docker, downloads the monitor image, starts
   it with Docker Compose, waits for it to be healthy, hands it the selected printer (IP address,
   serial number, LAN access code) and the two chimes chosen in the card, checks its page from this
   computer, and opens it in the browser already signed in.
3. The status line gives the address, such as `http://<host>:8790/`. Bambu Studio turns its own
   chime switch off, so a door does not ring twice.

**Open** asks the monitor for a new one-time sign-in link and opens the page signed in.
**Copy password** copies the password Bambu Studio chose for a monitor that had none, for signing
in from a phone or another computer. A monitor that already had a password keeps it; Bambu Studio
never replaces it.

Without a printer that Bambu Studio can reach over the LAN, the monitor still starts with the
chimes set, and the status line says the printer was left out. Connect to the printer over the
LAN and press **Set up door monitor** again, or enter it on the monitor's own page. Pressing the
button again also updates the monitor to the newest image and keeps its door log.

When something stops the setup, the status line says which step and why: Docker not installed or
not running, the SSH host unreachable without a password prompt, the image not downloadable,
port 8790 already taken, the monitor not getting healthy within 90 seconds, or its page blocked by
a firewall.

## Files and tests

- `src/slic3r/GUI/DoorChime/DoorChime.hpp`: the chime library, the G-code, the door edge detector
  and the printing rule, free of wxWidgets.
- `src/slic3r/GUI/DoorChime/DoorChimeService.cpp`: the watcher, the settings and Preview.
- `src/slic3r/GUI/DoorChime/DoorChimeCard.cpp`: the Device page card.
- `src/slic3r/GUI/DoorChime/DoorMonitorSetup.cpp`: the one-click Door monitor setup (Docker, provisioning, sign-in link).
- `tests/door_chime_test.cpp` (196 assertions, run by `tests/door_chime.test.mjs`).

Verified: the assertions above with MSVC, the Cantonese catalog check, and the card in the built
app on a hidden desktop with no printer connected. **Set up door monitor** was pressed in the built
app for both targets, this computer and a Docker host over SSH: each time the monitor came up
healthy on 1.1.0 with a password and the chosen chimes, and the status line gave its LAN address. Not verified: a chime played by a real printer
and the door edge on a real H2C, which need the printer.

## Related

- [Printer melodies](printer-melody.md): compose your own tune and play it on the printer.
- [Live readings](live-readings.md): the door reading shown on the Device page.
