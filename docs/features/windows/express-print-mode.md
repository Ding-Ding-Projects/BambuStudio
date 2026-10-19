# Express print mode

Express print mode skips the optional preparation a Bambu Lab printer runs before every print and keeps only what a print needs: heating, one homing pass, the AMS filament load, the purge line and the temperature waits. It is one process option, `express_print_mode`, with two halves: the start G-code half and the send half. Both act only when the option is on; it is off by default.

## What is skipped and what is kept

| Skipped in express mode | Kept |
| --- | --- |
| Bed leveling (`G29`) | Heating of the bed, the nozzle and, where the printer has one, the chamber |
| Nozzle wipe | One homing pass (`G28`) |
| Vibration (mech mode) check and sweep (`M970`, `M974`) | AMS filament load (`M620`, `M621`) and the flush that follows it |
| Flow (extrusion) calibration and nozzle offset calibration | The purge or load line that primes the nozzle |
| Scanner, laser, camera, hand-eye, heatbed and build plate detections (`M972`, `M973`) | The final temperature wait and the `M1002 gcode_claim_action : 0` that ends preparation |
| Start sound (`M1006`) and cog noise reduction (`M982.2`) | Z offset trims (`G29.1`), vibration suppression (`M975 S1`) and the first-layer scan when that option is on |

The first layer is not corrected in express mode. Use it on a printer that was leveled and calibrated recently and whose build plate has not been moved; the mode is a time saver for repeated prints on a known machine, not a replacement for leveling. On the H2 family the hotend type detection is skipped too; the print setup dialog still checks the nozzle diameter and type against the printer.

## The option

The option sits on the process settings **Others** page, in the **Special mode** group, after **Timelapse**. It is a plain on/off switch in simple mode, is saved with the process preset and with the project, and is reachable through the settings search and the command palette's **Search in settings** row. Switching it invalidates only the G-code export, so the plate is re-exported, not re-sliced, the next time it is needed.

## The start G-code half

The Bambu Lab start G-code templates (`resources/profiles/BBL/machine/*template machine_start_gcode.json`) carry `{if !express_print_mode} ... {endif}` guards around the sections that otherwise run unconditionally: the start sound, cog noise reduction, the nozzle wipe, the vibration check, the detections and the z offset calibration. Bed leveling, extrusion calibration and toolhead offset calibration were already gated by the printer firmware behind send-time flags (`M1002 judge_flag` with `M622 J1` or `J2` blocks); they are left as they are, because the send half turns those flags off and their `M622 J0` branches still home. Each template also starts with an `{if express_print_mode}` comment so the exported file says why its start is short.

The guards are applied by `scripts/profiles/express_start_gcode_guards.py` from a per-family marker table. The script is idempotent, refuses a range that would split a placeholder block or an `M622`/`M623` pair, re-encodes the JSON string literal byte for byte, and has `--check` and `--render express|normal <file>` modes. `ui-md3/tests/express-start-gcode.test.mjs` resolves every template in both modes with an evaluator of its own and fails when the express view keeps a leveling, vibration, detection, sound or z offset command, loses homing, the filament load, a temperature wait, the prime line or the final claim action, or when the normal view loses a command its family is known to have.

A custom start G-code without these guards gets only the send half. A file started from the printer's own screen or an SD card with leveling or flow calibration turned on there still runs those firmware-gated blocks; that is the intended escape hatch.

## The send half

The **Send print job** dialog shows an **Express print mode** row first among the options. Unlike the other rows it is not a per-printer saved choice: it mirrors the process preset. Switching it writes the preset through the process settings tab, the slice is marked as needing a new export, the dialog reports the plate as needing a new slice and offers **Slice again**, so a plate sliced with one value can never be sent with the other. The row acts only when Slice again can: a normal send of the current plate while nothing is being sent. For **Print all**, the SD card view or while sending it is shown disabled with a tip and keeps the preset's value.

While the mode is on, **Auto Bed Leveling**, **Flow Dynamics Calibration** and **Nozzle Offset Calibration** are forced to off and disabled with the tip "Turned off by Express print mode."; their saved per-printer values come back when the mode is turned off. The job is then sent with every preparation flag off, "auto" included, which `apply_express_print_mode()` in `src/slic3r/GUI/SendPrintReadiness.hpp` decides and `tests/send_print_readiness_test.cpp` executes. Timelapse is not a preparation step and keeps the user's choice.

The multi-machine send page sends bed leveling and flow calibration off and greys out its two check boxes while the mode is on. The automation bridge defaults `bedLeveling` to off for an archive sliced with the mode on; a caller may still pass `bedLeveling: true`.

## Time estimate

The time estimator adds a preparation allowance once per file, when it meets the `G29` inside an `M622 J1` block. With express mode on for a Bambu Lab printer it uses a fixed 60 seconds (`EXPRESS_PREPARE_COMPENSATION_TIME`) instead of the printer's `machine_prepare_compensation_time` (260 seconds in the stock profiles). The 60 seconds stand for the homing, filament load and purge line that the estimator does not model; they are an estimate, not a measurement.

## Verification

Verified on this change: the g++ syntax check of every touched translation unit on Linux, `tests/send_print_readiness_test.cpp` (47 assertions), `ui-md3/tests/express-start-gcode.test.mjs`, `ui-md3/tests/send-dialog-express-mode.test.mjs`, `ui-md3/tests/send-dialog-stale-slice.test.mjs`, the Cantonese catalog check with `--require-complete`, and a reading of the resolved express start G-code for the X1 Carbon, X1E, P1S, A1, A1 mini, H2D and P2S templates. Not verified: an MSVC build, the dialog in the running application, a print on a physical printer and the time estimate against a real preparation. The hosted Windows build of the branch is the native verdict; a release note that claims printer behaviour must cite a print.

## Related

- [Plate print actions and material mapping](print-actions.md): the Slice again path the express row relies on.
- [Printer melodies](../device/printer-melody.md): the start sound that express mode skips can be replaced by a melody of your own, which is never skipped.
