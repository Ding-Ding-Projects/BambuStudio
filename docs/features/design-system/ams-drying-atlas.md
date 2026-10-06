# AMS drying controls: Studio Atlas

The native AMS control and embedded `open_humidity` route both reach `AMSDryCtrWin`. Its settings, filament-storage readiness guide, startup progress, active drying, unavailable reasons and error disclosure remain driven by the existing device state.

Status telemetry and drying settings now have separate density-aware semantic cards. Captions use supporting typography while measured values use a stronger heading. Temperature and duration controls retain their units and validators and wrap as complete groups. Guide content and Back/Start actions wrap. Main, guide and progress pages own scrolling, and deferred layout refreshes update visible scroll extents after resize, DPI and content changes. The dialog can be resized without replacing its existing minimum size.

Primary actions use the shared Filled variant, Back uses Outlined, and Stop uses Danger. The existing button implementation retains focus, hover, disabled states and reduced-motion handling. Actual button measurements still release the prior minimum before measuring changed labels or DPI. No animation, hardware-state inference, product string or device command is added.

Verification at baseline `50715f4355e8b845042bd4809bac2da2ce5c40f4`: `tests/ams_drying_atlas.test.mjs` has three passing preservation cases and three failing design cases; current source passes 6/6. The comparison preserves existing non-construction methods, action bindings and literal strings, normalizing only explicit layout-refresh calls. Deliberate command/timer mutations are rejected. `tests/ams_drying_buttons.test.mjs` compiles the actual two production button methods in a non-window fixture: 5/5 pass for primary, secondary, destructive, changing text and DPI; changing Danger to Filled deliberately gives 4/5.

No full application build, launch, capture or hardware action was performed. Native card geometry, scrolling, keyboard focus and the minimum-client-area, language, theme and scale matrix remain unverified. The pre-existing error-sizer attachment finding is handled in a separate correctness change. No authentication, network, persistence or drying capability behavior changes.
