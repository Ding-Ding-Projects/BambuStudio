# Animation and transition

The appearance editor's fifth tab, **Animation and transition**, edits how the
exact element the editor is anchored to moves when its state changes, and how
the window that contains it fades in. Open the editor with **Edit
appearance...** in an element's context menu, Shift+right-click, or
Ctrl+Shift+E on the focused control, then choose the tab or press Ctrl+Page
Down until it is selected.

## The global motion setting decides first

**Preferences > Interface motion** and the Windows client-area animation
setting stay in charge. When either asks for reduced motion, every transition
and entrance shows its final state at once, and so does the tab's preview.
Values set here are kept and play again when motion is allowed. The top of the
tab states which case applies and updates within a second of a change.

This app has no animated rainbow colour and no global speed level, so nothing
here competes with one.

## State transitions

Buttons, fields, lists and cards animate their state colours (background,
border and text together) when their state changes. Choose the change under
**Trigger**:

| Trigger | When it plays |
| --- | --- |
| Hover | The pointer enters or leaves the element |
| Focus | The element gains or loses keyboard focus |
| Press | A pointer button goes down or up on the element |
| Selection | A toggle button or check is turned on or off |
| Enable and disable | The element becomes available or unavailable |

When several states change in one event, press wins, then selection, focus,
hover and enable. Each trigger has three values:

| Value | Range | Unset |
| --- | --- | --- |
| Duration | 0 to 10000 ms; 16 ms or less shows the final state at once | 100 ms, the Material short 2 duration |
| Delay | -10000 to 10000 ms; a negative delay starts part of the way through | 0 ms |
| Easing | A named curve, `cubic-bezier()`, `steps()` or `linear()` | Material standard |

During a positive delay the element keeps its starting colour. Every
transition plays once and ends exactly on the new state, whatever the curve
does on the way. A new state change interrupts a running transition and starts
from the colour on screen. Nothing waits for a transition: state, focus and
input change at once.

Each value shows where it comes from: set for this element, inherited from a
parent element (the part of the id before a slash) or the active preset, or
the Material default. Each has its own reset button.

## Easing

The **Easing** list holds the Material curves (standard and emphasized, each
with accelerate and decelerate forms), linear, ease, ease in, ease out and
ease in and out. **Custom** moves to the **Easing function** field, where you
type any of these and press Enter:

| Form | Example | Rules |
| --- | --- | --- |
| Cubic Bezier | `cubic-bezier(0.2, 0, 0, 1)` | x1 and x2 from 0 to 1; y1 and y2 from -5 to 5 |
| Steps | `steps(4, jump-end)` | 1 to 100 steps; jump-start, jump-end, jump-none (2 or more steps) or jump-both |
| Linear stops | `linear(0, 0.25 40%, 1)` | 2 to 64 stops; each a number with up to two percentages |

Text that does not parse is refused with the reason, and the element keeps its
current easing. The field always shows the canonical form of what is stored.
Dragging Bezier handles is not available yet; the curve preview redraws as
soon as the numbers change.

## Preview

The preview draws the selected transition (solid) beside the Material default
(dashed) against a time ruler, shades the delay, and shows the colour at the
playhead in a strip below. **Play**, **Play reversed** and **Stop** use the
same arithmetic the element plays; playing again restarts. **Preview speed**
slows the preview to half or quarter speed without changing the element.
**Preview position** seeks to any millisecond. The written summary under each
timing states the total active time, so the preview is never the only
description.

## Window entrance

Dialogs with the Material caption, the command palette, the regex builder,
menus and this editor fade in from a quarter of full opacity when they open.
When the window that contains the element has such an entrance and is an
adopted element, **Window entrance** edits its duration, delay and easing.
Otherwise the section says why there is nothing to edit. The editor's own
opening fade is the entrance of the element `appearance-editor`.

## Changes, history, presets and files

- **Undo** and **Redo** step through the motion edits made in this editor,
  across elements; their tooltips name what they will change.
- **Reset element motion** drops every motion value of this element, known or
  not, in one undoable action. **Reset element** and **Reset all** at the
  bottom of the editor still reset everything.
- Every change is saved at once with the other element styles and recorded in
  **Version history**; undo, redo and reset are named entries there.
- **Save as preset** keeps motion values, and a preset's statement names each
  motion value it sets.
- Exported themes carry motion values. Import checks them: an invalid value is
  listed in the import result and on the tab under **Kept but not applied**,
  and the Material default plays instead. A motion setting this version does
  not know is kept in the file and listed there too.

Motion values are stored as `motion.<trigger>.<field>` properties of the
element, where the trigger is hover, focus, press, selection, enable or
entrance and the field is duration, delay or easing.

## Renderer capabilities

The bottom of the tab lists every motion capability of the appearance
contract with what this renderer does with it; search it in plain text or
with the regex builder. Unavailable capabilities stay listed as disabled
entries with the exact reason, among them layers, tracks, keyframes and
markers, motion paths, per-property transition lists, transforms and control
opacity, repeats, direction and fill modes, playback rate, springs, document
and scroll timelines, sequencing and stagger, exit animations, validation,
loading, success, warning and error states, copy and paste style, scheduling
and command palette teleport.

The hand-written matrix behind the list is
[`design/native-appearance/motion-capability-inventory.json`](../../../design/native-appearance/motion-capability-inventory.json).
Each wired row names the source that plays it; a source contract keeps the
matrix and the tab's list identical.

## Verification

Pure tests cover the easing grammar and arithmetic, timing with positive,
negative and zero delays, validation, resolution through the style layers,
the save and load round trip, undo and redo, and the capability table. The
style registry tests cover parent and preset inheritance, import reports and
the live resolution the widgets call. Source contracts check that the tab
exists, that every inventory row matches the tab, that each wired row's
consumer is present and that removing any of them is detected.

Not yet verified: the built Windows application. A timed recording or sampled
frames must still show each trigger, duration, delay, curve, entrance,
interruption and reduced-motion case on real controls, with per-click
captures of the tab in English, Cantonese and bilingual modes, both themes and
every display scale.
