# Checkbox and slider motion

The shared custom checkbox and slider now own short decorative emphasis through
the existing weak-owner `MD3::Motion::Anim`. This is source implementation;
hosted compilation and rendered temporal verification remain pending.

## Reachable controls

Filament Setting Overrides creates the custom `CheckBox` through
`TabFilament::add_filament_overrides_page` in `Tab.cpp`. Its native checked value,
indeterminate state and toggle event remain immediate. Selection changes render
the final check/bar and outline without fading them. A separate outer edge,
inside the existing bitmap footprint, decays over `short2` (100 ms). The existing
keyboard-focus ring remains immediate and is painted at its original strength.
The disabled bitmap has no decorative emphasis. If graphics-context creation
fails, rendering retains the original glyph.

Actual shared-slider callers include the fan controls in `StatusPanel.cpp` and
the alpha control in `MD3ColorPicker.cpp`. Generic field construction alone is
not used as proof that a particular settings field reaches this implementation.
Focus acquisition and drag start add a short emphasis pulse to the existing
halo, which remains immediately visible at its original resting strength. The
additional alpha decays from 24 to zero over 100 ms. Focus loss, release and
capture loss settle immediately. Thumb position, track geometry, pointer
mapping, value callbacks and accessibility announcements do not wait for motion.
This does not add pointer lag or an animated value filter.

## Ownership and interruption

Each control owns one animator and stops it before base-window destruction.
Starting another emphasis cancels the previous generation. There is no detached
timer, global show hook, completion action, child-window transparency or delayed
semantic state change. Hidden owners settle through the shared owner policy;
their own hide events also stop decorative work. Reduced motion renders the
final state and settles active work. Disabled controls do not retain emphasis.

Checkbox theme changes and rescaling stop the pulse before rebuilding cached
bitmaps. Slider rescaling, size and scheme changes settle its pulse; paint also
detects changed live accent/background colors and settles before drawing.
Current theme colors are used rather than interpolating stale color snapshots.
All decoration stays within existing control geometry. This is control feedback,
not panel or page-content animation.

## Required hosted verification

No local compilation, tests or UI execution were performed. On the exact built
candidate, verify real Filament Setting Overrides, a fan slider without sending
hardware commands, and the color-picker alpha slider. Record native state and
callbacks alongside genuine temporal frames for:

- Checked, unchecked and indeterminate changes, including rapid reversal and
  programmatic replacement, with unchanged immediate glyphs and focus ring.
- Keyboard focus, mouse drag, release and capture loss, proving values and thumb
  geometry track input immediately and the resting halo is never delayed.
- Hide/reopen, destruction during motion, disabled state, and reduced-motion
  changes before and during a pulse, with no callbacks after destruction.
- Theme, accent and DPI changes, preserving final colors and bitmap dimensions.
- English, Cantonese and bilingual modes, light/dark themes, normal/minimum
  dimensions and measured 100/125/150/200 percent display scales.

Bind evidence to the source commit, executable hash, actual control and input,
measured geometry, scale and privacy verdict. A callback-only fixture cannot
prove glyph contrast, decorative-edge placement or rendered timing. The existing
shared animator runtime fixture covers its own lifetime contract only; it does
not replace these adapter-specific observations. No physical printing or printer
transfer is authorized by this verification description.
