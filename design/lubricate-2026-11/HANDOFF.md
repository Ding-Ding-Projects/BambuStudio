# Design handoff: lubricate mode and print melodies

Three boards for the lubricate mode feature, drawn from the app's own Material Design 3 tokens
([boards/kit.css](boards/kit.css), shared with [../local-tools-2026-11](../local-tools-2026-11)).
The Bambu Lab H2C is the primary printer for this feature, so the G-code board shows the H2C start
G-code.

| Board | What it shows | Native code |
| --- | --- | --- |
| [LubricateDevice](boards/LubricateDevice.dc.html) | The Lubricate button under the jog grid on the Device page, its tooltip, the Yes/No confirmation, and the disabled state while printing | `StatusPanel::create_axis_control`, `StatusPanel::on_lubricate` |
| [LubricateMode](boards/LubricateMode.dc.html) | The Lubricate mode switch on the Others page beside Express print mode, its tooltip, and where the routine runs in the H2C start G-code | `lubricate_mode` in `PrintConfig.cpp`, `Tab.cpp`, `GCode/LubricateRoutine.cpp` |
| [MelodyPicker](boards/MelodyPicker.dc.html) | The Lubricate melody section of the print dialog: the shared search field with its regex toggle and builder, the melody list, the note for each mode, plain, regex and no-match states | `SelectMachineDialog::create_lubricate_melody_panel` |

Every string on the boards is the product's copy as it is in the source, in English. The Cantonese
strings are in `bbl/i18n/yue_HK/BambuStudio_yue_HK.po`.

## Behaviour the boards rely on

- With lubricate mode on, the routine runs after the last full homing line of the start G-code
  (`G28`, never a `G28.<n>` calibration command and never one inside an `M622`/`M623` firmware
  conditional). On the H2C that is `G28 Z P0 T250`. The routine ends by repeating that line.
- The chosen melody plays on every print on Bambu Lab printers: one note per 25 mm bed segment with
  lubricate mode on, the whole tune once at the very start with it off, Express print mode included.
  Silent turns it off.
- Choosing a melody in the print dialog writes the process preset, the same route as the Express
  print mode switch, so the plate is sliced again before it is sent.

## Guidance used

- Stageshow (`benjitaylor/skills`, folder `stageshow`), installed fresh for this pass. Its files
  match the pinned copy at commit `299140fafd08df55a9c234e610c3f64af3f45531` once line endings are
  ignored (the installer wrote CRLF), so no refresh was needed.
- Read: `SKILL.md`, `references/craft.md`, `references/framing.md`, `references/review.md`.
- Gold example copied: none. These are real app screens, not marketing scenes. Applied: restraint
  (real words only where the product shows them), hierarchy, icons from Material Symbols at one
  size, spacing and the review. Not applicable: cursor and motion, stage framing and bleed, the
  phone-width compact design, loops.
- Board titles in [boards/canvas.json](boards/canvas.json) are the public names.

## Review

See [REHEARSAL.md](REHEARSAL.md).

## Captures from the built app

See [captures](captures/README.md): the three features captured from the built app on the H2C, with what was not reached and where the app still differs from the boards.
