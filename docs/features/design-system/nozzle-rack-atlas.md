# Studio Atlas nozzle rack and hotend details

The Monitor nozzle rack, mapping selection tiles, and Hotends Info rows now share compact rounded cards, neutral surfaces, and a clear identity-first hierarchy. Selected tiles use the Device secondary-container fill and primary border; disabled tiles use a muted surface while retaining the existing selection marker. Error text and firmware-update accents retain their existing semantic roles. Filament swatches remain actual material colours.

## Reachable surfaces and preserved behaviour

- `StatusPanel.cpp` owns the Monitor rack. `wgtDeviceNozzleRackNozzleItem` is also used by `wgtDeviceNozzleRackSelect` inside mapping.
- The selection panel retains rack order `16, 18, 20, 17, 19, 21`, left/right extruder identities, dynamic-nozzle restrictions, compatibility decisions, and the existing mouse-release event payloads.
- The rack's Hotends Info action opens `wgtDeviceNozzleRackUpgradeDlg`. Its rows retain empty, unknown, normal, abnormal, reading, refresh, firmware-version, serial-number, and used-time data paths. Reading, motion, homing, upgrade navigation, and capability checks are unchanged.

## Measurement and lifecycle

Tiles retain an 88 by 100 DIP minimum, but no maximum ceiling. The actual sizer content contributes to both dimensions after construction, label changes, and DPI changes. The label fonts refresh before measurement, and the old minimum is cleared before calculating the new one. This allows longer translated content to request space from its owner.

Hotend material and firmware columns retain 220 and 183 DIP preferred minima without a 40 DIP height ceiling. Data updates and DPI changes refresh fonts and layout. The error and refresh icon containers pass the logical size 20 to the shared button API, avoiding a second conversion at high display scales. Existing glyph sizing, event handling, and focus behaviour remain intact.

## Verification and remaining limits

`node --test ui-md3/tests/nozzle-rack-atlas.test.mjs` passes four source/non-window checks. The previous source at `50715f4355e8b845042bd4809bac2da2ce5c40f4` produces three failures and one preservation pass. The measurement check executes the production expressions with supplied content extents at 100%, 125%, 150%, and 200%; it does not measure native text. Nineteen hardware, mapping, and firmware method bodies are compared with that baseline, allowing only the added row-measurement call.

No native build, hardware action, rendered capture, or real language/theme/density matrix was run. Outer rack widths, mapping-popup placement, Hotends Info viewport fit, long serial numbers, and update-row overflow still require built-surface inspection. Existing keyboard selection limitations, nested nozzle dialogs, and drying controls are outside this change. This article does not claim complete printer-surface redesign or rendered parity.

There are no new settings, strings, persisted data, external services, or security-sensitive operations.
