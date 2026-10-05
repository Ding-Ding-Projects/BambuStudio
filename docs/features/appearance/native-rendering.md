# Native appearance rendering

The element registry now exposes `ElementStyle::metrics_for(id, baseline)` as the shared typed adapter for radius, border width, padding, margin, letter spacing and line height. It resolves inherited values before applying finite bounds. Geometry uses logical pixels; owner-drawn controls convert through `FromDIP`. `StyleMetrics::radius_for`, `inset_for` and `line_advance` provide bounded layout calculations.

The generic wxWindow adopter applies margin to its containing sizer item, preserving the original border and layout flags. Reset restores those original values. An element outside a sizer cannot apply this property. Font and foreground/background behavior remain the existing native implementations.

The appearance editor's shape page renders a real preview of radius, border width/color, padding, margin, font, foreground/background, and line height. Its label retains native shaping. Preview content is constrained to its sample box. This preview demonstrates the renderer adapter, not universal adoption by other controls.

## Integration points

- Owner-drawn widgets call `metrics_for(ElementStyle::element_id_of(this), existing_baseline)` in both measurement and paint, so padding cannot change only the paint while the hit target remains stale.
- Use `colour_for(id, StyleProp::border_color, existing_border)` for outlines. Radius and stroke stay inside the control's bounds.
- The command palette can enumerate `StyleProp::all()` and open `AppearanceEditorPopover::open_for(anchor, id)`. Registration must retain the exact live target rather than a cached raw pointer.
- New strings require catalog extraction and Cantonese/bilingual review in the integration lane.

## Capability limits

Native wxFont covers family, size, numeric weight, italic, single underline and single strikethrough. Per-glyph letter spacing is retained but not applied to native labels; splitting Unicode strings into code points would break shaping and is intentionally not substituted for a proper text layout engine.

Variable axes, oblique angle, styled/colored underline, double strike, overline, small caps, superscript/subscript, text outline/shadow/glow, word spacing, baseline shift, direction and paragraph alignment need a rich-text adapter. The editor names those unavailable capabilities. They are not silently represented as implemented. The complete layered image editor and state-specific appearance renderer remain unfinished.

## Verification

`style_metrics_tests` is a dependency-free C++17 executable registered in the appearance CMake directory. It checks clamping, small/negative bounds, line height and nonfinite input. On 2026-10-05, compiling and executing it with GCC returned 0. Removing clamping in an isolated temporary header caused the invariant executable to abort with a nonzero exit. Production source was never mutated for this check.

The Catch appearance suite adds inherited metric resolution and reset checks. It has not been run in this lane because the full MSVC/wxWidgets build is unavailable. No packaged interaction, screenshot, accessibility or layout-matrix success is claimed. The parent integration must build the real application, verify margin reset and each adopted widget, then capture normal/minimum viewport, both themes, all three language modes and 100/125/150/200 percent scale.
