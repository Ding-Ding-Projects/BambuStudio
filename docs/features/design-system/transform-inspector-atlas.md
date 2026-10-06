# Studio Atlas transform inspector

This source-only appearance unit is confined to Gizmos/GizmoObjectManipulation.cpp. It follows the renderer inventory made from baseline 2a3528c451f3a5caccdfd7fd9e8d9d8964e4d4c8 and does not establish native compilation or rendered acceptance.

## Actual ownership and selected group

All paths below are relative to src/slic3r/GUI/.

- Gizmos/GLGizmosManager.cpp dispatches the active tool input window through GLGizmoBase::render_input_window in Gizmos/GLGizmoBase.cpp. That base owns anchor conversion and first-render invalidation.
- Gizmos/GLGizmoMove.cpp, GLGizmoRotate.cpp and GLGizmoScale.cpp call the three corresponding windows in Gizmos/GizmoObjectManipulation.cpp. Its md3_value_input helper owns all fifteen numeric fields, axis_header paints the coordinate headings, and show_align_icon owns the local alignment tooltip.
- Separate source-defined tool panels remain in Gizmos/GLGizmoAdvancedCut.cpp, GLGizmoCut.cpp, GLGizmoBrimEars.cpp, GLGizmoAssembly.cpp, GLGizmoFaceDetector.cpp, GLGizmoFdmSupports.cpp, GLGizmoFlatten.cpp, GLGizmoFuzzySkin.cpp, GLGizmoHollow.cpp, GLGizmoMeasure.cpp, GLGizmoMeshBoolean.cpp, GLGizmoMmuSegmentation.cpp, GLGizmoSeam.cpp, GLGizmoSimplify.cpp, GLGizmoSlaSupports.cpp, GLGizmoSVG.cpp and GLGizmoText.cpp. Finding an override in source is not proof that every tool is registered or reachable for every printer mode; those activation inventories remain separate work.
- GLCanvas3D.cpp owns LayersEditing::render_overlay and its plate, assembly-thumbnail, assembly-preview, assembly-step, return, fit-camera, collapse, paint and assembly-control overlays. Those functions remain unchanged by this unit.

The selected coherent group is the Move/Rotate/Scale inspector and its alignment tooltip. No shared Widgets source, native workflow, monitor, vendored renderer or header changes are included.

## Appearance and functional boundary

Numeric text remains in the existing monospaced face and OnSurface role. The field body uses SurfaceContainerLow. An outline uses OutlineVariant at rest, Outline on hover and Primary while active. It reads IsItemActive, IsItemHovered and the final GetItemRect bounds after the existing input function handles the event. Stroke width is bounded by the existing frame padding; the path stays inside the existing rectangle. It introduces no item, cursor movement, text-size change, filled overlay, input interception or additional animation.

An outer hover/active style override alone would be ineffective here: the vendored BBLInputDouble sets BorderActive internally, and its InputTextEx path paints FrameBg. The local post-input decoration therefore uses the actual final bounds instead of attempting an ignored override. The inspected numeric path has no separate validation-color renderer; parsing, expression handling and edited-state reporting are unchanged.

Axis headings retain their X/Y/Z data colors and original centering. A neutral column separator is drawn in the existing row gap without consuming layout space. The local alignment tooltip uses InverseSurface/InverseOn and the actual WindowRounding style selected for tooltip windows, with a balanced push/pop. Its conditions and copy remain unchanged.

The three window functions keep all original cursor, width, row, positioning, unit and callback statements. Existing nonnumeric controls retain their enclosing style. Transformation, parsing, bounds, camera, selection and model mutation code are untouched.

## Existing motion ownership

ImGuiWrapper.cpp owns the per-context menu decoration, popup and tooltip timelines. menu_decoration_progress uses bounded 100 ms feedback and settles immediately under MD3::Motion::reduced. render limits popup paint treatment to owned popup lists, restores original vertex colors after drawing and requests extra frames only during the bounded transition. tooltip_decoration owns its separate content/source-keyed 100 ms marker and reduced-motion settlement. ImGuiWrapper.hpp stores those bounded motion maps. None is edited here.

GLGizmoBase's first-render invalidation is a visibility/lifecycle mechanism, not a new decorative animation. IMSlider.cpp owns real playback timing. Neither path is altered. This unit adds no timer, canvas fade, geometry interpolation or frame scheduling.

## Source verification

The existing focused check passed:

~~~text
node --test --test-name-pattern='gizmo object-manipulation' ui-md3/tests/md3-conversion-contracts.test.mjs
~~~

One check passed. Its fifteen-field inventory was also observed rejecting a deliberately missing helper call; original bytes were restored and the check passed again. This existing source check establishes helper coverage, not appearance or runtime correctness.

A baseline comparison confirmed all fifteen numeric call statements and arguments, the raw BBLInputDouble delegation, all axis identities, and every cursor/width statement unchanged. Public-bound text and staged whitespace checks accompany the unit.

No full build, application launch, screenshot, hardware action or release was performed. Native compilation and actual text/caret clearance, focus visibility, disabled/read-only behavior, expression editing, tooltip wrapping, theme/custom-seed contrast and supported scale/language tuples remain unverified. Revert this appearance unit independently if rejected, retaining earlier renderer and functional changes.
