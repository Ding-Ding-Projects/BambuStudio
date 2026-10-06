# Studio Atlas common floating inspector framing

The only production owner changed is src/slic3r/GUI/Gizmos/GLGizmoBase.cpp, in GizmoImguiBegin and GizmoImguiEnd. Baseline: b5c42ce78a8e021509bdc504e5a40ab62d9cd8e4. The parent baseline was fast-forwarded into the isolated implementation branch before editing.

## Coverage and appearance

Twelve existing paired callers use this owner: GLGizmoAdvancedCut.cpp, GLGizmoAssembly.cpp, GLGizmoBrimEars.cpp, GLGizmoFdmSupports.cpp, GLGizmoFlatten.cpp, GLGizmoFuzzySkin.cpp, GLGizmoMeasure.cpp, GLGizmoMmuSegmentation.cpp, GLGizmoMeshBoolean.cpp, GLGizmoSeam.cpp, GLGizmoSVG.cpp and GLGizmoText.cpp, all under src/slic3r/GUI/Gizmos/. Object-manipulation windows bypass this wrapper and remain outside this unit.

The framing resolves an opaque SurfaceContainerLowest plate and OutlineVariant edge in the canvas theme. WindowRounding follows existing density and canvas scale. A short Primary heading rule uses the inner half of existing top padding, above the first content row; it is omitted when the existing space is insufficient. No content item or extra layout space is introduced. Existing caller borders and local control styles remain authoritative.

Begin forwards the exact name and flags and returns the original visibility result. End retains the original width recording and end call, then restores the two colors and one rounding value even when Begin returned false. No cursor, item width, padding, input identity, transform, tool calculation, callback, selection, camera, data color, timer or frame scheduling changes. No new motion is added; existing shared motion and reduced-motion behavior remain unchanged.

## Verification and limits

Run: node --test tests/gizmo_inspector_framing.test.mjs

Three source checks pass: caller flags/return/style lifetime, the paired route in all twelve panels, and a normalized hash proving base-tool source outside the two wrappers unchanged. Deliberately replacing forwarded flags with zero makes the first check fail; restoring original bytes returns all three checks to passing. These are source contracts, not a native compile or window interaction result.

No full build, launch, screenshot, hardware operation or release was performed. Light/dark contrast, actual heading-rule visibility, text clearance, all panel states, scaling, density, language combinations and runtime style restoration remain unverified. This unit may be reverted independently without changing earlier renderer, notification, embedded or object-manipulation work.
