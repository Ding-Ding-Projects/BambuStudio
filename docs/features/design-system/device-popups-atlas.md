# Studio Atlas device popups

These are source implementation receipts for the six native device-popup anchors. They are not native compilation, rendered layout, accessibility interaction, or hardware evidence.

## Camera controls

`CameraPopup.cpp` uses a low-container body, a 16-DIP perimeter, closer label/control columns and a stronger Resolution heading. Resolution choices are real radio/text pairs owned by a horizontal sizer, with measured width and height plus eight-DIP vertical padding. This replaces absolute positioning inside a fixed 20-DIP row. Existing radio and label callbacks, capability checks, resolution identifiers, recording/liveview controls, guide link and persistence remain unchanged.

`Widgets/CameraHUD.cpp` resolves the fixed-dark camera chrome through Atlas slate roles. High-contrast system colors remain authoritative. Temperature chips use the same 13-pixel monospaced face for measurement and painting; the measured height includes eight DIP of vertical room and a 26-DIP minimum. Painting uses the allocated height, and rescale refreshes the font and cached measurement. Existing temperature strings, live state, animation timing, reduced motion, keyboard activation and camera callbacks remain unchanged.

Four checks in `ui-md3/tests/camera-popup-atlas.test.mjs` failed against the preceding revision and pass against this implementation. They verify real resolution-row sizer ownership and callback presence, body composition, production-derived temperature geometry at 100%, 125%, 150% and 200%, rescale measurement and preserved high-contrast alternatives. This is source-derived geometry, not a native window test.

## Fan, mapping and material controls

`Widgets/FanControl.cpp` uses quiet lowest-container cards for individual fans and the shared mode switch. The existing stepper has a low-container inset fill, a full focus stroke inside its existing target and monospaced percentage text. Target dimensions, mouse partitions, keyboard increments, confirmation, requested speed, returned telemetry, modes and hardware calls are unchanged.

`AmsMappingPopup.cpp` gives the AMS Slots header a padded measured heading rather than a fixed 30-DIP band. The Enable/Disable AMS explanation has explicit heading/supporting-text hierarchy and DPI-scaled spacing. Physical slot colors, spool gradients, tray and nozzle identities, material matching, hit testing and selection events are unchanged.

`AMSMaterialsSetting.cpp` uses semantic neutral field fills and outlines, a consistent field radius at construction and DPI rescale, and monospaced nozzle-range readouts. Actual filament colors and color-picker payloads remain unchanged. `AMSSetting.cpp` separates the page surround from its padded body, strengthens the page and section headings and lets the real body sizer determine its extent. Insertion/startup/capacity/switching behavior and all option callbacks remain unchanged.

Four checks in `ui-md3/tests/device-popup-atlas.test.mjs` fail against the preceding camera-only revision and pass after these changes. They cover production-derived fan focus bounds at supported scales, mapping-header size ownership, material-field radius across the real DPI lifecycle, and AMS body-sizer ownership. These are narrow source checks.

## Remaining surfaces and evidence

All six anchors have explicit source changes, but their full nested coverage remains incomplete. Mapping wheel rendering, multicolor material surfaces, nozzle-mapping detail, humidity explanations, material selection/search, pressure-advance profile popovers, custom color editing, AMS type/order panels, fan-name wrapping, capability-dependent status text and auxiliary printer-specific dialogs retain source/runtime review work. Adjacent popup hosts and web views have separate ownership. A color change in one host does not complete these nested surfaces.

All six anchors still require a native build and genuine interaction/capture evidence in English, Cantonese and bilingual modes, both themes, comfortable/compact density and every supported scale and minimum size. In particular, verify long radio labels, popup display bounds, camera temperature/header coexistence, keyboard focus, pending versus confirmed telemetry and every capability-dependent row. The application was not launched and no hardware operation was attempted.
