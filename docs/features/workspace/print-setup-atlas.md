# Print setup and storage transfer presentation

This Studio Atlas source pass covers the existing `SelectMachineDialog`, printer-list
popup rows, and `SendToPrinterDialog`. It changes presentation and local sizing without
adding messages, settings, transport paths, or submission logic.

## Review hierarchy

The print-job review groups the existing summary, material mapping, and options into
outlined tonal cards. The material heading and project name use the section type scale.
Project-name editing uses a measured density floor instead of a 24/25-DIP height cap.
The existing final Send button uses the shared large-button geometry, including its own
DPI scaling. Its confirmation/progress region measures its pages instead of capping the
button at 32 DIP.

The existing scroll owner receives the smaller of its content height, the 650-DIP
preferred height, and the space remaining in the current display work area after actual
caption, footer, diagnostics, native frame, and margins. The calculation runs after
caption adoption and on DPI, progress-height, and diagnostic-layout changes. It does not
move the dialog after the existing centering call or change the selected review page.
Nonpositive available space produces a zero body allocation, never an unsigned wrap or
an impossible positive minimum. A display smaller than the fixed chrome still requires
native assessment; the arithmetic cannot manufacture screen space.

The storage-transfer review presents the project name before its genuine thumbnail and
statistics. A 198-DIP thumbnail leaves more room for destination and storage choices.
Printer and storage controls occupy matching cards. Refresh is tonal and the existing
explicit Send remains filled. Storage entries and the transfer progress/cancel controls
remain the original objects.

Printer-list rows fill the existing viewport, use measured density height, and distinguish
hover/focus with a tonal fill and rounded outline. Status icons retain their actual
meaning. The existing edit/unbind drawing and hit rectangles, device identities, keyboard
activation, filtering, regex builder, timer, and connection routes remain intact. Group
headings use the stronger section scale; the list viewport flexes inside its existing
popup instead of reserving its old full height alongside the search row.

## Action preservation

The selected/all-plate input, AMS and nozzle mapping, compatibility/readiness checks,
final confirmation, rename validation, transport dispatch, cancel/join behavior, storage
selection, and recovery logic are unchanged. Opening or restyling these surfaces does not
send or print. Geometry callbacks only resize and lay out existing controls.

A source comparison against `7602228306b7dba316a2d7a91c1f2a195de0e43b` found 32
sensitive function bodies unchanged, including printing/storage submission, cancellation,
plate preparation, AMS mapping, readiness enablement, keyboard selection, and existing
physical-action confirmation branches. This is source preservation evidence, not proof
that a native interaction ran.

## Remaining nested surfaces

These existing continuations retain their current implementations in this unit:

| Family | Nested surfaces still requiring their own visual/runtime review |
| --- | --- |
| Material and nozzle selection | `AmsMapingPopup`, `AmsMapingTipPopup`, `AmsTutorialPopup`, `AmsReplaceMaterialDialog`, `ReselectMachineDialog` |
| Final print decisions | `ConfirmBeforeSendDialog`, the local timelapse storage chooser and storage-warning dialog, nozzle/filament `MessageDialog` warnings |
| Connection and account | `ConnectPrinterDialog`, `InputIpAddressDialog`, `BindMachineDialog`, `UnBindMachineDialog`, login `SecondaryCheckDialog` |
| Device management | `EditDevNameDialog`, PIN-code and direct-connection rows |
| Transfer recovery | `SendFailedConfirm`, rename/transfer `MessageDialog` prompts, the existing status-bar progress/cancel content |

The inventory is based on source calls and owned popup members. Runtime reachability,
all nested layout states, keyboard traversal through the new card containers, and
accessibility output remain unverified. It is not a claim that the nested surfaces were
redesigned or that the entire application now matches Studio Atlas.

## Verification and privacy

The focused check set passes ten tests. The production C++17 body clamp passes 106
assertions covering short displays, 100/125/150/200% scaling, natural content size,
progress/diagnostic reservations, nonpositive space, and integer boundaries. Ignoring the
available display height makes its negative regression fail. The existing Print state
model separately passes 30 behavioral assertions, including busy/stale output rejection.
Dialog-header include and caption-title checks also pass.

No full native build, application launch, installer execution, physical command, or
screenshot was performed. English, Cantonese and bilingual layouts, both themes, normal
and minimum supported viewports, all four scales, cancellation during transfer, and final
confirmation interaction still need built-artifact verification. Storage transfer remains
on its existing shared dialog shell; this unit does not claim a new full-body scrolling
implementation for that dialog. No new data flow or persisted preference is introduced.
Future screenshots must use isolated nonprivate device/project names and never expose
access codes or account data.
