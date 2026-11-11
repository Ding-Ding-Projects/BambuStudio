# Door chime captures from the built app

Taken on 2026-11-11 from `build\src\Release\bambu-studio.exe` built by `build.bat -BuildOnly`,
running on a hidden Windows desktop at 150 % display scale with the Bambu Lab H2C 0.4 nozzle
selected and no printer connected. Each one was looked at before it was kept.

| Capture | What it shows | Built from |
| --- | --- | --- |
| [door-chime-card.png](door-chime-card.png) | Device page at 1200x800, scrolled down: the Door chime card under the Move card, switched on, with Ding-dong and Dong-ding chosen and Preview off because no printer is connected | `10823088b` |
| [door-chime-card-on.png](door-chime-card-on.png) | The card alone right after the switch was turned on: the switch redraws at once | `10823088b` |
| [door-chime-close-picked.png](door-chime-close-picked.png) | After a restart on the same data directory: the switch is still on and the close chime is still Falling arpeggio, picked from the keyboard before the restart | `8266cc95b` |
| [door-chime-card-yue-light.png](door-chime-card-yue-light.png) | The Device page with the card in Cantonese with the light theme | `10823088b` |

Checked in the same runs, without a picture: the switch and the close chime choice are written to
the application settings (`door_chime_enabled`, `door_chime_close`) and come back after a restart
on the same data directory.

## Not reached

- A chime played by a real printer, the Preview buttons enabled, and the door open and close edges:
  all need a connected printer with a door sensor, such as the H2C.
- The chime lists opened as drop-downs: the drop-down did not open from a background click on the
  hidden desktop, so the keyboard was used to change the choice instead.
