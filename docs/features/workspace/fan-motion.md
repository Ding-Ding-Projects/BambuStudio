# Device fan motion

The Device page previews the part cooling and auxiliary fans independently. Each
row keeps an accessible percentage slider and opens the existing fan control
dialog for commands. The blade icon rotates at a rate proportional to the
reported fan speed, with different shapes and rates for part and auxiliary
cooling. The slider track eases between reported values. A press ring acknowledges
input locally, while a separate ripple marks a sent command until matching
telemetry arrives.

Initial telemetry sets the display without a track transition. Identical polls
do not restart the motion, zero speed stops rotation, and a new speed retargets
the current transition. The timer stops while the control is hidden. The view
resets on disconnect or printer change, and reduced motion shows the latest
reported value without animation. The dialog remains responsible for fan
availability, printing warnings, air duct modes, and command submission.

The preview stores no user setting or command history and makes no network
request. If telemetry stops, the last reported speed remains visible and a
pending command ripple stays pending rather than implying confirmation.

`tests/native_gui/fan_motion_math.cpp` checks PWM conversion, command matching,
retargeting, and zero speed. The native Device build and interactive capture
remain the checks for rendering, accessibility, and timing.
