# Native interface motion inventory

This handoff covers the requested interface-wide animation and transition work.
Material Designer creation/export tools are unavailable in the current tool
inventory, and local application execution is prohibited for this task. Reuse
the native framework and checked-in `prepare-sidebar-tabs.md` and
`native-print-workflows.md`. No prototype or source result substitutes for hosted
runtime evidence.

| Slice | Existing implementation | Remaining transitions and states |
| --- | --- | --- |
| Shared policy/lifetime | Saved system/reduced preference, interruptible weak-owner Anim, hidden settlement and destroyed-owner cancellation; hosted pure 59-outcome policy/paint contract passed | Preference persistence and actual installed surface behavior |
| Native state and tabs | StateHandler color interpolation, Button/StaticBox paint routing, TabStrip underline/pill, Notebook/TabButton selection feedback, SwitchButton knob | Actual page-content transitions, temporal contrast, interruption, theme/DPI and installed lifetime evidence |
| Choices | CheckBox decorative selection emphasis and Slider focus/drag halo, with immediate values and input geometry | Installed raster cost, rapid input, capture loss, hidden/reduced settlement and genuine temporal evidence |
| Panels/dialogs/tooltips | Owned captioned-dialog, CommandPalette and ParamTooltip entrances; filament/monitor disclosure rails; RegexBuilderPopup owns its entrance | Full panel-content motion, repeat/reverse/hide/destruction evidence and composited native opacity capability |
| Assembly export frame | Separate reviewed source candidate `92813f71ef3809442bdceb32ca92f54187462a0f` owns reveal/hide/destruction through MD3TransientEntrance | Candidate compilation and real export fixture, no-activation, cancellation and composited-opacity verification |
| ImGui menus and states | Root/modal/nearest-popup child draw lists share bounded context/ID entrance timelines; native MD3Menu owns entrance and decorative hover/filter feedback with full-contrast semantic text | Canvas tooltip entrance, canvas hover/filter feedback, genuine temporal evidence and runtime state bounds |
| Notifications/loading | NotificationManager and ProgressBar now consult shared reduced/hidden behavior; existing FanControl/CameraHUD activity signals remain | Installed completion/cancellation, idle scheduling and full reduced-motion coverage of remaining activity indicators |

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

## Evidence and reachability boundaries

The source inventory above is grounded in integrated revision
`c7aefb4b84f31a4bc2519d8052ac78b3ccaa3a17`, with the separately reviewed
`9aa64b75bf327d4268782df6e45fb0d9a10cb359` correction removing SearchField's
duplicate external RegexBuilderPopup fade. The assembly frame candidate is
separate and is not claimed integrated by this document.

Hosted run [37100814236](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/37100814236)
passed 14 real wx event-loop cases with 95 assertions for the fixture at
`a266dbbf53096ca41dcaeda6533f4e57ddcbb2d1`. This is actual animator lifetime
evidence, not installed full-GUI or rendered-motion coverage. Hosted run
[37101358501](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/37101358501)
at `c7aefb4b84f31a4bc2519d8052ac78b3ccaa3a17` passed the 59-outcome pure
policy/paint contract; its log states `Motion policy checks passed: 59` and
records 7,712 validated translations. These results do not establish product
pixels. No universal pixel verdict exists. Source review, pure arithmetic,
event-loop assertions and installed temporal pixels are distinct evidence layers.

`MarkdownTip::ShowTip` returns false under `NDEBUG`; no external ShowTip/AttachTo/
DetachFrom caller exists in the inspected release source. Its remaining Reload,
Recreate and ExitTip calls do not create the singleton. This legacy surface is
not applicable to the currently reachable release, not an implemented entrance.
See [content ownership](../docs/features/windows/content-motion.md).

## Concrete remaining implementation scopes

- Page content: `Notebook.hpp::DoShowPage` and `Tabbook.hpp::DoShowPage` retain
  NONE/zero native effects. MainFrame Home/Device/Project/Calibration, Monitor
  Status/Storage/Firmware and Auxiliary project pages are actual callers.
  Select one paint-owned panel or loaded web document for a bounded transition;
  selected-tab feedback is not page-content motion. Heavy canvas composition
  requires a separate renderer-owned design.
- Canvas tooltips: `ImGuiWrapper::tooltip` and `IMSlider::show_tooltip` use
  independent BeginTooltip windows, outside the current popup-ancestor timeline.
  Preview simulation controls reach the latter. Use a separate bounded tooltip
  identity and preserve readable semantic text.
- Canvas hover/filter: `menu_search` updates visibility and selectable paint
  changes hover color immediately. Add only bounded item-owned decorative
  feedback; never delay results, callbacks or move hitboxes.
- Busy notice responsiveness: `BusyInfo` in MsgDialog.cpp forces a paint before
  callers in Plater's replace/reload paths block the event loop. A timer alone
  cannot animate this interval. Nonblocking work ownership is a separate scope,
  not a reason to claim a cosmetic timer as a completed transition.

Immediate selection, cancellation, validation and dismissal semantics are
intentional. Reduced motion intentionally settles visuals without intermediate
movement. Neither exemption excuses missing full-motion feedback elsewhere.
All remaining installed and rendered evidence stays explicitly pending.
