# Destructive-action super confirmation

**Surface:** `SuperConfirmGate` (`src/slic3r/GUI/Widgets/SuperConfirmGate.{hpp,cpp}`), driven by
the toolkit-free state machine `SuperConfirmState.hpp`. It replaces the Yes/No message boxes that
used to stand in front of the application's genuinely irreversible actions.

## Behavior

The gate is a borderless Material 3 surface (SurfaceContainerHigh, MD3 caption strip, DWM rounded
corners) that opens **anchored beside the control that asked for the destructive action**: below it
when there is room, otherwise above, right or left, always clamped to the display and never covering
the anchor. When the anchor is a whole panel (the 3D view, the plater) the surface centres over it
instead of hanging off an edge. With no anchor at all it falls back to an honest centred modal
dialog.

From top to bottom it shows:

1. **The safety facts** - the caller's exact consequence sentence in `Head_16` OnSurface
   ("Every object on every plate will be removed from this project."), the affected item names as a
   bulleted list (the first eight, then "... and N more"), and an Error-coloured count line
   ("7 items will be affected. This cannot be undone."). These strings go through `_L()` so the
   language mode and funny level style the *voice*; the action, the items and the count are never
   softened, reworded by humour, or hidden.
2. **A stage caption** narrating what is left: "Step 1 of 2: turn both keys." /
   "Step 2 of 2: slide all the way to the end." / "Keep sliding... release early to back out."
3. **Two key switches** - painted rotary keys (vertical slot = off, horizontal = on,
   PrimaryContainer fill when on). Each is its own focusable control operated by click, `Space` or
   `Enter`, announced to assistive technology as a check button named "Key 1 of 2: turn to arm" /
   "Key 2 of 2: turn to arm" with a live checked state. Turning the second key on hands keyboard
   focus to the slider; turning either key off drops the slider back to rest.
4. **A hazard charge bar** - fills with ErrorContainer as the ritual progresses (a quarter per key,
   the remaining half tracking the slide) with Error hazard stripes that crawl while the knob is
   travelling. Under reduced motion (`SPI_GETCLIENTAREAANIMATION` off) every animation jumps to its
   end state; the stripe crawl never starts.
5. **The full-range `SlideToConfirm`** - disabled until both keys are on. Drag the knob to the end,
   or walk it with the arrow keys and `End`. A partial slide snaps back; travel alone never
   authorizes (the state machine caps live travel at 99 percent and only the slider's own completion
   event reaches 100).
6. **Emergency exit** - a tonal button that is always enabled, holds the initial focus (so `Enter`
   can never confirm), and cancels. `Escape` and closing the window do the same; clicking outside an
   anchored gate dismisses it as a cancel.

On authorization a distinct completion burst plays (PrimaryContainer floods the bar and a Primary
disc with a check glyph grows from the centre, `medium2`, emphasized easing), then the gate hides,
**returns focus to the control that had it when the gate opened** (falling back to the anchor) and
only then reports success. Cancel restores focus the same way.

The callback is fired only when `SuperConfirm::State::may_fire()` is true - both keys on, slider at
100, not cancelled. A click, a key press, a close, or a caller passing `true` cannot bypass that
predicate.

## API

```cpp
SuperConfirmGate::Spec spec;
spec.action      = _L("Delete plate");
spec.consequence = wxString::Format(_L("Plate %d and every object on it will be removed from this project."), n);
spec.affected    = { "Benchy", "Calibration cube" };   // names; spec.affected_count overrides the total
if (SuperConfirmGate::Run(anchor_window, spec))      // blocking: nested loop (anchored) or modal (no anchor)
    do_the_irreversible_thing();

SuperConfirmGate::Show(anchor_window, spec,          // callback form: non-modal, self-destroying
                       [] { do_the_irreversible_thing(); },
                       [] { /* cancelled */ });
```

## Gated actions

