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

## Graphite shell motion specification

This section specifies the motion drawn into the Graphite shell artboards (`graphite-shell/*.dc.html`). It is a design specification: the artboards prove timing, easing and sequencing in a browser, and the native kit has to implement each row on its own animator. Nothing here claims that a native control already behaves this way; the native status of each slice stays in the tables above.

### Tokens

Durations and easing come from the Material Design 3 motion tokens. The artboards define them as CSS custom properties (`--d2` to `--d12`, `--e-std`, `--e-dec`, `--e-acc`, `--e-emp-dec`, `--e-emp-acc`, `--e-spring`) and the Tokens artboard plays each one on hover.

| Token | Value | Use |
| --- | --- | --- |
| short2 | 100 ms | press feedback, hover state layer on small controls |
| short3 | 150 ms | state layer fade, scrim and toast exit |
| short4 | 200 ms | colour and elevation changes, scrim enter |
| medium2 | 300 ms | selection, card lift, switch knob, screen shared-axis entrance, dialog entrance |
| medium4 | 400 ms | segmented thumb, rail indicator, staggered content entrance |
| long2 | 500 ms | reserved for large surface transitions |
| long4 | 600 ms | progress bar growth on first display |
| standard | cubic-bezier(.2, 0, 0, 1) | state and colour changes, element moves that stay on screen |
| standard decelerate | cubic-bezier(0, 0, 0, 1) | elements entering at full size |
| standard accelerate | cubic-bezier(.3, 0, 1, 1) | elements leaving the screen |
| emphasized decelerate | cubic-bezier(.05, .7, .1, 1) | entrances: screens, dialogs, toasts, staggered cards |
| emphasized accelerate | cubic-bezier(.3, 0, .8, .15) | exits of dialogs and the palette |
| expressive spring | cubic-bezier(.34, 1.45, .64, 1) | position, size and shape only: rail indicator, segmented thumb, switch knob, card lift, button shape morph |

The spring is an overshoot approximation of the Expressive spatial spring. It is never applied to colour or opacity, and a native implementation may replace it with a damped spring that settles within the same duration.

### Interaction map

Every row has an enter or change definition, an exit where one applies, the native control it maps to, and the reduced-motion equivalent. The reduced-motion equivalent applies when the saved preference or the system setting asks for reduced motion: the state changes immediately, with no travel, scale or shimmer, and focus rings and selection stay fully visible.

