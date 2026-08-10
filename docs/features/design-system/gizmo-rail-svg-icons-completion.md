# Gizmo rail composite SVG completion

## Scope

This record covers the remaining direct SVG loads named in the `gizmo-rail-svg-icons` parity warning at commit `8f2ba7047e385e2d28c8a64d5cd9d1b8689f507f`.

The main rail glyph migration is already present. This completion replaces only the remaining semantic composites while keeping their established resource keys and dimensions.

## Chosen closure path

The parity warning explicitly identifies MD3-tokenized artwork as an acceptable alternative where a generic glyph would remove meaning or contrast. That alternative is used here because:

- fit camera requires a plate over the `NoBackground` 3D-scene overlay;
- X/Y/Z align and distribute controls are a semantic matrix, not one generic icon;
- the 30×22 keyboard hint is intentionally non-square;
- a local deterministic vector is lower risk than extending the GL font bridge for four isolated formatting/hint resources.

## Token mapping

| Artwork role | Light | Dark / inverse |
|---|---:|---:|
| Resting plate | `SurfaceContainerLow` `#F4F2F9` | `SurfaceContainer` `#25262B` |
| Hover plate | `SurfaceContainerHigh` `#E8E7EE` | `SurfaceContainerHighest` `#393A41` |
| Resting glyph | `OnSurfaceVariant` `#44464E` | `OnSurfaceVariant` `#CDCED8` |
| Emphasized glyph | `OnSurface` `#1A1B1F` | `OnSurface` `#E8E7EE` |
| Border | `OutlineVariant` `#C5C6D0` | `OutlineVariant` `#4A4C54` |
| Keyboard plate | `InverseSurface` `#2F3036` | theme-independent |
| Keyboard glyph | `InverseOn` `#F1F0F7` | theme-independent |

Viewport axis colors are preserved exactly as functional data colors: X `#EA4335`, Y `#34A853`, Z `#4C8BF5`.

## Safety constraints

The SVGs contain only `svg`, `rect`, `circle`, `path`, and `polygon`. They contain no font dependency, `<text>`, CSS, script, external reference, image, filter, or dynamic color expression. This keeps them compatible with the existing SVG-to-OpenGL texture loader.

## Verification status

Static inventory, XML, palette, geometry, uniqueness, and Inkscape rasterization pass. Native Windows wxWidgets/OpenGL evidence is still required before changing the parity row from `partial` to `done`. Use `RUNTIME_SIGNOFF_BAMBUSTUDIO_MD3.md` and the repository's headless run skill.
