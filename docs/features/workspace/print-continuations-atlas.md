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
footer, nonclient frame and 12-DIP upper/lower work-area margins, then bounds the body
viewport. A second pass accounts for a vertical scrollbar. Continuation is available only
when the measured disclosure has a readable floor (up to three lines plus padding, at
least 64 DIP for longer content) and the normal dialog width fits the work area.

If the work area is unknown or normal controls leave insufficient disclosure space, the
owner enters disclosure-only mode. It moves the actual scroll body into a temporary layout
with a Close action. The body retains its full content and both-axis scrolling; no copied
summary replaces the disclosure. The Close tooltip reuses the existing insufficient-space
explanation. Header/footer controls move beneath a hidden parent, preserving their own
visibility state. The Close action invokes the owner's existing cancellation/teardown route.

Requested capability is independent of presentation availability. A local button adapter
records every caller Enable/Disable request, even a repeated Disable while presentation
already disabled the native control. Confirmation, nozzle update, connection, retry and
reconnect handlers also check availability before dispatch. Growth or reopening restores
the original sizer and remeasures before restoring only the capabilities still requested.
Changes to visibility or disabled state made during readback survive restoration.

The dialog owns the active sizer; the noncopyable layout state owns only the inactive one.
The body is detached before insertion into either layout. Windows retain one wx parent,
and restoration reparents the normal controls before returning to their original sizer.
A reentry flag bounds nested layout. On extremely tiny displays the readback reserves most
of the remaining client height for disclosure and a smaller area for Close. This is a
non-authorizing fallback, not a claim that arbitrary physical pixels can render readable
text or a full-size button. Actual focus and pixel behavior still require native evidence.

The fitter runs when content changes and before presentation, and from the production move
and display-change event bindings. Address-entry status updates,
serial-number fallback and DPI changes use it as well. Hidden dialogs use their parent's
display work area. Existing centering remains, while the old extra 50-DIP upward offset in
address entry is removed so it does not move an otherwise bounded dialog toward the edge.
Shared buttons retain their existing motion and reduced-motion implementation. No callback
waits for animation, and no new animation or opening-time network operation is added.

## Preservation and focused evidence

`ui-md3/tests/print-continuations-atlas.test.mjs` passes three tests. It compiles the actual
production layout adapter, button capability adapter, owner fit/show methods, confirmation
and failed-send action callbacks, cancellation callbacks, and address validator. The exact
connection-handler entry checks run with their unchanged networking suffix replaced by an
observation counter, so the fixture cannot connect to a printer.

Its 166 owner assertions cover tiny/unknown work areas, shrink-grow-shrink, cached reopening,
caller disable and visibility changes during readback, production move/display-change
bindings, live cancellation, direct-event
rejection, restored retry results, and 100/125/150/200% scaling with both density paddings.
The doubles enforce one containing sizer per window. They are source-bound lifecycle
observations, not proof of native ownership, scrolling, focus, cancellation pixels or fonts.
The previous `b60bd4c8b8fa5eb0d3e5a9f706621e73f8a1a476` source fails the same owner fixture at
`exhausted disclosure disables confirmation`; the repaired source passes. Removing the
production work-area clamp also turns that assertion red.

The baseline ledger checks 17 action/state function bodies and five action callbacks.
Normalization permits only the named presentation-fit substitution and exact availability
checks. The button capability adapter is compiled separately with the owner lifecycle.
Changing the Retry modal result is rejected by the same comparison. Connection workers,
validation conditions, result codes, cancellation, event payloads and final physical-action
confirmation remain authoritative after the presentation check. No hardware command ran.

## Limits, configuration and privacy

There are no new translation keys, settings, persistence fields or catalog changes. Existing theme,
density, language and reduced-motion settings remain the configuration route. Existing
validation and connection failures keep their original messages and recovery choices.
No access code, address, serial number or account data is added to diagnostics or documentation.

Native compilation, real font measurement, keyboard focus, scrolling, English/Cantonese/
bilingual rendering, both themes, and printer/network behavior remain unverified. No full
build, application/browser launch, screenshot, installer execution or hardware action ran.
The required built-artifact layout matrix remains a later integration responsibility.
The shared status-bar internals and unrelated release-note, binding, AMS and confirmation
components are outside this unit and are not claimed redesigned here.
