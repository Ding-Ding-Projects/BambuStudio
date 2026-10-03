# Native interface motion inventory

This handoff covers the requested interface-wide animation and transition work.
Material Designer creation/export tools are unavailable in the current tool
inventory, and local application execution is prohibited for this task. Reuse
the native framework and checked-in `prepare-sidebar-tabs.md` and
`native-print-workflows.md`. No prototype or source result substitutes for hosted
runtime evidence.

| Slice | Existing implementation | Remaining transitions and states |
| --- | --- | --- |
| Shared policy/lifetime | MD3Motion durations, easing, interruptible Anim and FadeIn; saved system/reduced preference in this unit | Hosted policy execution, restart persistence, live interruption, hidden/destroyed owners |
| Native state and tabs | Button/StaticBox registered state colors; horizontal TabStrip underline and vertical selection pill; SwitchButton knob | Hosted temporal, destruction, reduced/hidden and layout proof; unconverted controls, Tabbook/Notebook and tab content changes |
| Panels/dialogs/tooltips | MD3DialogChrome and appearance/search popovers have entrances | Repeated show, dismissal, panel expansion/collapse, ParamTooltip and MarkdownTip transitions |
| ImGui menus and states | Shared menu search and popup layout; native MD3Menu already has entrance fades | Root/nested ImGui entrances/dismissal, result changes, hover/focus/press; owner- and ID-scoped state |
| Notifications/loading | NotificationManager fades, ProgressBar pulses, FanControl/CameraHUD animate | Shared reduced-motion policy, progress changes, loading completion/cancellation and idle scheduling |

Semantic state changes remain immediate: disabled controls stop accepting input,
selection and cancellation take effect, and destruction releases ownership
without waiting for decorative motion. Visual state follows those changes.
Do not animate hit targets away from their input geometry or fade arbitrary
child HWNDs/OpenGL panels. New callbacks need weak ownership, cancellation and
generation checks. Do not add continuous movement to otherwise idle surfaces.
Fan telemetry and actual loading activity retain their meaningful state signals.

Each slice needs an explicit start/interruption/end state and a reduced-motion
equivalent. Record its normal and minimum geometry, language/theme/actual-scale
tuple, focus return, final opacity and owner identity. Transitions require a
genuine temporal sequence or clip, input/state receipts and exact executable
hash, not just a final still. Cover rapid reversal, repeated opening, destruction
mid-transition, hidden/minimized state, theme/DPI changes and system/application
reduced-motion changes. Stop frame scheduling after convergence.

All five slices remain incomplete until their hosted evidence is reviewed.
Source now includes the shared preference/owner-policy foundation and bounded native state-color/tab-indicator slice. Neither source coverage nor pure arithmetic checks establish rendered motion coverage.
