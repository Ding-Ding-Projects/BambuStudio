# Resizable Preferences

Preferences retains its custom caption and now requests a native resizing border, including when a caller supplies its own window style. Drag an edge or corner to change its size. The section strip, search field, scrolling pages and wrapping action footer retain their existing actions.

## Size and display changes

- The initial size is 780 by 640 device-independent pixels. The ordinary minimum is 640 by 480.
- Closing the dialog stores its size in the local application configuration as `preferences_width` and `preferences_height`, in device-independent pixels. Opening it again restores that size and centers it on its parent.
- Empty, malformed, nonpositive or excessive stored dimensions use the initial dimension. Configuration values are bounded before display scaling.
- The size and position are constrained to the selected display's usable work area, excluding its taskbar. On an unusually small display the ordinary minimum yields to the available work area.
- A DPI transition uses the operating system's suggested rectangle and then applies the work-area constraint. Page content no longer calls `Fit()` to reset the dialog dimensions.
- The page book and its surrounding card allow their scrolling contents to shrink independently of their preferred content size. This does not claim that every individual settings row reflows correctly.

Only window size is stored. No setting value, label, search query or personal wording payload is added to the geometry configuration. A configuration write failure is reported through the existing warning log and does not throw from dialog destruction.

## Verification

### Content-free geometry diagnostic

With the existing explicit `BAMBU_AUTOMATION=1` opt-in, the current-user local automation pipe accepts `preferences_geometry` with an empty `arguments` object. The local-capabilities-only listener rejects it. The existing dispatcher executes it on the UI thread; it neither opens Preferences nor changes its size, settings, visibility or capture protection. Unexpected arguments are rejected. Do not enable the general layout dump for a protected Preferences dialog.

The version-1 result has `kind: preferences-geometry`, `state` and `roles`. Fixed roles are `dialog`, `content_card`, `book`, `search`, `tab_strip` and the selected page's `viewport`. Construction supplies known object pointers, held weakly. No window-name search or descendant enumeration takes place. The footer is a sizer rather than a window and is not reported. A closed or destroying dialog returns `unavailable` with no roles; an absent role is `unavailable`, a destroying role is `destroying`, and an off-thread call returns `wrong_thread`.

Available roles report only screen rectangles, client and minimum sizes, DPI, shown/enabled flags and parent-client containment. Containment is null for the top-level dialog, whose owner is not its layout container. It is a geometric relationship, not a clipping verdict for hidden or scrollable content. No labels, names, values, accessibility text, pixels, paths, personal wording or credential content are read or emitted. Source, binary hash, process identity and capture-affinity readback belong in the external verification receipt. Protection remains active, including whole-dialog capture exclusion.

`node tests/preferences_geometry_probe.test.mjs` compiles the production module with neutral window doubles and the actual dispatch admission block. It exercises current-page selection, changed bounds, containment, hidden/destroyed/absent roles, thread admission, permission rejection and unexpected arguments. Canary getters reject text or child access. `--privacy-mutant` adds child enumeration and `--geometry-mutant` falsifies containment; both must fail. These checks are supplementary, not native-window evidence. Built verification must still bind the exact source and binary, exercise normal and minimum sizes, and retain protection readback. Geometry alone cannot establish typography, colors, text overflow or the full language/theme/DPI matrix.

`node tests/native_preferences_geometry.test.mjs` compiles and runs the production geometry helper with seven scenarios: saved size, corrupt configuration, chosen size retention, DPI scaling, removed display/taskbar bounds, negative monitor coordinates and small-display/minimum bounds. Three source mutation regressions reject removal of native resizing, the DPI constraint and size persistence. The baseline option verifies rejection of the previous constructor style.

These are source and non-window checks. Native edge dragging, keyboard navigation, restart persistence, monitor/DPI transitions and English, Cantonese and bilingual layout at minimum sizes remain pending until a built application is exercised. Existing `native_preferences_atlas.test.mjs` has four failing cases on both the task baseline and current source; its broad historical fingerprints are not reported as passing.
