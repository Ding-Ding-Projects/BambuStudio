# Print simulation playback (feedrate-true)

The Preview tab's horizontal moves timeline plays back the sliced print as a
simulation whose pace follows the real, feedrate-derived print clock instead of
the old fixed six-second sweep.

## Behavior

- **Feedrate-true clock.** Every moves-slider tick is mapped to the cumulative
  print time of its G-code move (`MoveVertex::time`, the prefix sum computed by
  the G-code processor during finalize). Only moves that own a time block carry
  a value, so the table is forward-filled. While playing, the simulated print
  clock `t` advances by `wall-clock dt x speed multiplier`; the shown move is the
  one executing at `t` (binary search over the cumulative table, O(log n) per
  frame). A one-hour print therefore takes one hour at 1x — use the speed chip.
- **Speed multipliers.** The speed chip in the right cluster of the transport
  bar cycles 1x → 10x → 100x → 1000x. The multiplier scales the simulated
  clock, so slow perimeters visibly crawl and rapid travels flash past at any
  setting.
- **Transport bar.** Play/pause (circular primary button), skip to start / skip
  to end, the speed chip, an `elapsed / total` readout of the simulated print
  clock, and the `Move N / M` counter. On narrow windows chrome sheds
  progressively — readout first, then the speed chip, then the skip buttons and
  the counter — so the scrub groove never overlaps what remains.
- **Scrubbing semantics.** The user always wins: dragging the handle, clicking
  the track, or using the skip buttons makes the slider index authoritative for
  that frame and re-anchors the simulated clock to the selected move's time.
  Playback (if running) resumes from the scrubbed position on the next frame.
  Reaching the end pauses the simulation (it does not stop); pressing play at
  the end restarts from the beginning.
- **Marker through layers.** While a simulation is running or paused, the
  nozzle marker (sequential view) stays visible even at positions where it
  would normally hide (for example with the slider at its maximum), so the
  virtual nozzle can be followed continuously through layer boundaries.
- **Renderer scope.** Both G-code renderers are supported. On the advanced
  renderer the moves timeline covers the current top layer (its ticks are
  per-layer segment indices); on the legacy renderer it covers the whole
  visible layer range.

## Legacy-renderer throttle

The legacy renderer rebuilds its render paths on every sequential-view seek.
When a fast-forward frame (typically 1000x) jumps more than about 5000 ticks,
the slider pushes the sequential-view update only every other frame; skipped
frames still repaint the canvas so the clock and readout stay smooth. The final
frame that pauses at the end is always pushed. The advanced renderer needs no
throttle.

## Reduced motion / accessibility

Playback never starts on its own: there is no autoplay, and a new slice, layer
change, or view change leaves the timeline stopped. Motion only ever begins
from the user pressing the play button, and pausing or scrubbing halts it
immediately, so the feature is inert for users who avoid ambient motion.

## Localization

The transport controls' user-visible strings ("Play print simulation", "Pause
print simulation", "Simulation speed") are translated in the English and Hong
Kong Cantonese catalogs like every other Preview string.

## Failure modes

- **No time data.** If the loaded result carries no usable move times (for
  example an empty visible-segment list on the advanced renderer), the speed
  chip and readout hide and the play button falls back to a plain six-second
  index sweep, so the transport never dead-ends.
- **Icon atlas unavailable.** Without the merged Material Symbols atlas the
  whole transport bar already falls back to the legacy groove + value chip;
  simulation controls are simply absent there, matching the pre-existing
  fallback behavior.

## Verification

Covered by a Windows build plus manual preview interaction: slice a plate,
open Preview, press play at each speed, scrub during playback, skip to both
ends, and shrink the window to confirm the shedding order. The i18n gate
(`compile_translation.py` then `--check` in `bbl/i18n/yue_HK`) validates the
new catalog entries.