| Interaction | Motion | Duration and easing | Native control | Reduced motion |
| --- | --- | --- | --- | --- |
| Rail selection indicator | The mint pill slides from the previous destination to the new one; the destination label colour cross-fades | 400 ms spring (position), 200 ms standard (label) | `ButtonsListCtrl` and `Notebook` selected tile | Pill jumps; label colour changes at once |
| Rail tile hover and press | 8 % state layer on hover; pill scales to 92 % while pressed | 100 ms standard, 300 ms spring on release | `ButtonsListCtrl` tile paint | State layer only, no scale |
| Screen change between workspaces | Shared axis on the horizontal axis: the incoming screen fades in while travelling 28 px toward its resting position; the outgoing screen fades out in 90 ms without travel | 300 ms emphasized decelerate in, 90 ms standard accelerate out | `Notebook::DoShowPage` and `Tabbook::DoShowPage` page content (not implemented natively today, see the page content scope above) | Cross-fade replaced by an immediate swap |
| Staggered content entrance | Cards and rows rise 14 px and fade in, 40 ms apart, up to ten items, the rest at the end of the sequence | 400 ms emphasized decelerate, 40 ms step | Home web page cards (CSS in `resources/web/homepage3`), Prepare sidebar cards on first show | All items shown at once |
| Card hover and press | Card lifts 2 px with a deeper shadow and moves to the next surface step; press returns it and scales to 98.5 % | 300 ms spring, 100 ms on press | `StaticBox` and card paint routing | Surface colour change only |
| Button shape morph | A pill button changes its corner radius to 16 px on hover and 14 px while pressed, with a 12 % state layer | 200 ms standard | `Button` and `SideButton` paint | Radius stays; state layer only |
| Segmented control | The selected thumb slides under the labels; label colour fades | 400 ms spring, 200 ms standard | `TabStrip` pill, language switch, World or Object toggle, print speed levels | Thumb jumps |
| Switch | Knob slides 20 px, grows from 16 to 20 px and the track fills; the knob widens to 24 px while pressed | 300 ms spring, 150 ms size | `SwitchButton` | Knob jumps; track colour changes |
| Chip and toggle state change | Fill and label colour cross-fade; check glyph swaps in the Preview option chips | 200 ms standard | `Button` chip states and the Preview option toggles | Immediate |
| Dialog enter | Scrim fades in; the dialog scales from 90 % and rises 12 px while fading in | 200 ms standard (scrim), 300 ms emphasized decelerate (dialog) | Captioned dialog entrance (`MD3TransientEntrance`): Send print job, Melody creator | Dialog appears at once |
| Dialog exit | The dialog scales to 95 % and fades; the scrim fades out | 150 ms emphasized accelerate | Same owner | Immediate |
| Command palette open | Scales from 94 % and fades in from the title bar search field; list rows follow with a 20 ms step for the first six | 300 ms emphasized decelerate | `CommandPalette` owned entrance | Immediate |
| Command row hover | 5 % state layer | 150 ms standard | Palette row paint | Colour only |
| Printing progress | The bar grows from zero on first display; while a job is running a soft highlight travels along the filled part every 1.8 s | 600 ms emphasized decelerate, 1.8 s loop | `StatusPanel` progress, `ProgressBar` | No growth and no travelling highlight; the filled width shows the value |
| Live camera indicator | The red dot fades between full and 35 % opacity every 2 s while the stream is live | 2 s standard loop | `StatusPanel` camera badge | Static dot |
| Layer slider scrubbing | The handle halo grows on hover; the layer value bubble follows the handle without lag; the current layer outline nudges up 3 px when the layer changes | 150 ms standard (halo and bubble), 400 ms standard (nudge) | `IMSlider` (ImGui), see the canvas tooltip scope above | Halo and bubble only; no nudge |
| Legend item hover | The colour swatch scales to 125 % | 300 ms spring | Preview legend (`GCodeRenderer`) | No scale |
| Toast enter | Rises 28 px from below and fades in; leaves by fading out in 150 ms | 300 ms emphasized decelerate, 500 ms delay after the screen settles in the artboards | `NotificationManager` | Immediate |
| Badge enter | Notification and LAN badges pop in from 90 % | 300 ms spring | `BBLTopbar` bell and LAN indicator bitmaps | Immediate |
| Slot and tube hover | AMS tubes and project thumbnails move up 3 px and scale 5 % or round their corners | 300 ms spring | AMS panel (web or native), Home project tiles | No movement |
| Fan and temperature sliders | Handle halo on hover | 200 ms standard | Device fan and temperature controls | Halo only |

### Behaviour rules

- Semantic state changes stay immediate: selection, cancellation, validation and dismissal take effect when the user acts, and the visual transition follows.
- Hit targets never move with a decorative transform; lift and scale effects animate the paint, not the input rectangle.
- Only real activity loops: the progress highlight while a job runs and the live camera dot while a stream is live. Idle surfaces do not animate continuously.
- Entrance sequences play once when a screen is shown and are not replayed on every redraw.
- Every animation has a reduced-motion equivalent, which is a state change with no intermediate frames, and the static frames in `graphite-shell/frames/` are what that mode shows.
- Entrances are capped at 600 ms and staggers at ten items so screens never feel slower than the content loads.

### Artboard reference

The artboards carry the motion as shared CSS classes. They are the reference for timing and sequencing, not shipped code.

| Class | Purpose |
| --- | --- |
| `.screen` | Shared-axis entrance of a workspace |
| `.st` | Staggered entrance of the children |
| `.lift`, `.sl` | Card lift and state layers |
| `.seg`, `.sw` | Segmented control thumb and switch |
| `.prog` | Progress growth and highlight |
| `.dlg`, `.scrim`, `.toast` | Dialog, scrim and toast entrances |
| `.ind` (NavRail) | Selection indicator slide, driven by the `active` property |

The `prefers-reduced-motion: reduce` block at the end of the shared CSS collapses every duration, removes the highlight travel and keeps the final state visible. It was checked by rendering the artboards with reduced motion emulated, and the result matches the full-motion frame at rest. The dialog exit, the outgoing screen fade and the palette row stagger are specified here but are not drawn in the static artboards, because the artboards only show enter states.
