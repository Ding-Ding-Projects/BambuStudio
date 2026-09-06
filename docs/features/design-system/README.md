# Design system

This category documents how the native wxWidgets/OpenGL application consumes the vendored Material
Design 3 design system.

- [Vendored Material Design 3 design system](md3-design-system.md) — token source of truth, the
  ground-up color/type/metric migration, contextual schemes, fonts, failure modes, and the parity
  audit result.
- [MD3 parity register](md3-parity-register.md) — the canonical element-by-element conformance
  register and wave plan driving the structural-anatomy migration. The register itself carries the
  live done / deviation / open counts; consult it rather than any snapshot elsewhere.
- [Gizmo rail SVG completion](gizmo-rail-svg-icons-completion.md) — the bounded 34-asset MD3-token
  overlay for the remaining semantic gizmo composites, with native-runtime evidence still required
  before the parity row can move from partial to done.

- [Kit widgets added in the every-element sweep](kit-widgets-2026-09.md) — LabeledRadioButton and
  RadioGroup, TextArea, ListBox, Button::SetIconBitmap, and the Material-by-default Button.
- [Runtime layout probe](layout-probe.md) — the off-by-default NDJSON walker that finds starved
  sizer rows, zero-sized controls and clipped labels mechanically, and its report reader.
- [Themed surface colors on StaticBox cards](themed-surface-colors.md) — how a card gets its fill,
  why `SetBackgroundColorNormal()` could silently do nothing, and the stale constructor-time window
  background behind light plates in dark mode.
- [Generated visual showcase](generated-visual-showcase.md) — the image suite shared by the
  interactive app, GitHub Pages landing page, and social preview, including loading, accessibility,
  deployment, and verification behavior.

## Design source

The canonical in-repo design source is [`ui-md3/design-system/`](../../../ui-md3/design-system/).
Token values there match `src/slic3r/GUI/Widgets/MD3Tokens.hpp` exactly; the header is the native
source of truth that the C++ code resolves against.

## Postman collections

Not applicable. The design system is a compile-time token and typography layer for a desktop
application; it exposes no HTTP or API surface, so no Postman collection is provided for this
category.
