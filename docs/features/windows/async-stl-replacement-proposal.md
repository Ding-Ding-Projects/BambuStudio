# Asynchronous STL replacement proposal

Status: unfinished design, not implemented or verified. Source inspected at
`23d01afd3d53a3f7d5dcc42db5885281730bca31`. No local execution was performed.

## Scope and observed behavior

The first candidate should cover only a single-volume plain-STL replacement.
Reload batches, STEP dialogs, OBJ color callbacks and project imports retain their
existing behavior until separately designed.

`Plater::priv::replace_volume_with_stl` reads and normalizes a detached model before
constructing `BusyInfo`. Reload's short non-OBJ block destroys its `BusyInfo` before
reading the file. Both paths then mutate the live model and update GUI state.
Animating the busy frame cannot make that synchronous work responsive.

`JobNew::process` and GUI-thread `finalize`, through the existing
`PlaterWorker<BoostThreadWorker>`, provide a detached-import seam. That worker does
not automatically acquire the legacy `Jobs::before_start` slicing exclusion.

## Proposed ownership boundary

Investigate a move-only idle reservation in `BackgroundSlicingProcess.cpp/.hpp`:

1. Acquire with `try_lock` only in `STATE_INITIAL` or `STATE_IDLE`, with no pending
   UI task. Contention, active work and completed-but-unsettled states are unavailable.
2. Bind the reservation to an operation identity, native generation, print and plate.
   Retain a reservation flag while releasing the mutex before any callback.
3. Check that flag under the same mutex in `start()`, before thread creation or a
   state transition. A reservation prevents new starts through the central API.
4. Retain reservation ownership on the GUI side across detached import and result
   application. The worker receives immutable input and owns only its detached result.
5. Release only the matching reservation. A nonblocking release and owner-destruction
   protocol is still required; a destructor that waits for the mutex is not acceptable.

Do not use the optional worker observation as a lease. Do not hold its mutex across
model application, callbacks or methods that acquire it again. Do not acquire by
calling `stop()`, which can wait, or treat cancellation requested as cancellation
completed. The first design does not settle `STATE_FINISHED` or `STATE_CANCELED`.

## Target validation remains unresolved

The GUI completion must validate its weak owner, current operation identity, exact
model/object/volume identities and every relevant target state before taking one
undo snapshot and applying the result. Vector indices and raw pointers cannot be
retained as identity across the asynchronous interval.

`ModelObject` and `ModelVolume` inherit `ObjectBase`; the undo snapshot timestamp is
not a complete mutation revision. Replacement reads configuration, transformation,
source offsets and conversion flags, volume type/material, four facet annotations,
object geometry and sibling volumes for bed placement, and SLA points/holes for
reprojection. Instances, workspace replacement and pending continuation state also
need an explicit treatment. An incomplete hand-written fingerprint is insufficient.

Existing cereal serialization uses special wrappers and includes mutable geometry
caches. Its byte equality and GUI cost have not been established as a suitable
validation contract. Full mesh serialization on the GUI thread could recreate the
responsiveness problem. No complete cheap validation mechanism is claimed here.

## Required review and hosted evidence

Before implementation, independently refute reservation acquisition/release,
central start exclusion, reentrancy, owner destruction and target-validation
completeness. Keep model, GUI and GL application on the GUI thread with no nested
event-loop or modal interaction during the validated application interval.

Hosted verification must demonstrate heartbeat progress during actual slow import,
cancel without later application, stale target/workspace rejection, owner closure,
import exceptions, exactly one undo snapshot, preserved transforms/configuration/
annotations and correct scene refresh. Record import and GUI-finalization durations
separately. A passing detached-import check does not establish responsive finalization.
