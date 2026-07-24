# Stop-print safety interlock

**Surface:** Device ▸ print task ▸ Stop button → `StopPrintGateDialog`
(`src/slic3r/GUI/StopPrintGate.{hpp,cpp}`), replacing the previous Yes/No
confirmation.

## Behavior

Stopping a print now walks a launch-console interlock, in strict order:

1. **Turn both key switches** — painted rotary keys; a click rotates the key
   slot from vertical (off) to horizontal (on).
2. **Press each of the three arming buttons twice** — the button face counts
   down `Arm n (2)` → `Arm n (1)` → `Armed n`.
3. **Slide to confirm** — the shared `SlideToConfirm` gate (drag or
   keyboard: arrows advance, `End` completes) only enables once stages 1–2
   are done.
4. **Lift the safety cover** — a hazard-striped flap; clicking it after the
   slide reveals the actual **STOP PRINT** button. Only that button aborts
   the task (`command_task_abort`).

A stage caption ("Step n of 4: …") narrates exactly what is left, so the
flow is theatrical but never ambiguous — per the destructive-action copy
rules, every label states plainly that the print will be discarded.
"Keep printing" (or closing the dialog any other way) stops nothing.

## Considerations

- The interlock deliberately trades speed for certainty. It is keyboard
  reachable end to end (buttons and the slider are focusable; the cover
  opens on click after the slide).
- No state persists: reopening the dialog starts the interlock from zero.

## Verification

- Compiles into `libslic3r_gui`. The full flow needs a connected printer
  with an active task to reach the real Stop button, so end-to-end
  verification against hardware is recorded as pending; the dialog logic
  (stage gating, reset, cancel-stops-nothing) is exercised by construction.
