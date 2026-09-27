# Native print workflow design

The source of truth for this update is [the state and implementation handoff](native-print-workflows.md). It describes the new native Bambu Studio controls, their states, accessibility names, and the capture matrix. The source files under `src/slic3r/GUI/` remain the production implementation.

Material Designer was built at `a3a12816bb1e135f30afbb198e8c76e1ce896588` and launched through the required isolated off-screen route. The live window rendered black in two real captures and did not expose a usable project creation or export surface. Its process tree and off-screen desktop were closed. This design therefore uses the existing native Material Design 3 kit and the request's visual references. A Material Designer prototype is still pending; no generated design export is represented as completed.

Production captures and layout probe receipts will be linked here only after they are obtained from the built application and tied to the exact source revision and executable hash.
