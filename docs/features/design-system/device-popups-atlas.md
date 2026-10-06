# Studio Atlas device popups

These are source implementation receipts for the six native device-popup anchors. They are not native compilation, rendered layout, accessibility interaction, or hardware evidence.

## Camera controls

`CameraPopup.cpp` uses a low-container body, a 16-DIP perimeter, closer label/control columns and a stronger Resolution heading. Resolution choices are real radio/text pairs owned by a horizontal sizer, with measured width and height plus eight-DIP vertical padding. This replaces absolute positioning inside a fixed 20-DIP row. Existing radio and label callbacks, capability checks, resolution identifiers, recording/liveview controls, guide link and persistence remain unchanged.

`Widgets/CameraHUD.cpp` resolves the fixed-dark camera chrome through Atlas slate roles. High-contrast system colors remain authoritative. Temperature chips use the same 13-pixel monospaced face for measurement and painting; the measured height includes eight DIP of vertical room and a 26-DIP minimum. Painting uses the allocated height, and rescale refreshes the font and cached measurement. Existing temperature strings, live state, animation timing, reduced motion, keyboard activation and camera callbacks remain unchanged.

Four checks in `ui-md3/tests/camera-popup-atlas.test.mjs` failed against the preceding revision and pass against this implementation. They verify real resolution-row sizer ownership and callback presence, body composition, production-derived temperature geometry at 100%, 125%, 150% and 200%, rescale measurement and preserved high-contrast alternatives. This is source-derived geometry, not a native window test.

## Remaining surfaces and evidence

The next owned source units are `Widgets/FanControl.cpp`, `AmsMappingPopup.cpp`, `AMSMaterialsSetting.cpp`, and `AMSSetting.cpp`. No completion of those surfaces is implied by the camera unit. Adjacent popup hosts, web views, printer-specific controls and other files retain their separate ownership.

All six anchors still require a native build and genuine interaction/capture evidence in English, Cantonese and bilingual modes, both themes, comfortable/compact density and every supported scale and minimum size. In particular, verify long radio labels, popup display bounds, camera temperature/header coexistence, keyboard focus, pending versus confirmed telemetry and every capability-dependent row. The application was not launched and no hardware operation was attempted.
