# Studio Atlas renderer surfaces

This source-only appearance update covers the viewport tool rail and floating toolbar, ImGui overlay styles and tooltips, Preview transport, and the header and statistics card in the Preview legend. It does not establish native compilation, visual parity, or complete interface delivery.

## Presentation

- The floating toolbar uses a lowest-container action island with the existing outline and shadow. Its outer corner radius follows the existing comfortable/compact density setting.
- The tool rail paints a high-container hover layer within the existing item rectangle. Disabled items do not receive that layer; the existing selected glyph and primary fill remain authoritative.
- Overlay controls share low/high/highest resting, hover and pressed surface roles in both themes. Menu selection retains its context container, while hover and press use distinct neutral layers. Window, field and popup rounding follows the existing density roles without changing padding, item spacing or target rectangles.
- Tooltips explicitly pair an opaque inverse surface with inverse text and a density-aware corner. Existing bilingual text, wrapping, dismissal and reduced-motion decoration remain intact. All added style pushes are restored after the tooltip.
- Timeline handles, playback controls and marker accents resolve the Preview scheme instead of the general scheme. Data-derived filament, toolpath and swatch colors are unchanged.
- The legend header plate uses its existing content span rather than painting beyond the window width. An accent marker and bottom divider separate controls from the data. The statistics card uses the lowest container with the existing outline, making summary values distinct from controls without changing their layout or computation.

## Preservation boundary

Changes in BaseRenderer.cpp are restricted to paint within BaseRenderer::render_legend. Legend measurements, fold state, item callbacks, visibility, time estimates, layer counts and swatch data remain unchanged. No slicing, hardware, simulation timing, camera, model selection, slider value, toolpath-buffer or frame-loop behavior is modified. GLCanvas3D.cpp and all headers remain unchanged.

## Verification

The existing source checks were run with:

~~~text
node --test ui-md3/tests/preview-overlays.test.mjs ui-md3/tests/preview-counter-font.test.mjs
~~~

All eight checks passed: six overlay/geometry source checks and two translated-counter font checks. The counter check was also observed rejecting a deliberate translated-text insertion inside a numeric mono-font span. The source was restored byte-for-byte and both counter checks passed again. No new paint-value assertion was added.

An initial overlay run lacked its SVG fixture in the sparse checkout. Restoring the existing resources/images read scope allowed the same check to pass. The separate native wx tooltip source suite could not load scripts/md3/check-tooltips.py, which is outside the assigned sparse scope; that suite was not a verification of the changed ImGui tooltip and remains unrun successfully here.

No full build, application launch, hardware interaction, installer execution or screenshot capture was performed. Native compilation, actual hover and focus states, user-seed contrast, bilingual text fit, reduced-motion behavior and light/dark comfortable/compact rendering at supported scales still require the parent task's built-artifact verification. Source checks do not establish those results.

## Tooltip rounding follow-up

The initial tooltip override used PopupRounding, but the vendored ImGui tooltip flags do not include Popup. Its actual rounding selection therefore reads WindowRounding. The wrapper now overrides WindowRounding before BeginTooltip and restores the existing single style entry after EndTooltip.

The focused renderer-tooltip-rounding.test.mjs check evaluates the vendored flag and rounding expressions using symbolic style values, then compares that selected member with the wrapper override and checks the single push/pop lifetime. It was observed failing against the original PopupRounding override and passing after this correction. This is source-contract evidence, not native compilation or rendered corner evidence.
