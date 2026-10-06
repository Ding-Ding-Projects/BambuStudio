# Device-name editor presentation

The reachable device-name editor now uses the Studio Atlas form spacing and the shared
medium confirmation button. This is a bounded appearance change in `SelectMachinePop.cpp`
and its essential header, not a change to naming rules or device commands.

## Entry and behavior

The actual path is the edit hit target in `MachineObjectPanel::on_mouse_left_up`, followed
by `EVT_EDIT_PRINT_NAME` in `SelectMachinePopup::update_user_devices`, then
`EditDevNameDialog::set_machine_obj` and `ShowModal`. The existing editor value, validation,
and final `DeviceManager::modify_device_name` dispatch remain unchanged.

`on_edit_name` still rejects illegal characters, the modified-preset suffix, empty names,
leading/trailing spaces, and names longer than 32 characters. Its existing message strings,
UTF-8 conversion, selected device identity and modal result are preserved. No extra command
is dispatched by layout or opening. The text field retains `wxTE_PROCESS_ENTER`, the
existing Confirm binding is unchanged, and this pass adds no key or focus handlers.
The existing Escape/close route remains with the same dialog base and caption.

## Layout

The input fills the form width and uses actual text height with a 40/32-DIP density floor.
Comfortable/compact spacing is 16/10 DIP. The footer aligns the existing Confirm action
at the trailing edge and lets the shared button measure its translated label instead of
pinning it to 72 by 24 DIP. Its normal radius/DPI lifecycle remains active.

Validation occupies a separate full-width row using the semantic error foreground. The
existing validation handler wraps its actual message and requests layout. A presentation
size listener responds to a growing validation label by comparing the full dialog sizer's
measured minimum with its client area, growing either dimension if necessary. It does not
shrink the active dialog when a message clears. A reentry flag bounds nested size events.
The original centering call is retained; validation growth does not recenter the dialog.

## Verification and limits

`device-name-layout.test.mjs` compiles the actual production `fit_validation_content`
method against observable size/sizer doubles. Its 48 assertions cover two density floors,
100/125/150/200% scaling, synthetic measured long/bilingual footer and validation extents,
large field fonts, no unnecessary resize, nested callback reentry, and absent-sizer handling.
Deleting the growth operation fails the same executable's content-fit assertion. These
measurements are test inputs, not a claim that native fonts were rendered.

Source comparison against `6b4ebfa5a6207d66750a1a059124119229fa38cf` confirms six bodies
unchanged: validation/rename dispatch, selected-device setup, mouse edit routing, keyboard
routing, primary activation, and the popup's user-device event bindings.

Native compilation, size-event delivery, English/Cantonese/bilingual rendering, both themes,
keyboard focus, Enter/Escape, and real device renaming remain unverified. No application
launch, full build, hardware command, or screenshot was performed. Later verification must
use an isolated test device/name and must not expose account data or access codes.

## Adjacent inventory boundary

Final-send confirmation remains in `ReleaseNote.cpp`; material mapping/refill in
`AmsMappingPopup.cpp`; nozzle/material reselection in `DeviceTab/uiAMSBestPositionPopup.cpp`;
local timelapse choices in `SelectMachine.cpp`; access-code entry in `ConnectPrinter.cpp`;
PIN/bind/unbind flows in `BindDialog.cpp`; manual IP entry in `ReleaseNote.cpp`.
Those surfaces are not redesigned by this unit. `AmsMapingTipPopup` and `AmsTutorialPopup`
are constructed as members in the inspected sources, but no actual open call was found;
their reachability remains an inventory gap rather than a claimed reachable flow.
