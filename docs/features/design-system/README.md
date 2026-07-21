# Design system

This category documents how the native wxWidgets/OpenGL application consumes the vendored Material
Design 3 design system.

- [Vendored Material Design 3 design system](md3-design-system.md) — token source of truth, the
  ground-up color/type/metric migration, contextual schemes, fonts, failure modes, and the parity
  audit result.

## Design source

The canonical in-repo design source is [`ui-md3/design-system/`](../../../ui-md3/design-system/).
Token values there match `src/slic3r/GUI/Widgets/MD3Tokens.hpp` exactly; the header is the native
source of truth that the C++ code resolves against.

## Postman collections

Not applicable. The design system is a compile-time token and typography layer for a desktop
application; it exposes no HTTP or API surface, so no Postman collection is provided for this
category.
