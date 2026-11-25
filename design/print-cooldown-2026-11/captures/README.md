# Cool down captures from the built app

Taken on 2026-11-25 from `build\src\Release\bambu-studio.exe` built by `build.bat --silent` from
`108e0c6fc`, running on a hidden Windows desktop at 150 % display scale with no printer connected.
Each one was looked at before it was kept.

| Capture | What it shows |
| --- | --- |
| [cool-down-card-off.png](cool-down-card-off.png) | Device page scrolled down: the Cool down card under the Door chime card, switched off, with the line saying what turning it on does |
| [cool-down-card-on.png](cool-down-card-on.png) | The same card right after the switch was turned on: the line asks for a connected printer |

Checked in the same run, without a picture: the switch is written to the application settings
(`print_cooldown_enabled`) when turned on and again when turned off.

## Not reached

- The fans turning on at the end of a print, the cooling line with temperatures and time left,
  and the cooled and five minute states: all need a connected printer finishing a print.
- The card in Cantonese: the build tree ran with resources from before the new strings were
  compiled, so the new strings appear in English here. The installed payload under `install-dir`
  holds the compiled Cantonese strings.
