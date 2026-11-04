# Live printer readings and printer options

The print monitor on the Device tab shows a row of read-only chips under the **Control** heading for readings the printer reports and the application already parses. It also offers every Print Options and Safety Options setting a printer reports support for. This article lists what is shown, where each value comes from, and when it is hidden.

Runtime behaviour on a real printer has not been verified. The rules below were checked against the source and against a header-level test; the chips and the changed entry points have not been driven on hardware.

## Live readings strip

Each chip is a label and a value. A chip appears only when its value was reported and is plausible; a printer that reports none of them shows no strip at all. Nothing is guessed, defaulted or filled with a placeholder.

| Chip | Value | Where it comes from | Hidden when |
| --- | --- | --- | --- |
| Wi-Fi signal | for example `-61 dBm, Fair` | `wifi_signal` in the print status message, stored in `MachineObject::wifi_signal` (`DeviceManager.cpp`, signals block) | the text is empty, malformed or outside -120 to 0 dBm |
| Air duct | Cooling, Heating, Exhaust or Full Cooling | `device.airduct.modeCur`, parsed by `DevFan::ParseV3_0` into `AirDuctData::curren_mode` | the printer reports no air duct, or the mode is not one of the four above |
| Open Door Detection | Off, Warning or Pause printing | the configured reaction to an open door, `xcam_door_open_check` (config bits 20 to 21 in `MachineObject::parse_new_info`) | the printer does not report door-open support (function bit 12) |
| Nozzle (Right or Left on dual-nozzle models) | for example `0.4 mm, Hardened Steel`, with `High Flow` and similar added only for a non-standard flow type | per-extruder nozzle in `DevNozzleSystem` (`DevNozzle::GetNozzleDiameter`, `GetNozzleType`, `GetNozzleFlowType`) | the diameter is zero, the material is unknown, or the nozzle is flagged unreliable, unknown or abnormal |

The signal quality words are a display heuristic, not a protocol value: -60 dBm or stronger is Strong, down to -75 dBm is Fair, anything weaker is Weak.

**Door (beta)** shows Open or Closed on printers that report door-open support. It reads bit 23 of `home_flag` (older status messages) or of `print.stat` (newer ones), stored in `MachineObject::door_open_bit`. That bit position comes from community protocol notes, not from a vendor document, and it has not been checked against a real printer from this build, so the chip is labelled beta. It is hidden when the printer reports neither field or has no door sensor. The Open Door Detection chip is separate: it shows the setting that decides what the printer does when the door opens.

**Chamber temperature, nozzle and bed temperature, fan speeds, speed level, chamber light, storage state, AMS humidity and drying** are already shown elsewhere on the Device tab and are not repeated here. The chamber fan speed is parsed with a default of zero when absent, so an unreported value cannot be told apart from a stopped fan; it is therefore not offered as a chip.

Each chip is not focusable, carries `label: value` as its name and tooltip, follows the light and dark theme, wraps onto further lines on a narrow monitor and is rebuilt on a DPI or theme change. The strings are localized, with the Cantonese entries in `bbl/i18n/yue_HK`.

## Print Options and Safety Options entries

The **Print Options** button used to appear only for first-layer inspection, AI monitoring, build plate marker detection and automatic step-loss recovery. A printer that supported only other options, such as filament tangle detection, nozzle blob detection, foreign-object or displacement checks, air purification at print end, print status snapshots or save-to-storage, had no way into the dialog. The button now appears when the printer reports support for any option the dialog can show, and each row inside the dialog is still gated on its own capability, so no control appears for an option the printer does not support. This applies to every model, including the H2C.

The **Safety Options** dialog holds the door-open check and idle heating protection. It was reachable only on models whose printer configuration enables it and which report door-open support. The H2D, H2S and H2C configurations do not enable it, so idle heating protection, which those printers can report (function bit 62), was unreachable. The entry now appears when either the configured door-open route applies or the printer reports idle heating protection. On models that do not enable the safety dialog the door-open check stays in Print Options, and the Safety dialog no longer repeats it there.

The shared rules are in `src/slic3r/GUI/DeviceLiveReadings.hpp` and are used by the status panel and the safety dialog.

## Failure modes

- A malformed `wifi_signal` or an unknown air duct id hides the chip instead of showing a wrong value.
- An option the printer stops reporting disappears from Print Options on the next status update.
- If the status message arrives before nozzle information, the nozzle chips appear on the update that carries it.

## Security and privacy

The readings stay in the application. Nothing is sent, logged or stored beyond the existing status handling. The strip is read-only and issues no printer command.

## Verification

- `node --test tests/device_live_readings.test.mjs` compiles the real header into a fixture and pins the production wiring to the parsed fields. Pass `--negative` to confirm it fails when the Wi-Fi wiring is removed.
- `python bbl/i18n/yue_HK/compile_translation.py --check` validates the catalogue after the new strings.
- Not verified: a native build of this change, the strip and the entry buttons on a real H2C or other printer, and captures of the new strip.
