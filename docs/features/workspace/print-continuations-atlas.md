# Print confirmation and connection continuations

This source-only Studio Atlas unit covers `ConfirmBeforeSendDialog`,
`InputIpAddressDialog`, and `SendFailedConfirm` in `ReleaseNote.cpp/.hpp`.
The existing target-owned `design/workflow-refresh` handoff supplies the visual direction.
The live Material Designer creation/export route was unavailable within this unit's explicit
no-application-launch boundary. No design preview is presented as runtime evidence.

## Real entry paths

| Surface | Existing caller and result |
| --- | --- |
| Warning/error confirmation | `SelectMachineDialog::show_errors` and the warning branch in `on_ok_btn` populate `ConfirmBeforeSendDialog`. Only its existing explicit confirmation event reaches the caller's `on_send_print`. Blocking warnings still hide Confirm. |
| Address and access-code entry | Direct-connection targets in `SelectMachinePop.cpp`, and `GUI_App::show_modal_ip_address_enter_dialog`, open `InputIpAddressDialog`. The existing selected-machine setup, address/code validation, serial-number stage and asynchronous connection flow remain. |
| Failed-send choices | `SendToPrinterDialog` opens `SendFailedConfirm` from the send job's address-check failure callback. `wxYES` queues its existing retry; `wxAPPLY` clears the address and opens the existing reconnect route. Closing does neither. |

## Presentation

The confirmation body uses padded 14-point shared labels and a tonal scroll surface.
The three existing actions retain their order and callbacks in a vertical measured stack,
so the long nozzle action does not compete with the other labels for a single row.
Replacing message content clears its previous presentation children before creating the
new list; warning colors and existing wiki links remain intact.

Address-entry instructions, help, both existing field groups and status remain together
inside one scroll owner. The Connect action stays outside the scrolling body. Fields use
measured character height plus padding with the active 40/32-DIP row floor; field labels
rewrap at their column width after DPI changes. The existing help caption provides section
hierarchy without new copy. The diagnostic success/error colors retain their semantic roles.

Failed-send recovery uses a tonal body, readable shared text, a filled Retry action and an
outlined reconnect action. Both use the shared medium button sizing and normal DPI/radius
lifecycle instead of fixed 24-pixel action heights.

A file-local fitter wraps text before measuring height. It reserves the actual header,
footer, nonclient frame and 12-DIP upper/lower work-area margins, then bounds only the body
viewport. A second pass accounts for a vertical scrollbar. The full content remains in the
scrollable virtual area. A reentry flag bounds nested layout. Nonpositive available height
produces a zero-height body rather than arithmetic underflow or an impossible body minimum.
The fixed header and actions still require a usable display area; arbitrarily small displays
and unmeasured extreme custom fonts are not claimed supported.

The fitter runs when content changes and before presentation. Address-entry status updates,
serial-number fallback and DPI changes use it as well. Hidden dialogs use their parent's
display work area. Existing centering remains, while the old extra 50-DIP upward offset in
address entry is removed so it does not move an otherwise bounded dialog toward the edge.
Shared buttons retain their existing motion and reduced-motion implementation. No callback
waits for animation, and no new animation or opening-time network operation is added.

## Preservation and focused evidence

`ui-md3/tests/print-continuations-atlas.test.mjs` passes three tests. It compiles the actual
production body fitter and address validator against non-window geometry doubles, exercising
129 assertions across 100/125/150/200% scaling, both density paddings, and 360/600/1000-DIP
work-area heights. It covers scrollbar-adjusted wrapping, retained content, nested layout,
hidden-parent display selection, nonpositive space and existing complete-address decisions.
Removing the actual production work-area clamp makes the executable fail the height assertion.

The reviewed baseline ledger checks 17 action/state function bodies and five action callbacks.
Only the explicitly named `Fit()` to `fit_content()` presentation substitution is normalized.
Changing the Retry modal result is rejected by the same comparison. Connection workers,
validation conditions, result codes, cancellation, event payloads and final physical-action
confirmation remain authoritative. The tests do not send a command or connect to a printer.

## Limits, configuration and privacy

There are no new strings, settings, persistence fields or catalog changes. Existing theme,
density, language and reduced-motion settings remain the configuration route. Existing
validation and connection failures keep their original messages and recovery choices.
No access code, address, serial number or account data is added to diagnostics or documentation.

Native compilation, real font measurement, keyboard focus, scrolling, English/Cantonese/
bilingual rendering, both themes, and printer/network behavior remain unverified. No full
build, application/browser launch, screenshot, installer execution or hardware action ran.
The required built-artifact layout matrix remains a later integration responsibility.
The shared status-bar internals and unrelated release-note, binding, AMS and confirmation
components are outside this unit and are not claimed redesigned here.
