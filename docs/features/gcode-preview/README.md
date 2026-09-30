# G-code preview

This category documents the sliced **Preview** page — the toolpath viewport, its
color-scheme legend, and the transport/layer controls shown after a plate is sliced.
(For the pre-import 3D model viewer used by the MakerWorld "Download and Open" flow,
see [`../model-preview/`](../model-preview/) instead — that is a different surface.)

- [Toolpath color-scheme legend](toolpath-legend.md) — the ImGui legend overlaid on
  the Preview viewport: color-scheme selector, the per-filament `FILAMENT | MODEL`
  usage table, the greyed change-times / cost summary lines, options chips, and the
  time-estimation card.
- [Preview overlays](preview-overlays.md): the rules that keep the plate strip, the
  status pill, the legend dock and the notification column from covering each other,
  and how the ink grouping card is sized from its content.

No Postman collection is applicable: this category exposes no HTTP API.