| Action | Call site | Anchor | Notes |
| --- | --- | --- | --- |
| Delete all objects (File / plater "Delete all") | `Plater::reset_with_confirm` (`Plater.cpp`) | plater panel (centred) | Lists every object name. |
| Delete preset | `Tab::delete_preset` (`Tab.cpp`) | preset combo box | Keeps the original explanatory text as the consequence; the third-party-printer auto-confirm path is unchanged. |
| Delete filament slot | `Sidebar::delete_filament_with_confirm` (`Plater.cpp`), used by the per-row filament menu (`GUI_Factories.cpp`) and the paint-gizmo delete button (`EVT_DEL_FILAMENT`) | the slot's combo box | Names the slot number and preset. Bulk filament actions and merge keep their own confirmation and call `delete_filament()` directly, so a batch is confirmed once, not per slot. |
| Delete plate (plate toolbar and plate hover action) | `Plater::confirm_delete_plate` (`Plater.cpp`) | 3D view (centred) | Only when the plate carries objects; an empty plate loses nothing and is deleted without a gate. |
| Stop print | `StopPrintGateDialog` (`StopPrintGate.cpp`) | modal | Pre-existing interlock with the same anatomy (two keys, arming buttons, slide, cover); see [Stop-print safety interlock](stop-print-interlock.md). |
| Export / import the whole data folder | `ConfigProfilesDialog` | in-dialog | Pre-existing inline `SlideToConfirm` arming; not yet migrated to the two-key gate (see Failure modes). |

**Deliberately not gated:** deleting objects, parts or instances from the object list / 3D scene,
and restoring a project-history version. Both are undoable - object deletion takes an undo/redo
snapshot and a restore is recorded as a new history version - so a super confirmation there would
be friction without protection. Object-list deletion never asked a Yes/No before either.

## Configuration

None. The gate has no opt-out and no persisted state; every opening starts from untouched.
Reduced motion is read from the OS on each animation. Copy follows the active language mode and
per-language funny level through the shared `_L()` catalogue.

## Failure modes

- **Slider completes while a key flips in the same instant** - the widget asks the state machine to
  set 100; if it refuses, the slider is reset and the gate stays armed/disarmed as the keys say.
- **Anchor destroyed while open** - the anchor and return-focus targets are `wxWeakRef`s; focus
  restoration is skipped rather than dereferencing a dead window.
- **Deactivation** - an anchored gate cancels itself when it loses activation, so it can never be
  left hanging behind the main window with a half-armed slide.
- **Reduced motion** - `MD3::Motion::Anim::Play` runs `tick(1.0)` and `done()` synchronously; the
  completion burst therefore finishes (and fires the callback) inside the slider's completion
  handler. The stripe crawl is guarded against restarting itself under that mode.
- **Config profiles export/import** still uses the inline slider-only gate from before this change;
  it is listed above so the gap is a recorded decision, not an oversight.

## Security considerations

- The gate is a user-experience interlock, not an authorization boundary: it protects against slips,
  not against code that calls the underlying operation directly.
- Nothing about the action, the items, or the count is derived from remote data; the caller
  supplies them and they are rendered as plain text.
- `Run()` blocks on a nested `wxGUIEventLoop`; the parent window stays enabled (popover semantics)
  but the gate cancels on deactivation, so two gates cannot stack from the same anchor.

## Verification

- `tests/super_confirm/super_confirm_tests.cpp` (Catch2, target `super_confirm_tests`) covers the
  pure state machine: untouched, one key only, both keys, partial slider, full slider, clamping,
  disarming mid-slide, cancel, Escape before and after authorization, key order, and forged fields.
  Built and run by hand on 2026-09-08: **87 assertions in 10 test cases, all passed**.
- `SuperConfirmGate.cpp`, `SlideToConfirm.cpp` and `Plater.cpp` pass `cl /Zs` with the GUI module's
  include and define set. `Tab.cpp` and `GUI_Factories.cpp` show only errors that the untouched
  files also show under that helper (`TreeView_*` macros, boost `CP_ACP`) - the edited regions
  compile.
- The visual behaviour (anchoring, animation, focus return, screen-reader names) is exercised by
  construction and code review; a hidden-desktop capture of each gated site is recorded as pending
  until the next full build.

## Suggested articles

- [Stop-print safety interlock](stop-print-interlock.md) - the original two-key interlock this gate
  generalizes.
- [Keyboard, assistive, and responsive GUI accessibility](gui-accessibility.md) - the focus,
  naming and reduced-motion rules the gate follows.
- [Native Material Design 3 UI](md3-native-ui.md) - tokens, motion and dialog chrome.
- [English, Hong Kong Cantonese, and bilingual modes](language-modes.md) - how the gate's copy is
  styled without changing its facts.
