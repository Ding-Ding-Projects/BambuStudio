# Continuous liveview and camera view controls

New and migrated profiles enable automatic liveview startup, continuous monitoring and interruption retry. Explicit values for these three settings are preserved. Open the camera settings menu to change **Start liveview automatically**, **Keep liveview active**, or **Retry interrupted liveview**. The general preference **Keep liveview active** controls the same continuous-monitoring setting.

Continuous monitoring preserves an active stream across tab changes, minimization, inactivity and periods with no printing. Turning it off restores visibility and inactivity stopping. Pressing Stop pauses the selected printer session. Returning to the tab does not cancel that pause. Press Play to resume, or select a different printer to begin a new eligible session. Closing the application releases its stream. These settings do not enable automatic recording, timelapse recording or virtual-camera broadcasting; their existing controls remain separate.

A transient interruption shows an inline status and retries after 5, 10, 15 seconds and so on, capped at 60 seconds. A successful connection resets the delay. Known unsupported, authentication and player-availability responses stop automatic attempts and leave Play available for a manual retry. Automatic retries do not repeatedly open LAN address dialogs. Connection initialization times out after 15 seconds even while the camera tab is hidden.

The native camera view fits the complete image at 100%, then supports digital zoom through 500%. Use the pointer wheel or supported native pinch gesture to zoom around the pointer, drag to pan a zoomed image, or use the camera header's minus, percentage menu, plus and reset controls. Keyboard shortcuts on the camera view are `+`, `-`, arrow keys and `0` (reset). Pan bounds retain image coverage where the image fills an axis and keep unavoidable letterboxing centered. Invalid saved values are normalized safely.

Embedded and fullscreen views use the same media control and normalized transform. Each printer's zoom and image center are stored in the local application configuration under `camera_view`; changes are saved at most once a second, and printer changes save the current view before restoring the next one. The full configuration remains local and is not sent in camera telemetry.

## Verification

`tests/camera_view_geometry_test.cpp` exercises fitted geometry, pointer anchoring, pan bounds, reset, zoom limits, nonfinite values, zero-size geometry and constrained axes. `tests/camera_playback_policy_test.cpp` exercises visibility, eligibility, explicit pause, retry delays and terminal response classification. Both are standalone C++ assertion executables. Build them without `NDEBUG` so assertions execute.

Native build and real embedded/fullscreen interaction evidence are separate release checks. Standalone geometry and policy checks do not prove camera transport, printer authentication or hardware behavior. This implementation makes no physical print, heater or movement request.
