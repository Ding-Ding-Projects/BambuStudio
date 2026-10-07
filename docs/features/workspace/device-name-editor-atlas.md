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
Confirm presentation wrapper calls the existing validator exactly once. It fits only when
the dialog remains shown after validation. This pass adds no key or focus handlers.
The existing Escape/close route remains with the same dialog base and caption.

## Layout

The input fills the form width and uses actual text height with a 40/32-DIP density floor.
Comfortable/compact spacing is 16/10 DIP. The footer aligns the existing Confirm action
at the trailing edge and lets the shared button measure its translated label instead of
pinning it to 72 by 24 DIP. Its normal radius/DPI lifecycle remains active.

Validation occupies a separate full-width row using the semantic error foreground. The
label disables native automatic resizing, so `SetLabel` cannot synchronously fit its
intermediate unwrapped width before the existing validator reaches `Wrap`. The editor's
allocated width remains the wrapping reference; the dialog width never comes from a raw
validation string. The previous size-event listener is removed.

After validation, a presentation-only fitter wraps the original message against the
allocated content width, measures its wrapped height, and reserves the actual caption,
field, footer and 12-DIP display margins. A vertical scroll area retains the full message
when the display work area cannot hold it. Two bounded passes account for the scrollbar's
width, and a reentry flag prevents nested fitting. Nonpositive available space produces a
zero-height validation viewport rather than arithmetic underflow or a forced body minimum.
The fixed field/footer still require a usable display area; an arbitrarily tiny display is
not claimed supported. The original centering call is retained, and validation fitting
does not recenter the dialog.

## Verification and limits

`device-name-layout.test.mjs` compiles the actual `Label::SetLabel`, `on_edit_name`,
confirmation wrapper and `fit_validation_content` definitions. Observable native-control
doubles model the synchronous automatic-size event, and synthetic long translated-message
metrics exercise the real SetLabel, event, Wrap and fit order. Its 26 assertions cover
100/125/150/200% scaling, stable dialog width, wrapping before fitting, work-area height
bounds, retained scrollable content, nested fitting protection, invalid-name rejection,
and exactly one valid-name dispatch followed by modal close without another fit.

Running the same test against `5b77527856e266aa952392f88d031da05f67c715` fails at
`SetLabel must not widen the dialog before Wrap`; the repaired source passes. These
measurements are test inputs, not rendered native-font evidence. The test uses the real
production methods rather than supplying their final measured sizes alone.

Source comparison against `5b77527856e266aa952392f88d031da05f67c715` confirms six bodies
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
