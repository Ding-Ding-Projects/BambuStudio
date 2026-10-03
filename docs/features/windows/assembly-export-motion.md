# Assembly export progress entrance

The reachable assembly export progress frame owns one `MD3TransientEntrance`.
`AssemblyStepsUtils::update_assembly_export_progress` creates the frame and calls
its `update_progress`; `hide_assembly_export_progress` hides the same owner.
This is a top-level shaped frame, not a child panel or an OpenGL surface.

Progress text, range, value, percentage, layout and placement update immediately.
The existing `ShowWithoutActivating` call remains the only reveal operation.
After a successful hidden-to-shown transition, the frame starts the shared short
entrance. Repeated progress updates while visible do not restart it. Existing
`Raise`, cancellation callbacks and cancellation enablement remain unchanged.

The frame stops its controller on its own hide event and before destruction.
The shared controller invalidates queued starts, binds a weak owner and exact
native handle, and restores only the layered state it acquired. It skips child
or already-layered handles rather than taking another animator's ownership.
Reduced motion leaves the frame at its final appearance; changing the preference
during an entrance settles through the existing animator policy. No dismissal,
progress update or action waits for an animation.

## Verification pending

This change has received source inspection only. Compilation and all runtime
verification must run on the hosted Windows workflow against the exact package.
Use a project with at least one actual assembly step and a real supported export.
Inspect genuine temporal captures of first reveal, frequent progress updates,
hide and immediate reopen, cancellation during entrance, and owner destruction.
Confirm that focus does not move on reveal, progress and cancellation remain
immediate, and no old opacity callback affects a replacement frame. Exercise
reduced motion before reveal and during motion, theme changes and measured DPI.

The capture route must demonstrate that it records composited frame opacity;
an uncomposited client capture cannot establish this entrance. Source ownership
and final-state images alone do not prove intermediate motion or responsiveness.
No local product execution, build, tests or screenshots were performed.
